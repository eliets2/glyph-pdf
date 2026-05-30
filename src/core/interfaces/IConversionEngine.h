#pragma once
#include <QString>
#include <QRectF>
#include <QVariantMap>

class IConversionEngine {
public:
    enum class TargetFormat { Word, Excel, Html, Image, Csv, OfficeToPdf, Text, PowerPoint, ImagesToPdf };
    struct TextElement { QString text; QRectF rect; double fontSize; QString fontName; };

    virtual ~IConversionEngine() = default;
    virtual bool convertTo(const QString &pdfPath, const QString &outputPath, TargetFormat format, const QVariantMap &options = {}) = 0;
protected:
    IConversionEngine() = default;
    IConversionEngine(const IConversionEngine&) = delete;
    IConversionEngine& operator=(const IConversionEngine&) = delete;
};
