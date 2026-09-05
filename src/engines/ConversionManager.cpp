// SPDX-License-Identifier: Apache-2.0
#include "engines/ConversionManager.h"
#include <memory>
#include <podofo/podofo.h>
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <cmath>
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include "core/TempFileManager.h"

// Forward declarations or include headers if available
#ifdef HAS_OPENXLSX
#include <OpenXLSX.hpp>
#endif

#ifdef HAS_DUCKX
#include <duckx.hpp>
#endif

#include <QProcess>
#include <QImage>
#include <QPainter>
#include <QFileInfo>
#include <QDir>
#include <QPageSize>
#include <QStandardPaths>
#include <QCoreApplication>
#ifdef Q_OS_WIN
#include <QSettings>
#endif
#include <podofo/main/PdfPainter.h>
#include <podofo/main/PdfPage.h>
#include "engines/pdfium/PdfiumBackend.h"

class ConversionManager::Private {
public:
    PoDoFo::PdfMemDocument *document = nullptr;
    // R09 (F07): extraction goes through PDFium's decoded text path (via the
    // backend boundary) instead of interpreting Tj/TJ bytes directly —
    // subset-font glyph codes must never leak into Word/Excel/CSV/Text output.
    QList<ConversionManager::TextElement> extractTextFromPage(PdfiumBackend &backend, int pageIndex);
    // Deterministic ordering: cluster runs into visual lines, order lines
    // top-to-bottom with a strict-weak-ordering-safe sort. Never re-sorts runs
    // inside a line (keeps logical order for RTL/bidi content).
    static QList<QList<ConversionManager::TextElement>> clusterIntoRows(
        const QList<ConversionManager::TextElement> &elements);
};

ConversionManager::ConversionManager(QObject *parent)
    : QObject(parent), d(std::make_unique<Private>())
{
}

ConversionManager::~ConversionManager() = default;

// R10 (F08): truthful capability. Both writers exist unconditionally (vendored
// lib when compiled in, in-house OOXML otherwise), so Word/Excel export is
// always genuinely available — see the header contract and ExportEngine.
bool ConversionManager::hasNativeWordExport()
{
    return true;
}

bool ConversionManager::hasNativeExcelExport()
{
    return true;
}

bool ConversionManager::convertTo(const QString &pdfPath, const QString &outputPath, TargetFormat format, const QVariantMap &options)
{
    if (format == TargetFormat::OfficeToPdf) {
        return convertOfficeToPdf(pdfPath, outputPath);
    }
    
    if (format == TargetFormat::Image) {
        return exportToImage(pdfPath, outputPath, options);
    }

    if (format == TargetFormat::Html) {
        return exportToHtml(pdfPath, outputPath);
    }

    if (format == TargetFormat::Text) {
        return exportToText(pdfPath, outputPath);
    }

    if (format == TargetFormat::PowerPoint) {
        return exportToPowerPoint(pdfPath, outputPath, options);
    }

    try {
        // R09 (F07): a per-operation backend — the document/page/text handles
        // live inside this call and are never shared with the live viewer or
        // other threads. The load also validates the input BEFORE any output
        // file is opened or truncated (R10).
        PdfiumBackend backend;
        if (!backend.loadDocument(pdfPath)) {
            qWarning() << "Conversion: PDFium failed to load document:" << pdfPath;
            return false;
        }

        QList<QList<TextElement>> allRows;

        for (int i = 0; i < backend.pageCount(); ++i) {
            QList<TextElement> pageElements = d->extractTextFromPage(backend, i);
            allRows.append(Private::clusterIntoRows(pageElements));
        }

        if (format == TargetFormat::Word) {
            return exportToWord(outputPath, allRows);
        } else if (format == TargetFormat::Excel) {
            return exportToExcel(outputPath, allRows);
        } else if (format == TargetFormat::Csv) {
            return exportToCsv(outputPath, allRows);
        }

        return false;

    } catch (const PoDoFo::PdfError &e) {
        qWarning() << "PoDoFo error during conversion:" << e.what();
        return false;
    } catch (const std::exception &e) {
        qWarning() << "Exception during conversion:" << e.what();
        return false;
    } catch (...) {
        qWarning() << "Unknown error during conversion";
        return false;
    }
}

// R09 (F07): decoded text + baseline geometry straight from the backend's
// page-text-with-boxes method. Runs arrive in PDFium char order (content
// stream / logical order) with user-space rects normalized once by the
// backend; this mapping does no reordering of its own.
QList<ConversionManager::TextElement> ConversionManager::Private::extractTextFromPage(PdfiumBackend &backend, int pageIndex)
{
    QList<TextElement> elements;
    const QList<PdfiumBackend::TextRun> runs = backend.extractPageTextRuns(pageIndex);
    elements.reserve(runs.size());
    for (const PdfiumBackend::TextRun &run : runs) {
        if (run.text.isEmpty()) continue; // empty-element skipping (452bfa2 behavior kept)
        TextElement el;
        el.text = run.text;
        el.rect = run.rect;
        el.fontSize = run.fontSize;
        el.fontName = run.fontName;
        elements.append(el);
    }
    return elements;
}

// R09: deterministic ordering pipeline, replacing the previous
// "close enough in Y" comparator INSIDE std::sort — a pairwise,
// non-transitive predicate that violates the strict-weak-ordering contract
// (real UB / crash risk). Now:
//   1. runs arrive from the backend in extracted (logical) order;
//   2. cluster them into visual lines: a run joins the first line whose
//      baseline is within the documented tolerance (half the larger font
//      size, 1pt floor) of the run's baseline; otherwise it starts a new line;
//   3. order LINES top-to-bottom with std::stable_sort on the pure numeric
//      line baseline (descending Y) — exact numeric keys, valid strict weak
//      ordering, and stable order breaks any exact tie deterministically;
//   4. runs within a line KEEP extracted order — never re-sorted by X, which
//      preserves the Unicode logical order PDFium already computed for
//      RTL/bidi/mixed-direction content (x-order alone is insufficient there).
QList<QList<ConversionManager::TextElement>> ConversionManager::Private::clusterIntoRows(
    const QList<ConversionManager::TextElement> &elements)
{
    struct LineGroup {
        double baselineY;
        double maxFont;
        QList<TextElement> els;
    };
    QList<LineGroup> groups;
    for (const TextElement &el : elements) {
        bool placed = false;
        for (LineGroup &g : groups) {
            const double tol = qMax(1.0, 0.5 * qMax(el.fontSize, g.maxFont));
            if (std::fabs(el.rect.y() - g.baselineY) <= tol) {
                g.els.append(el);
                g.maxFont = qMax(g.maxFont, el.fontSize);
                placed = true;
                break;
            }
        }
        if (!placed) {
            groups.append({el.rect.y(), el.fontSize, {el}});
        }
    }
    // Lines only, well separated by construction: exact numeric keys are a
    // valid strict weak ordering. stable_sort = deterministic tie order.
    std::stable_sort(groups.begin(), groups.end(),
                     [](const LineGroup &a, const LineGroup &b) {
                         return a.baselineY > b.baselineY;
                     });
    QList<QList<TextElement>> rows;
    rows.reserve(groups.size());
    for (const LineGroup &g : groups)
        rows.append(g.els);
    return rows;
}

