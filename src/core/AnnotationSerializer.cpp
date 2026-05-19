#include "core/AnnotationSerializer.h"
#include <QDebug>

QJsonDocument AnnotationSerializer::toJson(const QList<AnnotationItem>& items)
{
    QJsonArray array;
    for (const auto &anno : items) {
        QJsonObject obj;
        obj["mode"] = static_cast<int>(anno.mode);
        obj["pageIndex"] = anno.pageIndex;
        obj["rect_x"] = anno.rect.x();
        obj["rect_y"] = anno.rect.y();
        obj["rect_w"] = anno.rect.width();
        obj["rect_h"] = anno.rect.height();
        obj["color"] = anno.color.name(QColor::HexArgb);
        obj["text"] = anno.text;
        obj["thickness"] = anno.thickness;
        
        QJsonArray points;
        for (const auto &p : anno.points) {
            QJsonObject pt;
            pt["x"] = p.x();
            pt["y"] = p.y();
            points.append(pt);
        }
        obj["points"] = points;
        array.append(obj);
    }
    return QJsonDocument(array);
}

QList<AnnotationItem> AnnotationSerializer::fromJson(const QJsonDocument& doc)
{
    QList<AnnotationItem> items;
    QJsonArray array;
    if (doc.isArray()) {
        array = doc.array();
    } else if (doc.isObject()) {
        QJsonObject root = doc.object();
        array = root["annotations"].toArray();
    }

    for (int i = 0; i < array.size(); ++i) {
        QJsonObject obj = array[i].toObject();
        AnnotationItem item;
        int modeInt = obj["mode"].toInt();
        if (modeInt < static_cast<int>(ToolMode::HandTool) || modeInt > static_cast<int>(ToolMode::EditImage)) {
            qWarning() << "Skipping annotation with invalid ToolMode:" << modeInt;
            continue;
        }
        item.mode = static_cast<ToolMode>(modeInt);
        item.pageIndex = obj["pageIndex"].toInt();
        item.rect = QRectF(obj["rect_x"].toDouble(), obj["rect_y"].toDouble(), 
                           obj["rect_w"].toDouble(), obj["rect_h"].toDouble());
        item.color = QColor(obj["color"].toString());
        item.text = obj["text"].toString();
        item.thickness = obj["thickness"].toInt();

        QJsonArray points = obj["points"].toArray();
        for (int j = 0; j < points.size(); ++j) {
            QJsonObject pt = points[j].toObject();
            item.points.append(QPointF(pt["x"].toDouble(), pt["y"].toDouble()));
        }
        items.append(item);
    }
    return items;
}
