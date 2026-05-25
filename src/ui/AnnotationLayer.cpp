#include "ui/AnnotationLayer.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QDebug>
#include <QtMath>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

AnnotationLayer::AnnotationLayer(QWidget *parent)
    : QWidget(parent)
    , m_currentMode(ToolMode::HandTool)
    , m_selectedColor(Qt::red)
    , m_selectedThickness(2)
    , m_isDrawing(false)
    , m_rotation(0)
    , m_selectedIndex(-1)
    , m_isMoving(false)
    , m_pageAtCallback(nullptr)
    , m_resizeHandle(-1)
{
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
    setMouseTracking(true);
    setAccessibleName(tr("Annotation canvas"));
    setAccessibleDescription(tr("Draw highlights, underlines, text boxes, and other annotations on the document"));
}

void AnnotationLayer::setRotation(int rotation)
{
    m_rotation = rotation;
    update();
}

void AnnotationLayer::setMode(ToolMode mode)
{
    m_currentMode = mode;
    // If not in a drawing mode, we might want to pass events through, 
    // but for now let's just ignore them in those modes.
    if (mode == ToolMode::HandTool || mode == ToolMode::SelectText) {
        setAttribute(Qt::WA_TransparentForMouseEvents, true);
    } else {
        setAttribute(Qt::WA_TransparentForMouseEvents, false);
    }
}

void AnnotationLayer::setColor(const QColor &color)
{
    m_selectedColor = color;
}

void AnnotationLayer::setThickness(int thickness)
{
    m_selectedThickness = thickness;
}

void AnnotationLayer::setAnnotations(const QList<AnnotationItem> &items)
{
    QRectF dirty;
    auto getBounds = [](const AnnotationItem &anno) {
        QRectF bounds = anno.rect;
        if (anno.mode == ToolMode::DrawFreehand || anno.mode == ToolMode::AddSignature) {
            bounds = QRectF();
            for (const auto &p : anno.points) {
                if (bounds.isNull()) bounds = QRectF(p, p);
                else bounds = bounds.united(QRectF(p, p));
            }
        }
        return bounds.normalized().adjusted(-15, -15, 15, 15);
    };

    for (const auto &item : m_annotations) {
        dirty = dirty.united(getBounds(item));
    }
    for (const auto &item : items) {
        dirty = dirty.united(getBounds(item));
    }

    m_annotations = items;
    emit annotationsChanged();
    if (dirty.isNull()) {
        update();
    } else {
        update(dirty.toAlignedRect());
    }
}

void AnnotationLayer::clearAll()
{
    m_annotations.clear();
    m_selectedIndex = -1;
    emit annotationsChanged();
    update();
}

void AnnotationLayer::deleteAnnotation(int index)
{
    if (index >= 0 && index < m_annotations.size()) {
        m_annotations.removeAt(index);
        m_selectedIndex = -1;
        emit annotationsChanged();
        update();
    }
}

void AnnotationLayer::deleteSelected()
{
    deleteAnnotation(m_selectedIndex);
}

void AnnotationLayer::setSelectedIndex(int index)
{
    if (m_selectedIndex != index) {
        m_selectedIndex = index;
        emit selectionChanged(m_selectedIndex);
        update();
    }
}

void AnnotationLayer::setPageAtCallback(std::function<int(QPoint)> callback)
{
    m_pageAtCallback = callback;
}

void AnnotationLayer::setOcrResults(const QList<OcrResult> &results)
{
    QRectF dirty;
    for (const auto &res : m_ocrResults) {
        dirty = dirty.united(res.boundingBox.adjusted(-5, -5, 5, 5));
    }
    for (const auto &res : results) {
        dirty = dirty.united(res.boundingBox.adjusted(-5, -5, 5, 5));
    }
    m_ocrResults = results;
    if (dirty.isNull()) {
        update();
    } else {
        update(dirty.toAlignedRect());
    }
}

void AnnotationLayer::setImageOverlays(const QList<PdfImageInfo> &images)
{
    QRectF dirty;
    for (const auto &img : m_imageOverlays) {
        dirty = dirty.united(img.placement.adjusted(-10, -10, 10, 10));
    }
    for (const auto &img : images) {
        dirty = dirty.united(img.placement.adjusted(-10, -10, 10, 10));
    }
    m_imageOverlays = images;
    if (dirty.isNull()) {
        update();
    } else {
        update(dirty.toAlignedRect());
    }
}