bool ConversionManager::exportToWord(const QString &outputPath, const QList<QList<TextElement>> &rows)
{
#ifdef HAS_DUCKX
    m_lastWordEngine = ExportEngine::NativeOoxml;
    duckx::Document doc(outputPath.toStdString());
    doc.open();
    auto p = doc.append_paragraph();
    
    for (const auto &row : rows) {
        QString line;
        for (const auto &el : row) {
            line += el.text + " ";
        }
        p.add_run(line.toStdString());
        p = doc.append_paragraph();
    }
    doc.save();
    return QFileInfo(outputPath).size() > 0;
#else
    // §9.5 P0: no duckx in this build — produce REAL OOXML in-house instead of
    // the old mislabeled HTML-as-.docx fallback (audit §9.5, research Lane C).
    return exportToWordInHouse(outputPath, rows);
#endif
}

bool ConversionManager::exportToExcel(const QString &outputPath, const QList<QList<TextElement>> &rows)
{
#ifdef HAS_OPENXLSX
    m_lastExcelEngine = ExportEngine::NativeOoxml;
    OpenXLSX::XLDocument doc;
    doc.create(outputPath.toStdString());
    auto wks = doc.workbook().worksheet("Sheet1");
    
    int rowIdx = 1;
    for (const auto &row : rows) {
        int colIdx = 1;
        for (const auto &el : row) {
            wks.cell(OpenXLSX::XLCellReference(rowIdx, colIdx)).value() = el.text.toStdString();
            colIdx++;
        }
        rowIdx++;
    }
    doc.save();
    return QFileInfo(outputPath).size() > 0;
#else
    // §9.5 P0: no OpenXLSX in this build — produce REAL OOXML in-house instead
    // of the old CSV-under-.xlsx fallback (audit §9.5, research Lane C).
    // (Plain CSV stays available as its own honestly-labeled TargetFormat::Csv.)
    return exportToExcelInHouse(outputPath, rows);
#endif
}

