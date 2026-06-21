// SPDX-License-Identifier: Apache-2.0
#include "engines/pdfium/PdfiumEnvironment.h"
#include <QMutexLocker>

int PdfiumEnvironment::s_refCount = 0;
QMutex PdfiumEnvironment::s_mutex;

PdfiumEnvironment::PdfiumEnvironment() {
#ifdef HAS_PDFIUM
    QMutexLocker locker(&s_mutex);
    if (s_refCount == 0) {
        FPDF_InitLibrary();
    }
    s_refCount++;
#endif
}

PdfiumEnvironment::~PdfiumEnvironment() {
#ifdef HAS_PDFIUM
    QMutexLocker locker(&s_mutex);
    s_refCount--;
    if (s_refCount == 0) {
        FPDF_DestroyLibrary();
    }
#endif
}
