// SPDX-License-Identifier: Apache-2.0
#include "ui/PdfViewerWidget.h"
#include "GpMainWindow.h"
#include "shell/StatusBar.h"
#include "core/AnnotationSerializer.h"
#include <QDebug>
#include <QMessageBox>
#include <QPdfDocument>
#include <QPdfView>
#include <QPdfSearchModel>
#include <QPdfLink>
#include <QPdfBookmarkModel>
#include <QPdfPageNavigator>
#include <QPdfPageRenderer>
#include <QVBoxLayout>
#include <QPrinter>
#include <QPrintDialog>
#include <QPainter>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <limits>
#include <QThread>
#include <QPointer>
#include <QProgressDialog>
#include <QTemporaryFile>
#include <QTimer>
#include <QRubberBand>
#include <QMouseEvent>
// D-02 fix: do NOT include <podofo/podofo.h> in the UI layer.
// All PoDoFo page-manipulation operations are routed through
// gp::PdfPageOps (engines/podofo/PdfPageOps.h) which lives in pdfws_engines.
#include "engines/podofo/PdfPageOps.h"
#include <QMap>
#include <QGraphicsColorizeEffect>
#include <QGraphicsEffect>
#include <QScrollArea>
#include <QLabel>
#include <QPdfLinkModel>
#include <QDesktopServices>
#include <QUrl>
#include <QCursor>
#include "util/Badge.h"

// Wave 2B #4: real content-level Night Mode. Qt ships no built-in "invert
// colors" QGraphicsEffect (only blur/colorize/opacity/shadow), so this
// subclasses QGraphicsEffect and inverts the source pixmap's RGB channels
// directly -- the standard Qt technique for a full color-inversion filter.
// Distinct from Eye Care (QGraphicsColorizeEffect sepia *tint* -- the page
// stays white/glaring, only tinted) and Dark Mode (chrome-only, MainWindow::
// toggleTheme()); this genuinely inverts the rendered page content pixels.
namespace {
class NightModeEffect : public QGraphicsEffect {
public:
    explicit NightModeEffect(QObject *parent = nullptr) : QGraphicsEffect(parent) {}
protected:
    void draw(QPainter *painter) override {
        QPoint offset;
        QPixmap pixmap = sourcePixmap(Qt::LogicalCoordinates, &offset, QGraphicsEffect::PadToEffectiveBoundingRect);
        if (pixmap.isNull()) {
            drawSource(painter);
            return;
        }
        QImage img = pixmap.toImage().convertToFormat(QImage::Format_ARGB32);
        img.invertPixels(QImage::InvertRgb); // preserves the alpha channel
        painter->drawImage(offset, img);
    }
};
}

PdfViewerWidget::PdfViewerWidget(QWidget *parent)
    : QWidget(parent)
    , m_document(new QPdfDocument(this))
    , m_pdfView(new QPdfView(this))
    , m_searchModel(new QPdfSearchModel(this))
    , m_bookmarkModel(new QPdfBookmarkModel(this))
    , m_pageNavigator(nullptr)
    , m_pageRenderer(new QPdfPageRenderer(this))
    , m_annotationLayer(new AnnotationLayer(this))
    , m_zoomFactor(1.0)
    , m_toolMode(ToolMode::HandTool)
    , m_rotation(0)
    , m_saveDebounceTimer(new QTimer(this))
    , m_pageChangeTimer(new QTimer(this))
{
    m_searchModel->setDocument(m_document);
    m_bookmarkModel->setDocument(m_document);

    // Wire up QPdfPageRenderer in multi-threaded mode (Fix 5)
    m_pageRenderer->setDocument(m_document);
    m_pageRenderer->setRenderMode(QPdfPageRenderer::RenderMode::MultiThreaded);

    m_pdfView->setDocument(m_document);
    m_pdfView->setSearchModel(m_searchModel);
    m_pdfView->setObjectName("pdfView");
    m_pdfView->setPageMode(QPdfView::PageMode::MultiPage);
    m_pdfView->setZoomMode(QPdfView::ZoomMode::Custom);

    // Wave 1B #2: real hyperlink (URI + internal GoTo) click-navigation.
    // QPdfView exposes no built-in link handling for QWidgets (unlike the Qt
    // Quick PdfMultiPageView), so we index the current page's links via
    // QPdfLinkModel ourselves and watch the viewport for hover/click.
    m_linkModel = new QPdfLinkModel(this);
    m_linkModel->setDocument(m_document);
    m_pdfView->viewport()->installEventFilter(this);
    m_pdfView->viewport()->setMouseTracking(true);

    m_annotationLayer->setMode(m_toolMode);
    m_annotationLayer->raise();

    m_annotationLayer->setPageAtCallback([this](QPoint){
        return m_pageNavigator->currentPage();
    });

    // Use the view's built-in page navigator
    m_pageNavigator = m_pdfView->pageNavigator();
    connect(m_pageNavigator, &QPdfPageNavigator::currentPageChanged, this, &PdfViewerWidget::onPageChanged);
    connect(m_annotationLayer, &AnnotationLayer::annotationsChanged, this, &PdfViewerWidget::annotationsChanged);
    connect(m_annotationLayer, &AnnotationLayer::textEditRequested, this, &PdfViewerWidget::textEditRequested);

    // Save debounce: annotationsChanged restarts a 2-second timer (Fix 7)
    m_saveDebounceTimer->setSingleShot(true);
    m_saveDebounceTimer->setInterval(2000);
    connect(m_saveDebounceTimer, &QTimer::timeout, this, &PdfViewerWidget::saveAnnotations);
    connect(m_annotationLayer, &AnnotationLayer::annotationsChanged, this, [this]() {
        m_saveDebounceTimer->start();
    });

    // Wave 1C #3: keep two-page mode's per-page overlays live -- refresh
    // whenever the annotation set changes (drawn/edited/deleted in the
    // primary single-page view) or the document's search results change
    // (new search string, or async result population), instead of the
    // previous "render once on toggle/page-change" static behavior.
    connect(m_annotationLayer, &AnnotationLayer::annotationsChanged, this, [this]() {
        if (m_twoPageMode) updateTwoPageView();
    });
    connect(m_searchModel, &QAbstractItemModel::modelReset, this, [this]() {
        if (m_twoPageMode) updateTwoPageView();
    });
    connect(m_searchModel, &QAbstractItemModel::rowsInserted, this, [this]() {
        if (m_twoPageMode) updateTwoPageView();
    });

    // Page change coalescing (Fix 13)
    m_pageChangeTimer->setSingleShot(true);
    m_pageChangeTimer->setInterval(50);
    connect(m_pageChangeTimer, &QTimer::timeout, this, [this]() {
        if (m_pageNavigator && m_document) {
            emit pageChanged(m_pageNavigator->currentPage() + 1, m_document->pageCount());
        }
    });

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Create a container to stack the PDF view and the annotation layer
    QWidget *container = new QWidget(this);
    m_pdfView->setParent(container);
    m_annotationLayer->setParent(container);

    // Wave 1A §9.7: on-page signature validity badge, floating in the
    // top-right corner above the page content. Hidden until
    // setSignatureValidityBadge() is called with a non-empty summary.
    m_signatureBadge = new gp::Badge(QString(), gp::Badge::Info, container);
    m_signatureBadge->hide();
    m_signatureBadge->raise();

    // Setup TwoPage view
    m_twoPageScrollArea = new QScrollArea(container);
    m_twoPageScrollArea->setAlignment(Qt::AlignCenter);
    QWidget *twoPageWidget = new QWidget();
    QHBoxLayout *twoPageLayout = new QHBoxLayout(twoPageWidget);
    m_leftPageLabel = new QLabel();
    m_rightPageLabel = new QLabel();
    twoPageLayout->addWidget(m_leftPageLabel);
    twoPageLayout->addWidget(m_rightPageLabel);
    m_twoPageScrollArea->setWidget(twoPageWidget);
    m_twoPageScrollArea->setWidgetResizable(true);
    m_twoPageScrollArea->hide();

    // Wave 1C #3: two-page mode used to be a static, render-once bitmap pair
    // with the shared AnnotationLayer hidden outright -- annotations and
    // search matches silently disappeared with no warning the moment a user
    // switched into two-page view. Give each visible page its own live
    // AnnotationLayer overlay (same class the single-page view uses, so all
    // existing paint/hit-test logic is reused, not reimplemented) so
    // annotation visibility and search-result highlighting stay in sync with
    // the primary view instead of going stale. Parented directly to each
    // QLabel so their geometry always tracks that page's rendered bitmap.
    m_leftAnnotationLayer = new AnnotationLayer(m_leftPageLabel);
    m_rightAnnotationLayer = new AnnotationLayer(m_rightPageLabel);
    for (AnnotationLayer *overlay : {m_leftAnnotationLayer, m_rightAnnotationLayer}) {
        overlay->setMode(ToolMode::HandTool);          // read-only display for now (see report)
        overlay->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    }

    // We'll manage sizes manually in resizeEvent for true overlap
    layout->addWidget(container);
}

