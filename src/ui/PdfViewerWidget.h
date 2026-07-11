// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QWidget>
#include <QHash>
#include <QPixmap>
#include <QTimer>
#include <QPdfDocument>
#include <QPdfView>
#include <QPdfSearchModel>
#include <QPdfBookmarkModel>
#include <QPdfPageNavigator>
#include <QPdfPageRenderer>
#include <QPdfLink>
#include "core/PdfEnums.h"
#include "ui/AnnotationLayer.h"

QT_BEGIN_NAMESPACE
class QSpinBox;
class QLabel;
class QRubberBand;
class QMouseEvent;
QT_END_NAMESPACE

namespace gp { class Badge; }


class PdfViewerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PdfViewerWidget(QWidget *parent = nullptr);
    ~PdfViewerWidget();

    bool loadDocument(const QString &fileName);
    bool isLoaded() const;
    void reload();
    ToolMode toolMode() const { return m_toolMode; }
    void setToolMode(ToolMode mode);
    // Read-only mode (e.g. for expired documents): blocks editing/annotation
    // tool modes, allowing only viewing and text selection.
    void setReadOnly(bool readOnly);
    bool isReadOnly() const { return m_readOnly; }
    void setAnnotationColor(const QColor &color);
    void setAnnotationThickness(int thickness);
    void saveAnnotations();
    void loadAnnotations();
    void setAnnotations(const QList<AnnotationItem> &items);
    void deleteSelectedAnnotation();
    QList<AnnotationItem> annotations() const;
    void searchDocument(const QString &text, bool forward, bool matchCase, bool wholeWords);

    void setOcrResults(const QList<OcrResult> &results);

    // Page navigation
    void goToPage(int page);
    void goBack();
    void goForward();
    bool canGoBack() const;
    bool canGoForward() const;
    int currentPage() const;
    int pageCount() const;

    // Zoom presets
    void zoomFitWidth();
    void zoomFitPage();
    void setZoomLevel(qreal level);
    qreal zoomLevel() const;

    // View settings
    void rotateClockwise();
    void rotateCounterClockwise();
    void setPageMode(QPdfView::PageMode mode);
    void setTwoPageMode(bool enabled);
    void toggleEyeCareMode();
    // Wave 2B #4: real content-level Night Mode (full color inversion of the
    // rendered page pixels), distinct from chrome Dark Mode (ViewController /
    // MainWindow::toggleTheme(), UI chrome only) and Eye Care (a warm sepia
    // *tint* that leaves the page background glaring white -- see
    // toggleEyeCareMode() above). Mutually exclusive with Eye Care.
    void toggleNightMode();
    bool isNightMode() const { return m_nightMode; }
    void setOverlayImage(const QImage &img);

    // Export
    QImage renderPage(int page, qreal scaleFactor = 2.0) const;
    void extractPages(int from, int to, const QString &outputFile);
    void deletePages(int from, int to, const QString &outputFile);
    void insertBlankPage(int index, const QString &outputFile);
    void rotatePages(int from, int to, int angle, const QString &outputFile);

    bool saveDocumentAs(const QString &outputFile);
    // Wave 1A §9.9: returns the real success/failure of the underlying
    // gp::mergeDocuments() engine call instead of only qWarning()-logging it.
    // Callers (ConvertController::mergePdfs()) must check the result and show a
    // real error dialog on failure instead of always reporting "Successfully
    // merged".
    static bool mergeDocuments(const QStringList &files, const QString &outputFile);
    void printDocument();

    // Accessors
    QPdfDocument* document() const { return m_document; }
    QPdfSearchModel* searchModel() const { return m_searchModel; }
    QPdfBookmarkModel* bookmarkModel() const { return m_bookmarkModel; }
    QPdfPageNavigator* pageNavigator() const { return m_pageNavigator; }
    AnnotationLayer* annotationLayer() const { return m_annotationLayer; }
    QString filePath() const { return m_filePath; }

    // Wave 1A §9.7: on-page signature validity badge. `allValid` selects the
    // Ok/Err badge styling; `summary` is the tooltip text (e.g. "2 of 2
    // signatures valid"). Pass an empty summary to hide the badge (unsigned
    // document, or no manager available). Presentation-layer only -- the
    // underlying SignatureInfo::isValid/trustStatus data is already computed
    // by ISignatureManager::validateSignatures(); this just surfaces it on the
    // page instead of only inside the side Signatures panel.
    void setSignatureValidityBadge(bool allValid, const QString &summary);

