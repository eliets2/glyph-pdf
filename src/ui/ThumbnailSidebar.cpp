// SPDX-License-Identifier: Apache-2.0
#include "ThumbnailSidebar.h"
#include "PdfViewerWidget.h"
#include "engines/RenderCache.h"
#include "core/interfaces/IPdfRenderer.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QScrollBar>
#include <QLabel>
#include <QPushButton>
#include <QEvent>
#include <QMouseEvent>
#include <QSpacerItem>
#include <QStyle>
#include <QDrag>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QApplication>
#include <QPainter>
#include <QPixmap>
#include <QPdfDocument>

// ---------------------------------------------------------------------------
// D2: IPdfRenderer adapter over the viewer's PDFium-backed QPdfDocument.
// Renders a page at the requested DPI; RenderCache wraps this for LRU caching
// so each thumbnail is rendered once and reused across virtualization passes.
// ---------------------------------------------------------------------------
class ThumbnailRenderer : public IPdfRenderer {
public:
    explicit ThumbnailRenderer(PdfViewerWidget* viewer) : m_viewer(viewer) {}

    QImage renderPage(int pageIndex, int dpi) override {
        if (!m_viewer) return QImage();
        // PdfViewerWidget::renderPage takes a points->pixels scale factor.
        // dpi/72 converts DPI to that factor; the viewer renders via QPdfDocument
        // (Qt's PDFium-backed PDF module).
        const qreal scaleFactor = static_cast<qreal>(dpi) / 72.0;
        return m_viewer->renderPage(pageIndex, scaleFactor);
    }

    QImage renderTile(int pageIndex, const QRectF& /*subRect*/, int dpi) override {
        // Thumbnails never tile; full-page render is sufficient.
        return renderPage(pageIndex, dpi);
    }

private:
    PdfViewerWidget* m_viewer;
};

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

ThumbnailSidebar::ThumbnailSidebar(QWidget* parent)
    : QWidget(parent)
{
    setObjectName("thumbnailSidebar");
    setAcceptDrops(true);
    setAccessibleName(tr("Page thumbnails"));
    setAccessibleDescription(tr("Thumbnail preview of document pages. Click to navigate, drag to reorder."));

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Toolbar: "PAGES . N" + zoom buttons
    auto* toolbar = new QWidget;
    toolbar->setObjectName("thumbToolbar");
    toolbar->setFixedHeight(24);

    auto* tbLayout = new QHBoxLayout(toolbar);
    tbLayout->setContentsMargins(12, 0, 8, 0);
    tbLayout->setSpacing(4);

    m_pageCountLabel = new QLabel(tr("PAGES · 0"));
    m_pageCountLabel->setObjectName("thumbPageCountLabel");
    m_pageCountLabel->setAccessibleName(tr("Page count"));
    tbLayout->addWidget(m_pageCountLabel);
    tbLayout->addStretch();

    auto* zoomOutBtn = new QPushButton(QStringLiteral("\xE2\x88\x92")); // minus sign
    zoomOutBtn->setFixedSize(20, 18);
    zoomOutBtn->setProperty("variant", "mini");
    zoomOutBtn->setToolTip(QStringLiteral("Smaller thumbnails"));
    tbLayout->addWidget(zoomOutBtn);

    auto* zoomInBtn = new QPushButton(QStringLiteral("+"));
    zoomInBtn->setFixedSize(20, 18);
    zoomInBtn->setProperty("variant", "mini");
    zoomInBtn->setToolTip(QStringLiteral("Larger thumbnails"));
    tbLayout->addWidget(zoomInBtn);

    mainLayout->addWidget(toolbar);

    // Scroll area
    m_scroll = new QScrollArea;
    m_scroll->setObjectName("thumbScrollArea");
    m_scroll->setWidgetResizable(true);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_container = new QWidget;
    m_container->setObjectName("thumbContainer");
    m_layout = new QVBoxLayout(m_container);
    m_layout->setContentsMargins(12, 8, 12, 8);
    m_layout->setSpacing(8);

    // Spacers for virtualization (Fix 4)
    m_topSpacer = new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Fixed);
    m_bottomSpacer = new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Fixed);
    m_layout->addSpacerItem(m_topSpacer);
    m_layout->addSpacerItem(m_bottomSpacer);

    m_scroll->setWidget(m_container);
    mainLayout->addWidget(m_scroll, 1);

    // Re-virtualize on scroll (Fix 4)
    connect(m_scroll->verticalScrollBar(), &QScrollBar::valueChanged,
            this, &ThumbnailSidebar::updateVisibleThumbnails);
}

