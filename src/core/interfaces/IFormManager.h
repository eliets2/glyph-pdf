#pragma once
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QRectF>

class IFormManager {
public:
    virtual ~IFormManager() = default;
    virtual bool extractFormFields(const QString &pdfFilePath) = 0;
    virtual bool fillForm(const QString &pdfFilePath, const QVariantMap &fieldData, const QString &outputPath) = 0;
    virtual bool hasXfaForms(const QString &pdfFilePath) = 0;
    virtual bool addTextField(const QString &pdfFilePath, int pageIndex, const QRectF &rect,
                               const QString &fieldName, const QString &outputPath) = 0;
    virtual bool addCheckBox(const QString &pdfFilePath, int pageIndex, const QRectF &rect,
                              const QString &fieldName, const QString &outputPath) = 0;
    virtual bool addRadioButton(const QString &pdfFilePath, int pageIndex, const QRectF &rect,
                                 const QString &fieldName, const QString &outputPath) = 0;
    virtual bool addDropdown(const QString &pdfFilePath, int pageIndex, const QRectF &rect,
                              const QString &fieldName, const QStringList &options, const QString &outputPath) = 0;
protected:
    IFormManager() = default;
    IFormManager(const IFormManager&) = delete;
    IFormManager& operator=(const IFormManager&) = delete;
};
