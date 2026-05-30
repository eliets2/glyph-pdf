#pragma once

#include <QString>
#include <QObject>
#include <QRectF>
#include <QList>
#include <memory>
#include <QPageSize>

#include "core/interfaces/IConversionEngine.h"

struct ImageImportOptions {
    int dpi = 150;
    bool fitToPage = true;
    QPageSize::PageSizeId pageSize = QPageSize::A4;
};

class ConversionManager final : public QObject, public IConversionEngine {
    Q_OBJECT
public:
    explicit ConversionManager(QObject *parent = nullptr);
    ~ConversionManager() override;

    bool convertTo(const QString &pdfPath, const QString &outputPath, TargetFormat format, const QVariantMap &options = {}) override;

    bool convertImagesToPdf(const QStringList &imagePaths, const QString &outputPath,
                            ImageImportOptions options = {});

private:
    bool exportToWord(const QString &outputPath, const QList<QList<TextElement>> &rows);
    bool exportToExcel(const QString &outputPath, const QList<QList<TextElement>> &rows);
    bool exportToHtml(const QString &pdfPath, const QString &outputPath);
    bool exportToText(const QString &pdfPath, const QString &outputPath);
    bool exportToPowerPoint(const QString &pdfPath, const QString &outputPath, const QVariantMap &options);
    bool exportToImage(const QString &pdfPath, const QString &outputPath, const QVariantMap &options);
    bool exportToCsv(const QString &outputPath, const QList<QList<TextElement>> &rows);
    bool convertOfficeToPdf(const QString &officePath, const QString &outputPath,
                            int timeoutMs = 120000);

    class Private;
    std::unique_ptr<Private> d;
};
