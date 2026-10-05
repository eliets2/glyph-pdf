// SPDX-License-Identifier: Apache-2.0
#pragma once
//
// ImageExtractEngine — §9.17/§9.18 Compose Mode support: the /XObject /Image
// inventory of one page of a PDF on disk, with dimensions, filters and decoded
// pixels. The visual image picker in ComposeMode shows this inventory; a
// picked entry's pixels travel into the destination document through
// IPdfEditorEngine::placeImageOnPage.
//
// The engine is deliberately STAND-ALONE (the PdfPageOps pattern): it opens a
// fresh read-only PoDoFo document per call and never touches a resident
// editor, so it is safe to call against a file another engine has loaded.
// PoDoFo headers live only in the .cpp — callers in pdfws_ui stay PoDoFo-free.
//
#include <QString>
#include <QStringList>
#include <QList>
#include <QImage>

namespace gp {

/// One embedded image of one page, as the compose image picker reports it.
struct ComposeImageInfo {
    int pageIndex = 0;          ///< 0-based page the image lives on
    QString xobjectName;        ///< resource name in the page's /XObject dict (e.g. "/Im0")
    int widthPx = 0;            ///< native pixel width (/Width)
    int heightPx = 0;           ///< native pixel height (/Height)
    QStringList filters;        ///< /Filter entries in order (e.g. "DCTDecode", "FlateDecode")
    QImage pixels;              ///< decoded RGBA pixels; null when the image could not be decoded
};

class ImageExtractEngine {
public:
    ImageExtractEngine() = delete;   // static-only facade (PdfPageOps pattern)

    /// Inventory of the /Subtype /Image XObjects of `pageIndex` (0-based) in
    /// the PDF at `pdfPath`. Resource-level inventory (with dimensions and
    /// filters) — placements of the same XObject are one entry. Malformed or
    /// out-of-range input yields an empty list, never a crash.
    static QList<ComposeImageInfo> listPageImages(const QString& pdfPath, int pageIndex);

    /// True when the two inventories describe the same XObject name — the
    /// compose picker's identity key across a transfer (same name on the
    /// source page means the same object for the pin assertions).
    static bool sameXObject(const ComposeImageInfo& info, const QString& name) {
        return info.xobjectName == name;
    }
};

} // namespace gp
