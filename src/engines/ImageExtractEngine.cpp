// SPDX-License-Identifier: Apache-2.0
// ImageExtractEngine.cpp — the PoDoFo-backed body of the §9.17 compose image
// inventory. Includes PoDoFo directly (the PdfPageOps isolation rule: this is
// an engines-side TU; UI callers include only ImageExtractEngine.h).
#include "ImageExtractEngine.h"

#include <QDebug>

#include <podofo/podofo.h>
#include <podofo/main/PdfMemDocument.h>
#include <podofo/main/PdfImage.h>
#include <podofo/main/PdfXObject.h>

namespace gp {

namespace {

// The same 10 000 px per-axis guard every image writer in the backend enforces
// (addImageWatermark, replaceImage). An inventory entry beyond it is reported
// with its dimensions but its pixels are NOT decoded — a hostile dimension
// pair must not become a hostile allocation.
constexpr int kMaxImageAxis = 10000;

QStringList readFilterNames(const PoDoFo::PdfObject* obj) {
    QStringList out;
    if (!obj) return out;
    auto* filter = obj->GetDictionary().FindKey("Filter");
    if (!filter) return out;
    if (filter->IsArray()) {
        for (const auto& item : filter->GetArray()) {
            if (item.IsName())
                out << QString::fromStdString(std::string(item.GetName().GetString()));
        }
    } else if (filter->IsName()) {
        out << QString::fromStdString(std::string(filter->GetName().GetString()));
    }
    return out;
}

QImage decodePixels(PoDoFo::PdfObject& obj, int width, int height) {
    if (width <= 0 || height <= 0 || width > kMaxImageAxis || height > kMaxImageAxis)
        return QImage();
    std::unique_ptr<PoDoFo::PdfXObject> xobj;
    if (!PoDoFo::PdfXObject::TryCreateFromObject(obj, xobj))
        return QImage();
    auto* image = dynamic_cast<PoDoFo::PdfImage*>(xobj.get());
    if (!image)
        return QImage();
    try {
        // RGBA keeps any /SMask soft transparency for the picker preview AND
        // for the placement (placeImageOnPage re-packs RGB + alpha plane).
        const PoDoFo::charbuff raw = image->GetDecodedCopy(PoDoFo::PdfPixelFormat::RGBA);
        const qsizetype rowBytes = static_cast<qsizetype>(width) * 4;
        if (static_cast<qsizetype>(raw.size()) < rowBytes * height)
            return QImage();
        QImage decoded(width, height, QImage::Format_RGBA8888);
        for (int y = 0; y < height; ++y)
            memcpy(decoded.scanLine(y), raw.data() + rowBytes * y, static_cast<size_t>(rowBytes));
        return decoded;
    } catch (const std::exception& e) {
        qWarning() << "ImageExtractEngine: decode of" << width << "x" << height
                   << "image failed:" << e.what();
        return QImage();
    } catch (...) {
        return QImage();
    }
}

} // namespace

QList<ComposeImageInfo> ImageExtractEngine::listPageImages(const QString& pdfPath, int pageIndex)
{
    QList<ComposeImageInfo> out;
    try {
        PoDoFo::PdfMemDocument doc;
        doc.Load(pdfPath.toUtf8().constData());
        auto& pages = doc.GetPages();
        if (pageIndex < 0 || static_cast<size_t>(pageIndex) >= pages.GetCount())
            return out;
        auto& page = pages.GetPageAt(pageIndex);
        auto resources = page.GetResources();
        auto* xobjDict = resources.GetDictionary().FindKey("XObject");
        if (!xobjDict || !xobjDict->IsDictionary())
            return out;

        for (auto& kv : xobjDict->GetDictionary()) {
            PoDoFo::PdfObject* obj = &kv.second;
            if (obj->IsReference())
                obj = &doc.GetObjects().MustGetObject(obj->GetReference());
            if (!obj || !obj->IsDictionary()) continue;
            auto* subtype = obj->GetDictionary().FindKey("Subtype");
            if (!subtype || !subtype->IsName()
                || subtype->GetName().GetString() != "Image")
                continue;

            ComposeImageInfo info;
            info.pageIndex = pageIndex;
            info.xobjectName = QString::fromStdString(std::string(kv.first.GetString()));
            auto* widthObj = obj->GetDictionary().FindKey("Width");
            auto* heightObj = obj->GetDictionary().FindKey("Height");
            info.widthPx = widthObj && widthObj->IsNumber()
                ? static_cast<int>(widthObj->GetNumber()) : 0;
            info.heightPx = heightObj && heightObj->IsNumber()
                ? static_cast<int>(heightObj->GetNumber()) : 0;
            info.filters = readFilterNames(obj);
            info.pixels = decodePixels(*obj, info.widthPx, info.heightPx);
            out.append(info);
        }
    } catch (const PoDoFo::PdfError& e) {
        qWarning() << "ImageExtractEngine: inventory of" << pdfPath
                   << "page" << pageIndex << "failed:" << e.what();
        return QList<ComposeImageInfo>();
    } catch (const std::exception& e) {
        qWarning() << "ImageExtractEngine: inventory of" << pdfPath
                   << "page" << pageIndex << "failed:" << e.what();
        return QList<ComposeImageInfo>();
    }
    return out;
}

} // namespace gp
