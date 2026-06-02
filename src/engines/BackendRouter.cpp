// SPDX-License-Identifier: Apache-2.0
#include "engines/BackendRouter.h"
#include "engines/podofo/PoDoFoBackend.h"
#ifdef HAS_PDFIUM
#include "engines/pdfium/PdfiumBackend.h"
#endif

IPdfRenderer* BackendRouter::rendererFor(const QString &path) {
#ifdef HAS_PDFIUM
    auto* backend = new PdfiumBackend();
    if (backend->loadDocument(path)) {
        return backend;
    }
    delete backend;
#endif
    return nullptr;
}

IPdfDocument* BackendRouter::documentBackendFor(const QString &path) {
    Q_UNUSED(path);
    return new PoDoFoBackend();
}

IPdfWriter* BackendRouter::writerFor(const QString &path) {
    Q_UNUSED(path);
    return new PoDoFoBackend();
}