PdfViewerWidget::~PdfViewerWidget()
{
    // Flush any pending debounced save (Fix 7)
    if (m_saveDebounceTimer->isActive()) {
        m_saveDebounceTimer->stop();
        saveAnnotations();
    }
}

bool PdfViewerWidget::loadDocument(const QString &fileName)
{
    m_filePath = fileName;
    clearPageCache();
    m_document->load(fileName);
    if (isLoaded()) loadAnnotations();
    return isLoaded();
}

void PdfViewerWidget::reload()
{
    if (!m_filePath.isEmpty()) {
        loadDocument(m_filePath);
    }
}

bool PdfViewerWidget::isLoaded() const
{
    return m_document->pageCount() > 0;
}

// ---- Zoom ----

void PdfViewerWidget::zoomIn()
{
    m_zoomFactor *= 1.25;
    m_pdfView->setZoomFactor(m_zoomFactor);
    if (m_twoPageMode) updateTwoPageView();
    else if (m_rotation != 0) updateRotatedPageView();
}

void PdfViewerWidget::zoomOut()
{
    m_zoomFactor /= 1.25;
    if (m_zoomFactor < 0.1) m_zoomFactor = 0.1;
    m_pdfView->setZoomFactor(m_zoomFactor);
    if (m_twoPageMode) updateTwoPageView();
    else if (m_rotation != 0) updateRotatedPageView();
}

void PdfViewerWidget::zoomFitWidth()
{
    m_pdfView->setZoomMode(QPdfView::ZoomMode::FitToWidth);
}

void PdfViewerWidget::zoomFitPage()
{
    m_pdfView->setZoomMode(QPdfView::ZoomMode::FitInView);
}

void PdfViewerWidget::setZoomLevel(qreal level)
{
    m_zoomFactor = level;
    m_pdfView->setZoomMode(QPdfView::ZoomMode::Custom);
    m_pdfView->setZoomFactor(m_zoomFactor);
    if (m_twoPageMode) updateTwoPageView();
    else if (m_rotation != 0) updateRotatedPageView();
}

qreal PdfViewerWidget::zoomLevel() const
{
    return m_zoomFactor;
}

void PdfViewerWidget::rotateClockwise()
{
    m_rotation = (m_rotation + 90) % 360;
    updateRotation();
}

void PdfViewerWidget::rotateCounterClockwise()
{
    m_rotation = (m_rotation + 270) % 360;
    updateRotation();
}

void PdfViewerWidget::updateRotation()
{
    // Wave 1B #1: this used to ONLY rotate AnnotationLayer's overlay while the
    // actual page stayed upright underneath -- audit §9.1's headline
    // correctness bug ("current behavior rotates only an invisible-until-
    // annotated overlay while the page stays upright"). AnnotationLayer's own
    // paint/hit-test rotation transform was already correct and designed to
    // pair with a genuinely-rotated page; it was simply never given one.
    m_annotationLayer->setRotation(m_rotation);

    // The page-bitmap cache is keyed by scale factor only (see renderPage()),
    // so any bitmap rendered before this rotation change must be evicted --
    // otherwise a stale, wrongly-oriented pixmap would be served back under
    // the same scale factor.
    clearPageCache();

    if (m_twoPageMode) {
        // updateTwoPageView() already renders through renderPage(), which is
        // now rotation-aware -- nothing else needed here.
        updateTwoPageView();
    } else {
        updateRotatedPageView();
    }
}

