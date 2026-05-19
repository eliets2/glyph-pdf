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

signals:
    void annotationsChanged();
    void selectionChanged(int index);
    void textEditRequested(int pageIndex, QPointF pos);
    void imageSelected(const QString &xobjectName, const QRectF &placement);
    void imageMoved(const QString &xobjectName, double dx, double dy);
    void imageResized(const QString &xobjectName, double newW, double newH);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

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
};
