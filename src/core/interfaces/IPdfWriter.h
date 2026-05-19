#pragma once
#include <QString>

class IPdfWriter {
public:
    virtual ~IPdfWriter() = default;
    virtual bool writeDocument(const QString &path) = 0;
    virtual bool writeUpdate(const QString &path) = 0;
};