// Wave 1B #1: QPdfView (QtPdfWidgets) exposes no rotation API at all -- see
// header comment on m_rotatedPageLabel. While a non-zero view rotation is
// active, replace the native QPdfView surface with a manual bitmap render of
// the current page (through the now rotation-aware renderPage()), fit to the
// same rect QPdfView would otherwise occupy so AnnotationLayer's existing
// rotate-around-center transform stays visually aligned with it. Reverts to
// native QPdfView at rotation 0.
//
// Known limitation (disclosed, not silently swept under the rug): unlike
// QPdfView's native continuous scrolling, this fallback displays one
// "fit-to-view" page at a time, so free pixel-scrolling is unavailable while
// rotated -- page navigation (Next/Prev, page-number entry, keyboard
// shortcuts) still works normally via goToPage()/onPageChanged().
void PdfViewerWidget::updateRotatedPageView()
{
    if (m_rotation == 0) {
        if (m_rotatedPageLabel) m_rotatedPageLabel->hide();
        if (!m_twoPageMode) m_pdfView->show();
        return;
    }
    if (m_twoPageMode || !m_document || m_document->pageCount() == 0) return;

    if (!m_rotatedPageLabel) {
        m_rotatedPageLabel = new QLabel(m_pdfView->parentWidget());
        m_rotatedPageLabel->setAlignment(Qt::AlignCenter);
    }

    m_pdfView->hide();
    m_rotatedPageLabel->setGeometry(m_pdfView->geometry());

    const int page = currentPage();
    const QSizeF pagePts = m_document->pagePointSize(page);
    const bool swapped = (m_rotation == 90 || m_rotation == 270);
    const qreal pageW = swapped ? pagePts.height() : pagePts.width();
    const qreal pageH = swapped ? pagePts.width()  : pagePts.height();
    const qreal availW = qMax(1, m_rotatedPageLabel->width());
    const qreal availH = qMax(1, m_rotatedPageLabel->height());
    const qreal fitScale = qMax(0.05, qMin(availW / qMax(pageW, 1.0), availH / qMax(pageH, 1.0)));

    const QImage img = renderPage(page, fitScale);
    m_rotatedPageLabel->setPixmap(QPixmap::fromImage(img));
    m_rotatedPageLabel->show();
    m_annotationLayer->raise();
}

// ---- Tool Mode ----

void PdfViewerWidget::setReadOnly(bool readOnly)
{
    m_readOnly = readOnly;
    if (readOnly)
        setToolMode(ToolMode::HandTool);
}

void PdfViewerWidget::setToolMode(ToolMode mode)
{
    // In read-only mode (e.g. an expired document) only viewing and text
    // selection are permitted; any editing/annotation mode falls back to Hand.
    if (m_readOnly && mode != ToolMode::HandTool && mode != ToolMode::SelectText)
        mode = ToolMode::HandTool;

    m_toolMode = mode;
    m_annotationLayer->setMode(mode);

    switch (mode) {
        case ToolMode::HandTool:
            m_pdfView->setCursor(Qt::OpenHandCursor);
            break;
        case ToolMode::EditObject:
            m_pdfView->setCursor(Qt::ArrowCursor);
            break;
        case ToolMode::SelectText:
            m_pdfView->setCursor(Qt::IBeamCursor);
            break;
        case ToolMode::DrawFreehand:
        case ToolMode::DrawShape:
        case ToolMode::Crop:
            m_pdfView->setCursor(Qt::CrossCursor);
            break;
        case ToolMode::FormAddText:
        case ToolMode::FormAddCheckbox:
        case ToolMode::FormAddRadio:
        case ToolMode::FormAddDropdown:
        case ToolMode::FormAddListBox:
        case ToolMode::FormAddDate:
        case ToolMode::FormAddNumeric:
        case ToolMode::FormAddSignature:
        case ToolMode::FormAddButton:
        case ToolMode::FormAddCalculated:
            m_pdfView->setCursor(Qt::CrossCursor);
            break;
        default:
            m_pdfView->setCursor(Qt::ArrowCursor);
            break;
    }
}

// static helper — must be defined before first use in this TU
bool PdfViewerWidget::isFormBuilderMode(ToolMode mode) {
    switch (mode) {
        case ToolMode::FormAddText:
        case ToolMode::FormAddCheckbox:
        case ToolMode::FormAddRadio:
        case ToolMode::FormAddDropdown:
        case ToolMode::FormAddListBox:
        case ToolMode::FormAddDate:
        case ToolMode::FormAddNumeric:
        case ToolMode::FormAddSignature:
        case ToolMode::FormAddButton:
        case ToolMode::FormAddCalculated:
            return true;
        default:
            return false;
    }
}

void PdfViewerWidget::setAnnotationColor(const QColor &color)
{
    m_annotationLayer->setColor(color);
}

void PdfViewerWidget::setAnnotationThickness(int thickness)
{
    m_annotationLayer->setThickness(thickness);
}

void PdfViewerWidget::deleteSelectedAnnotation()
{
    m_annotationLayer->deleteSelected();
}

QList<AnnotationItem> PdfViewerWidget::annotations() const
{
    return m_annotationLayer->annotations();
}

void PdfViewerWidget::saveAnnotations()
{
    if (m_filePath.isEmpty()) return;

    QJsonDocument doc = AnnotationSerializer::toJson(m_annotationLayer->annotations());
    const QString filePath = m_filePath + ".ann";

    QThread* worker = QThread::create([filePath, doc]() {
        QFile file(filePath);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(doc.toJson());
            file.close();
        }
    });
    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

void PdfViewerWidget::loadAnnotations()
{
    if (m_filePath.isEmpty()) return;

    QFile file(m_filePath + ".ann");
    if (!file.open(QIODevice::ReadOnly)) return;

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    m_annotationLayer->setAnnotations(AnnotationSerializer::fromJson(doc));
    file.close();
}

void PdfViewerWidget::setAnnotations(const QList<AnnotationItem> &items)
{
    if (m_annotationLayer)
        m_annotationLayer->setAnnotations(items);
}

// ---- Search ----

void PdfViewerWidget::searchDocument(const QString &text, bool forward, bool matchCase, bool wholeWords)
{
    if (m_searchModel->searchString() != text) {
        m_searchModel->setSearchString(text);
    }
}

// ---- Page Navigation ----

void PdfViewerWidget::goToPage(int page)
{
    if (page >= 0 && page < m_document->pageCount()) {
        if (!m_navigatingHistory) {
            // Trim forward history when navigating to a new page
            if (m_historyIndex >= 0 && m_historyIndex < m_pageHistory.size() - 1) {
                m_pageHistory = m_pageHistory.mid(0, m_historyIndex + 1);
            }
            // Don't duplicate the same page
            if (m_pageHistory.isEmpty() || m_pageHistory.last() != page) {
                m_pageHistory.append(page);
                // Cap history at 100 entries
                if (m_pageHistory.size() > 100)
                    m_pageHistory.removeFirst();
            }
            m_historyIndex = m_pageHistory.size() - 1;
            emit navigationChanged(canGoBack(), canGoForward());
        }
        m_pageNavigator->jump(page, QPointF());
    }
}

