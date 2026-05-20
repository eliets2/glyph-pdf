#pragma once

#include <QString>
#include <QObject>
#include <QRectF>
#include <QList>
#include <memory>

#include "core/interfaces/IConversionEngine.h"

class ConversionManager final : public QObject, public IConversionEngine {
    Q_OBJECT
public:
    explicit ConversionManager(QObject *parent = nullptr);
    ~ConversionManager() override;

    bool convertTo(const QString &pdfPath, const QString &outputPath, TargetFormat format, const QVariantMap &options = {}) override;

private:
    bool exportToWord(const QString &outputPath, const QList<QList<TextElement>> &rows);
    bool exportToExcel(const QString &outputPath, const QList<QList<TextElement>> &rows);
    bool exportToHtml(const QString &pdfPath, const QString &outputPath);
    bool exportToImage(const QString &pdfPath, const QString &outputPath, const QVariantMap &options);
    bool exportToCsv(const QString &outputPath, const QList<QList<TextElement>> &rows);
    bool convertOfficeToPdf(const QString &officePath, const QString &outputPath);

    class Private;
    std::unique_ptr<Private> d;
};
