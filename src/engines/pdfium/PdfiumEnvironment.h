// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QMutex>

#ifdef HAS_PDFIUM
#include <fpdfview.h>
#endif

class PdfiumEnvironment {
public:
    PdfiumEnvironment();
    ~PdfiumEnvironment();

private:
    static int s_refCount;
    static QMutex s_mutex;
};
