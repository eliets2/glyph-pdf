// SPDX-License-Identifier: Apache-2.0
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

    // ── R01 test seam: deterministic save-boundary failure injection ────────
    // Every form mutator persists through ONE shared transactional boundary:
    // serialize to a unique temp candidate, close the writer, reopen and
    // validate the candidate, then commit to the destination (QSaveFile).
    // Tests can force the boundary to fail at a given stage without relying
    // on filesystem permissions. The setting is sticky for the process;
    // tests must reset it to None. Failure at any stage leaves the
    // destination file byte-identical and the operation returns false.
    enum class SaveFault { None = 0, CandidateSave, Validation, Commit };
    static void setSaveFaultForTesting(SaveFault fault);
    static SaveFault saveFaultForTesting();

    // Map AcroForm dictionaries to UI Widgets using PoDoFo/qpdf
    bool extractFormFields(const QString &pdfFilePath) override;
    
    // Fill out and flatten AcroForms
    bool fillForm(const QString &pdfFilePath, const QVariantMap &fieldData, const QString &outputPath, bool lockFields = true, QStringList *unsupportedFields = nullptr) override;
    
    // Check if the document has XFA forms
    bool hasXfaForms(const QString &pdfFilePath) override;

    bool addTextField(const QString &pdfFilePath, int pageIndex, const QRectF &rect, const QString &fieldName, const QString &outputPath) override;
    bool addDateField(const QString &pdfFilePath, int pageIndex, const QRectF &rect, const QString &fieldName, const QString &outputPath) override;
    bool addNumericField(const QString &pdfFilePath, int pageIndex, const QRectF &rect, const QString &fieldName, const QString &outputPath) override;
    bool addCheckBox(const QString &pdfFilePath, int pageIndex, const QRectF &rect, const QString &fieldName, const QString &outputPath) override;
    bool addRadioButton(const QString &pdfFilePath, int pageIndex, const QRectF &rect, const QString &fieldName, const QString &outputPath) override;
    bool addDropdown(const QString &pdfFilePath, int pageIndex, const QRectF &rect, const QString &fieldName, const QStringList &options, const QString &outputPath) override;
    bool addListBox(const QString &pdfFilePath, int pageIndex, const QRectF &rect, const QString &fieldName, const QStringList &options, bool multiSelect, const QString &outputPath) override;

    bool createButton(const QString &pdfFilePath, int pageIndex, const QRectF &rect, const QString &caption, const QString &action, const QString &outputPath) override;
    bool addCalculatedField(const QString &pdfFilePath, int pageIndex, const QRectF &rect,
                            const QString &fieldName, const QString &expression,
                            const QString &outputPath) override;

    /// §9.6 P0: persist tooltip (/TU) and required (/Ff bit 2) as real PDF data.
    bool setFieldMetadata(const QString &pdfFilePath, const QString &fieldName,
                          const QString &tooltip, bool required,
                          const QString &outputPath) override;

    /// R02 (F09): full property snapshot + one transactional apply.
    FormFieldSnapshot captureFieldSnapshot(const QString &pdfFilePath, const QString &fieldName) override;
    bool applyFieldSnapshot(const QString &pdfFilePath, const FormFieldSnapshot &target, const QString &outputPath) override;

    QList<FieldSuggestion> autoDetectFields(const QString &pdfFilePath, int pageIndex) override;

    bool removeFieldByName(const QString &pdfFilePath, const QString &fieldName, const QString &outputPath) override;
    bool updateFieldRect(const QString &pdfFilePath, const QString &fieldName,
                         int pageIndex, const QRectF &newRect, const QString &outputPath) override;
    QStringList listFields(const QString &pdfFilePath) override;
    bool setTabOrder(const QString &pdfFilePath, const QStringList &orderedNames, const QString &outputPath) override;

    bool exportFormData(const QString &pdfFilePath, const QString &outputPath, const QString &format) override;
    bool importFormData(const QString &pdfFilePath, const QString &dataFilePath, const QString &outputPath, QStringList *unsupportedFields = nullptr) override;
    bool flattenForm(const QString &pdfFilePath, const QString &outputPath) override;

private:
    class Private;
    std::unique_ptr<Private> d;
};