void PdfViewerWidget::goBack()
{
    if (!canGoBack()) return;
    m_navigatingHistory = true;
    m_historyIndex--;
    m_pageNavigator->jump(m_pageHistory.at(m_historyIndex), QPointF());
    m_navigatingHistory = false;
    emit navigationChanged(canGoBack(), canGoForward());
}

void PdfViewerWidget::goForward()
{
    if (!canGoForward()) return;
    m_navigatingHistory = true;
    m_historyIndex++;
    m_pageNavigator->jump(m_pageHistory.at(m_historyIndex), QPointF());
    m_navigatingHistory = false;
    emit navigationChanged(canGoBack(), canGoForward());
}

bool PdfViewerWidget::canGoBack() const
{
    return m_historyIndex > 0;
}

bool PdfViewerWidget::canGoForward() const
{
    return m_historyIndex >= 0 && m_historyIndex < m_pageHistory.size() - 1;
}

int PdfViewerWidget::currentPage() const
{
    return m_pageNavigator->currentPage();
}

int PdfViewerWidget::pageCount() const
{
    return m_document->pageCount();
}

void PdfViewerWidget::onPageChanged()
{
    m_pageChangeTimer->start();
    if (m_twoPageMode) {
        updateTwoPageView();
    } else if (m_rotation != 0) {
        updateRotatedPageView();
    }
}

void PdfViewerWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (m_pdfView && m_annotationLayer) {
        m_pdfView->resize(size());
        m_annotationLayer->resize(size());
        if (m_twoPageScrollArea) {
            m_twoPageScrollArea->resize(size());
        }
    }
    if (m_rotation != 0 && !m_twoPageMode) {
        updateRotatedPageView();
    }
    repositionSignatureBadge();
}

// Wave 1A §9.7: keep the floating signature badge pinned to the top-right
// corner of the viewer regardless of viewport size.
void PdfViewerWidget::repositionSignatureBadge()
{
    if (!m_signatureBadge || !m_signatureBadge->isVisible()) return;
    m_signatureBadge->adjustSize();
    const int margin = 12;
    m_signatureBadge->move(width() - m_signatureBadge->width() - margin, margin);
    m_signatureBadge->raise();
}

void PdfViewerWidget::setSignatureValidityBadge(bool allValid, const QString &summary)
{
    if (!m_signatureBadge) return;
    if (summary.isEmpty()) {
        m_signatureBadge->hide();
        return;
    }
    m_signatureBadge->setText(allValid ? tr("SIGNATURES VALID") : tr("SIGNATURE ISSUES"));
    m_signatureBadge->setKind(allValid ? gp::Badge::Ok : gp::Badge::Err);
    m_signatureBadge->setToolTip(summary);
    m_signatureBadge->show();
    repositionSignatureBadge();
}

void PdfViewerWidget::setPageMode(QPdfView::PageMode mode)
{
    if (m_twoPageMode) setTwoPageMode(false);
    m_pdfView->setPageMode(mode);
}

void PdfViewerWidget::setTwoPageMode(bool enabled)
{
    m_twoPageMode = enabled;
    if (enabled) {
        m_pdfView->hide();
        // Wave 1C #3: previously the shared AnnotationLayer was unconditionally
        // hidden here -- annotations (and, since it also covered search-result
        // painting for the primary view, any sense of "what matched") silently
        // vanished the instant a user switched into two-page view, with no
        // warning. The per-page overlay layers (m_leftAnnotationLayer /
        // m_rightAnnotationLayer) now take over visibility for the two visible
        // pages, so this hide() is scoped correctly: only the single-page
        // overlay -- which is meaningless in a two-up layout -- goes away.
        m_annotationLayer->hide();
        if (m_rotatedPageLabel) m_rotatedPageLabel->hide();
        m_twoPageScrollArea->show();
        updateTwoPageView();
    } else {
        m_twoPageScrollArea->hide();
        m_pdfView->show();
        m_annotationLayer->show();
        if (m_rotation != 0) updateRotatedPageView();
    }
}

// Wave 1C #3: keeps one page's overlay AnnotationLayer showing exactly that
// page's annotations and current search-result highlights, in the same pixel
// coordinate space renderPage() just rendered `label`'s pixmap in (raw
// m_zoomFactor pixels-per-point -- the same convention the primary single-
// page AnnotationLayer already uses; like the primary view, annotation rects
// here are not vector-rescaled on zoom change, only re-rendered at the then-
// current zoom, which is why the two-page bitmap render below now uses
// m_zoomFactor directly instead of the previous unrelated 2x factor).
void PdfViewerWidget::syncPageOverlay(AnnotationLayer *overlay, QLabel *label, int page, qreal scale)
{
    if (!overlay || !label) return;
    overlay->resize(label->size());

    QList<AnnotationItem> pageItems;
    for (const auto &item : m_annotationLayer->annotations()) {
        if (item.pageIndex == page) pageItems.append(item);
    }
    overlay->setAnnotations(pageItems);

    QList<QRectF> highlights;
    if (m_searchModel) {
        const auto results = m_searchModel->resultsOnPage(page);
        for (const QPdfLink &link : results) {
            for (const QRectF &r : link.rectangles())
                highlights.append(QRectF(r.topLeft() * scale, r.size() * scale));
        }
    }
    overlay->setSearchHighlights(highlights);
}

void PdfViewerWidget::updateTwoPageView()
{
    if (!m_twoPageMode || !m_document || m_document->pageCount() == 0) return;

    int current = currentPage();
    int leftPage = (current % 2 == 0) ? current : current - 1;
    if (leftPage < 0) leftPage = 0;
    int rightPage = leftPage + 1;

    // Wave 1C #3: render at the real current zoom (m_zoomFactor), not the
    // previous hardcoded *2.0, so the bitmap's pixel space matches the
    // coordinate convention m_leftAnnotationLayer/m_rightAnnotationLayer (and
    // AnnotationItem rects generally) already use -- required for the
    // annotation overlays to land in the right place, not just be present.
    const qreal scale = m_zoomFactor;

    QImage leftImg = renderPage(leftPage, scale);
    if (!leftImg.isNull()) {
        m_leftPageLabel->setPixmap(QPixmap::fromImage(leftImg));
        m_leftPageLabel->resize(leftImg.size());
        m_leftPageLabel->show();
        syncPageOverlay(m_leftAnnotationLayer, m_leftPageLabel, leftPage, scale);
        m_leftAnnotationLayer->show();
        m_leftAnnotationLayer->raise();
    } else {
        m_leftPageLabel->hide();
        m_leftAnnotationLayer->hide();
    }

    if (rightPage < pageCount()) {
        QImage rightImg = renderPage(rightPage, scale);
        if (!rightImg.isNull()) {
            m_rightPageLabel->setPixmap(QPixmap::fromImage(rightImg));
            m_rightPageLabel->resize(rightImg.size());
            m_rightPageLabel->show();
            syncPageOverlay(m_rightAnnotationLayer, m_rightPageLabel, rightPage, scale);
            m_rightAnnotationLayer->show();
            m_rightAnnotationLayer->raise();
        } else {
            m_rightPageLabel->hide();
            m_rightAnnotationLayer->hide();
        }
    } else {
        m_rightPageLabel->hide();
        m_rightAnnotationLayer->hide();
    }
}

