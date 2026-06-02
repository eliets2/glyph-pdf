// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QString>
#include <QStringList>
#include <QStringList>
#include <QVariantMap>
#include <QRectF>
#include <QList>

struct FieldSuggestion {
    QRectF rect;
    QString type;
    QString suggestedName;
};

class IFormManager {
public:
    virtual ~IFormManager() = default;
    virtual bool extractFormFields(const QString &pdfFilePath) = 0;
    virtual bool fillForm(const QString &pdfFilePath, const QVariantMap &fieldData, const QString &outputPath) = 0;
    virtual bool hasXfaForms(const QString &pdfFilePath) = 0;
    virtual bool addTextField(const QString &pdfFilePath, int pageIndex, const QRectF &rect,
                               const QString &fieldName, const QString &outputPath) = 0;
    virtual bool addDateField(const QString &pdfFilePath, int pageIndex, const QRectF &rect,
                               const QString &fieldName, const QString &outputPath) = 0;
    virtual bool addNumericField(const QString &pdfFilePath, int pageIndex, const QRectF &rect,
                               const QString &fieldName, const QString &outputPath) = 0;
    virtual bool addCheckBox(const QString &pdfFilePath, int pageIndex, const QRectF &rect,
                              const QString &fieldName, const QString &outputPath) = 0;
    virtual bool addRadioButton(const QString &pdfFilePath, int pageIndex, const QRectF &rect,
                                 const QString &fieldName, const QString &outputPath) = 0;
    virtual bool addDropdown(const QString &pdfFilePath, int pageIndex, const QRectF &rect,
                              const QString &fieldName, const QStringList &options, const QString &outputPath) = 0;
    virtual bool addListBox(const QString &pdfFilePath, int pageIndex, const QRectF &rect,
                             const QString &fieldName, const QStringList &options, bool multiSelect, const QString &outputPath) = 0;

    virtual bool createButton(const QString &pdfFilePath, int pageIndex, const QRectF &rect,
                              const QString &caption, const QString &action, const QString &outputPath) = 0;

    virtual QList<FieldSuggestion> autoDetectFields(const QString &pdfFilePath, int pageIndex) = 0;

    // Import / Export / Flatten
    virtual bool exportFormData(const QString &pdfFilePath, const QString &outputPath, const QString &format) = 0; // format: "FDF" or "CSV"
    virtual bool importFormData(const QString &pdfFilePath, const QString &dataFilePath, const QString &outputPath) = 0;
    virtual bool flattenForm(const QString &pdfFilePath, const QString &outputPath) = 0;
protected:
    IFormManager() = default;
    IFormManager(const IFormManager&) = delete;
    IFormManager& operator=(const IFormManager&) = delete;
};
