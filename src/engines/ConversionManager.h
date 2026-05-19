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

    bool convertTo(const QString &pdfPath, const QString &outputPath, TargetFormat format) override;

private:
    bool exportToWord(const QString &outputPath, const QList<QList<TextElement>> &rows);
    bool exportToExcel(const QString &outputPath, const QList<QList<TextElement>> &rows);

    class Private;
    std::unique_ptr<Private> d;
};