signals:
    void pageChanged(int currentPage, int totalPages);
    void navigationChanged(bool canBack, bool canForward);
    void annotationsChanged();
    void textEditRequested(int pageIndex, QPointF pos);
    void pageOperationFinished();
    void cropRequested(int pageIndex, QRectF cropRect);
    void textSelected(const QString& selectedText);
    void fieldPlacementRequested(int pageIndex, QRectF pdfRect, ToolMode mode);

public slots:
    void zoomIn();
    void zoomOut();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    // Wave 1B #2: watches m_pdfView's viewport for hover (cursor feedback) and
    // click (navigation) on hyperlink annotations -- see linkAtViewportPos().
    bool eventFilter(QObject *watched, QEvent *event) override;

    void onPageChanged();
    void updateRotation();

private:
    void clearPageCache();
    QPdfLink linkAtViewportPos(const QPoint &viewportPos) const;
    void activateLink(const QPdfLink &link);

    QPdfDocument *m_document;
    QPdfView *m_pdfView;
    QPdfSearchModel *m_searchModel;
    QPdfBookmarkModel *m_bookmarkModel;
    QPdfPageNavigator *m_pageNavigator;
    QPdfPageRenderer *m_pageRenderer;
    class QPdfLinkModel *m_linkModel = nullptr;
    bool m_hoveringLink = false;
    AnnotationLayer *m_annotationLayer;
    // Wave 1A §9.7: floating on-page signature validity badge (top-right corner).
    gp::Badge *m_signatureBadge = nullptr;
    void repositionSignatureBadge();
    qreal m_zoomFactor;
    ToolMode m_toolMode;
    bool m_readOnly = false;
    QString m_filePath;
    int m_rotation;
    QImage m_overlayImage;

    // Crop selection
    QRubberBand *m_rubberBand = nullptr;
    QPoint m_rubberBandOrigin;
    bool m_isSelectingCrop = false;

    // View Modes
    bool m_twoPageMode = false;
    bool m_eyeCareMode = false;
    class QGraphicsColorizeEffect *m_eyeCareEffect = nullptr;
    bool m_nightMode = false;
    class QScrollArea *m_twoPageScrollArea = nullptr;
    class QLabel *m_leftPageLabel = nullptr;
    class QLabel *m_rightPageLabel = nullptr;
    class AnnotationLayer *m_leftAnnotationLayer = nullptr;
    class AnnotationLayer *m_rightAnnotationLayer = nullptr;
    void updateTwoPageView();
    void syncPageOverlay(AnnotationLayer *overlay, QLabel *label, int page, qreal scale);

    // Wave 1B #1: rotation-aware bitmap fallback for the primary interactive
    // view. QPdfView (QtPdfWidgets) exposes no rotation API at all, so a
    // session-only "rotate view" action can only take visible effect on the
    // actual page pixels by switching away from QPdfView's native painting to
    // a manual bitmap render (via the now rotation-aware renderPage()) while
    // rotation is non-zero. Reverts to native QPdfView at rotation 0 for full
    // scrolling/search/selection fidelity.
    class QLabel *m_rotatedPageLabel = nullptr;
    void updateRotatedPageView();

    // Form-builder field placement (M3-PROMPT-1)
    QRubberBand *m_formRubberBand = nullptr;
    QPoint m_formRubberBandOrigin;
    bool m_isPlacingField = false;

    static bool isFormBuilderMode(ToolMode mode);

    // Render cache (Fix 5)
    struct CachedPage {
        QPixmap pixmap;
        qreal scaleFactor = 0.0;
        qint64 lastAccessed = 0;
        qint64 bytes = 0;       // cached pixmap size; tracked so eviction needn't re-sum
    };
    mutable QHash<int, CachedPage> m_pageCache;
    mutable qint64 m_cacheAccessCounter = 0;
    mutable qint64 m_cacheTotalBytes = 0;   // P9: running sum of all cached pixmap bytes
    static constexpr qint64 MaxCacheBytes = 256 * 1024 * 1024; // 256MB

    // Save debounce (Fix 7)
    QTimer *m_saveDebounceTimer;

    // Page change coalescing (Fix 13)
    QTimer *m_pageChangeTimer = nullptr;

    // Page history (D5)
    QList<int> m_pageHistory;
    int m_historyIndex = -1;
    bool m_navigatingHistory = false;
};
