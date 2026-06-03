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
#include <QScrollArea>
#include <QLabel>

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
    m_rightPageLabel = new QLabel();
    twoPageLayout->addWidget(m_leftPageLabel);
    twoPageLayout->addWidget(m_rightPageLabel);
    m_twoPageScrollArea->setWidget(twoPageWidget);
    m_twoPageScrollArea->setWidgetResizable(true);
    m_twoPageScrollArea->hide();

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
}

void PdfViewerWidget::zoomOut()
{
    m_zoomFactor /= 1.25;
    if (m_zoomFactor < 0.1) m_zoomFactor = 0.1;
    m_pdfView->setZoomFactor(m_zoomFactor);
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
    // If we can't rotate the view directly, we'll wait for a backend that can,
    // or use a more complex QGraphicsView setup. 
    // For now, let's at least rotate the annotations which we CAN control.
    m_annotationLayer->setRotation(m_rotation);
}

// ---- Tool Mode ----

void PdfViewerWidget::setToolMode(ToolMode mode)
{
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

void PdfViewerWidget::redactAllMatches(const QString &text, bool matchCase, bool wholeWords)
{
    if (text.isEmpty()) return;
    
    if (m_searchModel->searchString() != text) {
        m_searchModel->setSearchString(text);
        // In a real app, we'd wait for the search to complete.
        // For now, we'll process existing results or ask the user to search first.
    }

    QList<AnnotationItem> annos = m_annotationLayer->annotations();
    int count = m_searchModel->rowCount(QModelIndex());
    for (int i = 0; i < count; ++i) {
        // A-05: use the REAL matched-text geometry, not a fixed 80x18pt box. The
        // old constant box under-covered any match wider than 80pt (long emails,
        // IBANs, large fonts): the downstream rectangle-based excision then left
        // the tail glyphs in the content stream, recoverable. QPdfLink::rectangles()
        // gives the true per-match bounding rectangle(s) in PDF points.
        const QPdfLink link = m_searchModel->resultAtIndex(i);
        const int page = link.page();
        const QList<QRectF> rects = link.rectangles();
        if (rects.isEmpty()) {
            // Fallback: no geometry available — fall back to the point location with a
            // conservative box rather than dropping the match silently.
            const QModelIndex idx = m_searchModel->index(i, 0);
            const QPointF pos = m_searchModel->data(idx, static_cast<int>(QPdfSearchModel::Role::Location)).toPointF();
            AnnotationItem item;
            item.pageIndex = page;
            item.mode = ToolMode::Redact;
            item.color = Qt::black;
            item.thickness = 1;
            item.rect = QRectF(pos.x(), pos.y() - 15, 80, 18);
            annos.append(item);
            continue;
        }
        for (const QRectF &r : rects) {
            AnnotationItem item;
            item.pageIndex = page;
            item.mode = ToolMode::Redact;
            item.color = Qt::black;
            item.thickness = 1;
            item.rect = r;   // exact matched-text rectangle
            annos.append(item);
        }
    }
    m_annotationLayer->setAnnotations(annos);
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
        m_twoPageScrollArea->show();
        updateTwoPageView();
    } else {
        m_twoPageScrollArea->hide();
        m_pdfView->show();
        m_annotationLayer->show();
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
        m_leftPageLabel->setPixmap(QPixmap::fromImage(leftImg));
        m_leftPageLabel->show();
    } else {
        m_leftPageLabel->hide();
    }
    
    if (rightPage < pageCount()) {
        QImage rightImg = renderPage(rightPage, m_zoomFactor * 2.0);
        if (!rightImg.isNull()) {
            m_rightPageLabel->setPixmap(QPixmap::fromImage(rightImg));
            m_rightPageLabel->show();
        } else {
            m_rightPageLabel->hide();
        }
    } else {
        m_rightPageLabel->hide();
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

    // Check cache (Fix 5) -- match scale factor exactly
    if (m_pageCache.contains(page) && qFuzzyCompare(m_pageCache.value(page).scaleFactor, scaleFactor)) {
        m_cacheAccessCounter++;
        m_pageCache[page].lastAccessed = m_cacheAccessCounter;
        return m_pageCache.value(page).pixmap.toImage();
    }

    QSizeF pageSize = m_document->pagePointSize(page);
    QSize imageSize(pageSize.width() * scaleFactor, pageSize.height() * scaleFactor);

    QPdfDocumentRenderOptions opts;
    QImage result = m_document->render(page, imageSize, opts);

    // Store in cache
    m_cacheAccessCounter++;
    CachedPage item;
    item.pixmap = QPixmap::fromImage(result);
    item.scaleFactor = scaleFactor;
    item.lastAccessed = m_cacheAccessCounter;
    m_pageCache.insert(page, item);

    // LRU eviction based on memory budget
    qint64 totalBytes = 0;
    for (const auto &cached : m_pageCache) {
        totalBytes += pixmapSizeInBytes(cached.pixmap);
    }

    while (totalBytes > MaxCacheBytes && !m_pageCache.isEmpty()) {
        int worstKey = -1;
        qint64 oldestAccess = std::numeric_limits<qint64>::max();
        for (auto it = m_pageCache.begin(); it != m_pageCache.end(); ++it) {
            if (it.value().lastAccessed < oldestAccess) {
                oldestAccess = it.value().lastAccessed;
                worstKey = it.key();
            }
        }
        if (worstKey >= 0) {
            totalBytes -= pixmapSizeInBytes(m_pageCache.value(worstKey).pixmap);
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

void PdfViewerWidget::applyRedactions(const QString &outputFile)
{
    // D-02 fix: PoDoFo calls are routed through gp::applyRedactionsToFile so
    // this UI translation unit does not need to include <podofo/podofo.h>.

    QList<AnnotationItem> redactions;
    for (const auto& anno : m_annotationLayer->annotations()) {
        if (anno.mode == ToolMode::Redact)
            redactions.append(anno);
    }

    if (redactions.isEmpty()) {
        // Nothing to redact — just copy the source file.
        QFile::copy(m_filePath, outputFile);
        return;
    }

    // Convert screen-space annotation rects to PDF user-space coords.
    // QPdfDocument::pagePointSize() returns page dimensions in points (same unit
    // as PoDoFo's user space), with origin at top-left.  PoDoFo uses bottom-left
    // origin, so we flip: pdfY = pageHeight − screenY − rectHeight.
    QMap<int, QList<QRectF>> rectsByPage;
    int totalPages = m_document->pageCount();
    for (const auto& anno : redactions) {
        if (anno.pageIndex < 0 || anno.pageIndex >= totalPages) continue;
        QSizeF pageSize = m_document->pagePointSize(anno.pageIndex);
        double pageHeight = pageSize.height();
        QRectF pdfRect(anno.rect.x(),
                       pageHeight - anno.rect.y() - anno.rect.height(),
                       anno.rect.width(),
                       anno.rect.height());
        rectsByPage[anno.pageIndex].append(pdfRect);
    }

    if (!gp::applyRedactionsToFile(m_filePath, rectsByPage, outputFile))
        qWarning() << "applyRedactions: engine failed on" << outputFile;
    else
        qDebug() << "Redactions applied with content stream filtering to:" << outputFile;
}

void PdfViewerWidget::mergeDocuments(const QStringList &files, const QString &outputFile)
{
    if (!gp::mergeDocuments(files, outputFile))
        qWarning() << "mergeDocuments: engine failed on" << outputFile;
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

void PdfViewerWidget::setOverlayImage(const QImage &img) {
    m_overlayImage = img;
    update();
}