// Out-of-line destructor: ThumbnailRenderer (held by unique_ptr) is only a
// complete type in this translation unit, so the deleter must be instantiated
// here rather than in the header / MOC unit.
ThumbnailSidebar::~ThumbnailSidebar() = default;

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void ThumbnailSidebar::setViewer(PdfViewerWidget* viewer)
{
    m_viewer = viewer;

    // (Re)build the 75-DPI thumbnail render cache + renderer for this viewer.
    if (viewer) {
        m_renderer = std::make_unique<ThumbnailRenderer>(viewer);
        m_renderCache = std::make_shared<RenderCache>();
        // Thumbnails are small (≈140x181 px); a modest budget holds plenty.
        m_renderCache->setMaxCacheSize(32LL * 1024 * 1024);
    } else {
        m_renderer.reset();
        m_renderCache.reset();
    }

    rebuild();
}

void ThumbnailSidebar::rebuild()
{
    // Clear all live widgets
    for (auto it = m_liveWidgets.begin(); it != m_liveWidgets.end(); ++it) {
        m_layout->removeWidget(it.value());
        it.value()->deleteLater();
    }
    m_liveWidgets.clear();

    // Drop cached page renders — rebuild() means the document (and therefore
    // the page->image mapping) may have changed, so stale renders for reused
    // page indices must not be shown.
    if (m_renderCache) m_renderCache->clear();

    if (!m_viewer) {
        m_totalPages = 0;
        m_pageCountLabel->setText(QStringLiteral("PAGES \xC2\xB7 0"));
        m_topSpacer->changeSize(0, 0);
        m_bottomSpacer->changeSize(0, 0);
        m_layout->invalidate();
        return;
    }

    m_totalPages = m_viewer->pageCount();
    m_pageCountLabel->setText(QStringLiteral("PAGES \xC2\xB7 %1").arg(m_totalPages));

    // Set total virtual height so the scrollbar is correct
    int totalHeight = m_totalPages * ThumbItemHeight;
    m_topSpacer->changeSize(0, 0);
    m_bottomSpacer->changeSize(0, totalHeight);
    m_layout->invalidate();

    // Force an initial population
    QMetaObject::invokeMethod(this, &ThumbnailSidebar::updateVisibleThumbnails, Qt::QueuedConnection);
}

// ---------------------------------------------------------------------------
// Virtualization engine (Fix 4)
// ---------------------------------------------------------------------------

void ThumbnailSidebar::updateVisibleThumbnails()
{
    if (m_totalPages == 0) return;

    int scrollY = m_scroll->verticalScrollBar()->value();
    int viewportH = m_scroll->viewport()->height();

    // Determine which page indices are visible
    int firstVisible = qMax(0, scrollY / ThumbItemHeight - VisibleBuffer);
    int lastVisible  = qMin(m_totalPages - 1, (scrollY + viewportH) / ThumbItemHeight + VisibleBuffer);

    // Remove widgets outside the visible range
    QList<int> toRemove;
    for (auto it = m_liveWidgets.begin(); it != m_liveWidgets.end(); ++it) {
        if (it.key() < firstVisible || it.key() > lastVisible)
            toRemove.append(it.key());
    }
    for (int idx : toRemove) {
        QWidget *w = m_liveWidgets.take(idx);
        m_layout->removeWidget(w);
        w->deleteLater();
    }

    // Create widgets for newly visible pages
    for (int i = firstVisible; i <= lastVisible; ++i) {
        if (m_liveWidgets.contains(i))
            continue;
        QWidget *w = createThumbWidget(i);
        m_liveWidgets.insert(i, w);
    }

    // Re-order: remove all widgets from layout, set spacers, re-add in order
    // First remove everything except spacers
    for (auto it = m_liveWidgets.begin(); it != m_liveWidgets.end(); ++it) {
        m_layout->removeWidget(it.value());
    }
    // Remove spacers from layout so we can re-add in order
    m_layout->removeItem(m_topSpacer);
    m_layout->removeItem(m_bottomSpacer);

    // Top spacer covers pages [0, firstVisible)
    int topH = firstVisible * ThumbItemHeight;
    m_topSpacer->changeSize(0, topH);
    m_layout->addSpacerItem(m_topSpacer);

    // Add widgets in page order
    QList<int> sortedKeys = m_liveWidgets.keys();
    std::sort(sortedKeys.begin(), sortedKeys.end());
    for (int key : sortedKeys) {
        m_layout->addWidget(m_liveWidgets[key]);
    }

    // Bottom spacer covers pages (lastVisible, totalPages)
    int bottomH = qMax(0, (m_totalPages - lastVisible - 1) * ThumbItemHeight);
    m_bottomSpacer->changeSize(0, bottomH);
    m_layout->addSpacerItem(m_bottomSpacer);

    m_layout->invalidate();
}

