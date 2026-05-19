#include "engines/ConversionManager.h"
#include <memory>
#include <podofo/podofo.h>
#include <algorithm>
#include <cmath>
#include <QDebug>
#include <QFile>
#include <QTextStream>

// Forward declarations or include headers if available
#ifdef HAS_OPENXLSX
#include <OpenXLSX.hpp>
#endif

#ifdef HAS_DUCKX
#include <duckx.hpp>
#endif

class ConversionManager::Private {
public:
    PoDoFo::PdfMemDocument *document = nullptr;
    QList<ConversionManager::TextElement> extractTextFromPage(int pageIndex, PoDoFo::PdfMemDocument &doc);
};

ConversionManager::ConversionManager(QObject *parent)
    : QObject(parent), d(std::make_unique<Private>())
{
}

ConversionManager::~ConversionManager() = default;

bool ConversionManager::convertTo(const QString &pdfPath, const QString &outputPath, TargetFormat format)
{
    try {
        PoDoFo::PdfMemDocument doc;
        doc.Load(pdfPath.toUtf8().constData());

        QList<QList<TextElement>> allRows;

        for (unsigned i = 0; i < doc.GetPages().GetCount(); ++i) {
            QList<TextElement> pageElements = d->extractTextFromPage(i, doc);
            
            // Text Flow Heuristic: Group into rows
            // PDF coordinates: Y increases upwards. We want top-to-bottom.
            std::sort(pageElements.begin(), pageElements.end(), [](const TextElement &a, const TextElement &b) {
                // If Y is close enough, they are on the same line. Sort by X.
                if (std::abs(a.rect.y() - b.rect.y()) < (std::min(a.fontSize, b.fontSize) * 0.5)) {
                    return a.rect.x() < b.rect.x();
                }
                // Otherwise, higher Y (top of page) comes first
                return a.rect.y() > b.rect.y();
            });

            QList<TextElement> currentRow;
            for (const auto &el : pageElements) {
                if (currentRow.isEmpty()) {
                    currentRow.append(el);
                } else {
                    double yDiff = std::abs(currentRow.last().rect.y() - el.rect.y());
                    if (yDiff < el.fontSize * 0.8) {
                        currentRow.append(el);
                    } else {
                        allRows.append(currentRow);
                        currentRow.clear();
                        currentRow.append(el);
                    }
                }
            }
            if (!currentRow.isEmpty()) allRows.append(currentRow);
        }

        if (format == TargetFormat::Word) {
            return exportToWord(outputPath, allRows);
        } else {
            return exportToExcel(outputPath, allRows);
        }

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

QList<ConversionManager::TextElement> ConversionManager::Private::extractTextFromPage(int pageIndex, PoDoFo::PdfMemDocument &doc)
{
    QList<TextElement> elements;
    try {
        PoDoFo::PdfPage& page = doc.GetPages().GetPageAt(pageIndex);
        PoDoFo::PdfContentStreamReader reader(page);
        
        double currentX = 0, currentY = 0;
        double currentFontSize = 10.0;
        QString currentFontName = "Helvetica";
        
        PoDoFo::PdfContent content;
        while (reader.TryReadNext(content)) {
            if (content.GetType() == PoDoFo::PdfContentType::Operator) {
                std::string_view kw = content.GetKeyword();
                const auto& stack = content.GetStack();
                
                if (kw == "Tm" && stack.size() >= 6) {
                    if (stack[4].IsNumberOrReal()) currentX = stack[4].GetReal();
                    if (stack[5].IsNumberOrReal()) currentY = stack[5].GetReal();
                } else if ((kw == "Td" || kw == "TD") && stack.size() >= 2) {
                    if (stack[0].IsNumberOrReal()) currentX += stack[0].GetReal();
                    if (stack[1].IsNumberOrReal()) currentY += stack[1].GetReal();
                } else if (kw == "Tf" && stack.size() >= 2) {
                    if (stack[0].IsName()) currentFontName = QString::fromStdString(std::string(stack[0].GetName().GetString()));
                    if (stack[1].IsNumberOrReal()) currentFontSize = stack[1].GetReal();
                } else if (kw == "Tj" && stack.size() >= 1) {
                    if (stack[0].IsString()) {
                        TextElement el;
                        el.text = QString::fromStdString(std::string(stack[0].GetString().GetString()));
                        el.rect = QRectF(currentX, currentY, el.text.length() * currentFontSize * 0.6, currentFontSize);
                        el.fontSize = currentFontSize;
                        el.fontName = currentFontName;
                        elements.append(el);
                    }
                } else if (kw == "TJ" && stack.size() >= 1) {
                    if (stack[0].IsArray()) {
                        QString fullText;
                        for (const auto& item : stack[0].GetArray()) {
                            if (item.IsString()) {
                                fullText += QString::fromStdString(std::string(item.GetString().GetString()));
                            }
                        }
                        TextElement el;
                        el.text = fullText;
                        el.rect = QRectF(currentX, currentY, el.text.length() * currentFontSize * 0.6, currentFontSize);
                        el.fontSize = currentFontSize;
                        el.fontName = currentFontName;
                        elements.append(el);
                    }
                }
            }
        }
    } catch (...) {}
    return elements;
}

bool ConversionManager::exportToWord(const QString &outputPath, const QList<QList<TextElement>> &rows)
{
#ifdef HAS_DUCKX
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
    return true;
#else
    // Fallback: Generate HTML-based DOC (Word can open it)
    QFile file(outputPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    QTextStream out(&file);
    out << "<html><body>";
    for (const auto &row : rows) {
        out << "<p>";
        for (const auto &el : row) {
            out << el.text.toHtmlEscaped() << " ";
        }
        out << "</p>";
    }
    out << "</body></html>";
    return true;
#endif
}

bool ConversionManager::exportToExcel(const QString &outputPath, const QList<QList<TextElement>> &rows)
{
#ifdef HAS_OPENXLSX
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
    return true;
#else
    // Fallback: Generate CSV
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
    return true;
#endif
}