void PdfViewerWidget::toggleEyeCareMode()
{
    m_eyeCareMode = !m_eyeCareMode;
    if (m_eyeCareMode) {
        // Wave 2B #4: Eye Care and Night Mode are alternative reading filters;
        // only one content-level effect is meaningful at a time.
        if (m_nightMode) toggleNightMode();

        if (!m_eyeCareEffect) {
            m_eyeCareEffect = new QGraphicsColorizeEffect(this);
            m_eyeCareEffect->setColor(QColor(245, 222, 179)); // Warm Sepia
            m_eyeCareEffect->setStrength(0.5);
        }
        m_pdfView->setGraphicsEffect(m_eyeCareEffect);
        m_twoPageScrollArea->setGraphicsEffect(new QGraphicsColorizeEffect(this));
        static_cast<QGraphicsColorizeEffect*>(m_twoPageScrollArea->graphicsEffect())->setColor(QColor(245, 222, 179));
        static_cast<QGraphicsColorizeEffect*>(m_twoPageScrollArea->graphicsEffect())->setStrength(0.5);
    } else {
        m_pdfView->setGraphicsEffect(nullptr);
        m_twoPageScrollArea->setGraphicsEffect(nullptr);
    }
}

// Wave 2B #4: real content-level Night Mode -- full RGB inversion of the
// rendered page pixels via NightModeEffect (see top of file), distinct from:
//   - Dark Mode (ToolId::DarkMode -> MainWindow::toggleTheme()): chrome-only,
//     the page content itself stays untouched.
//   - Eye Care (toggleEyeCareMode() above): a warm sepia *tint* layered on
//     top of the still-white page -- the audit's literal complaint ("pages
//     stay glaring white while only chrome darkens") applies to Eye Care too,
//     since QGraphicsColorizeEffect blends a color, it does not invert.
// Applied to the same widgets Eye Care uses (m_pdfView / m_twoPageScrollArea)
// for scope parity with that sibling feature; the rotated-bitmap fallback
// view (m_rotatedPageLabel) picks it up too since it is parented under
// m_pdfView's container and QGraphicsEffect propagates to children when set
// on a shared ancestor -- but m_pdfView is hidden while that fallback is
// active, so the effect is applied directly to it as well for correctness.
void PdfViewerWidget::toggleNightMode()
{
    m_nightMode = !m_nightMode;
    if (m_nightMode) {
        if (m_eyeCareMode) toggleEyeCareMode();

        // A fresh effect instance per widget per toggle-on -- each
        // QWidget::setGraphicsEffect() call takes ownership of exactly one
        // effect object, so the same instance cannot be shared across widgets
        // (matches the existing Eye Care code's own pattern for
        // m_twoPageScrollArea, extended here to all three surfaces).
        m_pdfView->setGraphicsEffect(new NightModeEffect(this));
        m_twoPageScrollArea->setGraphicsEffect(new NightModeEffect(this));
        if (m_rotatedPageLabel) {
            m_rotatedPageLabel->setGraphicsEffect(new NightModeEffect(this));
        }
    } else {
        m_pdfView->setGraphicsEffect(nullptr);
        m_twoPageScrollArea->setGraphicsEffect(nullptr);
        if (m_rotatedPageLabel) m_rotatedPageLabel->setGraphicsEffect(nullptr);
    }
}

void PdfViewerWidget::mousePressEvent(QMouseEvent *event)
{
    if (m_toolMode == ToolMode::Crop && event->button() == Qt::LeftButton) {
        m_rubberBandOrigin = event->pos();
        if (!m_rubberBand) {
            m_rubberBand = new QRubberBand(QRubberBand::Rectangle, this);
        }
        m_rubberBand->setGeometry(QRect(m_rubberBandOrigin, QSize()));
        m_rubberBand->show();
        m_isSelectingCrop = true;
        event->accept();
    } else if (isFormBuilderMode(m_toolMode) && event->button() == Qt::LeftButton) {
        m_formRubberBandOrigin = event->pos();
        if (!m_formRubberBand) {
            m_formRubberBand = new QRubberBand(QRubberBand::Rectangle, this);
        }
        m_formRubberBand->setGeometry(QRect(m_formRubberBandOrigin, QSize()));
        m_formRubberBand->show();
        m_isPlacingField = true;
        event->accept();
    } else {
        QWidget::mousePressEvent(event);
    }
}

void PdfViewerWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_isSelectingCrop && m_rubberBand) {
        m_rubberBand->setGeometry(QRect(m_rubberBandOrigin, event->pos()).normalized());
        event->accept();
    } else if (m_isPlacingField && m_formRubberBand) {
        m_formRubberBand->setGeometry(QRect(m_formRubberBandOrigin, event->pos()).normalized());
        event->accept();
    } else {
        QWidget::mouseMoveEvent(event);
    }
}

void PdfViewerWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_isSelectingCrop && event->button() == Qt::LeftButton) {
        m_isSelectingCrop = false;
        if (m_rubberBand) {
            m_rubberBand->hide();
            QRect selection = m_rubberBand->geometry();
            if (selection.width() > 10 && selection.height() > 10) {
                // Determine page from pos
                // QPdfView handles the layout. We approximate or assume single page mode
                // For a robust implementation we would map from view to scene to page.
                // For now, we use the current page and pass the rect.
                int page = currentPage();

                // Map widget coordinates to PDF coordinates
                // Since this is a simple approximation:
                // We'll pass the unmapped rect and let the controller handle it or map it here.
                // Assuming scaling factor m_zoomFactor:
                QRectF pdfRect(selection.x() / m_zoomFactor,
                               selection.y() / m_zoomFactor,
                               selection.width() / m_zoomFactor,
                               selection.height() / m_zoomFactor);

                emit cropRequested(page, pdfRect);
            }
        }
        event->accept();
    } else if (m_isPlacingField && event->button() == Qt::LeftButton) {
        m_isPlacingField = false;
        if (m_formRubberBand) {
            m_formRubberBand->hide();
            QRect selection = m_formRubberBand->geometry();
            if (selection.width() > 10 && selection.height() > 10) {
                int page = currentPage();
                QRectF pdfRect(selection.x() / m_zoomFactor,
                               selection.y() / m_zoomFactor,
                               selection.width() / m_zoomFactor,
                               selection.height() / m_zoomFactor);
                emit fieldPlacementRequested(page, pdfRect, m_toolMode);
            }
        }
        event->accept();
    } else {
        QWidget::mouseReleaseEvent(event);
    }
}