bool ConversionManager::exportToHtml(const QString &pdfPath, const QString &outputPath) {
    // PDFium text extraction + positional CSS layout
#ifdef HAS_PDFIUM
    // Simple wrapper logic since PdfiumBackend doesn't expose char-by-char bounds easily without rewriting it.
    // However, we are asked to use PDFium for HTML text extraction. 
    // FPDF API requires init. Assuming initialized by PdfiumBackend.
    PdfiumBackend backend;
    if (!backend.loadDocument(pdfPath)) return false;

    QFile file(outputPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    QTextStream out(&file);

    out << "<!DOCTYPE html><html><head><meta charset=\"utf-8\"><style>"
        << "body { position: relative; margin: 0; padding: 0; background: #ccc; }"
        << ".page { position: relative; background: white; margin: 10px auto; overflow: hidden; box-shadow: 0 0 5px rgba(0,0,0,0.3); }"
        << ".text { position: absolute; white-space: pre; transform-origin: top left; }"
        << "</style></head><body>\n";

    for (int i = 0; i < backend.pageCount(); ++i) {
        QSizeF size = backend.pageSize(i);
        out << QString("<div class=\"page\" style=\"width: %1pt; height: %2pt;\">\n").arg(size.width()).arg(size.height());

        // R09 (F07): decoded text with real glyph geometry from the same
        // backend instance (no second, raw-byte extraction pass).
        const QList<TextElement> elements = d->extractTextFromPage(backend, i);
        for (const auto &el : elements) {
            // PDF coordinates: Y is from bottom. HTML Y is from top.
            // el.rect.y() is the run's baseline origin; the font size lifts
            // the box to its top, matching the pre-R09 anchor contract.
            double htmlY = size.height() - el.rect.y() - el.fontSize;
            out << QString("<div class=\"text\" style=\"left: %1pt; top: %2pt; font-size: %3pt; font-family: '%4';\">%5</div>\n")
                       .arg(el.rect.x()).arg(htmlY).arg(el.fontSize).arg(el.fontName).arg(el.text.toHtmlEscaped());
        }

        out << "</div>\n";
    }
    out << "</body></html>\n";
    file.close();
    return QFileInfo(outputPath).size() > 0;
#else
    return false;
#endif
}

bool ConversionManager::exportToImage(const QString &pdfPath, const QString &outputPath, const QVariantMap &options) {
    PdfiumBackend backend;
    if (!backend.loadDocument(pdfPath)) return false;

    int dpi = options.value("dpi", 150).toInt();
    int page = options.value("page", 0).toInt(); // 0 means all, but output path needs formatting
    QString format = options.value("format", "PNG").toString(); // PNG, JPEG, TIFF

    if (options.contains("page") && page >= 0 && page < backend.pageCount()) {
        QImage img = backend.renderPage(page, dpi);
        bool saved = img.save(outputPath, format.toUtf8().constData());
        return saved && QFileInfo(outputPath).size() > 0;
    } else {
        // Render all pages to separate files, assuming outputPath contains %1 for page number
        bool ok = true;
        for (int i = 0; i < backend.pageCount(); ++i) {
            QImage img = backend.renderPage(i, dpi);
            QString path = outputPath;
            if (path.contains("%1")) path = path.arg(i + 1);
            else path = path + QString("_page%1").arg(i + 1);
            bool saved = img.save(path, format.toUtf8().constData());
            ok &= (saved && QFileInfo(path).size() > 0);
        }
        return ok;
    }
}

bool ConversionManager::exportToCsv(const QString &outputPath, const QList<QList<TextElement>> &rows) {
    QFile file(outputPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    QTextStream out(&file);
    for (const auto &row : rows) {
        QStringList line;
        for (const auto &el : row) {
            QString escaped = el.text;
            escaped.replace("\"", "\"\"");
            line << "\"" + escaped + "\"";
        }
        out << line.join(",") << "\n";
    }
    file.close();
    return QFileInfo(outputPath).size() > 0;
}

QString ConversionManager::locateSoffice()
{
    // 1. Portable LibreOffice bundled alongside the application (if a future build
    //    ships one). Checked first so a bundled copy always wins over a system one.
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList bundled = {
        appDir + "/libreoffice/program/soffice.exe",
        appDir + "/libreoffice/program/soffice",
    };
    for (const QString &p : bundled) {
        if (QFileInfo::exists(p))
            return QDir::toNativeSeparators(p);
    }

    // 2. On the PATH (covers MSYS2 ucrt64 builds and user-modified PATHs).
    const QString onPath = QStandardPaths::findExecutable("soffice");
    if (!onPath.isEmpty())
        return QDir::toNativeSeparators(onPath);

    // 3. Standard install locations.
    const QStringList standard = {
        "C:/Program Files/LibreOffice/program/soffice.exe",
        "C:/Program Files (x86)/LibreOffice/program/soffice.exe",
        "/usr/bin/soffice",                                   // Linux
        "/Applications/LibreOffice.app/Contents/MacOS/soffice" // macOS
    };
    for (const QString &p : standard) {
        if (QFileInfo::exists(p))
            return QDir::toNativeSeparators(p);
    }

#ifdef Q_OS_WIN
    // 4. Windows registry — App Paths gives the install dir even for non-default
    //    locations chosen by the user during a LibreOffice install.
    const QStringList regKeys = {
        "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\soffice.exe",
        "HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\soffice.exe",
    };
    for (const QString &key : regKeys) {
        QSettings reg(key, QSettings::NativeFormat);
        const QString path = reg.value("Default").toString().remove('"');
        if (!path.isEmpty() && QFileInfo::exists(path))
            return QDir::toNativeSeparators(path);
    }
#endif

    return QString(); // No converter found — caller degrades gracefully.
}

bool ConversionManager::convertOfficeToPdf(const QString &officePath, const QString &outputPath,
                                            int timeoutMs)
{
    const QString sofficePath = locateSoffice();
    if (sofficePath.isEmpty()) {
        qWarning() << "convertOfficeToPdf: no LibreOffice/soffice converter found on this machine";
        return false;
    }

    // Validate extension: supported Office input formats
    static const QStringList supportedExts = {
        "docx", "doc", "xlsx", "xls", "pptx", "ppt",
        "odt", "ods", "odp", "rtf", "csv", "txt"
    };
    QFileInfo inInfo(officePath);
    if (!inInfo.exists()) {
        qWarning() << "convertOfficeToPdf: input file does not exist:" << officePath;
        return false;
    }
    if (!supportedExts.contains(inInfo.suffix().toLower())) {
        qWarning() << "convertOfficeToPdf: unsupported input format:" << inInfo.suffix();
        return false;
    }

    // Remove blanket pre-kill to avoid destroying user's unsaved work.
    // Instead, launch soffice with a private profile so it doesn't wait on a shared lock.
    QString profileDir = TempFileManager::instance().createTempDir("glyphpdf-soffice");
    // Ensure path uses forward slashes for the file:/// URI
    QString profileUri = "file:///" + QDir::toNativeSeparators(profileDir).replace("\\", "/");

    QFileInfo outInfo(outputPath);
    const QString outDir = outInfo.absolutePath();
    QDir().mkpath(outDir);

    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start(sofficePath, {
        "--env:UserInstallation=" + profileUri,
        "--headless",
        "--convert-to", "pdf:writer_pdf_Export",
        "--outdir", outDir,
        officePath
    });

    if (!process.waitForStarted(5000)) {
        qWarning() << "convertOfficeToPdf: soffice failed to start:" << process.errorString();
        return false;
    }

    const bool finished = process.waitForFinished(timeoutMs);
    if (!finished) {
        // Timeout — kill the entire process tree on Windows
        const qint64 pid = process.processId();
        qWarning() << "convertOfficeToPdf: timeout after" << timeoutMs << "ms; killing PID" << pid;
        process.kill();
#ifdef Q_OS_WIN
        QProcess::execute("taskkill", {"/F", "/T", "/PID", QString::number(pid)});
#endif
        return false;
    }

    if (process.exitCode() != 0) {
        qWarning() << "convertOfficeToPdf: soffice exited with code" << process.exitCode()
                   << process.readAll();
        return false;
    }

    // LibreOffice writes <basename>.pdf into outDir; rename to caller's outputPath if different.
    const QString expectedOut = QDir(outDir).filePath(inInfo.completeBaseName() + ".pdf");
    if (QFileInfo(expectedOut).canonicalFilePath() != QFileInfo(outputPath).canonicalFilePath()) {
        QFile::remove(outputPath);
        if (!QFile::rename(expectedOut, outputPath)) {
            qWarning() << "convertOfficeToPdf: could not rename" << expectedOut << "to" << outputPath;
            return false;
        }
    }

    if (!QFileInfo(outputPath).exists() || QFileInfo(outputPath).size() == 0) {
        qWarning() << "convertOfficeToPdf: output PDF is empty or missing:" << outputPath;
        return false;
    }
    return true;
}
#include <zip.h>
#include <QXmlStreamWriter>
#include <QBuffer>

bool ConversionManager::exportToText(const QString &pdfPath, const QString &outputPath) {
    try {
        // R09 (F07): PDFium-decoded text via a per-operation backend; the
        // document is validated BEFORE the output file is opened (R10).
        PdfiumBackend backend;
        if (!backend.loadDocument(pdfPath)) return false;

        QFile file(outputPath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
        QTextStream out(&file);

        for (int i = 0; i < backend.pageCount(); ++i) {
            QList<TextElement> elements = d->extractTextFromPage(backend, i);
            const QList<QList<TextElement>> rows = Private::clusterIntoRows(elements);
            for (const QList<TextElement> &row : rows) {
                QStringList parts;
                for (const auto &el : row) {
                    if (!el.text.isEmpty()) parts << el.text;
                }
                if (!parts.isEmpty()) out << parts.join(QLatin1Char(' ')) << "\n";
            }
            out << "\n";
        }
        file.close();
        return QFileInfo(outputPath).size() > 0;
    } catch (...) {
        return false;
    }
}

static void addZipFile(zip_t* za, const char* name, const QByteArray& data) {
    // Genuine fix: libzip reads the source buffer lazily at zip_close(). With
    // freep=0 the caller's QByteArray had to outlive that call — loop-local
    // buffers (slide images/XMLs) were freed first, producing corrupt
    // archives under memory pressure (flaky PPTX tests). Copy into a
    // malloc'd buffer and let libzip own/free it.
    const qint64 size = data.size();
    char* copy = static_cast<char*>(malloc(size > 0 ? static_cast<size_t>(size) : 1));
    if (!copy) return;
    if (size > 0) memcpy(copy, data.constData(), static_cast<size_t>(size));
    zip_source_t* source = zip_source_buffer(za, copy, size, 1);
    if (!source) {
        free(copy);
        return;
    }
    if (zip_file_add(za, name, source, ZIP_FL_OVERWRITE | ZIP_FL_ENC_UTF_8) < 0) {
        zip_source_free(source); // frees our malloc'd buffer too
    }
}

// ── §9.5 P0: in-house OOXML writers (Word/.docx, Excel/.xlsx) ───────────────
// Same plumbing as the PPTX writer above: libzip for the package, QXmlStreamWriter
// for the parts, canonical http://schemas.openxmlformats.org/... URIs. They exist
// so a build without the optional duckx/OpenXLSX libs still ships real OOXML
// under the .docx/.xlsx extensions instead of mislabeled HTML/CSV bytes.
// Corruption checklist (each item pinned by tests/TestExportPathBadge.cpp):
//   1. [Content_Types].xml: Default for rels+xml, Override per written part.
//   2. XML-escape all text (QXmlStreamWriter) + strip C0 control chars
//      (invalid in XML 1.0) except the legal whitespace \t \n \r.
//   3. Relationship rIds present and monotonic; every r:id resolves — no dangles.
//   4. xlsx rows/cells carry present, monotonic A1-style r="..." references.
//   5. w:t carries xml:space="preserve" (leading/trailing spaces survive).
//   6. zip entry names with forward slashes, no duplicates (libzip + literal names).

// XML 1.0 forbids most C0 control characters anywhere in a document. Strip every
// C0 control except the three legal whitespace chars (\t \n \r); also drop DEL.
// QXmlStreamWriter would otherwise happily embed raw control bytes, and Word /
// Excel would then demand a repair.
static QString sanitizeTextForXml(const QString &raw)
{
    QString out;
    out.reserve(raw.size());
    for (const QChar &ch : raw) {
        const char16_t c = ch.unicode();
        if (c == 0x09 || c == 0x0A || c == 0x0D) { out += ch; continue; } // legal XML whitespace
        if (c < 0x20)  continue; // other C0 controls: invalid in XML 1.0
        if (c == 0x7F) continue; // DEL: never meaningful in extracted PDF text
        out += ch;
    }
    return out;
}

// 1-based column index -> spreadsheet column name: 1..26 -> A..Z, 27 -> AA, ...
static QString xlsxColumnName(int col)
{
    QString name;
    while (col > 0) {
        const int rem = (col - 1) % 26;
        name.prepend(QChar(u'A' + rem));
        col = (col - 1) / 26;
    }
    return name;
}

// Minimal WordprocessingML package: [Content_Types].xml + _rels/.rels +
// word/document.xml. styles.xml / settings.xml / numbering are optional per the
// Open XML spec (MS Learn "Structure of a WordprocessingML document") and are
// intentionally skipped — Word opens the result without a repair prompt.
bool ConversionManager::exportToWordInHouse(const QString &outputPath, const QList<QList<TextElement>> &rows)
{
    m_lastWordEngine = ExportEngine::InHouseOoxml;

    int errorp = 0;
    zip_t *za = zip_open(outputPath.toUtf8().constData(), ZIP_CREATE | ZIP_TRUNCATE, &errorp);
    if (!za) return false;

    // 1. [Content_Types].xml — Defaults (rels, xml) + Override for the document part.
    QByteArray contentTypes;
    {
        QXmlStreamWriter xml(&contentTypes);
        xml.writeStartDocument();
        xml.writeStartElement("Types");
        xml.writeAttribute("xmlns", "http://schemas.openxmlformats.org/package/2006/content-types");
        xml.writeEmptyElement("Default"); xml.writeAttribute("Extension", "rels"); xml.writeAttribute("ContentType", "application/vnd.openxmlformats-package.relationships+xml");
        xml.writeEmptyElement("Default"); xml.writeAttribute("Extension", "xml"); xml.writeAttribute("ContentType", "application/xml");
        xml.writeEmptyElement("Override"); xml.writeAttribute("PartName", "/word/document.xml"); xml.writeAttribute("ContentType", "application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml");
        xml.writeEndElement();
        xml.writeEndDocument();
    }
    addZipFile(za, "[Content_Types].xml", contentTypes);

    // 2. _rels/.rels — the officeDocument relationship pointing at word/document.xml.
    QByteArray rootRels;
    {
        QXmlStreamWriter xml(&rootRels);
        xml.writeStartDocument();
        xml.writeStartElement("Relationships");
        xml.writeAttribute("xmlns", "http://schemas.openxmlformats.org/package/2006/relationships");
        xml.writeEmptyElement("Relationship");
        xml.writeAttribute("Id", "rId1");
        xml.writeAttribute("Type", "http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument");
        xml.writeAttribute("Target", "word/document.xml");
        xml.writeEndElement();
        xml.writeEndDocument();
    }
    addZipFile(za, "_rels/.rels", rootRels);

    // 3. word/document.xml — one w:p paragraph per extracted row (same content
    //    contract the HTML fallback had: row elements joined with a space).
    QByteArray documentXml;
    {
        QXmlStreamWriter xml(&documentXml);
        xml.writeStartDocument();
        xml.writeStartElement("w:document");
        xml.writeAttribute("xmlns:w", "http://schemas.openxmlformats.org/wordprocessingml/2006/main");
        xml.writeStartElement("w:body");
        for (const auto &row : rows) {
            QStringList parts;
            for (const auto &el : row) {
                const QString text = sanitizeTextForXml(el.text);
                if (!text.isEmpty()) parts << text;
            }
            xml.writeStartElement("w:p");
            xml.writeStartElement("w:r");
            xml.writeStartElement("w:t");
            xml.writeAttribute("xml:space", "preserve");
            xml.writeCharacters(parts.join(QLatin1Char(' ')));
            xml.writeEndElement(); // w:t
            xml.writeEndElement(); // w:r
            xml.writeEndElement(); // w:p
        }
        xml.writeEndElement(); // w:body
        xml.writeEndElement(); // w:document
        xml.writeEndDocument();
    }
    addZipFile(za, "word/document.xml", documentXml);

    if (zip_close(za) != 0) return false;
    return QFileInfo(outputPath).size() > 0;
}

// Minimal SpreadsheetML package: [Content_Types].xml + _rels/.rels +
// xl/workbook.xml + xl/_rels/workbook.xml.rels + xl/worksheets/sheet1.xml.
// Strings are written as t="inlineStr" cells (<is><t>) — a first-class cell
// type per ECMA-376 — which eliminates sharedStrings.xml entirely (and with it
// the classic count/uniqueCount corruption pitfall).
bool ConversionManager::exportToExcelInHouse(const QString &outputPath, const QList<QList<TextElement>> &rows)
{
    m_lastExcelEngine = ExportEngine::InHouseOoxml;

    int errorp = 0;
    zip_t *za = zip_open(outputPath.toUtf8().constData(), ZIP_CREATE | ZIP_TRUNCATE, &errorp);
    if (!za) return false;

    // 1. [Content_Types].xml — Defaults (rels, xml) + Overrides (workbook, sheet).
    QByteArray contentTypes;
    {
        QXmlStreamWriter xml(&contentTypes);
        xml.writeStartDocument();
        xml.writeStartElement("Types");
        xml.writeAttribute("xmlns", "http://schemas.openxmlformats.org/package/2006/content-types");
        xml.writeEmptyElement("Default"); xml.writeAttribute("Extension", "rels"); xml.writeAttribute("ContentType", "application/vnd.openxmlformats-package.relationships+xml");
        xml.writeEmptyElement("Default"); xml.writeAttribute("Extension", "xml"); xml.writeAttribute("ContentType", "application/xml");
        xml.writeEmptyElement("Override"); xml.writeAttribute("PartName", "/xl/workbook.xml"); xml.writeAttribute("ContentType", "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml");
        xml.writeEmptyElement("Override"); xml.writeAttribute("PartName", "/xl/worksheets/sheet1.xml"); xml.writeAttribute("ContentType", "application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml");
        xml.writeEndElement();
        xml.writeEndDocument();
    }
    addZipFile(za, "[Content_Types].xml", contentTypes);

    // 2. _rels/.rels — the officeDocument relationship pointing at xl/workbook.xml.
    QByteArray rootRels;
    {
        QXmlStreamWriter xml(&rootRels);
        xml.writeStartDocument();
        xml.writeStartElement("Relationships");
        xml.writeAttribute("xmlns", "http://schemas.openxmlformats.org/package/2006/relationships");
        xml.writeEmptyElement("Relationship");
        xml.writeAttribute("Id", "rId1");
        xml.writeAttribute("Type", "http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument");
        xml.writeAttribute("Target", "xl/workbook.xml");
        xml.writeEndElement();
        xml.writeEndDocument();
    }
    addZipFile(za, "_rels/.rels", rootRels);

    // 3. xl/workbook.xml — one sheet, r:id="rId1" (resolved by the part below).
    QByteArray workbookXml;
    {
        QXmlStreamWriter xml(&workbookXml);
        xml.writeStartDocument();
        xml.writeStartElement("workbook");
        xml.writeAttribute("xmlns", "http://schemas.openxmlformats.org/spreadsheetml/2006/main");
        xml.writeAttribute("xmlns:r", "http://schemas.openxmlformats.org/officeDocument/2006/relationships");
        xml.writeStartElement("sheets");
        xml.writeEmptyElement("sheet");
        xml.writeAttribute("name", "Sheet1");
        xml.writeAttribute("sheetId", "1");
        xml.writeAttribute("r:id", "rId1");
        xml.writeEndElement(); // sheets
        xml.writeEndElement(); // workbook
        xml.writeEndDocument();
    }
    addZipFile(za, "xl/workbook.xml", workbookXml);

    // 4. xl/_rels/workbook.xml.rels — rId1 -> worksheets/sheet1.xml.
    QByteArray workbookRels;
    {
        QXmlStreamWriter xml(&workbookRels);
        xml.writeStartDocument();
        xml.writeStartElement("Relationships");
        xml.writeAttribute("xmlns", "http://schemas.openxmlformats.org/package/2006/relationships");
        xml.writeEmptyElement("Relationship");
        xml.writeAttribute("Id", "rId1");
        xml.writeAttribute("Type", "http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet");
        xml.writeAttribute("Target", "worksheets/sheet1.xml");
        xml.writeEndElement();
        xml.writeEndDocument();
    }
    addZipFile(za, "xl/_rels/workbook.xml.rels", workbookRels);

    // 5. xl/worksheets/sheet1.xml — rows/cells in list order, so the r="A1"
    //    references are present and monotonically increasing by construction.
    QByteArray sheetXml;
    {
        QXmlStreamWriter xml(&sheetXml);
        xml.writeStartDocument();
        xml.writeStartElement("worksheet");
        xml.writeAttribute("xmlns", "http://schemas.openxmlformats.org/spreadsheetml/2006/main");
        xml.writeStartElement("sheetData");
        int rowIdx = 0;
        for (const auto &row : rows) {
            ++rowIdx;
            xml.writeStartElement("row");
            xml.writeAttribute("r", QString::number(rowIdx));
            // §9.5: extraction emits positionally-grouped elements, many of
            // them empty — writing them would pad the sheet with hundreds of
            // blank inlineStr cells (and push the first real cell off A1).
            // Empty elements are skipped; column letters stay monotonic.
            int colIdx = 0;
            for (const auto &el : row) {
                const QString text = sanitizeTextForXml(el.text);
                if (text.isEmpty()) continue;
                ++colIdx;
                xml.writeStartElement("c");
                xml.writeAttribute("r", xlsxColumnName(colIdx) + QString::number(rowIdx));
                xml.writeAttribute("t", "inlineStr");
                xml.writeStartElement("is");
                xml.writeStartElement("t");
                xml.writeAttribute("xml:space", "preserve");
                xml.writeCharacters(text);
                xml.writeEndElement(); // t
                xml.writeEndElement(); // is
                xml.writeEndElement(); // c
            }
            xml.writeEndElement(); // row
        }
        xml.writeEndElement(); // sheetData
        xml.writeEndElement(); // worksheet
        xml.writeEndDocument();
    }
    addZipFile(za, "xl/worksheets/sheet1.xml", sheetXml);

    if (zip_close(za) != 0) return false;
    return QFileInfo(outputPath).size() > 0;
}

bool ConversionManager::exportToPowerPoint(const QString &pdfPath, const QString &outputPath, const QVariantMap &options) {
    PdfiumBackend backend;
    if (!backend.loadDocument(pdfPath)) return false;

    int errorp = 0;
    zip_t *za = zip_open(outputPath.toUtf8().constData(), ZIP_CREATE | ZIP_TRUNCATE, &errorp);
    if (!za) return false;

    int pageCount = backend.pageCount();
    int renderDpi = options.value("dpi", 150).toInt();

    // Conversion factor: 1 PDF point = 12700 EMU
    constexpr double PT_TO_EMU = 12700.0;

    // 1. [Content_Types].xml
    QByteArray contentTypes;
    {
        QXmlStreamWriter xml(&contentTypes);
        xml.writeStartDocument();
        xml.writeStartElement("Types");
        xml.writeAttribute("xmlns", "http://schemas.openxmlformats.org/package/2006/content-types");
        xml.writeEmptyElement("Default"); xml.writeAttribute("Extension", "jpeg"); xml.writeAttribute("ContentType", "image/jpeg");
        xml.writeEmptyElement("Default"); xml.writeAttribute("Extension", "rels"); xml.writeAttribute("ContentType", "application/vnd.openxmlformats-package.relationships+xml");
        xml.writeEmptyElement("Default"); xml.writeAttribute("Extension", "xml"); xml.writeAttribute("ContentType", "application/xml");
        xml.writeEmptyElement("Override"); xml.writeAttribute("PartName", "/ppt/presentation.xml"); xml.writeAttribute("ContentType", "application/vnd.openxmlformats-officedocument.presentationml.presentation.main+xml");
        xml.writeEmptyElement("Override"); xml.writeAttribute("PartName", "/ppt/slideLayouts/slideLayout1.xml"); xml.writeAttribute("ContentType", "application/vnd.openxmlformats-officedocument.presentationml.slideLayout+xml");
        xml.writeEmptyElement("Override"); xml.writeAttribute("PartName", "/ppt/slideMasters/slideMaster1.xml"); xml.writeAttribute("ContentType", "application/vnd.openxmlformats-officedocument.presentationml.slideMaster+xml");
        for (int i = 0; i < pageCount; ++i) {
            xml.writeEmptyElement("Override"); xml.writeAttribute("PartName", QString("/ppt/slides/slide%1.xml").arg(i+1)); xml.writeAttribute("ContentType", "application/vnd.openxmlformats-officedocument.presentationml.slide+xml");
        }
        xml.writeEndElement();
        xml.writeEndDocument();
    }
    addZipFile(za, "[Content_Types].xml", contentTypes);

    // 2. _rels/.rels
    QByteArray rootRels;
    {
        QXmlStreamWriter xml(&rootRels);
        xml.writeStartDocument();
        xml.writeStartElement("Relationships");
        xml.writeAttribute("xmlns", "http://schemas.openxmlformats.org/package/2006/relationships");
        xml.writeEmptyElement("Relationship"); xml.writeAttribute("Id", "rId1"); xml.writeAttribute("Type", "http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument"); xml.writeAttribute("Target", "ppt/presentation.xml");
        xml.writeEndElement();
        xml.writeEndDocument();
    }
    addZipFile(za, "_rels/.rels", rootRels);

    // 3. ppt/presentation.xml — slide size adapts to first page
    QSizeF firstPageSize = backend.pageSize(0);
    qint64 slideCx = static_cast<qint64>(firstPageSize.width() * PT_TO_EMU);
    qint64 slideCy = static_cast<qint64>(firstPageSize.height() * PT_TO_EMU);

    QByteArray presXml;
    {
        QXmlStreamWriter xml(&presXml);
        xml.writeStartDocument();
        xml.writeStartElement("p:presentation");
        xml.writeAttribute("xmlns:a", "http://schemas.openxmlformats.org/drawingml/2006/main");
        xml.writeAttribute("xmlns:r", "http://schemas.openxmlformats.org/officeDocument/2006/relationships");
        xml.writeAttribute("xmlns:p", "http://schemas.openxmlformats.org/presentationml/2006/main");
        xml.writeStartElement("p:sldIdLst");
        for (int i = 0; i < pageCount; ++i) {
            xml.writeEmptyElement("p:sldId"); xml.writeAttribute("id", QString::number(256 + i)); xml.writeAttribute("r:id", QString("rId%1").arg(i+2));
        }
        xml.writeEndElement();
        xml.writeEmptyElement("p:sldSz"); xml.writeAttribute("cx", QString::number(slideCx)); xml.writeAttribute("cy", QString::number(slideCy));
        xml.writeEndElement();
        xml.writeEndDocument();
    }
    addZipFile(za, "ppt/presentation.xml", presXml);

    // 4. ppt/_rels/presentation.xml.rels
    QByteArray presRels;
    {
        QXmlStreamWriter xml(&presRels);
        xml.writeStartDocument();
        xml.writeStartElement("Relationships");
        xml.writeAttribute("xmlns", "http://schemas.openxmlformats.org/package/2006/relationships");
        xml.writeEmptyElement("Relationship"); xml.writeAttribute("Id", "rId1"); xml.writeAttribute("Type", "http://schemas.openxmlformats.org/officeDocument/2006/relationships/slideMaster"); xml.writeAttribute("Target", "slideMasters/slideMaster1.xml");
        for (int i = 0; i < pageCount; ++i) {
            xml.writeEmptyElement("Relationship"); xml.writeAttribute("Id", QString("rId%1").arg(i+2)); xml.writeAttribute("Type", "http://schemas.openxmlformats.org/officeDocument/2006/relationships/slide"); xml.writeAttribute("Target", QString("slides/slide%1.xml").arg(i+1));
        }
        xml.writeEndElement();
        xml.writeEndDocument();
    }
    addZipFile(za, "ppt/_rels/presentation.xml.rels", presRels);

    // 5. Slide master and layout (minimal)
    QByteArray slideMaster;
    {
        QXmlStreamWriter xml(&slideMaster);
        xml.writeStartDocument();
        xml.writeStartElement("p:sldMaster");
        xml.writeAttribute("xmlns:a", "http://schemas.openxmlformats.org/drawingml/2006/main");
        xml.writeAttribute("xmlns:r", "http://schemas.openxmlformats.org/officeDocument/2006/relationships");
        xml.writeAttribute("xmlns:p", "http://schemas.openxmlformats.org/presentationml/2006/main");
        xml.writeStartElement("p:cSld");
        // Minimal spTree with required nvGrpSpPr
        xml.writeStartElement("p:spTree");
        xml.writeStartElement("p:nvGrpSpPr");
        xml.writeEmptyElement("p:cNvPr"); xml.writeAttribute("id", "1"); xml.writeAttribute("name", "");
        xml.writeEmptyElement("p:cNvGrpSpPr");
        xml.writeEmptyElement("p:nvPr");
        xml.writeEndElement(); // p:nvGrpSpPr
        xml.writeEmptyElement("p:grpSpPr");
        xml.writeEndElement(); // p:spTree
        xml.writeEndElement(); // p:cSld
        xml.writeStartElement("p:sldLayoutIdLst");
        xml.writeEmptyElement("p:sldLayoutId"); xml.writeAttribute("id", "2147483649"); xml.writeAttribute("r:id", "rId1");
        xml.writeEndElement();
        xml.writeEndElement();
        xml.writeEndDocument();
    }
    addZipFile(za, "ppt/slideMasters/slideMaster1.xml", slideMaster);

    QByteArray slideMasterRels;
    {
        QXmlStreamWriter xml(&slideMasterRels);
        xml.writeStartDocument();
        xml.writeStartElement("Relationships");
        xml.writeAttribute("xmlns", "http://schemas.openxmlformats.org/package/2006/relationships");
        xml.writeEmptyElement("Relationship"); xml.writeAttribute("Id", "rId1"); xml.writeAttribute("Type", "http://schemas.openxmlformats.org/officeDocument/2006/relationships/slideLayout"); xml.writeAttribute("Target", "../slideLayouts/slideLayout1.xml");
        xml.writeEndElement();
        xml.writeEndDocument();
    }
    addZipFile(za, "ppt/slideMasters/_rels/slideMaster1.xml.rels", slideMasterRels);

    QByteArray slideLayout;
    {
        QXmlStreamWriter xml(&slideLayout);
        xml.writeStartDocument();
        xml.writeStartElement("p:sldLayout");
        xml.writeAttribute("xmlns:a", "http://schemas.openxmlformats.org/drawingml/2006/main");
        xml.writeAttribute("xmlns:r", "http://schemas.openxmlformats.org/officeDocument/2006/relationships");
        xml.writeAttribute("xmlns:p", "http://schemas.openxmlformats.org/presentationml/2006/main");
        xml.writeStartElement("p:cSld");
        xml.writeStartElement("p:spTree");
        xml.writeStartElement("p:nvGrpSpPr");
        xml.writeEmptyElement("p:cNvPr"); xml.writeAttribute("id", "1"); xml.writeAttribute("name", "");
        xml.writeEmptyElement("p:cNvGrpSpPr");
        xml.writeEmptyElement("p:nvPr");
        xml.writeEndElement(); // p:nvGrpSpPr
        xml.writeEmptyElement("p:grpSpPr");
        xml.writeEndElement(); // p:spTree
        xml.writeEndElement(); // p:cSld
        xml.writeEndElement();
        xml.writeEndDocument();
    }
    addZipFile(za, "ppt/slideLayouts/slideLayout1.xml", slideLayout);

    QByteArray slideLayoutRels;
    {
        QXmlStreamWriter xml(&slideLayoutRels);
        xml.writeStartDocument();
        xml.writeStartElement("Relationships");
        xml.writeAttribute("xmlns", "http://schemas.openxmlformats.org/package/2006/relationships");
        xml.writeEmptyElement("Relationship"); xml.writeAttribute("Id", "rId1"); xml.writeAttribute("Type", "http://schemas.openxmlformats.org/officeDocument/2006/relationships/slideMaster"); xml.writeAttribute("Target", "../slideMasters/slideMaster1.xml");
        xml.writeEndElement();
        xml.writeEndDocument();
    }
    addZipFile(za, "ppt/slideLayouts/_rels/slideLayout1.xml.rels", slideLayoutRels);

    // 6. Generate slides: background image + structural text overlay
    for (int i = 0; i < pageCount; ++i) {
        QSizeF pageSize = backend.pageSize(i);
        qint64 pageCx = static_cast<qint64>(pageSize.width() * PT_TO_EMU);
        qint64 pageCy = static_cast<qint64>(pageSize.height() * PT_TO_EMU);

        // Render background image
        QImage img = backend.renderPage(i, renderDpi);
        QByteArray imgData;
        QBuffer buffer(&imgData);
        buffer.open(QIODevice::WriteOnly);
        img.save(&buffer, "JPEG", 85);
        addZipFile(za, QString("ppt/media/image%1.jpeg").arg(i+1).toUtf8().constData(), imgData);

        // Extract text elements from this page
        // R09 (F07): decoded Unicode + real glyph geometry from the backend's
        // page-text-with-boxes method (was: raw Tj/TJ byte interpretation).
        QList<TextElement> textElements = d->extractTextFromPage(backend, i);

        // Build the slide XML
        QByteArray slideXml;
        {
            QXmlStreamWriter xml(&slideXml);
            xml.writeStartDocument();
            xml.writeStartElement("p:sld");
            xml.writeAttribute("xmlns:a", "http://schemas.openxmlformats.org/drawingml/2006/main");
            xml.writeAttribute("xmlns:r", "http://schemas.openxmlformats.org/officeDocument/2006/relationships");
            xml.writeAttribute("xmlns:p", "http://schemas.openxmlformats.org/presentationml/2006/main");
            xml.writeStartElement("p:cSld");
            xml.writeStartElement("p:spTree");

            // Required group shape properties
            xml.writeStartElement("p:nvGrpSpPr");
            xml.writeEmptyElement("p:cNvPr"); xml.writeAttribute("id", "1"); xml.writeAttribute("name", "");
            xml.writeEmptyElement("p:cNvGrpSpPr");
            xml.writeEmptyElement("p:nvPr");
            xml.writeEndElement(); // p:nvGrpSpPr

            xml.writeStartElement("p:grpSpPr");
            xml.writeStartElement("a:xfrm");
            xml.writeEmptyElement("a:off"); xml.writeAttribute("x", "0"); xml.writeAttribute("y", "0");
            xml.writeEmptyElement("a:ext"); xml.writeAttribute("cx", "0"); xml.writeAttribute("cy", "0");
            xml.writeEmptyElement("a:chOff"); xml.writeAttribute("x", "0"); xml.writeAttribute("y", "0");
            xml.writeEmptyElement("a:chExt"); xml.writeAttribute("cx", "0"); xml.writeAttribute("cy", "0");
            xml.writeEndElement(); // a:xfrm
            xml.writeEndElement(); // p:grpSpPr

            // Background image (p:pic) — stretched to full slide
            xml.writeStartElement("p:pic");
            {
                xml.writeStartElement("p:nvPicPr");
                xml.writeEmptyElement("p:cNvPr"); xml.writeAttribute("id", "2"); xml.writeAttribute("name", "Background");
                xml.writeEmptyElement("p:cNvPicPr");
                xml.writeEmptyElement("p:nvPr");
                xml.writeEndElement(); // p:nvPicPr

                xml.writeStartElement("p:blipFill");
                xml.writeEmptyElement("a:blip"); xml.writeAttribute("r:embed", "rId1");
                xml.writeStartElement("a:stretch");
                xml.writeEmptyElement("a:fillRect");
                xml.writeEndElement(); // a:stretch
                xml.writeEndElement(); // p:blipFill

                xml.writeStartElement("p:spPr");
                xml.writeStartElement("a:xfrm");
                xml.writeEmptyElement("a:off"); xml.writeAttribute("x", "0"); xml.writeAttribute("y", "0");
                xml.writeEmptyElement("a:ext"); xml.writeAttribute("cx", QString::number(pageCx)); xml.writeAttribute("cy", QString::number(pageCy));
                xml.writeEndElement(); // a:xfrm
                xml.writeStartElement("a:prstGeom"); xml.writeAttribute("prst", "rect");
                xml.writeEmptyElement("a:avLst");
                xml.writeEndElement(); // a:prstGeom
                xml.writeEndElement(); // p:spPr
            }
            xml.writeEndElement(); // p:pic

            // Overlay text shapes (p:sp) for each extracted text element
            int shapeId = 10; // start after reserved IDs
            for (const TextElement &el : textElements) {
                if (el.text.trimmed().isEmpty()) continue;

                // PDF coords: origin bottom-left, Y increases upward
                // PPTX coords: origin top-left, Y increases downward
                qint64 emuX = static_cast<qint64>(el.rect.x() * PT_TO_EMU);
                qint64 emuW = static_cast<qint64>(el.rect.width() * PT_TO_EMU);
                qint64 emuH = static_cast<qint64>(el.fontSize * PT_TO_EMU * 1.2); // line height ~1.2x font
                // Flip Y: pptxY = (pageHeight - pdfY - elementHeight_in_pts) * PT_TO_EMU
                double pdfTopY = pageSize.height() - el.rect.y() - el.fontSize;
                qint64 emuY = static_cast<qint64>(pdfTopY * PT_TO_EMU);

                // Clamp to non-negative
                if (emuX < 0) emuX = 0;
                if (emuY < 0) emuY = 0;
                if (emuW < 1) emuW = 1;
                if (emuH < 1) emuH = 1;

                // Font size in PPTX is in hundredths of a point
                int pptxFontSize = static_cast<int>(el.fontSize * 100);
                if (pptxFontSize < 100) pptxFontSize = 100;

                xml.writeStartElement("p:sp");
                {
                    // Non-visual shape properties
                    xml.writeStartElement("p:nvSpPr");
                    xml.writeEmptyElement("p:cNvPr"); xml.writeAttribute("id", QString::number(shapeId++)); xml.writeAttribute("name", QString("TextBox %1").arg(shapeId));
                    xml.writeStartElement("p:cNvSpPr"); xml.writeAttribute("txBox", "1");
                    xml.writeEndElement(); // p:cNvSpPr
                    xml.writeEmptyElement("p:nvPr");
                    xml.writeEndElement(); // p:nvSpPr

                    // Shape properties — position and size
                    xml.writeStartElement("p:spPr");
                    xml.writeStartElement("a:xfrm");
                    xml.writeEmptyElement("a:off"); xml.writeAttribute("x", QString::number(emuX)); xml.writeAttribute("y", QString::number(emuY));
                    xml.writeEmptyElement("a:ext"); xml.writeAttribute("cx", QString::number(emuW)); xml.writeAttribute("cy", QString::number(emuH));
                    xml.writeEndElement(); // a:xfrm
                    xml.writeStartElement("a:prstGeom"); xml.writeAttribute("prst", "rect");
                    xml.writeEmptyElement("a:avLst");
                    xml.writeEndElement(); // a:prstGeom
                    // No fill, no line — transparent text box over the image
                    xml.writeEmptyElement("a:noFill");
                    xml.writeStartElement("a:ln");
                    xml.writeEmptyElement("a:noFill");
                    xml.writeEndElement(); // a:ln
                    xml.writeEndElement(); // p:spPr

                    // Text body
                    xml.writeStartElement("p:txBody");
                    // Body properties: no auto-fit, no margins, text anchored at top
                    xml.writeStartElement("a:bodyPr");
                    xml.writeAttribute("wrap", "none");
                    xml.writeAttribute("lIns", "0"); xml.writeAttribute("tIns", "0");
                    xml.writeAttribute("rIns", "0"); xml.writeAttribute("bIns", "0");
                    xml.writeAttribute("anchor", "t");
                    xml.writeEndElement(); // a:bodyPr
                    xml.writeEmptyElement("a:lstStyle");

                    xml.writeStartElement("a:p");
                    // Paragraph properties: no spacing
                    xml.writeStartElement("a:pPr");
                    xml.writeEmptyElement("a:spcBef"); // default 0
                    xml.writeEmptyElement("a:spcAft"); // default 0
                    xml.writeEndElement(); // a:pPr

                    xml.writeStartElement("a:r");
                    // Run properties: font, size, transparent color
                    xml.writeStartElement("a:rPr");
                    xml.writeAttribute("lang", "en-US");
                    xml.writeAttribute("sz", QString::number(pptxFontSize));
                    xml.writeAttribute("dirty", "0");
                    // Nearly transparent text — visible for selection, invisible visually
                    xml.writeStartElement("a:solidFill");
                    xml.writeStartElement("a:srgbClr"); xml.writeAttribute("val", "000000");
                    // §9.5 P0: actually apply the intended ~1% opacity. OOXML alpha
                    // is in 1000ths of a percent (100000 = opaque); without this
                    // child element the overlay rendered as solid black doubled text.
                    xml.writeStartElement("a:alpha"); xml.writeAttribute("val", "1000");
                    xml.writeEndElement(); // a:alpha
                    xml.writeEndElement(); // a:srgbClr
                    xml.writeEndElement(); // a:solidFill
                    // Font face
                    if (!el.fontName.isEmpty()) {
                        xml.writeEmptyElement("a:latin"); xml.writeAttribute("typeface", el.fontName);
                    }
                    xml.writeEndElement(); // a:rPr

                    xml.writeStartElement("a:t");
                    xml.writeCharacters(el.text);
                    xml.writeEndElement(); // a:t
                    xml.writeEndElement(); // a:r
                    xml.writeEndElement(); // a:p
                    xml.writeEndElement(); // p:txBody
                }
                xml.writeEndElement(); // p:sp
            }

            xml.writeEndElement(); // p:spTree
            xml.writeEndElement(); // p:cSld
            xml.writeEndElement(); // p:sld
            xml.writeEndDocument();
        }
        addZipFile(za, QString("ppt/slides/slide%1.xml").arg(i+1).toUtf8().constData(), slideXml);

        // Slide relationships: rId1 = image, rId2 = slideLayout
        QByteArray slideRels;
        {
            QXmlStreamWriter xml(&slideRels);
            xml.writeStartDocument();
            xml.writeStartElement("Relationships");
            xml.writeAttribute("xmlns", "http://schemas.openxmlformats.org/package/2006/relationships");
            xml.writeEmptyElement("Relationship"); xml.writeAttribute("Id", "rId1"); xml.writeAttribute("Type", "http://schemas.openxmlformats.org/officeDocument/2006/relationships/image"); xml.writeAttribute("Target", QString("../media/image%1.jpeg").arg(i+1));
            xml.writeEmptyElement("Relationship"); xml.writeAttribute("Id", "rId2"); xml.writeAttribute("Type", "http://schemas.openxmlformats.org/officeDocument/2006/relationships/slideLayout"); xml.writeAttribute("Target", "../slideLayouts/slideLayout1.xml");
            xml.writeEndElement();
            xml.writeEndDocument();
        }
        addZipFile(za, QString("ppt/slides/_rels/slide%1.xml.rels").arg(i+1).toUtf8().constData(), slideRels);
    }

    if (zip_close(za) != 0) return false;
    return QFileInfo(outputPath).size() > 0;
}

bool ConversionManager::convertImagesToPdf(const QStringList &imagePaths,
                                           const QString &outputPath,
                                           ImageImportOptions opts)
{
    if (imagePaths.isEmpty()) {
        qWarning() << "convertImagesToPdf: no input images provided";
        return false;
    }

    // Determine page dimensions in PDF user units (1 pt = 1/72 inch)
    // QPageSize sizes are in mm; convert to points.
    QPageSize qs(opts.pageSize);
    QSizeF pageSizeMm = qs.size(QPageSize::Millimeter);
    const double mmPerPt = 25.4 / 72.0;
    const double pageW = pageSizeMm.width()  / mmPerPt;  // points
    const double pageH = pageSizeMm.height() / mmPerPt;  // points

    try {
        PoDoFo::PdfMemDocument doc;

        for (const QString &imgPath : imagePaths) {
            QFileInfo fi(imgPath);
            if (!fi.exists()) {
                qWarning() << "convertImagesToPdf: image not found:" << imgPath;
                return false;
            }

            // Create a page
            PoDoFo::PdfPage& page = doc.GetPages().CreatePage(
                PoDoFo::Rect(0, 0, pageW, pageH));

            // Load image via PoDoFo codec (PNG/JPEG/TIFF auto-detected)
            auto image = doc.CreateImage();
            PoDoFo::PdfImageInfo imgInfo;
            try {
                imgInfo = image->Load(imgPath.toStdString());
            } catch (const std::exception &e) {
                // Fall back to loading via Qt and feeding raw pixel data
                QImage qimg;
                if (!qimg.load(imgPath)) {
                    qWarning() << "convertImagesToPdf: cannot load image:" << imgPath << e.what();
                    return false;
                }
                if (qimg.width() > 10000 || qimg.height() > 10000) {
                    qWarning() << "convertImagesToPdf: image dimensions too large";
                    return false;
                }
                // Convert to RGB24 for PoDoFo SetData
                QImage rgb = qimg.convertToFormat(QImage::Format_RGB888);
                const int w = rgb.width();
                const int h = rgb.height();
                const QByteArray rawBytes(reinterpret_cast<const char*>(rgb.constBits()),
                                          static_cast<qsizetype>(rgb.sizeInBytes()));
                image->SetData(
                    PoDoFo::bufferview(rawBytes.constData(), static_cast<size_t>(rawBytes.size())),
                    static_cast<unsigned>(w), static_cast<unsigned>(h),
                    PoDoFo::PdfPixelFormat::RGB24);
                imgInfo.Width  = static_cast<unsigned>(w);
                imgInfo.Height = static_cast<unsigned>(h);
            }

            // Compute draw rect — fit image to page (or use natural size at opts.dpi)
            double imgW = static_cast<double>(imgInfo.Width);
            double imgH = static_cast<double>(imgInfo.Height);

            double drawX = 0, drawY = 0, drawW, drawH;
            if (opts.fitToPage) {
                // Scale to fit inside page with uniform aspect ratio
                double scaleX = pageW / imgW;
                double scaleY = pageH / imgH;
                double scale  = std::min(scaleX, scaleY);
                drawW = imgW * scale;
                drawH = imgH * scale;
                // Center on page
                drawX = (pageW  - drawW) / 2.0;
                drawY = (pageH  - drawH) / 2.0;
            } else {
                // Natural DPI size
                const double ptsPerPx = 72.0 / opts.dpi;
                drawW = imgW * ptsPerPx;
                drawH = imgH * ptsPerPx;
                // Position at bottom-left (PDF origin)
            }

            PoDoFo::PdfPainter painter;
            painter.SetCanvas(page);
            // PoDoFo coordinate system: origin bottom-left; DrawImage y = bottom of image
            painter.DrawImage(*image, drawX, drawY, drawW / imgW, drawH / imgH);
            painter.FinishDrawing();
        }

        doc.Save(outputPath.toUtf8().constData());
        return true;
    } catch (const std::exception &e) {
        qWarning() << "convertImagesToPdf: PoDoFo error:" << e.what();
        return false;
    }
}

