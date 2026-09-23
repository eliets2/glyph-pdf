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

/// Phase-1 form-JS (run-side Calculate/Format): one honest, field-attributed
/// failure from the /AA /C calculate cascade. The affected field keeps its
/// committed value — failures are reported, never silently swallowed and
/// never a wrong computed value.
struct FormJsFailure {
    QString fieldName;
    QString kind;    // "timeout" | "memory" | "syntax" | "exception" | "rejected" | "engine"
    QString reason;  // user-presentable reason
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

/// R18(f): ONE /AA /K (Keystroke) evaluation for the Qt line-edit layer —
/// the form field's script gate on a text-changing edit BEFORE it takes
/// effect (Acrobat: the event fires as the user types, willCommit=false).
struct FormKeystrokeResult {
    bool ran = false;         // false = no runnable /AA /K (typing always stands)
    bool allowed = true;      // rc=false or any script failure → false (fail closed)
    QString valueToApply;     // non-empty = the script TRANSFORMED the text
    FormJsFailure failure;    // field-attributed reason when !allowed
};

class IFormManager {
public:
    virtual ~IFormManager() = default;
    virtual bool extractFormFields(const QString &pdfFilePath) = 0;
    /// Fill out and save. When non-null, `jsFailures` receives one entry per
    /// calculated field (/AA /C) whose script failed during the in-transaction
    /// calculate cascade — the user value still persists; failed calculated
    /// fields keep their committed value and are reported, never miscomputed.
    virtual bool fillForm(const QString &pdfFilePath, const QVariantMap &fieldData, const QString &outputPath, bool lockFields = true, QStringList *unsupportedFields = nullptr, QList<FormJsFailure> *jsFailures = nullptr) = 0;
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
    /// Phase-1 form-JS: the calculate cascade runs inside the same
    /// transaction; `jsFailures` (optional) reports per-field script failures.
    virtual bool applyFieldSnapshot(const QString &pdfFilePath, const FormFieldSnapshot &target, const QString &outputPath, QList<FormJsFailure> *jsFailures = nullptr) = 0;

    /// Phase-1 form-JS inspection (no execution): does the named field carry
    /// an /AA /C (Calculate) or /AA /F (Format) JavaScript action?
    virtual bool fieldHasCalculateScript(const QString &pdfFilePath, const QString &fieldName) = 0;
    virtual bool fieldHasFormatScript(const QString &pdfFilePath, const QString &fieldName) = 0;

    /// Phase-1 form-JS display pass: runs the field's /AA /F (Format) script
    /// as a DISPLAY-ONLY evaluation and returns the formatted presentation
    /// value. /V is never written. Returns a null QString when the field has
    /// no format script; on script failure returns the unformatted value and
    /// fills `failure` (field-attributed, honest).
    virtual QString formatFieldValue(const QString &pdfFilePath, const QString &fieldName, FormJsFailure *failure = nullptr) = 0;

    /// R18(f) Keystroke (/AA /K): runs the named field's keystroke script for
    /// ONE text-changing edit (`valueBefore` = field text before the edit,
    /// `change` = the inserted/replaced text, [selStart, selEnd) = the range
    /// of valueBefore it replaces — what AFMergeChange splices). Same
    /// caller-owned 250 ms event budget as the other form-JS events. No
    /// runnable script → ran=false (typing stands); rc=false or any script
    /// failure → allowed=false (the edit is rejected, fail closed);
    /// valueToApply carries the script-transformed text when it set one.
    virtual FormKeystrokeResult runKeystrokeEvent(const QString &pdfFilePath, const QString &fieldName,
                                                  const QString &valueBefore, const QString &change,
                                                  int selStart, int selEnd, FormJsFailure *failure = nullptr) = 0;

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

    /// Persist the keyboard TAB ORDER to each page's /Annots array (the named
    /// fields' widgets first, in requested order; every other annotation keeps
    /// its relative order) and declare /Tabs /W on touched pages that carry no
    /// author-declared tab order. The AcroForm /CO CALCULATION order is the
    /// document author's dependency order and is NEVER modified here (R18(b):
    /// the old implementation wrote the tab request into /CO — it changed when
    /// calculated fields recompute, not how tabbing works).
    /// orderedNames: field full names in desired tab order.
    /// Fields not in orderedNames keep their relative widget order after the
    /// ordered set. Returns false when no requested field exists or the save
    /// transaction fails.
    virtual bool setTabOrder(const QString &pdfFilePath, const QStringList &orderedNames, const QString &outputPath) = 0;

    // Import / Export / Flatten
    virtual bool exportFormData(const QString &pdfFilePath, const QString &outputPath, const QString &format) = 0; // format: "FDF" or "CSV"
    /// Import lands on fillForm (and therefore on the in-transaction calculate
    /// cascade); `jsFailures` (optional) reports calculated-field script failures.
    virtual bool importFormData(const QString &pdfFilePath, const QString &dataFilePath, const QString &outputPath, QStringList *unsupportedFields = nullptr, QList<FormJsFailure> *jsFailures = nullptr) = 0;
    virtual bool flattenForm(const QString &pdfFilePath, const QString &outputPath) = 0;
protected:
    IFormManager() = default;
    IFormManager(const IFormManager&) = delete;
    IFormManager& operator=(const IFormManager&) = delete;
};
