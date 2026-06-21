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