QWidget* ThumbnailSidebar::createThumbWidget(int pageIndex)
{
    bool isCurrent = (pageIndex == m_currentPage);

    auto* thumbWidget = new QWidget;
    thumbWidget->setObjectName("thumbItem");
    thumbWidget->setCursor(Qt::PointingHandCursor);
    thumbWidget->setProperty("pageIndex", pageIndex);
    thumbWidget->setProperty("current", isCurrent);
    thumbWidget->installEventFilter(this);

    auto* thumbLayout = new QVBoxLayout(thumbWidget);
    thumbLayout->setContentsMargins(4, 4, 4, 4);
    thumbLayout->setSpacing(4);
    thumbLayout->setAlignment(Qt::AlignHCenter);

    // Page number label
    auto* numLabel = new QLabel(QStringLiteral("%1").arg(pageIndex + 1, 3, 10, QChar('0')));
    numLabel->setObjectName("thumbNumLabel");
    numLabel->setAlignment(Qt::AlignCenter);
    numLabel->setProperty("current", isCurrent);
    thumbLayout->addWidget(numLabel);

    // Frame (dark border container)
    auto* frame = new QWidget;
    frame->setObjectName("thumbFrame");
    frame->setProperty("current", isCurrent);

    auto* frameLayout = new QVBoxLayout(frame);
    frameLayout->setContentsMargins(6, 6, 6, 6);
    frameLayout->setSpacing(0);

    // Paper (8.5:11 aspect ratio container)
    auto* paper = new QWidget;
    paper->setObjectName("thumbPaper");
    paper->setFixedSize(140, 181);

    // D2: real PDFium-rendered thumbnail (cached via RenderCache at 75 DPI),
    // replacing the former fake title/text/image block placeholders. The render
    // is produced once per page and reused across virtualization passes.
    auto* paperLayout = new QVBoxLayout(paper);
    paperLayout->setContentsMargins(0, 0, 0, 0);
    paperLayout->setSpacing(0);

    auto* pageImage = new QLabel;
    pageImage->setObjectName("thumbPaperImage");
    pageImage->setAlignment(Qt::AlignCenter);
    pageImage->setScaledContents(false);

    QImage rendered;
    if (m_renderCache && m_renderer) {
        // getOrRender's scale arg maps to DPI as dpi = scale * 72, so passing
        // 75/72 yields a 75-DPI render.
        const qreal scale = static_cast<qreal>(ThumbnailDpi) / 72.0;
        rendered = m_renderCache->getOrRender(pageIndex, scale, m_renderer.get());
    }

    if (!rendered.isNull()) {
        // Fit the real page render inside the paper area, preserving aspect.
        QPixmap pm = QPixmap::fromImage(rendered).scaled(
            paper->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        pageImage->setPixmap(pm);
    } else {
        // Honest fallback when no document/render is available yet (e.g. the
        // page failed to render) — a blank sheet, never fabricated content.
        pageImage->setText(QString());
    }

    paperLayout->addWidget(pageImage);
    frameLayout->addWidget(paper, 0, Qt::AlignCenter);
    thumbLayout->addWidget(frame);

    // Page label text below
    auto* labelText = new QLabel(QStringLiteral("Page %1").arg(pageIndex + 1));
    labelText->setObjectName("thumbLabelText");
    labelText->setAlignment(Qt::AlignCenter);
    thumbLayout->addWidget(labelText);

    return thumbWidget;
}

void ThumbnailSidebar::setCurrentPage(int page)
{
    if (page == m_currentPage) return;

    int oldPage = m_currentPage;
    m_currentPage = page;

    // Update old thumb styling if it is live
    if (m_liveWidgets.contains(oldPage)) {
        auto* oldWidget = m_liveWidgets[oldPage];
        oldWidget->setProperty("current", false);
        oldWidget->style()->unpolish(oldWidget);
        oldWidget->style()->polish(oldWidget);

        auto* numLabel = oldWidget->findChild<QLabel*>("thumbNumLabel");
        if (numLabel) {
            numLabel->setProperty("current", false);
            numLabel->style()->unpolish(numLabel);
            numLabel->style()->polish(numLabel);
        }

        auto* frame = oldWidget->findChild<QWidget*>("thumbFrame");
        if (frame) {
            frame->setProperty("current", false);
            frame->style()->unpolish(frame);
            frame->style()->polish(frame);
        }
    }

    // Update new thumb styling if it is live
    if (m_liveWidgets.contains(page)) {
        auto* newWidget = m_liveWidgets[page];
        newWidget->setProperty("current", true);
        newWidget->style()->unpolish(newWidget);
        newWidget->style()->polish(newWidget);

        auto* numLabel = newWidget->findChild<QLabel*>("thumbNumLabel");
        if (numLabel) {
            numLabel->setProperty("current", true);
            numLabel->style()->unpolish(numLabel);
            numLabel->style()->polish(numLabel);
        }

        auto* frame = newWidget->findChild<QWidget*>("thumbFrame");
        if (frame) {
            frame->setProperty("current", true);
            frame->style()->unpolish(frame);
            frame->style()->polish(frame);
        }

        // Ensure visible
        m_scroll->ensureWidgetVisible(newWidget, 0, 40);
    } else {
        // Scroll to the page position so virtualization picks it up
        int targetY = page * ThumbItemHeight;
        m_scroll->verticalScrollBar()->setValue(targetY);
    }
}

// ---------------------------------------------------------------------------
// Event filter (click handling)
// ---------------------------------------------------------------------------

bool ThumbnailSidebar::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::MouseButtonPress) {
        auto* widget = qobject_cast<QWidget*>(watched);
        if (widget) {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                m_dragStartPos = me->pos();
            }
        }
    } else if (event->type() == QEvent::MouseMove) {
        auto* widget = qobject_cast<QWidget*>(watched);
        if (widget) {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->buttons() & Qt::LeftButton) {
                if ((me->pos() - m_dragStartPos).manhattanLength() >= QApplication::startDragDistance()) {
                    bool ok = false;
                    int pageIndex = widget->property("pageIndex").toInt(&ok);
                    if (ok) {
                        QDrag *drag = new QDrag(this);
                        QMimeData *mimeData = new QMimeData;
                        mimeData->setText(QString::number(pageIndex));
                        drag->setMimeData(mimeData);
                        
                        // Create pixmap for drag
                        QPixmap pixmap = widget->grab();
                        drag->setPixmap(pixmap);
                        drag->setHotSpot(me->pos());
                        
                        drag->exec(Qt::MoveAction);
                        return true;
                    }
                }
            }
        }
    } else if (event->type() == QEvent::MouseButtonRelease) {
        auto* widget = qobject_cast<QWidget*>(watched);
        if (widget) {
            bool ok = false;
            int pageIndex = widget->property("pageIndex").toInt(&ok);
            if (ok) {
                emit pageClicked(pageIndex);
                return true;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

// ---------------------------------------------------------------------------
// Drag & Drop
// ---------------------------------------------------------------------------

void ThumbnailSidebar::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasText()) {
        event->acceptProposedAction();
    }
}

void ThumbnailSidebar::dragMoveEvent(QDragMoveEvent* event)
{
    if (event->mimeData()->hasText()) {
        event->acceptProposedAction();
        
        // Visual indicator could be drawn by overriding paintEvent or inserting a spacer.
        // For simplicity, we just accept the move.
        // A full implementation would highlight the drop target gap.
    }
}

void ThumbnailSidebar::dropEvent(QDropEvent* event)
{
    if (event->mimeData()->hasText()) {
        int sourceIndex = event->mimeData()->text().toInt();
        
        // Find target index based on drop position in the scroll area
        QPoint pos = m_scroll->widget()->mapFrom(this, event->pos());
        int targetIndex = qMax(0, qMin(m_totalPages - 1, pos.y() / ThumbItemHeight));
        
        if (sourceIndex != targetIndex) {
            emit pageReordered(sourceIndex, targetIndex);
        }
        
        event->acceptProposedAction();
    }
}

