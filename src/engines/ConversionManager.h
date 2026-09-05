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
    // Compile-time availability of the optional third-party OOXML libs (duckx /
    // OpenXLSX). Word/Excel export itself is ALWAYS available since §9.5 P0 — the
    // in-house writers produce real OOXML when these libs are absent.
    static bool hasNativeWordExport();
    static bool hasNativeExcelExport();
    // Which writer produced the last Word/Excel export (audit §9.16/§9.5 seam):
    //   Unknown      — no export run yet
    //   NativeOoxml  — a vendored third-party OOXML lib (duckx / OpenXLSX) wrote the file
    //   InHouseOoxml — GlyphPDF's built-in OOXML writer wrote the file (real OOXML;
    //                  §9.5 P0 — replaces the old HTML-as-.docx / CSV-as-.xlsx fallbacks)
    //   Fallback     — mislabeled non-OOXML bytes under an Office extension (the ONLY
    //                  state for which ConvertController shows its repair-prompt warning)
    enum class ExportEngine { Unknown, NativeOoxml, InHouseOoxml, Fallback };
    ExportEngine lastWordExportEngine() const { return m_lastWordEngine; }
    ExportEngine lastExcelExportEngine() const { return m_lastExcelEngine; }

private:
    bool exportToWord(const QString &outputPath, const QList<QList<TextElement>> &rows);
    bool exportToExcel(const QString &outputPath, const QList<QList<TextElement>> &rows);
    // §9.5 P0: in-house OOXML writers (libzip + QXmlStreamWriter, same idiom as the
    // PPTX writer). Used as the #else branch when HAS_DUCKX / HAS_OPENXLSX are
    // absent, so .docx/.xlsx output is never mislabeled HTML/CSV bytes.
    // docx = 3 parts: [Content_Types].xml, _rels/.rels, word/document.xml.
    // xlsx = 5 parts: the above plus xl/workbook.xml, xl/_rels/workbook.xml.rels,
    //         xl/worksheets/sheet1.xml (strings as t="inlineStr", no sharedStrings).
    bool exportToWordInHouse(const QString &outputPath, const QList<QList<TextElement>> &rows);
    bool exportToExcelInHouse(const QString &outputPath, const QList<QList<TextElement>> &rows);
    bool exportToHtml(const QString &pdfPath, const QString &outputPath);
    bool exportToText(const QString &pdfPath, const QString &outputPath);
    bool exportToPowerPoint(const QString &pdfPath, const QString &outputPath, const QVariantMap &options);
    bool exportToImage(const QString &pdfPath, const QString &outputPath, const QVariantMap &options);
    bool exportToCsv(const QString &outputPath, const QList<QList<TextElement>> &rows);

    class Private;
    std::unique_ptr<Private> d;
    ExportEngine m_lastWordEngine = ExportEngine::Unknown; ExportEngine m_lastExcelEngine = ExportEngine::Unknown;
};
