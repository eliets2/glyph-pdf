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

/// R02 (audit F09): complete snapshot of one AcroForm field's supported,
/// user-editable state, read BEFORE the first mutation so undo can restore
/// exactly what was there.
///
/// Documented meanings (engine and UI must agree on one):
///   - `value` / `valuePresent`  → the field's CURRENT value, PDF /V. The
///     properties panel's "Default" row edits /V (it has always been applied
///     through fillForm's SetText), so the panel's "default value" IS /V.
///   - `defaultValue` / `defaultPresent` → the PDF /DV default-value key.
///     /DV is captured and restored losslessly by undo but is NOT edited by
///     the properties panel.
///   - `tooltip` / `tooltipPresent` → /TU (read-only UI text).
///   - `required` → /Ff bit position 2.
///   - a `*Present == false` member means the key is ABSENT — distinct from
///     an explicitly empty string (present but zero length).
/// Non-text fields: `value` is captured as a raw string ("Yes"/"Off" for
/// checkbox state); applyFieldSnapshot rewrites /V only for text boxes and
/// checkboxes, which are the types the properties panel edits.
struct FormFieldSnapshot {
    bool found = false;              ///< explicit missing-field resolution
    QString name;                    ///< full name at capture time
    bool tooltipPresent = false;
    QString tooltip;
    bool required = false;
    bool valuePresent = false;
    QString value;
    bool defaultPresent = false;
    QString defaultValue;
};

class IFormManager {
public:
    virtual ~IFormManager() = default;
    virtual bool extractFormFields(const QString &pdfFilePath) = 0;
    virtual bool fillForm(const QString &pdfFilePath, const QVariantMap &fieldData, const QString &outputPath, bool lockFields = true, QStringList *unsupportedFields = nullptr) = 0;
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

    /// Add a read-only text field whose value is computed by a JavaScript/AcroForm
    /// calculation. `expression` is the JS calculation string (e.g.
    /// "AFSimple_Calculate('SUM', new Array('field1','field2'))"). The field is
    /// wired via /AA /C and registered in the AcroForm /CO calculation-order array.
    virtual bool addCalculatedField(const QString &pdfFilePath, int pageIndex,
                                    const QRectF &rect, const QString &fieldName,
                                    const QString &expression,
                                    const QString &outputPath) = 0;

    /// §9.6 P0: persist field metadata as real PDF dictionaries —
    /// tooltip → /TU (read-only UI text), required → /Ff bit position 2.
    virtual bool setFieldMetadata(const QString &pdfFilePath, const QString &fieldName,
                                  const QString &tooltip, bool required,
                                  const QString &outputPath) = 0;

    /// R02 (F09): read the complete supported property snapshot of the named
    /// field. If the document has no such field the returned snapshot has
    /// found == false (explicit resolution). Duplicate full names are a spec
    /// violation; the FIRST occurrence wins and is the one later mutations
    /// address.
    virtual FormFieldSnapshot captureFieldSnapshot(const QString &pdfFilePath, const QString &fieldName) = 0;

    /// R02 (F09): apply value (/V), tooltip (/TU) and required (/Ff bit 2)
    /// from `target` as ONE transactional mutation persisted through the R01
    /// safe-save boundary — value and metadata cannot partially persist.
    /// A snapshot with valuePresent == false clears /V (absent), an explicitly
    /// empty `value` writes /V as "". Returns false (writing nothing) when the
    /// field is missing or the save fails.
    virtual bool applyFieldSnapshot(const QString &pdfFilePath, const FormFieldSnapshot &target, const QString &outputPath) = 0;

    virtual QList<FieldSuggestion> autoDetectFields(const QString &pdfFilePath, int pageIndex) = 0;

    // Field mutation (persist changes to the PDF on disk)
    /// Remove the named AcroForm field from the PDF and write to outputPath.
    /// Returns false if the field is not found or a save error occurs.
    virtual bool removeFieldByName(const QString &pdfFilePath, const QString &fieldName, const QString &outputPath) = 0;

    /// Update the /Rect of the named widget annotation to newRect (PDF user-space coordinates).
    /// Converts from Qt widget coordinates (origin top-left) to PDF coordinates (origin bottom-left).
    virtual bool updateFieldRect(const QString &pdfFilePath, const QString &fieldName,
                                 int pageIndex, const QRectF &newRect, const QString &outputPath) = 0;

    /// Return a list of all AcroForm field names in the document (all pages).
    virtual QStringList listFields(const QString &pdfFilePath) = 0;

    /// Persist the tab order to the AcroForm /CO array.
    /// orderedNames: field full names in desired tab order.
    /// Fields not in orderedNames are appended after the ordered set.
    virtual bool setTabOrder(const QString &pdfFilePath, const QStringList &orderedNames, const QString &outputPath) = 0;

    // Import / Export / Flatten
    virtual bool exportFormData(const QString &pdfFilePath, const QString &outputPath, const QString &format) = 0; // format: "FDF" or "CSV"
    virtual bool importFormData(const QString &pdfFilePath, const QString &dataFilePath, const QString &outputPath, QStringList *unsupportedFields = nullptr) = 0;
    virtual bool flattenForm(const QString &pdfFilePath, const QString &outputPath) = 0;
protected:
    IFormManager() = default;
    IFormManager(const IFormManager&) = delete;
    IFormManager& operator=(const IFormManager&) = delete;
};
