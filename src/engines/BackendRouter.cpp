// SPDX-License-Identifier: Apache-2.0
#include "engines/BackendRouter.h"
#include "engines/podofo/PoDoFoBackend.h"
#ifdef HAS_PDFIUM
#include "engines/pdfium/PdfiumBackend.h"
#endif

std::unique_ptr<IPdfRenderer> BackendRouter::rendererFor(const QString &path) {
#ifdef HAS_PDFIUM
    auto backend = std::make_unique<PdfiumBackend>();
    if (backend->loadDocument(path))
        return backend;
    // loadDocument failed; backend destroyed automatically
#else
    Q_UNUSED(path)
#endif
    return nullptr;
}

std::unique_ptr<IPdfDocument> BackendRouter::documentBackendFor(const QString &path) {
    Q_UNUSED(path)
    // PoDoFo is the only document backend.  Returns unloaded; caller calls loadDocument().
    return std::make_unique<PoDoFoBackend>();
}

std::unique_ptr<IPdfWriter> BackendRouter::writerFor(const QString &path) {
    Q_UNUSED(path)
    // PoDoFo is the only writer backend.
    // IMPORTANT: signing must always use this backend — qpdf flattens xref
    // and invalidates /ByteRange digital signatures.
    return std::make_unique<PoDoFoBackend>();
}
