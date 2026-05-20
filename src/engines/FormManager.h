#pragma once

#include <QString>
#include <QVariantMap>
#include <QRectF>
#include <QStringList>
#include <memory>
#include "core/interfaces/IFormManager.h"

class FormManager final : public IFormManager
{
public:
    FormManager();
    ~FormManager() override;

    // Map AcroForm dictionaries to UI Widgets using PoDoFo/qpdf
    bool extractFormFields(const QString &pdfFilePath) override;
    
    // Fill out and flatten AcroForms
    bool fillForm(const QString &pdfFilePath, const QVariantMap &fieldData, const QString &outputPath) override;
    
    // Check if the document has XFA forms
    bool hasXfaForms(const QString &pdfFilePath) override;

    bool addTextField(const QString &pdfFilePath, int pageIndex, const QRectF &rect, const QString &fieldName, const QString &outputPath) override;
    bool addDateField(const QString &pdfFilePath, int pageIndex, const QRectF &rect, const QString &fieldName, const QString &outputPath) override;
    bool addNumericField(const QString &pdfFilePath, int pageIndex, const QRectF &rect, const QString &fieldName, const QString &outputPath) override;
    bool addCheckBox(const QString &pdfFilePath, int pageIndex, const QRectF &rect, const QString &fieldName, const QString &outputPath) override;
    bool addRadioButton(const QString &pdfFilePath, int pageIndex, const QRectF &rect, const QString &fieldName, const QString &outputPath) override;
    bool addDropdown(const QString &pdfFilePath, int pageIndex, const QRectF &rect, const QString &fieldName, const QStringList &options, const QString &outputPath) override;
    bool addListBox(const QString &pdfFilePath, int pageIndex, const QRectF &rect, const QString &fieldName, const QStringList &options, bool multiSelect, const QString &outputPath) override;

    bool exportFormData(const QString &pdfFilePath, const QString &outputPath, const QString &format) override;
    bool importFormData(const QString &pdfFilePath, const QString &dataFilePath, const QString &outputPath) override;
    bool flattenForm(const QString &pdfFilePath, const QString &outputPath) override;

private:
    class Private;
    std::unique_ptr<Private> d;
};