// ---- Hyperlink click-navigation (Wave 1B #2) ----

// Maps a click/hover position in m_pdfView's viewport (widget pixels) to a
// QPdfLink via QPdfLinkModel, if any link covers that point. Uses the same
// current-page + m_zoomFactor coordinate convention already established
// elsewhere in this file for widget-to-page-point mapping (see the Crop and
// form-field-placement handlers in mouseReleaseEvent() above, which use the
// identical `pos / m_zoomFactor` approximation against the current page) --
// exact in single-page mode, best-effort in continuous/multi-page scroll
// where a click may land on a page other than the "current" one.
QPdfLink PdfViewerWidget::linkAtViewportPos(const QPoint &viewportPos) const
{
    if (!m_linkModel || !m_document || m_document->pageCount() == 0)
        return QPdfLink();

    const int page = currentPage();
    if (m_linkModel->page() != page)
        m_linkModel->setPage(page);

    const QPointF pagePoint(viewportPos.x() / m_zoomFactor, viewportPos.y() / m_zoomFactor);
    return m_linkModel->linkAt(pagePoint);
}

// Opens a URI link in the system browser, or jumps to an internal GoTo
// destination page (reusing the existing page-history-tracked goToPage()).
void PdfViewerWidget::activateLink(const QPdfLink &link)
{
    if (!link.isValid()) return;

    const QUrl url = link.url();
    if (!url.isEmpty() && url.isValid()) {
        QDesktopServices::openUrl(url);
        return;
    }
    if (link.page() >= 0) {
        goToPage(link.page());
    }
}

