// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QString>
#include <memory>

class IPdfRenderer;
class IPdfDocument;
class IPdfWriter;

// BackendRouter — single static factory for PDF engine selection.
//
// All methods return std::unique_ptr<> so the caller owns the object.
// If a backend is unavailable (e.g. HAS_PDFIUM not set, or the file path
// cannot be loaded), nullptr is returned.
//
// D4 audit fix: raw-pointer return types were a source of leaks (D-08).
// Callers that need a non-owning observer pointer should call .get().
class BackendRouter {
public:
    // Returns a PdfiumBackend that has already loaded 'path', or nullptr.
    static std::unique_ptr<IPdfRenderer> rendererFor(const QString &path);

    // Returns a fresh PoDoFoBackend (unloaded). Caller calls loadDocument().
    // PoDoFo is the only document/writer backend; path is accepted for future
    // multi-backend support but is currently unused.
    static std::unique_ptr<IPdfDocument> documentBackendFor(const QString &path);

    // Returns a fresh PoDoFoBackend for write operations.
    // NOTE: signing operations MUST use this backend (PoDoFo is the only
    // engine that preserves /ByteRange across write — qpdf flattens xref and
    // invalidates digital signatures).
    static std::unique_ptr<IPdfWriter> writerFor(const QString &path);
};