void AnnotationLayer::setSelectedImageName(const QString &name)
{
    m_selectedImageName = name;
    update();
}

void AnnotationLayer::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Spatial culling: only draw items that intersect the damaged region (Fix 3)
    const QRectF clipRect = QRectF(event->rect()).adjusted(-20, -20, 20, 20);

    if (m_rotation != 0) {
        painter.translate(rect().center());
        painter.rotate(m_rotation);
        painter.translate(-rect().center());
    }

    int index = 0;
    for (const auto& anno : m_annotations) {
        // Compute the annotation's bounding rect for culling
        QRectF annoBounds;
        if (anno.mode == ToolMode::DrawFreehand || anno.mode == ToolMode::AddSignature) {
            if (!anno.points.isEmpty()) {
                annoBounds = QRectF(anno.points.first(), anno.points.first());
                for (const auto& p : anno.points)
                    annoBounds = annoBounds.united(QRectF(p, p));
            }
        } else {
            annoBounds = anno.rect.normalized();
        }

        // Skip annotations entirely outside the visible clip rect
        if (!annoBounds.isNull() && !clipRect.intersects(annoBounds.adjusted(-10, -10, 10, 10))) {
            ++index;
            continue;
        }

        painter.setPen(QPen(anno.color, anno.thickness, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));

        // Draw selection highlight if selected
        if (index == m_selectedIndex) {
            painter.save();
            painter.setPen(QPen(Qt::blue, 1, Qt::DashLine));
            painter.setBrush(QColor(0, 0, 255, 30));
            if (anno.mode == ToolMode::DrawFreehand || anno.mode == ToolMode::AddSignature) {
                painter.drawRect(annoBounds.adjusted(-5, -5, 5, 5));
            } else {
                painter.drawRect(anno.rect.adjusted(-5, -5, 5, 5));
            }
            painter.restore();
        }

        if (anno.mode == ToolMode::DrawFreehand) {
            for (int i = 0; i < anno.points.size() - 1; ++i) {
                painter.drawLine(anno.points[i], anno.points[i+1]);
            }
        } else if (anno.mode == ToolMode::Highlight) {
            QColor highColor = anno.color;
            highColor.setAlpha(100);
            painter.fillRect(anno.rect, highColor);
        } else if (anno.mode == ToolMode::Underline) {
            painter.setPen(QPen(anno.color, anno.thickness));
            painter.drawLine(anno.rect.bottomLeft(), anno.rect.bottomRight());
        } else if (anno.mode == ToolMode::Strikeout) {
            painter.setPen(QPen(anno.color, anno.thickness));
            QPointF midLeft(anno.rect.left(), anno.rect.center().y());
            QPointF midRight(anno.rect.right(), anno.rect.center().y());
            painter.drawLine(midLeft, midRight);
        } else if (anno.mode == ToolMode::Squiggly) {
            painter.setPen(QPen(anno.color, anno.thickness));
            QPainterPath path;
            path.moveTo(anno.rect.bottomLeft());
            int waves = qMax(1, static_cast<int>(anno.rect.width() / 4));
            qreal step = anno.rect.width() / waves;
            for (int w = 0; w < waves; ++w) {
                qreal x = anno.rect.left() + w * step;
                qreal y = anno.rect.bottom();
                path.quadTo(x + step / 4, y - 2, x + step / 2, y);
                path.quadTo(x + 3 * step / 4, y + 2, x + step, y);
            }
            painter.drawPath(path);
        } else if (anno.mode == ToolMode::Stamp) {
            painter.setPen(QPen(anno.color, 3, Qt::SolidLine));
            painter.drawRect(anno.rect);
            painter.setFont(QFont("Arial", 16, QFont::Bold));
            painter.drawText(anno.rect, Qt::AlignCenter, anno.text.isEmpty() ? "STAMP" : anno.text);
        } else if (anno.mode == ToolMode::Callout) {
            painter.setPen(QPen(anno.color, anno.thickness));
            // For Callout, rect is the text box, points[0] is the leader line end (pointing to something)
            painter.drawRect(anno.rect);
            painter.drawText(anno.rect, Qt::AlignLeft | Qt::AlignTop, anno.text);
            if (!anno.points.isEmpty()) {
                QPointF start = anno.rect.center();
                QPointF end = anno.points.first();
                painter.drawLine(start, end);
            }
        } else if (anno.mode == ToolMode::AddComment) {
            // Draw a sticky note icon
            painter.setBrush(anno.color);
            painter.setPen(Qt::black);
            painter.drawRect(anno.rect.topLeft().x(), anno.rect.topLeft().y(), 24, 24);
            painter.drawLine(anno.rect.topLeft().x() + 4, anno.rect.topLeft().y() + 6, anno.rect.topLeft().x() + 20, anno.rect.topLeft().y() + 6);
            painter.drawLine(anno.rect.topLeft().x() + 4, anno.rect.topLeft().y() + 12, anno.rect.topLeft().x() + 20, anno.rect.topLeft().y() + 12);
            painter.drawLine(anno.rect.topLeft().x() + 4, anno.rect.topLeft().y() + 18, anno.rect.topLeft().x() + 14, anno.rect.topLeft().y() + 18);
        } else if (anno.mode == ToolMode::AddTextBox) {
            painter.setPen(QPen(anno.color, 1));
            painter.drawRect(anno.rect);
            painter.drawText(anno.rect, Qt::AlignLeft | Qt::AlignTop, anno.text);
        } else if (anno.mode == ToolMode::Redact) {
            painter.setBrush(Qt::black);
            painter.setPen(Qt::NoPen);
            painter.drawRect(anno.rect);
        } else if (anno.mode == ToolMode::DrawRectangle) {
            painter.setBrush(Qt::NoBrush);
            painter.drawRect(anno.rect);
        } else if (anno.mode == ToolMode::DrawEllipse) {
            painter.setBrush(Qt::NoBrush);
            painter.drawEllipse(anno.rect);
        } else if (anno.mode == ToolMode::DrawLine) {
            painter.drawLine(anno.rect.topLeft(), anno.rect.bottomRight());
        } else if (anno.mode == ToolMode::DrawArrow) {
            QLineF line(anno.rect.topLeft(), anno.rect.bottomRight());
            painter.drawLine(line);
            qreal angle = std::atan2(-line.dy(), line.dx());
            QPointF arrowP1 = line.p2() - QPointF(std::cos(angle + M_PI / 6) * 12, -std::sin(angle + M_PI / 6) * 12);
            QPointF arrowP2 = line.p2() - QPointF(std::cos(angle - M_PI / 6) * 12, -std::sin(angle - M_PI / 6) * 12);
            painter.setBrush(anno.color);
            painter.drawPolygon(QPolygonF() << line.p2() << arrowP1 << arrowP2);
        } else if (anno.mode == ToolMode::AddSignature) {
            painter.setPen(QPen(Qt::darkBlue, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            for (int i = 0; i < anno.points.size() - 1; ++i) {
                painter.drawLine(anno.points[i], anno.points[i+1]);
            }
        }
        index++;
    }

    // Draw OCR Results overlay with culling (Fix 3)
    painter.setPen(QPen(QColor(0, 150, 0, 80), 1, Qt::DashLine));
    for (const auto& res : m_ocrResults) {
        if (!clipRect.intersects(res.boundingBox))
            continue;
        painter.drawRect(res.boundingBox);
        if (m_currentMode == ToolMode::SelectText) {
            // Highlight text areas when in selection mode
            painter.fillRect(res.boundingBox, QColor(0, 0, 255, 20));
        }
    }

    if (m_isDrawing) {
        painter.setPen(QPen(m_currentNote.color, m_currentNote.thickness, Qt::DashLine));
        if (m_currentMode == ToolMode::DrawFreehand || m_currentMode == ToolMode::AddSignature) {
            if (m_currentMode == ToolMode::AddSignature)
                painter.setPen(QPen(Qt::darkBlue, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));

            for (int i = 0; i < m_currentNote.points.size() - 1; ++i) {
                painter.drawLine(m_currentNote.points[i], m_currentNote.points[i+1]);
            }
        } else if (m_currentMode == ToolMode::Highlight) {
            QColor highColor = m_currentNote.color;
            highColor.setAlpha(50);
            painter.fillRect(m_currentNote.rect, highColor);
        } else if (m_currentMode == ToolMode::DrawRectangle) {
            painter.drawRect(m_currentNote.rect);
        } else if (m_currentMode == ToolMode::DrawEllipse) {
            painter.drawEllipse(m_currentNote.rect);
        } else if (m_currentMode == ToolMode::DrawLine || m_currentMode == ToolMode::DrawArrow) {
            painter.drawLine(m_currentNote.rect.topLeft(), m_currentNote.rect.bottomRight());
        }
    }

    // Draw image overlays in EditImage mode
    if (m_currentMode == ToolMode::EditImage) {
        for (const auto& img : m_imageOverlays) {
            QRectF r = img.placement;
            
            bool selected = (img.xobjectName == m_selectedImageName);
            
            // Draw border
            painter.setPen(QPen(selected ? Qt::blue : QColor(100, 100, 255, 120), selected ? 2 : 1, Qt::DashLine));
            painter.setBrush(selected ? QColor(0, 0, 255, 20) : Qt::NoBrush);
            painter.drawRect(r);
            
            if (selected) {
                // Draw 8 resize handles (4 corners + 4 edge midpoints)
                QColor handleColor(0, 120, 215);
                painter.setPen(Qt::NoPen);
                painter.setBrush(handleColor);
                constexpr double hs = 6.0; // handle half-size
                
                QPointF corners[] = {
                    r.topLeft(), r.topRight(), r.bottomRight(), r.bottomLeft()
                };
                for (auto& c : corners) {
                    painter.drawRect(QRectF(c.x() - hs, c.y() - hs, hs*2, hs*2));
                }
                
                QPointF edges[] = {
                    {r.center().x(), r.top()},
                    {r.right(), r.center().y()},
                    {r.center().x(), r.bottom()},
                    {r.left(), r.center().y()}
                };
                for (auto& e : edges) {
                    painter.drawRect(QRectF(e.x() - hs, e.y() - hs, hs*2, hs*2));
                }
                
                // Draw image name label
                painter.setPen(Qt::white);
                painter.setBrush(QColor(0, 0, 0, 180));
                QRectF labelRect(r.left(), r.top() - 20, 100, 18);
                painter.drawRect(labelRect);
                QFont f = painter.font();
                f.setPointSize(8);
                painter.setFont(f);
                painter.drawText(labelRect, Qt::AlignCenter, img.xobjectName);
            }
        }
    }
}

void AnnotationLayer::mousePressEvent(QMouseEvent *event)
{
    QPointF pos = event->position();
    if (m_rotation != 0) {
        QTransform trans;
        trans.translate(rect().center().x(), rect().center().y());
        trans.rotate(-m_rotation);
        trans.translate(-rect().center().x(), -rect().center().y());
        pos = trans.map(pos);
    }

    if (m_currentMode == ToolMode::EditImage) {
        // Check resize handles first (if an image is selected)
        if (!m_selectedImageName.isEmpty()) {
            for (const auto& img : m_imageOverlays) {
                if (img.xobjectName != m_selectedImageName) continue;
                QRectF r = img.placement;
                constexpr double hs = 8.0;
                
                QPointF handles[] = {
                    r.topLeft(), r.topRight(), r.bottomRight(), r.bottomLeft(),
                    {r.center().x(), r.top()}, {r.right(), r.center().y()},
                    {r.center().x(), r.bottom()}, {r.left(), r.center().y()}
                };
                for (int i = 0; i < 8; ++i) {
                    if (QRectF(handles[i].x()-hs, handles[i].y()-hs, hs*2, hs*2).contains(pos)) {
                        m_resizeHandle = i;
                        m_lastDragPos = pos;
                        return;
                    }
                }
            }
        }
        
        // Hit-test images for selection
        QString found;
        for (int i = m_imageOverlays.size() - 1; i >= 0; --i) {
            if (m_imageOverlays[i].placement.contains(pos)) {
                found = m_imageOverlays[i].xobjectName;
                break;
            }
        }
        m_selectedImageName = found;
        if (!found.isEmpty()) {
            for (const auto& img : m_imageOverlays) {
                if (img.xobjectName == found) {
                    emit imageSelected(found, img.placement);
                    m_originalImagePos = img.placement.topLeft();
                    break;
                }
            }
            m_isMoving = true;
            m_lastDragPos = pos;
        }
        update();
        return;
    }

    if (m_currentMode == ToolMode::EditObject) {
        int found = -1;
        // Hit test from top to bottom
        for (int i = m_annotations.size() - 1; i >= 0; --i) {
            const auto& anno = m_annotations[i];
            if (anno.mode == ToolMode::DrawFreehand || anno.mode == ToolMode::AddSignature) {
                // Simple distance check for points
                for (const auto& pt : anno.points) {
                    if (QLineF(pt, pos).length() < 10) {
                        found = i;
                        break;
                    }
                }
            } else {
                if (anno.rect.contains(pos)) {
                    found = i;
                }
            }
            if (found != -1) break;
        }
        setSelectedIndex(found);
        if (found != -1) {
            m_isMoving = true;
            m_lastDragPos = pos;
        }
        return;
    }

    if (m_currentMode == ToolMode::EditText) {
        int pageIndex = m_pageAtCallback ? m_pageAtCallback(event->pos()) : 0;
        emit textEditRequested(pageIndex, pos);
        return;
    }

    if (m_currentMode == ToolMode::DrawFreehand || m_currentMode == ToolMode::Highlight || 
        m_currentMode == ToolMode::AddTextBox || m_currentMode == ToolMode::AddComment ||
        m_currentMode == ToolMode::Redact || m_currentMode == ToolMode::AddSignature ||
        m_currentMode == ToolMode::DrawRectangle || m_currentMode == ToolMode::DrawEllipse ||
        m_currentMode == ToolMode::DrawLine || m_currentMode == ToolMode::DrawArrow) {
        m_isDrawing = true;
        m_currentNote.mode = m_currentMode;
        if (m_pageAtCallback) {
            m_currentNote.pageIndex = m_pageAtCallback(event->pos());
        } else {
            m_currentNote.pageIndex = -1;
        }
        m_currentNote.points.clear();
        m_currentNote.points.append(pos);
        m_currentNote.rect = QRectF(pos, pos);
        m_currentNote.color = m_selectedColor;
        m_currentNote.thickness = m_selectedThickness;
        if (m_currentMode == ToolMode::Highlight) {
            m_currentNote.color = Qt::yellow; // Default highlight
            m_currentNote.thickness = 10;
        }
        update();
    }
}

void AnnotationLayer::mouseMoveEvent(QMouseEvent *event)
{
    QPointF pos = event->position();
    if (m_rotation != 0) {
        QTransform trans;
        trans.translate(rect().center().x(), rect().center().y());
        trans.rotate(-m_rotation);
        trans.translate(-rect().center().x(), -rect().center().y());
        pos = trans.map(pos);
    }

    if (m_currentMode == ToolMode::EditImage && m_resizeHandle != -1 && !m_selectedImageName.isEmpty()) {
        QPointF delta = pos - m_lastDragPos;
        for (auto& img : m_imageOverlays) {
            if (img.xobjectName != m_selectedImageName) continue;
            QRectF& r = img.placement;
            switch (m_resizeHandle) {
                case 0: r.setTopLeft(r.topLeft() + delta); break;
                case 1: r.setTopRight(r.topRight() + delta); break;
                case 2: r.setBottomRight(r.bottomRight() + delta); break;
                case 3: r.setBottomLeft(r.bottomLeft() + delta); break;
                case 4: r.setTop(r.top() + delta.y()); break;
                case 5: r.setRight(r.right() + delta.x()); break;
                case 6: r.setBottom(r.bottom() + delta.y()); break;
                case 7: r.setLeft(r.left() + delta.x()); break;
            }
            break;
        }
        m_lastDragPos = pos;
        update();
        return;
    }

    if (m_currentMode == ToolMode::EditImage && m_isMoving && !m_selectedImageName.isEmpty()) {
        QPointF delta = pos - m_lastDragPos;
        for (auto& img : m_imageOverlays) {
            if (img.xobjectName == m_selectedImageName) {
                img.placement.translate(delta);
                break;
            }
        }
        m_lastDragPos = pos;
        update();
        return;
    }

    if (m_isMoving && m_selectedIndex != -1) {
        QPointF delta = pos - m_lastDragPos;
        auto& anno = m_annotations[m_selectedIndex];

        // Capture old bounds before move for dirty-rect (Fix 3)
        QRectF oldBounds;
        if (anno.mode == ToolMode::DrawFreehand || anno.mode == ToolMode::AddSignature) {
            for (const auto& pt : anno.points) {
                if (oldBounds.isNull()) oldBounds = QRectF(pt, pt);
                else oldBounds = oldBounds.united(QRectF(pt, pt));
            }
            for (auto& pt : anno.points) pt += delta;
        } else {
            oldBounds = anno.rect.normalized();
            anno.rect.translate(delta);
        }

        QRectF newBounds = (anno.mode == ToolMode::DrawFreehand || anno.mode == ToolMode::AddSignature)
            ? oldBounds.translated(delta) : anno.rect.normalized();
        QRectF dirty = oldBounds.united(newBounds).adjusted(-15, -15, 15, 15);

        m_lastDragPos = pos;
        update(dirty.toAlignedRect());
        return;
    }

    if (m_isDrawing) {
        QRectF oldBounds = m_currentNote.rect.normalized();
        if (m_currentMode == ToolMode::DrawFreehand || m_currentMode == ToolMode::AddSignature) {
            // For freehand, only the new segment needs repainting
            QPointF prevPt = m_currentNote.points.isEmpty() ? pos : m_currentNote.points.last();
            m_currentNote.points.append(pos);
            QRectF segDirty = QRectF(prevPt, pos).normalized().adjusted(-15, -15, 15, 15);
            update(segDirty.toAlignedRect());
        } else {
            m_currentNote.rect.setBottomRight(pos);
            QRectF dirty = oldBounds.united(m_currentNote.rect.normalized()).adjusted(-15, -15, 15, 15);
            update(dirty.toAlignedRect());
        }
    }
}

void AnnotationLayer::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_currentMode == ToolMode::EditImage) {
        if (m_resizeHandle != -1) {
            m_resizeHandle = -1;
            for (const auto& img : m_imageOverlays) {
                if (img.xobjectName == m_selectedImageName) {
                    emit imageResized(m_selectedImageName, img.placement.width(), img.placement.height());
                    break;
                }
            }
            return;
        }
        if (m_isMoving && !m_selectedImageName.isEmpty()) {
            m_isMoving = false;
            for (const auto& img : m_imageOverlays) {
                if (img.xobjectName == m_selectedImageName) {
                    QPointF totalDelta = img.placement.topLeft() - m_originalImagePos;
                    emit imageMoved(m_selectedImageName, totalDelta.x(), totalDelta.y());
                    break;
                }
            }
            return;
        }
    }

    if (m_isMoving) {
        m_isMoving = false;
        emit annotationsChanged();
        // Dirty-rect update for the moved annotation (Fix 3)
        if (m_selectedIndex >= 0 && m_selectedIndex < m_annotations.size()) {
            const auto &anno = m_annotations[m_selectedIndex];
            QRectF dirty = (anno.mode == ToolMode::DrawFreehand || anno.mode == ToolMode::AddSignature)
                ? QRectF() : anno.rect;
            if (anno.mode == ToolMode::DrawFreehand || anno.mode == ToolMode::AddSignature) {
                for (const auto &p : anno.points) {
                    if (dirty.isNull()) dirty = QRectF(p, p);
                    else dirty = dirty.united(QRectF(p, p));
                }
            }
            update(dirty.adjusted(-15, -15, 15, 15).toAlignedRect());
        } else {
            update();
        }
        return;
    }

    if (m_isDrawing) {
        m_isDrawing = false;
        m_annotations.append(m_currentNote);
        emit annotationsChanged();
        // Dirty-rect update for the newly finished annotation (Fix 3)
        QRectF dirty = m_currentNote.rect;
        if (m_currentNote.mode == ToolMode::DrawFreehand || m_currentNote.mode == ToolMode::AddSignature) {
            dirty = QRectF();
            for (const auto &p : m_currentNote.points) {
                if (dirty.isNull()) dirty = QRectF(p, p);
                else dirty = dirty.united(QRectF(p, p));
            }
        }
        update(dirty.adjusted(-15, -15, 15, 15).toAlignedRect());
    }
}
