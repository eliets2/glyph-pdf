// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QWidget>
#include <QHash>
#include <memory>

class QVBoxLayout;
class QScrollArea;
class QScrollBar;
class QLabel;
class QSpacerItem;
class PdfViewerWidget;
class ThumbItem;
class RenderCache;
class ThumbnailRenderer;

class ThumbnailSidebar : public QWidget {
    Q_OBJECT
public:
    explicit ThumbnailSidebar(QWidget* parent = nullptr);
    ~ThumbnailSidebar() override;  // out-of-line: ThumbnailRenderer is opaque here
    void setViewer(PdfViewerWidget* viewer);
    void setCurrentPage(int page);
    void rebuild();

signals:
    void pageClicked(int page);
    void pageReordered(int sourceIndex, int targetIndex);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    
    // Drag & Drop
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    QWidget* createThumbWidget(int pageIndex);
    void updateVisibleThumbnails();

    static constexpr int ThumbItemHeight = 260; // estimated height per thumb widget
    static constexpr int VisibleBuffer   = 2;   // extra widgets above/below viewport

    QScrollArea*         m_scroll;
    QWidget*             m_container;
    QVBoxLayout*         m_layout;
    QLabel*              m_pageCountLabel;
    PdfViewerWidget*     m_viewer      = nullptr;
    int                  m_currentPage = 0;
    int                  m_totalPages  = 0;
    
    QPoint               m_dragStartPos;
    int                  m_dropIndicatorIndex = -1;

    // Virtualization (Fix 4): sparse map of live widgets keyed by page index
    QHash<int, QWidget*> m_liveWidgets;
    QSpacerItem*         m_topSpacer   = nullptr;
    QSpacerItem*         m_bottomSpacer = nullptr;

    // D2: real PDFium-rendered thumbnails cached at 75 DPI. The RenderCache
    // (LRU, memory-budgeted) holds rendered page images; ThumbnailRenderer is
    // an IPdfRenderer adapter over the viewer's PDFium-backed QPdfDocument.
    static constexpr int ThumbnailDpi = 75;
    std::shared_ptr<RenderCache>       m_renderCache;
    std::unique_ptr<ThumbnailRenderer> m_renderer;
};
