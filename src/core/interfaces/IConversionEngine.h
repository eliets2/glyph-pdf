// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QString>
#include <QRectF>
#include <QVariantMap>

class IConversionEngine {
public:
    enum class TargetFormat { Word, Excel, Html, Image, Csv, OfficeToPdf, Text, PowerPoint, ImagesToPdf };
    // V03: `column` is the geometry-derived spreadsheet column (0-based) that
    // ConversionManager::Private::deriveColumns assigns from the retained
    // x-positions, consistent across all rows. Default 0 keeps single-run
    // lines and every consumer that ignores columns unchanged.
    struct TextElement { QString text; QRectF rect; double fontSize; QString fontName; int column = 0; };

    virtual ~IConversionEngine() = default;
    virtual bool convertTo(const QString &pdfPath, const QString &outputPath, TargetFormat format, const QVariantMap &options = {}) = 0;
protected:
    IConversionEngine() = default;
    IConversionEngine(const IConversionEngine&) = delete;
    IConversionEngine& operator=(const IConversionEngine&) = delete;
};
