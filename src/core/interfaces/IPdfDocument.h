// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QString>
#include "core/interfaces/IPdfEditorEngine.h" // for PdfMetadata

class IPdfDocument {
public:
    virtual ~IPdfDocument() = default;
    virtual bool loadDocument(const QString &path) = 0;
    virtual bool saveDocument(const QString &path) = 0;
    virtual int pageCount() const = 0;
    virtual PdfMetadata metadata() const = 0;
    virtual bool setMetadata(const PdfMetadata &metadata) = 0;
};