bool PdfViewerWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (m_pdfView && watched == m_pdfView->viewport() &&
        m_toolMode == ToolMode::HandTool && !m_twoPageMode) {
        if (event->type() == QEvent::MouseMove) {
            auto *me = static_cast<QMouseEvent *>(event);
            const bool onLink = linkAtViewportPos(me->pos()).isValid();
            if (onLink != m_hoveringLink) {
                m_hoveringLink = onLink;
                m_pdfView->viewport()->setCursor(onLink ? Qt::PointingHandCursor : Qt::OpenHandCursor);
            }
        } else if (event->type() == QEvent::MouseButtonRelease) {
            auto *me = static_cast<QMouseEvent *>(event);
            if (me->button() == Qt::LeftButton) {
                const QPdfLink link = linkAtViewportPos(me->pos());
                if (link.isValid()) {
                    activateLink(link);
                    return true; // consume: avoid a stray pan/selection side effect
                }
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

// ---- Export / Print ----

static qint64 pixmapSizeInBytes(const QPixmap &pixmap)
{
    if (pixmap.isNull()) return 0;
    return static_cast<qint64>(pixmap.width()) * pixmap.height() * pixmap.depth() / 8;
}

QImage PdfViewerWidget::renderPage(int page, qreal scaleFactor) const
{
    if (page < 0 || page >= m_document->pageCount())
        return QImage();

    // Check cache (Fix 5) -- match scale factor exactly. The cache is fully
    // cleared on every rotation change (see updateRotation()), so a scale-only
    // key remains correct -- entries never survive a rotation change to be
    // served stale under the same scale factor.
    if (m_pageCache.contains(page) && qFuzzyCompare(m_pageCache.value(page).scaleFactor, scaleFactor)) {
        m_cacheAccessCounter++;
        m_pageCache[page].lastAccessed = m_cacheAccessCounter;
        return m_pageCache.value(page).pixmap.toImage();
    }

    QSizeF pageSize = m_document->pagePointSize(page);

    // Wave 1B #1: apply the real view rotation to the actual rendered bitmap
    // instead of leaving the page upright (previously only AnnotationLayer's
    // overlay rotated -- audit §9.1's headline correctness bug). A 90/270
    // rotation swaps the effective output dimensions.
    QPdfDocumentRenderOptions opts;
    const bool swapped = (m_rotation == 90 || m_rotation == 270);
    switch (m_rotation) {
        case 90:  opts.setRotation(QPdfDocumentRenderOptions::Rotation::Clockwise90);  break;
        case 180: opts.setRotation(QPdfDocumentRenderOptions::Rotation::Clockwise180); break;
        case 270: opts.setRotation(QPdfDocumentRenderOptions::Rotation::Clockwise270); break;
        default:  break;
    }
    QSize imageSize = swapped
        ? QSize(pageSize.height() * scaleFactor, pageSize.width() * scaleFactor)
        : QSize(pageSize.width() * scaleFactor, pageSize.height() * scaleFactor);

    QImage result = m_document->render(page, imageSize, opts);

    // Store in cache. P9: keep a running byte total instead of re-summing the
    // whole cache on every insert. If this page already had an entry (e.g. cached
    // at a different scale), discount its bytes before inserting the replacement.
    m_cacheAccessCounter++;
    if (const auto old = m_pageCache.constFind(page); old != m_pageCache.constEnd()) {
        m_cacheTotalBytes -= old->bytes;
    }
    CachedPage item;
    item.pixmap = QPixmap::fromImage(result);
    item.scaleFactor = scaleFactor;
    item.lastAccessed = m_cacheAccessCounter;
    item.bytes = pixmapSizeInBytes(item.pixmap);
    m_cacheTotalBytes += item.bytes;
    m_pageCache.insert(page, item);

    // LRU eviction based on memory budget. The running total avoids the O(n)
    // re-sum per insert; each eviction is one linear scan for the LRU victim
    // (eviction is rare relative to inserts, and evicts only a handful of pages).
    while (m_cacheTotalBytes > MaxCacheBytes && !m_pageCache.isEmpty()) {
        int worstKey = -1;
        qint64 oldestAccess = std::numeric_limits<qint64>::max();
        for (auto it = m_pageCache.begin(); it != m_pageCache.end(); ++it) {
            if (it.value().lastAccessed < oldestAccess) {
                oldestAccess = it.value().lastAccessed;
                worstKey = it.key();
            }
        }
        if (worstKey >= 0) {
            m_cacheTotalBytes -= m_pageCache.value(worstKey).bytes;
            m_pageCache.remove(worstKey);
        } else {
            break;
        }
    }

    return result;
}

void PdfViewerWidget::clearPageCache()
{
    m_pageCache.clear();
    m_cacheTotalBytes = 0;
}

void PdfViewerWidget::extractPages(int from, int to, const QString &outputFile)
{
    if (from < 0 || to >= m_document->pageCount() || from > to) return;

    auto* progress = new QProgressDialog(tr("Extracting pages..."), QString(), 0, 0, window());
    progress->setWindowModality(Qt::WindowModal);
    progress->setMinimumDuration(0);
    progress->show();

    const QString inputPath = m_filePath;
    auto result = std::make_shared<std::atomic<bool>>(false);

    QThread* worker = QThread::create([inputPath, from, to, outputFile, result]() {
        result->store(gp::extractPages(inputPath, from, to, outputFile));
    });

    QPointer<PdfViewerWidget> self(this);
    connect(worker, &QThread::finished, this, [self, outputFile, progress, result]() {
        progress->close();
        progress->deleteLater();
        if (!self) return;
        bool ok = result->load();
        if (ok) {
            gp::MainWindow* mainWin = qobject_cast<gp::MainWindow*>(self->window());
            if (mainWin) {
                mainWin->statusBar()->showMessage(tr("Extracted pages to %1").arg(outputFile), 5000);
                if (QMessageBox::question(mainWin, tr("Open File"), tr("Operation completed. Would you like to open the extracted file?")) == QMessageBox::Yes) {
                    mainWin->openDocument(outputFile);
                }
            }
        } else {
            QMessageBox::critical(self->window(), tr("Error"), tr("Failed to extract pages."));
        }
        emit self->pageOperationFinished();
    });

    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

void PdfViewerWidget::deletePages(int from, int to, const QString &outputFile)
{
    if (from < 0 || to >= m_document->pageCount() || from > to) return;

    auto* progress = new QProgressDialog(tr("Deleting pages..."), QString(), 0, 0, window());
    progress->setWindowModality(Qt::WindowModal);
    progress->setMinimumDuration(0);
    progress->show();

    const QString inputPath = m_filePath;
    auto result = std::make_shared<std::atomic<bool>>(false);

    QThread* worker = QThread::create([inputPath, from, to, outputFile, result]() {
        result->store(gp::deletePages(inputPath, from, to, outputFile));
    });

    QPointer<PdfViewerWidget> self(this);
    connect(worker, &QThread::finished, this, [self, outputFile, progress, result]() {
        progress->close();
        progress->deleteLater();
        if (!self) return;
        bool ok = result->load();
        if (ok) {
            if (outputFile == self->m_filePath) {
                self->loadDocument(self->m_filePath);
            }
        } else {
            QMessageBox::critical(self->window(), tr("Error"), tr("Failed to delete pages."));
        }
        emit self->pageOperationFinished();
    });

    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

void PdfViewerWidget::insertBlankPage(int index, const QString &outputFile)
{
    auto* progress = new QProgressDialog(tr("Inserting blank page..."), QString(), 0, 0, window());
    progress->setWindowModality(Qt::WindowModal);
    progress->setMinimumDuration(0);
    progress->show();

    const QString inputPath = m_filePath;
    auto result = std::make_shared<std::atomic<bool>>(false);

    QThread* worker = QThread::create([inputPath, index, outputFile, result]() {
        result->store(gp::insertBlankPage(inputPath, index, outputFile));
    });

    QPointer<PdfViewerWidget> self(this);
    connect(worker, &QThread::finished, this, [self, outputFile, progress, result]() {
        progress->close();
        progress->deleteLater();
        if (!self) return;
        bool ok = result->load();
        if (ok) {
            if (outputFile == self->m_filePath) {
                self->loadDocument(self->m_filePath);
            }
        } else {
            QMessageBox::critical(self->window(), tr("Error"), tr("Failed to insert page."));
        }
        emit self->pageOperationFinished();
    });

    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

void PdfViewerWidget::rotatePages(int from, int to, int angle, const QString &outputFile)
{
    if (from < 0 || to >= m_document->pageCount() || from > to) return;

    auto* progress = new QProgressDialog(tr("Rotating pages..."), QString(), 0, 0, window());
    progress->setWindowModality(Qt::WindowModal);
    progress->setMinimumDuration(0);
    progress->show();

    const QString inputPath = m_filePath;
    auto result = std::make_shared<std::atomic<bool>>(false);

    QThread* worker = QThread::create([inputPath, from, to, angle, outputFile, result]() {
        result->store(gp::rotatePages(inputPath, from, to, angle, outputFile));
    });

    QPointer<PdfViewerWidget> self(this);
    connect(worker, &QThread::finished, this, [self, outputFile, progress, result]() {
        progress->close();
        progress->deleteLater();
        if (!self) return;
        bool ok = result->load();
        if (ok) {
            if (outputFile == self->m_filePath) {
                self->loadDocument(self->m_filePath);
            }
        } else {
            QMessageBox::critical(self->window(), tr("Error"), tr("Failed to rotate pages."));
        }
        emit self->pageOperationFinished();
    });

    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

bool PdfViewerWidget::saveDocumentAs(const QString &outputFile)
{
    if (m_filePath.isEmpty() || outputFile.isEmpty()) {
        qWarning() << "saveDocumentAs: empty source or destination path";
        return false;
    }

    QFileInfo srcInfo(m_filePath);
    QFileInfo dstInfo(outputFile);

    if (srcInfo.canonicalFilePath() == dstInfo.canonicalFilePath()) {
        // Saving to the same file: copy via secure temp then rename (Fix 2)
        QTemporaryFile tempFile(dstInfo.absolutePath() + QStringLiteral("/XXXXXX.pdf.tmp"));
        tempFile.setAutoRemove(false);
        if (!tempFile.open()) {
            qWarning() << "saveDocumentAs: failed to open temp file in" << dstInfo.absolutePath();
            return false;
        }
        QString tempPath = tempFile.fileName();
        tempFile.close();

        if (!QFile::copy(m_filePath, tempPath)) {
            qWarning() << "saveDocumentAs: QFile::copy to temp failed:" << m_filePath << "->" << tempPath;
            QFile::remove(tempPath);
            return false;
        }
        m_document->close();
        if (QFile::exists(outputFile) && !QFile::remove(outputFile)) {
            qWarning() << "saveDocumentAs: could not remove existing output before rename:" << outputFile;
            QFile::remove(tempPath);
            m_document->load(m_filePath);
            return false;
        }
        if (!QFile::rename(tempPath, outputFile)) {
            qWarning() << "saveDocumentAs: rename temp -> output failed:" << tempPath << "->" << outputFile;
            QFile::remove(tempPath);
            m_document->load(m_filePath);
            return false;
        }
        m_document->load(outputFile);
    } else {
        if (QFile::exists(outputFile) && !QFile::remove(outputFile)) {
            qWarning() << "saveDocumentAs: could not remove existing output:" << outputFile;
            return false;
        }
        if (!QFile::copy(m_filePath, outputFile)) {
            qWarning() << "saveDocumentAs: QFile::copy failed:" << m_filePath << "->" << outputFile;
            return false;
        }
    }

    // Copy the annotation sidecar file alongside the saved PDF
    QString srcAnn = m_filePath + QStringLiteral(".ann");
    QString dstAnn = outputFile + QStringLiteral(".ann");
    if (QFile::exists(srcAnn) && srcAnn != dstAnn) {
        if (QFile::exists(dstAnn) && !QFile::remove(dstAnn)) {
            qWarning() << "saveDocumentAs: could not remove existing annotation sidecar:" << dstAnn;
            // sidecar copy is best-effort — primary file is already written, so don't fail the save
        } else if (!QFile::copy(srcAnn, dstAnn)) {
            qWarning() << "saveDocumentAs: annotation sidecar copy failed:" << srcAnn << "->" << dstAnn;
            // sidecar copy is best-effort — do not propagate as failure
        }
    }

    return true;
}



bool PdfViewerWidget::mergeDocuments(const QStringList &files, const QString &outputFile)
{
    // Wave 1A §9.9: propagate the real result instead of only logging it --
    // ConvertController::mergePdfs() used to always show "Successfully merged"
    // regardless of what happened here.
    const bool ok = gp::mergeDocuments(files, outputFile);
    if (!ok)
        qWarning() << "mergeDocuments: engine failed on" << outputFile;
    return ok;
}

void PdfViewerWidget::printDocument()
{
    QPrinter *printer = new QPrinter(QPrinter::HighResolution);
    QPrintDialog dlg(printer, this);
    if (dlg.exec() != QDialog::Accepted) {
        delete printer;
        return;
    }

    int totalPages = m_document->pageCount();
    if (totalPages <= 0) {
        delete printer;
        return;
    }

    QProgressDialog *progress = new QProgressDialog(
        tr("Rendering pages for print..."), tr("Cancel"), 0, totalPages, this);
    progress->setWindowModality(Qt::WindowModal);
    progress->setMinimumDuration(0);
    progress->setValue(0);

    struct PrintState {
        QPrinter *printer = nullptr;
        QPainter *painter = nullptr;
        std::atomic<bool> canceled{false};
        int currentPage = 0;
        int totalPages = 0;
        std::function<void()> printNextPage;
    };
    auto state = std::make_shared<PrintState>();
    state->printer = printer;
    state->painter = new QPainter(printer);
    state->totalPages = totalPages;

    connect(progress, &QProgressDialog::canceled, this, [state]() {
        state->canceled.store(true);
    });

    QPointer<PdfViewerWidget> guard(this);
    QPdfDocument *doc = m_document;

    state->printNextPage = [guard, doc, progress, state]() {
        auto cleanup = [state, progress]() {
            if (state->painter) {
                state->painter->end();
                delete state->painter;
                state->painter = nullptr;
            }
            if (state->printer) {
                delete state->printer;
                state->printer = nullptr;
            }
            progress->close();
            progress->deleteLater();
            state->printNextPage = nullptr; // break circular reference
        };

        if (!guard || state->canceled.load()) {
            cleanup();
            return;
        }

        if (state->currentPage >= state->totalPages) {
            cleanup();
            return;
        }

        int pageIdx = state->currentPage;
        progress->setValue(pageIdx);

        // Spawn worker thread to render pageIdx
        QThread *worker = QThread::create([guard, doc, pageIdx, state, progress]() {
            QSizeF pageSize = doc->pagePointSize(pageIdx);
            QSize imageSize(pageSize.width() * 3.0, pageSize.height() * 3.0);
            QPdfDocumentRenderOptions opts;
            QImage pageImage = doc->render(pageIdx, imageSize, opts);

            // Once rendered, pass to GUI thread to paint, then print next page
            QMetaObject::invokeMethod(guard.data(), [guard, pageImage, state]() {
                if (!guard || state->canceled.load()) {
                    if (state->painter) {
                        state->painter->end();
                        delete state->painter;
                        state->painter = nullptr;
                    }
                    if (state->printer) {
                        delete state->printer;
                        state->printer = nullptr;
                    }
                    state->printNextPage = nullptr; // break circular reference
                    return;
                }

                // Paint the image
                if (state->currentPage > 0 && state->printer) {
                    state->printer->newPage();
                }

                if (state->painter) {
                    QRect target = state->painter->viewport();
                    QSize scaledSize = pageImage.size().scaled(target.size(), Qt::KeepAspectRatio);
                    QRect centered((target.width() - scaledSize.width()) / 2,
                                   (target.height() - scaledSize.height()) / 2,
                                   scaledSize.width(), scaledSize.height());
                    state->painter->drawImage(centered, pageImage);
                }

                // Advance page index
                state->currentPage++;

                // Trigger next page!
                if (state->printNextPage) {
                    state->printNextPage();
                }
            }, Qt::QueuedConnection);
        });

        connect(worker, &QThread::finished, worker, &QObject::deleteLater);
        worker->start();
    };

    // Start the printing sequence!
    state->printNextPage();
}

void PdfViewerWidget::setOcrResults(const QList<OcrResult> &results) { if (m_annotationLayer) m_annotationLayer->setOcrResults(results); }

// AR-7 D5: forward the overlay image to the AnnotationLayer, which has a
// real paintEvent and will draw it on top of all annotation content.
// The m_overlayImage member is kept for reference (e.g. unit tests that inspect state).
void PdfViewerWidget::setOverlayImage(const QImage &img) {
    m_overlayImage = img;
    if (m_annotationLayer) m_annotationLayer->setOverlayImage(img);
}
