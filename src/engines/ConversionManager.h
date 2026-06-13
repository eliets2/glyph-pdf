// SPDX-License-Identifier: Apache-2.0
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

    // Office (.docx/.xlsx/.pptx/.odt/…) → PDF via a LibreOffice `soffice` subprocess.
    bool convertOfficeToPdf(const QString &officePath, const QString &outputPath,
                            int timeoutMs = 120000);

    // Runtime discovery of the LibreOffice `soffice` executable. Searches, in order:
    //   1. a portable copy bundled next to GlyphPDF.exe (libreoffice/program/soffice.exe)
    //   2. the PATH
    //   3. standard install locations (Program Files [(x86)])
    //   4. the Windows registry (App Paths / LibreOffice keys)
    // Returns an empty string if no converter is installed. Detection is done at
    // runtime — never baked at build time — so the shipped binary works on whatever
    // machine the user has, regardless of what the build machine had.
    static QString locateSoffice();

    // Convenience: true when an Office→PDF converter is available on this machine.
    static bool isOfficeImportAvailable() { return !locateSoffice().isEmpty(); }

private:
    bool exportToWord(const QString &outputPath, const QList<QList<TextElement>> &rows);
    bool exportToExcel(const QString &outputPath, const QList<QList<TextElement>> &rows);
    bool exportToHtml(const QString &pdfPath, const QString &outputPath);
    bool exportToText(const QString &pdfPath, const QString &outputPath);
    bool exportToPowerPoint(const QString &pdfPath, const QString &outputPath, const QVariantMap &options);
    bool exportToImage(const QString &pdfPath, const QString &outputPath, const QVariantMap &options);
    bool exportToCsv(const QString &outputPath, const QList<QList<TextElement>> &rows);

    class Private;
    std::unique_ptr<Private> d;
};
