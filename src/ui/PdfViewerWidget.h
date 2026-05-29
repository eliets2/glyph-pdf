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
#include "core/PdfEnums.h"
#include "ui/AnnotationLayer.h"

QT_BEGIN_NAMESPACE
class QSpinBox;
class QLabel;
class QRubberBand;
class QMouseEvent;
QT_END_NAMESPACE


class PdfViewerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PdfViewerWidget(QWidget *parent = nullptr);
    ~PdfViewerWidget();

    bool loadDocument(const QString &fileName);
    bool isLoaded() const;
    ToolMode toolMode() const { return m_toolMode; }
    void setToolMode(ToolMode mode);
    void setAnnotationColor(const QColor &color);
    void setAnnotationThickness(int thickness);
    void saveAnnotations();
    void loadAnnotations();
    void setAnnotations(const QList<AnnotationItem> &items);
    void deleteSelectedAnnotation();
    QList<AnnotationItem> annotations() const;
    void searchDocument(const QString &text, bool forward, bool matchCase, bool wholeWords);
    void redactAllMatches(const QString &text, bool matchCase, bool wholeWords);
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
    void setOverlayImage(const QImage &img);

    // Export
    QImage renderPage(int page, qreal scaleFactor = 2.0) const;
    void extractPages(int from, int to, const QString &outputFile);
    void deletePages(int from, int to, const QString &outputFile);
    void insertBlankPage(int index, const QString &outputFile);
    void rotatePages(int from, int to, int angle, const QString &outputFile);
    void applyRedactions(const QString &outputFile);
    bool saveDocumentAs(const QString &outputFile);
    static void mergeDocuments(const QStringList &files, const QString &outputFile);
    void printDocument();

    // Accessors
    QPdfDocument* document() const { return m_document; }
    QPdfSearchModel* searchModel() const { return m_searchModel; }
    QPdfBookmarkModel* bookmarkModel() const { return m_bookmarkModel; }
    QPdfPageNavigator* pageNavigator() const { return m_pageNavigator; }
    AnnotationLayer* annotationLayer() const { return m_annotationLayer; }
    QString filePath() const { return m_filePath; }

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

    void onPageChanged();
    void updateRotation();

private:
    void clearPageCache();

    QPdfDocument *m_document;
    QPdfView *m_pdfView;
    QPdfSearchModel *m_searchModel;
    QPdfBookmarkModel *m_bookmarkModel;
    QPdfPageNavigator *m_pageNavigator;
    QPdfPageRenderer *m_pageRenderer;
    AnnotationLayer *m_annotationLayer;
    qreal m_zoomFactor;
    ToolMode m_toolMode;
    QString m_filePath;
    int m_rotation;
    QImage m_overlayImage;

    // Crop selection
    QRubberBand *m_rubberBand = nullptr;
    QPoint m_rubberBandOrigin;
    bool m_isSelectingCrop = false;

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
    };
    mutable QHash<int, CachedPage> m_pageCache;
    mutable qint64 m_cacheAccessCounter = 0;
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
