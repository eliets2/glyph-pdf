// SPDX-License-Identifier: Apache-2.0
#include "ui/PdfViewerWidget.h"
#include "GpMainWindow.h"
#include "shell/StatusBar.h"
#include "core/AnnotationSerializer.h"
#include <QDebug>
#include <QDesktopServices>
#include <QUrl>
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
#include <QScrollArea>
#include <QLabel>
#include <QDesktopServices>
#include <QUrl>
#include <QScrollBar>
#include <QToolTip>
#include <QHelpEvent>
#include <QPainterPath>
// §9.1: link extraction goes through the engine seam (setLinkReader) — the UI
// must not include a concrete backend header (D-02 rule).

// ── §9.7 P0: on-page signature validity badges (view-layer only) ────────────
// The badge is a viewer-drawn overlay: ISO 32000-2 forbids validation status
// inside the field appearance and Acrobat's ribbon is viewer-drawn too, so
// nothing here is ever written into the PDF or the .ann sidecar.
namespace {
// 16px disc (design: 14–18px), centered on the field rect's top-right corner.
constexpr qreal kBadgeRadius = 8.0;

void gpDrawSignatureBadge(QPainter &p, const QPointF &center, SignatureBadgeState state, qreal radius)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    // White halo ring keeps the disc legible on any page background.
    QPen halo(Qt::white, qMax<qreal>(1.5, radius * 0.22));
    halo.setJoinStyle(Qt::RoundJoin);
    p.setPen(halo);
    p.setBrush(PdfViewerWidget::signatureBadgeColor(state));
    p.drawEllipse(center, radius, radius);

