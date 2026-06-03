// SPDX-License-Identifier: Apache-2.0
//
// PdfPageOps.cpp — stand-alone PoDoFo file-level page operations.
//
// This is the ONLY translation unit in pdfws_ui's dependency graph that
// includes <podofo/podofo.h> from the engines side for page manipulation.
// By isolating the include here, PdfViewerWidget.cpp (and all other UI TUs)
// can call these functions without pulling PoDoFo headers into the UI layer.
//
#include "PdfPageOps.h"
#include <podofo/podofo.h>
#include <podofo/main/PdfMemDocument.h>
#include <podofo/main/PdfPainter.h>

#include <QMap>
#include <QDebug>

#include <sstream>
#include <string>
#include <vector>
#include <stdexcept>

namespace gp {

// ─── extractPages ────────────────────────────────────────────────────────────

bool extractPages(const QString& inputPath, int from, int to, const QString& outputPath)
{
    try {
        PoDoFo::PdfMemDocument srcDoc;
        srcDoc.Load(inputPath.toUtf8().constData());
        PoDoFo::PdfMemDocument dstDoc;
        dstDoc.GetPages().AppendDocumentPages(srcDoc, from, to - from + 1);
        dstDoc.Save(outputPath.toUtf8().constData());
        return true;
    } catch (const std::exception& e) {
        qWarning() << "PdfPageOps::extractPages error:" << e.what();
        return false;
    }
}

// ─── deletePages ─────────────────────────────────────────────────────────────

bool deletePages(const QString& inputPath, int from, int to, const QString& outputPath)
{
    try {
        PoDoFo::PdfMemDocument doc;
        doc.Load(inputPath.toUtf8().constData());
        for (int i = to; i >= from; --i)
            doc.GetPages().RemovePageAt(i);
        doc.Save(outputPath.toUtf8().constData());
        return true;
    } catch (const std::exception& e) {
        qWarning() << "PdfPageOps::deletePages error:" << e.what();
        return false;
    }
}

// ─── insertBlankPage ─────────────────────────────────────────────────────────

bool insertBlankPage(const QString& inputPath, int atIndex, const QString& outputPath)
{
    try {
        PoDoFo::PdfMemDocument srcDoc;
        srcDoc.Load(inputPath.toUtf8().constData());
        PoDoFo::PdfMemDocument dstDoc;

        if (atIndex > 0)
            dstDoc.GetPages().AppendDocumentPages(srcDoc, 0, atIndex);

        PoDoFo::Rect mediaBox(0, 0, 595.276, 841.890); // A4
        dstDoc.GetPages().CreatePage(mediaBox);

        int total = static_cast<int>(srcDoc.GetPages().GetCount());
        if (atIndex < total)
            dstDoc.GetPages().AppendDocumentPages(srcDoc, atIndex, total - atIndex);

        dstDoc.Save(outputPath.toUtf8().constData());
        return true;
    } catch (const std::exception& e) {
        qWarning() << "PdfPageOps::insertBlankPage error:" << e.what();
        return false;
    }
}

// ─── rotatePages ─────────────────────────────────────────────────────────────

bool rotatePages(const QString& inputPath, int from, int to, int angle,
                 const QString& outputPath)
{
    try {
        PoDoFo::PdfMemDocument doc;
        doc.Load(inputPath.toUtf8().constData());
        for (int i = from; i <= to; ++i) {
            PoDoFo::PdfPage& page = doc.GetPages().GetPageAt(i);
            int cur = static_cast<int>(page.GetRotation());
            page.SetRotation((cur + angle) % 360);
        }
        doc.Save(outputPath.toUtf8().constData());
        return true;
    } catch (const std::exception& e) {
        qWarning() << "PdfPageOps::rotatePages error:" << e.what();
        return false;
    }
}

// ─── applyRedactionsToFile ────────────────────────────────────────────────────

bool applyRedactionsToFile(const QString& inputPath,
                           const QMap<int, QList<QRectF>>& rectsByPage,
                           const QString& outputPath)
{
    try {
        PoDoFo::PdfMemDocument doc;
        doc.Load(inputPath.toUtf8().constData());

        for (auto it = rectsByPage.begin(); it != rectsByPage.end(); ++it) {
            int pageIndex = it.key();
            const QList<QRectF>& rects = it.value();

            if (pageIndex < 0 || static_cast<unsigned>(pageIndex) >=
                    doc.GetPages().GetCount())
                continue;

            PoDoFo::PdfPage& page = doc.GetPages().GetPageAt(pageIndex);

            // Phase 1: Scrub text operators from content stream
            auto* contentsObj = page.GetDictionary().FindKey("Contents");
            if (contentsObj && contentsObj->HasStream()) {
                PoDoFo::charbuff streamBuf;
                auto& stream = contentsObj->GetOrCreateStream();
                stream.CopyTo(streamBuf);
                std::string streamStr(streamBuf.data(), streamBuf.size());

                double textX = 0, textY = 0;
                bool inTextBlock = false;
                std::istringstream iss(streamStr);
                std::ostringstream oss;
                std::string token;
                std::vector<std::string> operandStack;

                while (iss >> token) {
                    if (token == "BT") {
                        inTextBlock = true; textX = 0; textY = 0;
                        oss << token << ' '; continue;
                    }
                    if (token == "ET") {
                        inTextBlock = false;
                        oss << token << ' ';
                        operandStack.clear(); continue;
                    }
                    if (token == "Tm" && operandStack.size() >= 6) {
                        try {
                            textX = std::stod(operandStack[operandStack.size() - 2]);
                            textY = std::stod(operandStack[operandStack.size() - 1]);
                        } catch (...) {}
                        bool hit = false;
                        for (const auto& r : rects) {
                            if (r.contains(textX, textY) ||
                                (textX >= r.left() && textX <= r.right() &&
                                 textY >= r.bottom() && textY <= r.top())) {
                                hit = true; break;
                            }
                        }
                        if (hit) { operandStack.clear(); oss << "1 0 0 1 0 0 Tm "; continue; }
                        for (const auto& op : operandStack) oss << op << ' ';
                        oss << token << ' '; operandStack.clear(); continue;
                    }
                    if ((token == "Td" || token == "TD") && operandStack.size() >= 2) {
                        try {
                            textX += std::stod(operandStack[operandStack.size() - 2]);
                            textY += std::stod(operandStack[operandStack.size() - 1]);
                        } catch (...) {}
                        for (const auto& op : operandStack) oss << op << ' ';
                        oss << token << ' '; operandStack.clear(); continue;
                    }
                    if ((token == "Tj" || token == "TJ" || token == "'" || token == "\"")
                        && inTextBlock) {
                        bool hit = false;
                        for (const auto& r : rects) {
                            if (r.contains(textX, textY) ||
                                (textX >= r.left() && textX <= r.right() &&
                                 textY >= r.bottom() && textY <= r.top())) {
                                hit = true; break;
                            }
                        }
                        if (hit) { operandStack.clear(); continue; }
                        for (const auto& op : operandStack) oss << op << ' ';
                        oss << token << ' '; operandStack.clear(); continue;
                    }
                    // Pass-through operators
                    if (token == "Tf" || token == "cm" || token == "re" || token == "m" ||
                        token == "l"  || token == "c"  || token == "v"  || token == "y"  ||
                        token == "h"  || token == "W"  || token == "W*" || token == "n"  ||
                        token == "f"  || token == "F"  || token == "f*" || token == "B"  ||
                        token == "B*" || token == "b"  || token == "b*" || token == "S"  ||
                        token == "s"  || token == "Do" || token == "gs" || token == "CS" ||
                        token == "cs" || token == "SC" || token == "sc" || token == "SCN"||
                        token == "scn"|| token == "G"  || token == "g"  || token == "RG" ||
                        token == "rg" || token == "K"  || token == "k"  || token == "w"  ||
                        token == "J"  || token == "j"  || token == "M"  || token == "d"  ||
                        token == "ri" || token == "i"  || token == "q"  || token == "Q"  ||
                        token == "Tc" || token == "Tw" || token == "Tz" || token == "TL" ||
                        token == "Tr" || token == "Ts" || token == "T*") {
                        for (const auto& op : operandStack) oss << op << ' ';
                        oss << token << ' '; operandStack.clear(); continue;
                    }
                    operandStack.push_back(token);
                }
                for (const auto& op : operandStack) oss << op << ' ';
                std::string filtered = oss.str();
                PoDoFo::charbuff newBuf(filtered);
                contentsObj->GetOrCreateStream().SetData(newBuf);
            }

            // Phase 2: Draw solid black rectangles for visual redaction marks
            PoDoFo::PdfPainter painter;
            painter.SetCanvas(page);
            painter.GraphicsState.SetNonStrokingColor(PoDoFo::PdfColor(0.0, 0.0, 0.0));
            for (const auto& r : rects)
                painter.DrawRectangle(r.x(), r.y(), r.width(), r.height(),
                                      PoDoFo::PdfPathDrawMode::Fill);
            painter.FinishDrawing();
        }

        doc.Save(outputPath.toUtf8().constData());
        return true;
    } catch (const std::exception& e) {
        qWarning() << "PdfPageOps::applyRedactionsToFile error:" << e.what();
        return false;
    }
}

// ─── mergeDocuments ──────────────────────────────────────────────────────────

bool mergeDocuments(const QStringList& inputs, const QString& outputPath)
{
    if (inputs.isEmpty()) return false;
    try {
        PoDoFo::PdfMemDocument dstDoc;
        for (const QString& fileName : inputs) {
            PoDoFo::PdfMemDocument srcDoc;
            srcDoc.Load(fileName.toUtf8().constData());
            int count = static_cast<int>(srcDoc.GetPages().GetCount());
            if (count > 0)
                dstDoc.GetPages().AppendDocumentPages(srcDoc, 0, count);
        }
        dstDoc.Save(outputPath.toUtf8().constData());
        return true;
    } catch (const std::exception& e) {
        qWarning() << "PdfPageOps::mergeDocuments error:" << e.what();
        return false;
    }
}

} // namespace gp
