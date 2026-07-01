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

    // Wave 1A §9.15: thumbnail zoom +/- (previously visible, wired-looking
    // buttons with no connect() call at all -- clicking them did nothing).
    // Steps through a small set of discrete scale factors and rebuilds.
    void zoomIn();
    void zoomOut();

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

    static constexpr int BaseThumbItemHeight = 260; // estimated height per thumb widget at 1.0x
    static constexpr int VisibleBuffer   = 2;   // extra widgets above/below viewport

    // Wave 1A §9.15: current thumbnail zoom step and its scale factor. Index
    // into kZoomSteps; ThumbItemHeight()/paper size/render DPI all derive from
    // this so the +/- buttons have a real, visible effect.
    static constexpr double kZoomSteps[] = {0.7, 0.85, 1.0, 1.2, 1.4, 1.6};
    static constexpr int kDefaultZoomIndex = 2; // 1.0x
    int m_zoomIndex = kDefaultZoomIndex;
    int ThumbItemHeight() const {
        return static_cast<int>(BaseThumbItemHeight * kZoomSteps[m_zoomIndex]);
    }

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
    static constexpr int BaseThumbnailDpi = 75;
    int ThumbnailDpi() const {
        return static_cast<int>(BaseThumbnailDpi * kZoomSteps[m_zoomIndex]);
    }
    std::shared_ptr<RenderCache>       m_renderCache;
    std::unique_ptr<ThumbnailRenderer> m_renderer;
};