    const qreal g = radius * 0.45;   // glyph half-extent
    switch (state) {
    case SignatureBadgeState::ValidTrusted: {
        // Check mark (✓): two round-capped strokes.
        QPen pen(Qt::white, qMax<qreal>(1.4, radius * 0.22),
                 Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        QPainterPath check;
        check.moveTo(center.x() - g * 0.9, center.y() + g * 0.05);
        check.lineTo(center.x() - g * 0.15, center.y() + g * 0.8);
        check.lineTo(center.x() + g, center.y() - g * 0.7);
        p.drawPath(check);
        break;
    }
    case SignatureBadgeState::ModifiedAfterSigning: {
        // X (✕): two round-capped strokes.
        QPen pen(Qt::white, qMax<qreal>(1.4, radius * 0.22),
                 Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawLine(QPointF(center.x() - g, center.y() - g), QPointF(center.x() + g, center.y() + g));
        p.drawLine(QPointF(center.x() + g, center.y() - g), QPointF(center.x() - g, center.y() + g));
        break;
    }
    case SignatureBadgeState::UntrustedChain:
    case SignatureBadgeState::Unknown: {
        // "?" glyph (Acrobat-style unknown/untrusted marker), white on the disc.
        QFont f = p.font();
        f.setBold(true);
        f.setPixelSize(qMax(9, qRound(radius * 1.4)));
        p.setFont(f);
        p.setPen(Qt::white);
        p.drawText(QRectF(center.x() - radius, center.y() - radius, radius * 2.0, radius * 2.0),
                   Qt::AlignCenter, QStringLiteral("?"));
        break;
    }
    }
    p.restore();
}
} // namespace

// The mouse-transparent child widget that paints the badges over the PDF view
// in single-page mode (two-page mode composites them via paintTwoPageOverlays).
class SignatureBadgeOverlay : public QWidget {
public:
    explicit SignatureBadgeOverlay(PdfViewerWidget *viewer)
        : QWidget(viewer), m_viewer(viewer)
    {
        setObjectName(QStringLiteral("signatureBadgeOverlay"));
        // Clicks, hover and drags must fall through to the PDF view and the
        // annotation layer beneath — the badge layer is paint-only.
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setFocusPolicy(Qt::NoFocus);
    }

protected:
    void paintEvent(QPaintEvent *) override {
        if (!m_viewer || !m_viewer->m_pdfView || !m_viewer->m_document)
            return;
        // Two-page mode paints badges into the page pixmaps instead; this
        // overlay is hidden there (grabbing a hidden overlay must stay empty).
        if (m_viewer->m_twoPageMode)
            return;
        QPdfView *view = m_viewer->m_pdfView;
        const QRect vp = view->viewport()->geometry();
        const int page = m_viewer->m_pageNavigator ? m_viewer->m_pageNavigator->currentPage() : -1;
        const qreal zoom = qMax<qreal>(m_viewer->m_zoomFactor, 0.01);

        QPainter p(this);
        p.translate(vp.topLeft());
        p.setClipRect(vp);
        for (const SignatureBadgeSpec &spec : m_viewer->m_badges) {
            if (spec.pageIndex != page || !spec.fieldRect.isValid())
                continue;   // not anchored to the visible page → nothing to draw
            const QPointF center = m_viewer->badgeViewportCenter(spec, vp.size(), zoom);
            gpDrawSignatureBadge(p, center, spec.state, kBadgeRadius);
        }
    }

private:
    PdfViewerWidget *m_viewer = nullptr;
};

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
    // §9.1 P0: overlay content changed while two-page mode is active (e.g. a
    // controller pushed new annotations via setAnnotations) — refresh the
    // composite instead of showing stale marks.
    connect(m_annotationLayer, &AnnotationLayer::annotationsChanged, this, [this]() {
        if (m_twoPageMode) updateTwoPageView();
    });
    // §9.1 P0: search results arrive asynchronously; keep the two-page spread
    // in sync with the same live QPdfSearchModel the single-page view paints.
    connect(m_searchModel, &QPdfSearchModel::countChanged, this, [this]() {
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

    // Setup TwoPage view
    m_twoPageScrollArea = new QScrollArea(container);
    m_twoPageScrollArea->setAlignment(Qt::AlignCenter);
    QWidget *twoPageWidget = new QWidget();
    QHBoxLayout *twoPageLayout = new QHBoxLayout(twoPageWidget);
    m_leftPageLabel = new QLabel();
    m_leftPageLabel->setObjectName("twoPageLeftLabel");
    m_rightPageLabel = new QLabel();
    m_rightPageLabel->setObjectName("twoPageRightLabel");
    twoPageLayout->addWidget(m_leftPageLabel);
    twoPageLayout->addWidget(m_rightPageLabel);
    m_twoPageScrollArea->setWidget(twoPageWidget);
    m_twoPageScrollArea->setWidgetResizable(true);
    m_twoPageScrollArea->hide();

    // §9.7 P0: badge overlay — stacked above the PDF view and the annotation
    // layer, mouse-transparent so every interaction keeps working unchanged.
    m_badgeOverlay = new SignatureBadgeOverlay(this);
    m_badgeOverlay->setParent(container);
    m_annotationLayer->raise();
    m_badgeOverlay->raise();
    // Scroll/zoom/content-size changes move the page under the badge anchor —
    // repaint so badges stay pinned to the field rect's corner. rangeChanged
    // also catches zoom-mode presets (FitToWidth/FitInView), which change the
    // zoom factor without going through zoomIn/zoomOut/setZoomLevel.
    connect(m_pdfView->horizontalScrollBar(), &QScrollBar::valueChanged,
            m_badgeOverlay, qOverload<>(&QWidget::update));
    connect(m_pdfView->verticalScrollBar(), &QScrollBar::valueChanged,
            m_badgeOverlay, qOverload<>(&QWidget::update));
    connect(m_pdfView->horizontalScrollBar(), &QScrollBar::rangeChanged,
            m_badgeOverlay, qOverload<>(&QWidget::update));
    connect(m_pdfView->verticalScrollBar(), &QScrollBar::rangeChanged,
            m_badgeOverlay, qOverload<>(&QWidget::update));

    // We'll manage sizes manually in resizeEvent for true overlap
    layout->addWidget(container);

    // §9.1 P0: clickable hyperlinks — intercept clicks on the view's viewport.
    m_pdfView->viewport()->installEventFilter(this);
    connect(m_pageNavigator, &QPdfPageNavigator::currentPageChanged,
            this, [this](int) { refreshPageLinks(); });
}

PdfViewerWidget::~PdfViewerWidget()
{
    // §9.1: sever the page-change connection BEFORE member destruction begins.
    // The constructor's currentPageChanged lambda touches members (link cache,
    // document) that are already gone once ~QWidget tears down children, so a
    // page jump followed by destruction would invoke it on a half-destroyed
    // object ("Called object is not of the correct type").
    disconnect(m_pageNavigator, &QPdfPageNavigator::currentPageChanged, this, nullptr);
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
    m_linksForPage = -1;   // §9.1: document changed → link cache is stale
    m_document->load(fileName);
    if (isLoaded()) loadAnnotations();
    refreshPageLinks(); // §9.1 P0: prime link cache for the opening page
    return isLoaded();
}

void PdfViewerWidget::reload()
{
    if (!m_filePath.isEmpty()) {
        // §9.1 P0: after an engine-side page mutation the document itself now
        // carries the orientation (/Rotate), so the viewer's own overlay
        // rotation must reset to zero — otherwise snapshots would double-rotate.
        m_rotation = 0;
        m_annotationLayer->setRotation(0);
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
    if (m_badgeOverlay) m_badgeOverlay->update();   // §9.7 P0: badges follow zoom
}

void PdfViewerWidget::zoomOut()
{
    m_zoomFactor /= 1.25;
    if (m_zoomFactor < 0.1) m_zoomFactor = 0.1;
    m_pdfView->setZoomFactor(m_zoomFactor);
    if (m_badgeOverlay) m_badgeOverlay->update();   // §9.7 P0: badges follow zoom
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
    if (m_badgeOverlay) m_badgeOverlay->update();   // §9.7 P0: badges follow zoom
}

qreal PdfViewerWidget::zoomLevel() const
{
    return m_zoomFactor;
}

void PdfViewerWidget::rotateClockwise()
{
    m_rotation = (m_rotation + 90) % 360;
    // §9.1 P0: QPdfView (this Qt build) exposes no render-options hook, so a
    // view-local bitmap rotation is not achievable here. Instead of silently
    // rotating only the invisible-until-annotated overlay, request the real
    // engine-side page rotation (writes /Rotate + reload) — connected in
    // GpMainWindow to PagesController.
    emit requestPageRotation(90);
    m_annotationLayer->setRotation(m_rotation);
}

void PdfViewerWidget::rotateCounterClockwise()
{
    m_rotation = (m_rotation + 270) % 360;
    emit requestPageRotation(-90);
    m_annotationLayer->setRotation(m_rotation);
}

void PdfViewerWidget::updateRotation()
{
    m_annotationLayer->setRotation(m_rotation);
}

// ── §9.1 P0: clickable hyperlinks (URI + internal GoTo) ────────────────────
void PdfViewerWidget::setLinkReader(
        std::function<QList<PdfLinkInfo>(const QString &path, int page)> reader)
{
    m_linkReader = std::move(reader);
    m_linksForPage = -1;   // reader changed → refetch on next refresh
    refreshPageLinks();
}

void PdfViewerWidget::refreshPageLinks(int page)
{
    if (page < 0)
        page = m_pageNavigator ? m_pageNavigator->currentPage() : -1;
    if (page >= 0 && page == m_linksForPage)
        return;   // cache hit — links already fetched for this page
    m_pageLinks.clear();
    m_linksForPage = -1;
    if (m_filePath.isEmpty() || !isLoaded() || page < 0 || !m_linkReader) return;
    // Engine-side PoDoFo parse of the on-disk file, injected via setLinkReader
    // so the UI layer stays free of concrete-backend includes (D-02 rule).
    m_pageLinks = m_linkReader(m_filePath, page);
    m_linksForPage = page;
}

bool PdfViewerWidget::isSafeLinkScheme(const QString &uri)
{
    const QUrl url(uri, QUrl::TolerantMode);
    const QString scheme = url.scheme().toLower();
    return scheme == QStringLiteral("http")
        || scheme == QStringLiteral("https")
        || scheme == QStringLiteral("mailto");
}

bool PdfViewerWidget::handleLinkClick(const QPoint &viewportPos)
{
    if (m_pageLinks.isEmpty()) return false;
    if (!m_document || !isLoaded()) return false;

    // Map viewport position → PDF user-space points for the current page.
    // Single-page/fit layouts: the page is centered; use zoom + scroll offsets.
    // laziness: this mapping assumes a single centered page. In MultiPage mode
    // the page is laid out in a flow, so the centering math is approximate.
    // upgrade path: use QPdfLinkModel (Qt 6.4+) for authoritative per-page
    // link hit-testing across all page modes, or compute the page's actual
    // viewport rect from QPdfView's layout.
    const QSizeF pageSize = m_document->pagePointSize(m_pageNavigator->currentPage());
    const qreal zoom = m_zoomFactor > 0.0 ? m_zoomFactor : 1.0;
    const qreal viewW = m_pdfView->viewport()->width();
    const qreal viewH = m_pdfView->viewport()->height();
    qreal pageW = pageSize.width() * zoom;
    qreal pageH = pageSize.height() * zoom;
    if (pageW <= 0 || pageH <= 0) return false;
    const qreal originX = qMax<qreal>(0, (viewW - pageW) / 2.0) - m_pdfView->horizontalScrollBar()->value();
    const qreal originY = qMax<qreal>(0, (viewH - pageH) / 2.0) - m_pdfView->verticalScrollBar()->value();
    QPointF pdfPos((viewportPos.x() - originX) / zoom,
                   (viewportPos.y() - originY) / zoom);
    // Flip to bottom-left-origin PDF space.
    pdfPos.setY(pageSize.height() - pdfPos.y());

    for (const auto& link : m_pageLinks) {
        if (link.rect.contains(pdfPos)) {
            if (link.isUri && !link.uri.isEmpty()) {
                // §9.1 P0 DEFECT 2(A): never open an arbitrary scheme from a
                // PDF. Only http/https/mailto are safe to hand to the OS.
                if (isSafeLinkScheme(link.uri))
                    QDesktopServices::openUrl(QUrl(link.uri));
                return true; // consumed either way — unsafe schemes are blocked
            }
            if (link.targetPage >= 0) {
                goToPage(link.targetPage);
                return true;
            }
        }
    }
    return false;
}

bool PdfViewerWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_pdfView->viewport() && event->type() == QEvent::MouseButtonRelease) {
        auto* me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton && m_toolMode == ToolMode::HandTool) {
            if (handleLinkClick(me->pos()))
                return true; // consumed — do not treat as canvas click
        }
    }
    // §9.7 P0: badge tooltips. The overlay is mouse-transparent, so the ToolTip
    // event arrives at the viewport; hit-test the badge layer here. (In
    // annotation-editing modes the annotation layer is no longer
    // mouse-transparent and keeps precedence — badges are a viewing feature.)
    if (watched == m_pdfView->viewport() && event->type() == QEvent::ToolTip) {
        auto *he = static_cast<QHelpEvent *>(event);
        const QString tip = signatureBadgeTooltipAt(he->pos());
        if (!tip.isEmpty()) {
            QToolTip::showText(he->globalPos(), tip, m_pdfView->viewport());
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
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
        case ToolMode::Erase:
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
        case ToolMode::AddSignatureTyped:   // §9.7 P0: placement crosshair
        case ToolMode::AddSignatureUpload:
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

void PdfViewerWidget::setPendingSignatureImage(const QImage &img)
{
    // §9.7 P0: must come AFTER setToolMode() armed the signature placement
    // mode — AnnotationLayer::setMode() discards the pending image for any
    // non-signature tool.
    if (m_annotationLayer)
        m_annotationLayer->setPendingSignatureImage(img);
}

// ---- Search ----

void PdfViewerWidget::searchDocument(const QString &text, bool forward, bool matchCase, bool wholeWords)
{
    // §9.15 P0: QPdfSearchModel only supports case-insensitive substring
    // search. Match Case / Whole Words / Regex are handled by EditController
    // via a page-text scan (see onSearchRequested); forward is implicit in
    // the model iteration there.
    if (m_searchModel->searchString() != text) {
        m_searchModel->setSearchString(text);
    }
    Q_UNUSED(forward);
    Q_UNUSED(matchCase);
    Q_UNUSED(wholeWords);
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
        // §9.1: prime the link cache for the target page now — the navigator's
        // currentPageChanged may be deferred, and clicks must hit fresh links.
        refreshPageLinks(page);
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
    if (m_badgeOverlay) m_badgeOverlay->update();   // §9.7 P0: badges follow the visible page
    if (m_twoPageMode) {
        updateTwoPageView();
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
    // §9.7 P0: keep the badge overlay covering the PDF view.
    syncBadgeOverlayGeometry();
}

void PdfViewerWidget::syncBadgeOverlayGeometry()
{
    if (m_badgeOverlay && m_pdfView)
        m_badgeOverlay->setGeometry(m_pdfView->geometry());
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
        m_annotationLayer->hide();
        m_badgeOverlay->hide();   // §9.7 P0: two-page mode paints badges into the pixmaps
        m_twoPageScrollArea->show();
        updateTwoPageView();
    } else {
        m_twoPageScrollArea->hide();
        m_pdfView->show();
        m_annotationLayer->show();
        // §9.7 P0: restore the badge overlay over the (possibly resized) view.
        syncBadgeOverlayGeometry();
        m_badgeOverlay->show();
        m_badgeOverlay->update();
    }
}

void PdfViewerWidget::updateTwoPageView()
{
    if (!m_twoPageMode || !m_document || m_document->pageCount() == 0) return;
    
    int current = currentPage();
    int leftPage = (current % 2 == 0) ? current : current - 1;
    if (leftPage < 0) leftPage = 0;
    int rightPage = leftPage + 1;
    
    QImage leftImg = renderPage(leftPage, m_zoomFactor * 2.0);
    if (!leftImg.isNull()) {
        // §9.1 P0: two-page mode must not silently hide annotations and search
        // highlights — composite the page's overlay content before displaying.
        paintTwoPageOverlays(&leftImg, leftPage, m_zoomFactor * 2.0);
        m_leftPageLabel->setPixmap(QPixmap::fromImage(leftImg));
        m_leftPageLabel->show();
    } else {
        m_leftPageLabel->hide();
    }

    if (rightPage < pageCount()) {
        QImage rightImg = renderPage(rightPage, m_zoomFactor * 2.0);
        if (!rightImg.isNull()) {
            paintTwoPageOverlays(&rightImg, rightPage, m_zoomFactor * 2.0);
            m_rightPageLabel->setPixmap(QPixmap::fromImage(rightImg));
            m_rightPageLabel->show();
        } else {
            m_rightPageLabel->hide();
        }
    } else {
        m_rightPageLabel->hide();
    }
}

// ── §9.1 P0: two-page mode must not silently hide overlays ─────────────────
// setTwoPageMode() hides m_pdfView (which paints search highlights) and
// m_annotationLayer (which paints the user's annotations), so everything the
// user placed used to vanish without warning on switching to two-page mode —
// a trust/correctness gap (audit 2026-07-01), not a cosmetic one.
//
// Minimum honest fix (Option A): composite the SAME models the single-page
// overlay path consumes onto each page's pixmap —
//   * AnnotationLayer's AnnotationItem list, filtered per page and painted via
//     AnnotationLayer::paintShape so both paths share one painter, and
//   * the live QPdfSearchModel results (QPdfLink::rectangles()).
// This paints only; it never stores a second copy of annotation state, and
// entering/leaving two-page mode cannot drop or duplicate items.
//
// Coordinates: AnnotationItem::rect is stored in view space at the draw-time
// zoom (the app-wide convention — see the crop mapping in mouseReleaseEvent:
// page points = viewRect / m_zoomFactor). The two-page pixmaps are rendered at
// renderScale = m_zoomFactor * 2, so the net view→pixmap factor is
// renderScale / m_zoomFactor. QPdfLink::rectangles() are page points with the
// origin at the page's TOP-LEFT (verified against PDFium text extraction for a
// glyph drawn at PDF y=700 on an 842pt page → reported y≈133), so they map
// directly by renderScale.
//
// Known boundary (same limitation as the single-page overlay): rects are kept
// in view space, so marks do not re-anchor if the zoom changes after drawing;
// here the overlay and the page pixmap are always rendered from the same
// m_zoomFactor snapshot, so they stay aligned with each other.
void PdfViewerWidget::paintTwoPageOverlays(QImage *pageImg, int pageIndex, qreal renderScale) const
{
    if (!pageImg || pageImg->isNull() || pageIndex < 0 || renderScale <= 0)
        return;
    // Never paint into the shared render-cache buffer.
    pageImg->detach();

    QPainter painter(pageImg);
    painter.setRenderHint(QPainter::Antialiasing);

    // Search highlights first: on-screen (single-page) QPdfView paints them
    // under m_annotationLayer, so the composite keeps the same stacking.
    if (m_searchModel) {
        QColor fill = palette().color(QPalette::Highlight);
        fill.setAlpha(127);
        painter.setPen(Qt::NoPen);
        painter.setBrush(fill);
        const QList<QPdfLink> results = m_searchModel->resultsOnPage(pageIndex);
        for (const auto &result : results) {
            for (const QRectF &r : result.rectangles()) {
                if (r.isValid() && r.width() > 0 && r.height() > 0)
                    painter.drawRect(QRectF(r.x() * renderScale, r.y() * renderScale,
                                            r.width() * renderScale, r.height() * renderScale));
            }
        }
    }

    const qreal viewToPixmap = renderScale / qMax<qreal>(m_zoomFactor, 0.01);
    painter.save();
    painter.scale(viewToPixmap, viewToPixmap);
    const QList<AnnotationItem> items = m_annotationLayer->annotations();
    for (const auto &anno : items) {
        if (anno.pageIndex != pageIndex)
            continue;
        AnnotationLayer::paintShape(painter, anno);
    }
    painter.restore();

    // §9.7 P0: signature badges — page points with the TOP-LEFT origin scale
    // directly by renderScale (the same convention as the search rectangles
    // above). The disc is scaled by viewToPixmap so it reads at the same
    // on-screen size as the single-page overlay.
    for (const SignatureBadgeSpec &spec : m_badges) {
        if (spec.pageIndex != pageIndex || !spec.fieldRect.isValid())
            continue;
        const QPointF center(spec.fieldRect.right() * renderScale,
                             spec.fieldRect.top() * renderScale);
        gpDrawSignatureBadge(painter, center, spec.state, kBadgeRadius * viewToPixmap);
    }
}

void PdfViewerWidget::toggleEyeCareMode()
{
    m_eyeCareMode = !m_eyeCareMode;
    if (m_eyeCareMode) {
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

    // Check cache (Fix 5) -- match scale factor AND rotation exactly
    if (m_pageCache.contains(page)
        && qFuzzyCompare(m_pageCache.value(page).scaleFactor, scaleFactor)
        && m_pageCache.value(page).rotation == m_rotation) {
        m_cacheAccessCounter++;
        m_pageCache[page].lastAccessed = m_cacheAccessCounter;
        return m_pageCache.value(page).pixmap.toImage();
    }

    QSizeF pageSize = m_document->pagePointSize(page);
    QSize imageSize(pageSize.width() * scaleFactor, pageSize.height() * scaleFactor);

    // §9.1 P0: no render-options rotation here — orientation lives in the
    // document /Rotate after an engine-side rotate + reload, and PDFium
    // applies it during render. Adding opts.setRotation on top would
    // double-rotate every snapshot.
    QPdfDocumentRenderOptions opts;
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
    item.rotation = m_rotation;
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
    if (!gp::mergeDocuments(files, outputFile)) {
        qWarning() << "mergeDocuments: engine failed on" << outputFile;
        return false;
    }
    return true;
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
void PdfViewerWidget::setOverlayImage(const QImage &img) {
    if (m_annotationLayer) m_annotationLayer->setOverlayImage(img);
}

// ── §9.7 P0: on-page signature validity badges (view-layer only) ────────────

void PdfViewerWidget::setSignatureBadges(const QList<SignatureBadgeSpec> &badges)
{
    m_badges = badges;
    if (m_badgeOverlay)
        m_badgeOverlay->update();
    // Two-page mode composites badges into the page pixmaps, so refresh those.
    if (m_twoPageMode)
        updateTwoPageView();
}

QList<SignatureBadgeSpec> PdfViewerWidget::signatureBadges() const
{
    return m_badges;
}

QColor PdfViewerWidget::signatureBadgeColor(SignatureBadgeState state)
{
    switch (state) {
    case SignatureBadgeState::ValidTrusted:
        return QColor(0x2e, 0x9e, 0x44);   // green — trusted signature
    case SignatureBadgeState::UntrustedChain:
        return QColor(0xf2, 0xa3, 0x3c);   // amber — integrity ok, untrusted chain
    case SignatureBadgeState::ModifiedAfterSigning:
        return QColor(0xd9, 0x30, 0x25);   // red — modified after signing
    case SignatureBadgeState::Unknown:
        break;
    }
    return QColor(0x8a, 0x8d, 0x91);       // gray — unknown / not validated
}

QPointF PdfViewerWidget::badgeViewportCenter(const SignatureBadgeSpec &spec,
                                             const QSize &vpSize, qreal zoom) const
{
    // Same viewport mapping as handleLinkClick: the page is centered in the
    // viewport and offset by the scrollbar values; fieldRect is in top-left
    // origin page space, so the anchor is the field rect's top-right corner.
    // Known limitation (shared with link clicks): the centering math is
    // approximate in MultiPage flow layouts.
    const QSizeF pageSize = m_document->pagePointSize(spec.pageIndex);
    const qreal originX = qMax<qreal>(0, (vpSize.width() - pageSize.width() * zoom) / 2.0)
                        - m_pdfView->horizontalScrollBar()->value();
    const qreal originY = qMax<qreal>(0, (vpSize.height() - pageSize.height() * zoom) / 2.0)
                        - m_pdfView->verticalScrollBar()->value();
    return QPointF(originX + spec.fieldRect.right() * zoom,
                   originY + spec.fieldRect.top() * zoom);
}

QString PdfViewerWidget::signatureBadgeTooltipAt(const QPoint &viewportPos) const
{
    if (!m_pdfView || !m_document || !isLoaded() || m_twoPageMode)
        return QString();
    const int page = m_pageNavigator ? m_pageNavigator->currentPage() : -1;
    if (page < 0 || m_badges.isEmpty())
        return QString();
    const qreal zoom = qMax<qreal>(m_zoomFactor, 0.01);
    const QSize vpSize = m_pdfView->viewport()->size();
    for (const SignatureBadgeSpec &spec : m_badges) {
        if (spec.pageIndex != page || !spec.fieldRect.isValid() || spec.tooltip.isEmpty())
            continue;
        const QPointF center = badgeViewportCenter(spec, vpSize, zoom);
        if (QLineF(viewportPos, center).length() <= kBadgeRadius + 3.0)
            return spec.tooltip;
    }
    return QString();
}

