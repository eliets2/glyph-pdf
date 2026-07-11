// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QWidget>
#include <QList>
#include <QPointF>
#include <functional>
#include "core/PdfEnums.h"
#include "core/OcrTypes.h"

#include "core/AnnotationTypes.h"
#include "core/ImageTypes.h"

class AnnotationLayer : public QWidget
{
    Q_OBJECT

public:
    explicit AnnotationLayer(QWidget *parent = nullptr);

    void setMode(ToolMode mode);
    void setColor(const QColor &color);
    void setThickness(int thickness);
    void clearAll();
    QList<AnnotationItem> annotations() const { return m_annotations; }
    void setAnnotations(const QList<AnnotationItem> &items);
    void deleteAnnotation(int index);
    void deleteSelected();
    int selectedIndex() const { return m_selectedIndex; }
    void setSelectedIndex(int index);
    void setRotation(int rotation);
    void setOcrResults(const QList<OcrResult> &results);
    void setPageAtCallback(std::function<int(QPoint)> callback);

    void setImageOverlays(const QList<PdfImageInfo> &images);
    void setSelectedImageName(const QString &name);
    QString selectedImageName() const { return m_selectedImageName; }

    // Wave 1C #3: search-result highlight rectangles (page-local, same
    // coordinate space as AnnotationItem::rect) to keep two-page mode's
    // search-highlight sync with the primary single-page view, which gets
    // native highlighting for free from QPdfView's built-in search model
    // binding. Pass an empty list to clear.
    void setSearchHighlights(const QList<QRectF> &rects);

    // AR-7 D5: paint a diff/overlay image on top of the annotation layer.
    // Caller passes a null QImage to clear the overlay.
    void setOverlayImage(const QImage &img);

signals:
    void annotationsChanged();
    void selectionChanged(int index);
    void textEditRequested(int pageIndex, QPointF pos);
    void imageSelected(const QString &xobjectName, const QRectF &placement);
    void imageMoved(const QString &xobjectName, double dx, double dy);
    void imageResized(const QString &xobjectName, double newW, double newH);
    // Wave 1A §9.2: rotate/delete/replace requested from the right-click menu or the
    // drag-rotate handle. EditController owns the actual RotateImageCommand /
    // DeleteImageCommand / ReplaceImageCommand invocation (needs engine + undo stack).
    void imageRotateRequested(const QString &xobjectName, double degrees);
    void imageDeleteRequested(const QString &xobjectName);
    void imageReplaceRequested(const QString &xobjectName);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    // Wave 1A §9.2: Delete key removes the selected image while in EditImage mode
    // (mirrors the existing right-click "Delete Image" menu entry). Requires focus,
    // so the constructor sets Qt::StrongFocus.
    void keyPressEvent(QKeyEvent *event) override;

private:
    ToolMode m_currentMode;
    QColor m_selectedColor;
    int m_selectedThickness;
    QList<AnnotationItem> m_annotations;
    AnnotationItem m_currentNote;
    bool m_isDrawing;
    bool m_isMoving;
    QPointF m_lastDragPos;
    int m_rotation;
    int m_selectedIndex;
    QList<OcrResult> m_ocrResults;
    std::function<int(QPoint)> m_pageAtCallback;
    QList<PdfImageInfo> m_imageOverlays;
    QString m_selectedImageName;
    int m_resizeHandle = -1;  // -1=none, 0-3=corners, 4-7=edges
    QPointF m_originalImagePos;
    // Wave 1A §9.2: drag-rotate handle for the selected image (a small circular
    // grip above the top-center resize handle). m_isRotatingImage is true while
    // the user is actively dragging it; m_imageRotateStartAngle/m_imageRotateAccum
    // track the running rotation for the current drag so imageRotateRequested can
    // be emitted with a total delta on release (mirrors the move/resize pattern).
    bool m_isRotatingImage = false;
    double m_imageRotateStartAngle = 0.0;
    double m_imageRotateAccum = 0.0;
    // AR-7 D5: overlay image (e.g. pixel-diff from CompareMode).
    QImage m_overlayImage;
    // Wave 1C #3: search-result highlight rects (see setSearchHighlights()).
    QList<QRectF> m_searchHighlights;
};
