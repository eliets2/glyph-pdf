#include "ui/PdfViewerWidget.h"
#include "GpMainWindow.h"
#include "shell/StatusBar.h"
#include "core/AnnotationSerializer.h"
#include <QMessageBox>
#include <QPdfDocument>
#include <QPdfView>
#include <QPdfSearchModel>
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
#include <podofo/podofo.h>
#include <sstream>
#include <vector>
#include <QMap>

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
            m_pdfView->setCursor(Qt::CrossCursor);
            break;
        default:
            m_pdfView->setCursor(Qt::ArrowCursor);
            break;
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
        QModelIndex idx = m_searchModel->index(i, 0);
        int page = m_searchModel->data(idx, static_cast<int>(QPdfSearchModel::Role::Page)).toInt();
        QPointF pos = m_searchModel->data(idx, static_cast<int>(QPdfSearchModel::Role::Location)).toPointF();

        AnnotationItem item;
        item.pageIndex = page;
        item.mode = ToolMode::Redact;
        item.color = Qt::black;
        item.thickness = 1;
        // Note: QPdfSearchModel location is bottom-left of the text.
        // We estimate height. In a production build, we'd use character box data.
        item.rect = QRectF(pos.x(), pos.y() - 15, 80, 18);
        annos.append(item);
    }
    m_annotationLayer->setAnnotations(annos);
}

// ---- Page Navigation ----

void PdfViewerWidget::goToPage(int page)
{
    if (page >= 0 && page < m_document->pageCount()) {
        m_pageNavigator->jump(page, QPointF());
    }
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
}

void PdfViewerWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (m_pdfView && m_annotationLayer) {
        m_pdfView->resize(size());
        m_annotationLayer->resize(size());
    }
}

