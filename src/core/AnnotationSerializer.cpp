// SPDX-License-Identifier: Apache-2.0
#include "core/AnnotationSerializer.h"
#include <QBuffer>
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
        obj["djotSource"] = anno.djotSource;
        obj["thickness"] = anno.thickness;
        
        obj["id"] = anno.id;
        obj["parentId"] = anno.parentId;
        obj["author"] = anno.author;
        obj["creationDate"] = anno.creationDate;
        obj["reviewState"] = static_cast<int>(anno.reviewState);
        
        QJsonArray repliesArray;
        for (const QString &reply : anno.replies) {
            repliesArray.append(reply);
        }
        obj["replies"] = repliesArray;
        
        QJsonArray points;
        for (const auto &p : anno.points) {
            QJsonObject pt;
            pt["x"] = p.x();
            pt["y"] = p.y();
            points.append(pt);
        }
        obj["points"] = points;

        // §9.7 P0: cache signature-picker raster ink (typed/uploaded modes) in
        // the sidecar so the overlay survives an app restart before the PDF is
        // saved. PNG keeps it small; an oversized raster is skipped rather
        // than stalling autosave (the authoritative PDF embed still carries it).
        if (!anno.image.isNull()) {
            QBuffer pngBuf;
            pngBuf.open(QIODevice::WriteOnly);
            if (anno.image.save(&pngBuf, "PNG") && pngBuf.size() <= 4 * 1024 * 1024)
                obj["image_png"] = QString::fromLatin1(pngBuf.data().toBase64());
        }
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
        // §9.7 P0: the gate used to stop at EditImage, silently dropping every
        // ordinal added after it (Stamp, Callout, Crop, … and the new
        // signature-picker modes). The bound lives in PdfEnums.h
        // (kPersistedToolModeMax) so appending a mode updates one constant;
        // anything outside is a corrupt sidecar, not a mode.
        if (modeInt < static_cast<int>(ToolMode::HandTool) || modeInt > kPersistedToolModeMax) {
            qWarning() << "Skipping annotation with invalid ToolMode:" << modeInt;
            continue;
        }
        item.mode = static_cast<ToolMode>(modeInt);
        item.pageIndex = obj["pageIndex"].toInt();
        item.rect = QRectF(obj["rect_x"].toDouble(), obj["rect_y"].toDouble(), 
                           obj["rect_w"].toDouble(), obj["rect_h"].toDouble());
        item.color = QColor(obj["color"].toString());
        item.text = obj["text"].toString();
        item.djotSource = obj["djotSource"].toString();
        item.thickness = obj["thickness"].toInt();
        
        item.id = obj["id"].toString();
        item.parentId = obj["parentId"].toString();
        item.author = obj["author"].toString();
        item.creationDate = obj["creationDate"].toString();
        {
            int rsInt = obj["reviewState"].toInt(0);
            if (rsInt < 0 || rsInt > static_cast<int>(ReviewState::LastValue))
                rsInt = 0;
            item.reviewState = static_cast<ReviewState>(rsInt);
        }

        QJsonArray repliesArray = obj["replies"].toArray();
        for (int j = 0; j < repliesArray.size(); ++j) {
            item.replies.append(repliesArray[j].toString());
        }

        QJsonArray points = obj["points"].toArray();
        for (int j = 0; j < points.size(); ++j) {
            QJsonObject pt = points[j].toObject();
            item.points.append(QPointF(pt["x"].toDouble(), pt["y"].toDouble()));
        }

        // §9.7 P0: restore the cached signature raster (typed/uploaded modes).
        if (obj.contains("image_png")) {
            const QByteArray raw =
                QByteArray::fromBase64(obj["image_png"].toString().toLatin1());
            if (!raw.isEmpty())
                item.image = QImage::fromData(raw, "PNG");
        }
        items.append(item);
    }
    return items;
}