void PdfViewerWidget::setPageMode(QPdfView::PageMode mode)
{
    m_pdfView->setPageMode(mode);
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
        try {
            PoDoFo::PdfMemDocument sourceDoc;
            sourceDoc.Load(inputPath.toUtf8().constData());
            
            PoDoFo::PdfMemDocument destDoc;
            destDoc.GetPages().AppendDocumentPages(sourceDoc, from, to - from + 1);
            destDoc.Save(outputFile.toUtf8().constData());
            result->store(true);
        } catch (const std::exception& e) {
            qWarning() << "PoDoFo error extracting pages:" << e.what();
            result->store(false);
        }
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
        try {
            PoDoFo::PdfMemDocument doc;
            doc.Load(inputPath.toUtf8().constData());
            
            for (int i = to; i >= from; --i) {
                doc.GetPages().RemovePageAt(i);
            }
            
            doc.Save(outputFile.toUtf8().constData());
            result->store(true);
        } catch (const std::exception& e) {
            qWarning() << "PoDoFo error deleting pages:" << e.what();
            result->store(false);
        }
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
        try {
            PoDoFo::PdfMemDocument sourceDoc;
            sourceDoc.Load(inputPath.toUtf8().constData());
            
            PoDoFo::PdfMemDocument destDoc;
            
            // Copy pages before the insertion index
            if (index > 0) {
                destDoc.GetPages().AppendDocumentPages(sourceDoc, 0, index);
            }
            
            // Create a blank page at the insertion index
            PoDoFo::Rect mediaBox = PoDoFo::Rect(0, 0, 595.276, 841.890); // default A4
            if (sourceDoc.GetPages().GetCount() > 0) {
                mediaBox = sourceDoc.GetPages().GetPageAt(0).GetMediaBox();
            }
            destDoc.GetPages().CreatePage(mediaBox);
            
            // Copy remaining pages after
            if (index < sourceDoc.GetPages().GetCount()) {
                destDoc.GetPages().AppendDocumentPages(sourceDoc, index, sourceDoc.GetPages().GetCount() - index);
            }
            
            destDoc.Save(outputFile.toUtf8().constData());
            result->store(true);
        } catch (const std::exception& e) {
            qWarning() << "PoDoFo error inserting blank page:" << e.what();
            result->store(false);
        }
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
        try {
            PoDoFo::PdfMemDocument doc;
            doc.Load(inputPath.toUtf8().constData());
            
            for (int i = from; i <= to; ++i) {
                PoDoFo::PdfPage& page = doc.GetPages().GetPageAt(i);
                int currentRot = page.GetRotation();
                page.SetRotation((currentRot + angle) % 360);
            }
            
            doc.Save(outputFile.toUtf8().constData());
            result->store(true);
        } catch (const std::exception& e) {
            qWarning() << "PoDoFo error rotating pages:" << e.what();
            result->store(false);
        }
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

void PdfViewerWidget::saveDocumentAs(const QString &outputFile)
{
    if (m_filePath.isEmpty() || outputFile.isEmpty()) return;

    QFileInfo srcInfo(m_filePath);
    QFileInfo dstInfo(outputFile);

    if (srcInfo.canonicalFilePath() == dstInfo.canonicalFilePath()) {
        // Saving to the same file: copy via secure temp then rename (Fix 2)
        QTemporaryFile tempFile(dstInfo.absolutePath() + QStringLiteral("/XXXXXX.pdf.tmp"));
        tempFile.setAutoRemove(false);
        if (!tempFile.open())
            return;
        QString tempPath = tempFile.fileName();
        tempFile.close();

        if (!QFile::copy(m_filePath, tempPath)) {
            QFile::remove(tempPath);
            return;
        }
        m_document->close();
        QFile::remove(outputFile);
        QFile::rename(tempPath, outputFile);
        m_document->load(outputFile);
    } else {
        if (QFile::exists(outputFile))
            QFile::remove(outputFile);
        QFile::copy(m_filePath, outputFile);
    }

    // Copy the annotation sidecar file alongside the saved PDF
    QString srcAnn = m_filePath + QStringLiteral(".ann");
    QString dstAnn = outputFile + QStringLiteral(".ann");
    if (QFile::exists(srcAnn) && srcAnn != dstAnn) {
        if (QFile::exists(dstAnn))
            QFile::remove(dstAnn);
        QFile::copy(srcAnn, dstAnn);
    }
}

void PdfViewerWidget::applyRedactions(const QString &outputFile)
{
    try {
        PoDoFo::PdfMemDocument doc;
        doc.Load(m_filePath.toUtf8().constData());

        QList<AnnotationItem> redactions;
        for (const auto& anno : m_annotationLayer->annotations()) {
            if (anno.mode == ToolMode::Redact) {
                redactions.append(anno);
            }
        }

        if (redactions.isEmpty()) {
            doc.Save(outputFile.toUtf8().constData());
            return;
        }

        // Group redactions by page
        QMap<int, QList<QRectF>> redactionsByPage;
        for (const auto& anno : redactions) {
            if (anno.pageIndex < 0 || static_cast<unsigned>(anno.pageIndex) >= doc.GetPages().GetCount()) continue;
            PoDoFo::PdfPage& page = doc.GetPages().GetPageAt(anno.pageIndex);
            double pageHeight = page.GetMediaBox().Height;
            QRectF pdfRect(anno.rect.x(),
                           pageHeight - anno.rect.y() - anno.rect.height(),
                           anno.rect.width(),
                           anno.rect.height());
            redactionsByPage[anno.pageIndex].append(pdfRect);
        }

        for (auto it = redactionsByPage.begin(); it != redactionsByPage.end(); ++it) {
            int pageIndex = it.key();
            const QList<QRectF>& rects = it.value();
            PoDoFo::PdfPage& page = doc.GetPages().GetPageAt(pageIndex);

            // Phase 1: Read the content stream and rebuild it without text
            // operators whose coordinates fall inside any redaction rectangle.
            PoDoFo::PdfContentStreamReader reader(page);
            PoDoFo::PdfContent content;

            double currentX = 0, currentY = 0;
            std::string newStream;
            bool insideRedactedZone = false;

            // We can't selectively remove operators from PoDoFo's content stream
            // reader in a round-trip fashion. Instead, we'll use the painter approach:
            // 1. Draw white rectangles to obliterate existing content visually AND in print
            // 2. Draw black rectangles on top for visible redaction marks
            // 3. Remove text from the content stream by rewriting it
            //
            // PoDoFo 1.1 does not provide a content stream WRITER that can selectively
            // filter operators. The reliable approach for this version:
            // - Get raw content stream bytes
            // - Parse and filter text operators by position
            // - Replace the content stream

            // Get the raw content stream
            auto* contentsObj = page.GetDictionary().FindKey("Contents");
            if (contentsObj && contentsObj->HasStream()) {
                PoDoFo::charbuff streamBuf;
                auto& stream = contentsObj->GetOrCreateStream();
                stream.CopyTo(streamBuf);
                std::string streamStr(streamBuf.data(), streamBuf.size());

                // Track text position via simple state machine
                // Parse Tm, Td, TD operators for position
                // Remove Tj/TJ operators when position is inside redaction rect
                double textX = 0, textY = 0;
                bool inTextBlock = false;

                std::istringstream iss(streamStr);
                std::ostringstream oss;
                std::string token;
                std::vector<std::string> operandStack;

                while (iss >> token) {
                    if (token == "BT") {
                        inTextBlock = true;
                        textX = 0;
                        textY = 0;
                        oss << token << " ";
                        continue;
                    }
                    if (token == "ET") {
                        inTextBlock = false;
                        oss << token << " ";
                        operandStack.clear();
                        continue;
                    }

                    if (token == "Tm" && operandStack.size() >= 6) {
                        try {
                            textX = std::stod(operandStack[operandStack.size() - 2]);
                            textY = std::stod(operandStack[operandStack.size() - 1]);
                        } catch (...) {}

                        bool redacted = false;
                        QPointF pos(textX, textY);
                        for (const auto& r : rects) {
                            if (r.contains(pos) || (textX >= r.left() && textX <= r.right() && textY >= r.bottom() && textY <= r.top())) {
                                redacted = true;
                                break;
                            }
                        }

                        if (redacted) {
                            operandStack.clear();
                            oss << "1 0 0 1 0 0 Tm ";
                            continue;
                        }

                        for (const auto& op : operandStack) oss << op << " ";
                        oss << token << " ";
                        operandStack.clear();
                        continue;
                    }

                    if ((token == "Td" || token == "TD") && operandStack.size() >= 2) {
                        try {
                            textX += std::stod(operandStack[operandStack.size() - 2]);
                            textY += std::stod(operandStack[operandStack.size() - 1]);
                        } catch (...) {}

                        for (const auto& op : operandStack) oss << op << " ";
                        oss << token << " ";
                        operandStack.clear();
                        continue;
                    }

                    if ((token == "Tj" || token == "TJ" || token == "'" || token == "\"") && inTextBlock) {
                        bool redacted = false;
                        QPointF pos(textX, textY);
                        for (const auto& r : rects) {
                            if (r.contains(pos) || (textX >= r.left() && textX <= r.right() && textY >= r.bottom() && textY <= r.top())) {
                                redacted = true;
                                break;
                            }
                        }

                        if (redacted) {
                            operandStack.clear();
                            continue;
                        }

                        for (const auto& op : operandStack) oss << op << " ";
                        oss << token << " ";
                        operandStack.clear();
                        continue;
                    }

                    if (token == "Tf" || token == "cm" || token == "re" || token == "m" ||
                        token == "l" || token == "c" || token == "v" || token == "y" ||
                        token == "h" || token == "W" || token == "W*" || token == "n" ||
                        token == "f" || token == "F" || token == "f*" || token == "B" ||
                        token == "B*" || token == "b" || token == "b*" || token == "S" ||
                        token == "s" || token == "Do" || token == "gs" || token == "CS" ||
                        token == "cs" || token == "SC" || token == "sc" || token == "SCN" ||
                        token == "scn" || token == "G" || token == "g" || token == "RG" ||
                        token == "rg" || token == "K" || token == "k" || token == "w" ||
                        token == "J" || token == "j" || token == "M" || token == "d" ||
                        token == "ri" || token == "i" || token == "q" || token == "Q" ||
                        token == "Tc" || token == "Tw" || token == "Tz" || token == "TL" ||
                        token == "Tr" || token == "Ts" || token == "T*") {
                        for (const auto& op : operandStack) oss << op << " ";
                        oss << token << " ";
                        operandStack.clear();
                        continue;
                    }

                    operandStack.push_back(token);
                }

                // Flush remaining operands
                for (const auto& op : operandStack) oss << op << " ";

                std::string filteredStream = oss.str();
                PoDoFo::charbuff newBuf(filteredStream.data(), filteredStream.size());
                contentsObj->GetOrCreateStream().SetData(newBuf);
            }

            // Phase 2: Draw black rectangles on top for visual redaction marks
            PoDoFo::PdfPainter painter;
            painter.SetCanvas(page);
            painter.GraphicsState.SetNonStrokingColor(PoDoFo::PdfColor(0.0, 0.0, 0.0));
            for (const auto& r : rects) {
                painter.DrawRectangle(r.x(), r.y(), r.width(), r.height(), PoDoFo::PdfPathDrawMode::Fill);
            }
            painter.FinishDrawing();
        }

        doc.Save(outputFile.toUtf8().constData());
        qDebug() << "Redactions applied with content stream filtering to:" << outputFile;
    } catch (const std::exception& e) {
        qWarning() << "PoDoFo error applying redactions:" << e.what();
    }
}

void PdfViewerWidget::mergeDocuments(const QStringList &files, const QString &outputFile)
{
    if (files.isEmpty()) return;

    try {
        PoDoFo::PdfMemDocument destDoc;
        
        for (const QString &fileName : files) {
            PoDoFo::PdfMemDocument sourceDoc;
            sourceDoc.Load(fileName.toUtf8().constData());
            
            int count = sourceDoc.GetPages().GetCount();
            if (count > 0) {
                destDoc.GetPages().AppendDocumentPages(sourceDoc, 0, count);
            }
        }
        
        destDoc.Save(outputFile.toUtf8().constData());
    } catch (const std::exception& e) {
        qWarning() << "PoDoFo error merging documents:" << e.what();
    }
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

