// SPDX-License-Identifier: Apache-2.0
#include "engines/FormManager.h"
#include "engines/podofo/PdfStringEscape.h"
#include <memory>
#include <functional>
#include <map>
#include <QDebug>
#include <podofo/podofo.h>
#include <QString>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QTemporaryFile>
#include <QTextStream>
#include <QRegularExpression>

class FormManager::Private {
public:
    // PoDoFo AcroForm extraction state
};

// ── R01: the shared form-save boundary ───────────────────────────────────────
//
// Audit F01 (P1): every FormManager mutator used to finish with a direct
// `doc.Save(outputPath)`. PoDoFo keeps the input stream of a loaded
// PdfMemDocument open for deferred object parsing, so saving to the SAME path
// truncates the file out from under the parser mid-write ("Object and
// generation number cannot be read") — a valid 15,257-byte, text-bearing PDF
// was reduced to zero bytes and the operation reported false. Direct writes to
// a separate destination had the mirrored hazard: a mid-write failure (e.g. a
// full disk, an open handle) destroyed the pre-existing destination, and
// setTabOrder additionally used a fixed ".tmp" name, removed the destination
// up front, ignored rename failure and returned success.
//
// Every mutator below now persists through runFormSaveTransaction():
//   1. apply the mutation to the in-memory document,
//   2. serialize the COMPLETE mutation to a unique temp candidate PDF
//      (never the destination — no fragile path-equality checks),
//   3. close the PoDoFo writer and every source handle it owns,
//   4. reopen the candidate and validate it: readable PDF, expected page
//      count, and the requested change actually present,
//   5. commit the validated bytes to the destination with a bounded copy into
//      QSaveFile + a checked commit() — the original is never deleted before
//      the atomic rename, and if the replacement is blocked (open handle) the
//      operation fails with the original byte-identical.
// There is deliberately NO direct-write fallback.
namespace {

// Deterministic test seam (see FormManager::setSaveFaultForTesting).
FormManager::SaveFault g_saveFaultForTesting = FormManager::SaveFault::None;

// Thrown by a mutator to abort a transaction before any bytes are written.
struct SaveAbort {
    QString reason;
};

// Reads the mutation and re-checks the reopened candidate.
using FieldMutator = std::function<void(PoDoFo::PdfMemDocument&)>;
using ChangeValidator = std::function<bool(PoDoFo::PdfMemDocument&)>;

QString pdfErrorText(const PoDoFo::PdfError& e)
{
    return QStringLiteral("PoDoFo error: %1").arg(QString::fromLatin1(e.what()));
}

// RAII removal of this operation's candidate file on every path.
class CandidateFileGuard {
public:
    explicit CandidateFileGuard(const QString& path) : m_path(path) {}
    ~CandidateFileGuard() { if (!m_path.isEmpty()) QFile::remove(m_path); }
    CandidateFileGuard(const CandidateFileGuard&) = delete;
    CandidateFileGuard& operator=(const CandidateFileGuard&) = delete;
private:
    QString m_path;
};

// Reserve a unique candidate path in the system temp dir. The handle is
// released before PoDoFo writes so the writer owns the file exclusively.
bool createUniqueCandidate(QString* out, QString* err)
{
    QTemporaryFile tmp(QDir::tempPath() + QStringLiteral("/glyphpdf-XXXXXX.pdf"));
    tmp.setAutoRemove(false);
    if (!tmp.open()) {
        if (err) *err = QStringLiteral("could not create unique candidate PDF: %1").arg(tmp.errorString());
        return false;
    }
    *out = tmp.fileName();
    tmp.close();
    return true;
}

// Bounded copy of the validated candidate into QSaveFile + checked commit().
// QSaveFile writes a hidden temp in the destination's directory and atomically
// renames over the destination at commit(); a failed or canceled commit never
// touches the original. Note PoDoFo is never handed QSaveFile::fileName().
bool commitCandidateToDestination(const QString& candidate, const QString& destPath, QString* err)
{
    QFile src(candidate);
    if (!src.open(QIODevice::ReadOnly)) {
        if (err) *err = QStringLiteral("validated candidate became unreadable: %1").arg(src.errorString());
        return false;
    }
    const qint64 expected = src.size();

    QSaveFile out(destPath);
    if (!out.open(QIODevice::WriteOnly)) {
        if (err) *err = QStringLiteral("cannot open destination for safe write: %1").arg(out.errorString());
        return false;
    }

    qint64 copied = 0;
    char buf[65536];
    while (copied < expected) {
        const qint64 want = qMin<qint64>(static_cast<qint64>(sizeof(buf)), expected - copied);
        const qint64 got = src.read(buf, want);
        if (got <= 0) {
            out.cancelWriting();
            if (err) *err = QStringLiteral("short read from validated candidate");
            return false;
        }
        if (out.write(buf, got) != got) {
            out.cancelWriting();
            if (err) *err = QStringLiteral("cannot write destination bytes: %1").arg(out.errorString());
            return false;
        }
        copied += got;
    }

    if (g_saveFaultForTesting == FormManager::SaveFault::Commit) {
        out.cancelWriting();
        if (err) *err = QStringLiteral("injected commit failure (test seam)");
        return false;
    }
    if (!out.commit()) {
        // Open-handle replacement failure lands here: reported as failure, the
        // original destination is byte-identical.
        if (err) *err = QStringLiteral("commit to destination failed: %1").arg(out.errorString());
        return false;
    }
    return true;
}

// Locate a field by full name on a (freshly loaded) document. If a document
// illegally carries duplicate full names, the FIRST occurrence wins — the
// same field a subsequent mutation would address.
const PoDoFo::PdfField* findFieldByName(const PoDoFo::PdfMemDocument& doc, const QString& name)
{
    auto* acroForm = doc.GetAcroForm();
    if (!acroForm) return nullptr;
    for (unsigned i = 0; i < acroForm->GetFieldCount(); ++i) {
        auto& f = acroForm->GetFieldAt(i);
        if (QString::fromStdString(f.GetFullName()) == name) return &f;
    }
    return nullptr;
}

bool fieldIsOfType(const PoDoFo::PdfMemDocument& doc, const QString& name, PoDoFo::PdfFieldType type)
{
    const PoDoFo::PdfField* f = findFieldByName(doc, name);
    return f && f->GetType() == type;
}

// The shared boundary. srcPath and destPath may be the same file; the source
// is only ever read, the destination only ever replaced by the atomic commit.
bool runFormSaveTransaction(const QString& srcPath,
                            const QString& destPath,
                            const FieldMutator& mutate,
                            const ChangeValidator& validate,
                            QString* err)
{
    if (g_saveFaultForTesting == FormManager::SaveFault::CandidateSave) {
        if (err) *err = QStringLiteral("injected candidate-save failure (test seam)");
        return false;
    }

    QString candidate;
    try {
        if (!createUniqueCandidate(&candidate, err)) return false;
        CandidateFileGuard guard(candidate);

        unsigned sourcePageCount = 0;
        try {
            PoDoFo::PdfMemDocument doc;
            doc.Load(srcPath.toUtf8().constData());
            sourcePageCount = doc.GetPages().GetCount();

            mutate(doc);

            // Serialize the complete mutation to the candidate. The
            // destination is not touched here even when it equals srcPath.
            doc.Save(candidate.toUtf8().constData());
        } catch (const SaveAbort& a) {
            if (err) *err = a.reason;
            return false;
        }
        // `doc` is destroyed above: the PoDoFo writer and any source handles
        // it owns are closed BEFORE the candidate is validated and committed.

        if (g_saveFaultForTesting == FormManager::SaveFault::Validation) {
            if (err) *err = QStringLiteral("injected validation failure (test seam)");
            return false;
        }
        try {
            PoDoFo::PdfMemDocument reopened;
            reopened.Load(candidate.toUtf8().constData());
            if (reopened.GetPages().GetCount() != sourcePageCount) {
                if (err) *err = QStringLiteral("candidate page count changed (%1 -> %2)")
                                     .arg(sourcePageCount).arg(reopened.GetPages().GetCount());
                return false;
            }
            if (validate && !validate(reopened)) {
                if (err) *err = QStringLiteral("candidate rejected: requested change not present after save");
                return false;
            }
        } catch (const PoDoFo::PdfError& e) {
            if (err) *err = QStringLiteral("candidate is not a valid PDF: %1").arg(pdfErrorText(e));
            return false;
        }

        return commitCandidateToDestination(candidate, destPath, err);
    } catch (const PoDoFo::PdfError& e) {
        if (err) *err = pdfErrorText(e);
        return false;
    }
}

} // namespace

void FormManager::setSaveFaultForTesting(SaveFault fault)
{
    g_saveFaultForTesting = fault;
}

FormManager::SaveFault FormManager::saveFaultForTesting()
{
    return g_saveFaultForTesting;
}

// ── R02 (F09): complete field snapshots ──────────────────────────────────────
// `defaultVal` in the properties panel means the field's CURRENT value (/V) —
// the panel has always applied it through fillForm's SetText. The PDF /DV key
// is captured and restored losslessly by undo, but nothing in the UI edits it.

FormFieldSnapshot FormManager::captureFieldSnapshot(const QString &pdfFilePath, const QString &fieldName)
{
    FormFieldSnapshot snap;
    snap.name = fieldName;
    try {
        PoDoFo::PdfMemDocument doc;
        doc.Load(pdfFilePath.toUtf8().constData());
        auto* acroForm = doc.GetAcroForm();
        if (!acroForm) return snap; // found == false: explicit missing-field resolution
        for (unsigned i = 0; i < acroForm->GetFieldCount(); ++i) {
            auto& field = acroForm->GetFieldAt(i);
            if (QString::fromStdString(field.GetFullName()) != fieldName) continue;
            snap.found = true;

            const PoDoFo::PdfDictionary& dict = (field.GetObject)().GetDictionary();
            if (const PoDoFo::PdfObject* tu = dict.FindKey("TU"); tu && tu->IsString()) {
                snap.tooltipPresent = true;
                snap.tooltip = QString::fromUtf8(tu->GetString().GetString().data(),
                                                 static_cast<int>(tu->GetString().GetString().size()));
            }
            if (const PoDoFo::PdfObject* ff = dict.FindKey("Ff"); ff && ff->IsNumber())
                snap.required = (ff->GetNumber() & 2) != 0;
            if (const PoDoFo::PdfObject* dv = dict.FindKey("DV"); dv && dv->IsString()) {
                snap.defaultPresent = true;
                snap.defaultValue = QString::fromUtf8(dv->GetString().GetString().data(),
                                                      static_cast<int>(dv->GetString().GetString().size()));
            }

            // Current value /V — type-specific so the merged field/widget
            // dictionary lookup follows PoDoFo's own semantics.
            switch (field.GetType()) {
                case PoDoFo::PdfFieldType::TextBox: {
                    if (auto* t = dynamic_cast<PoDoFo::PdfTextBox*>(&field)) {
                        auto v = t->GetText(); // nullable: non-const accessors
                        snap.valuePresent = v.has_value();
                        if (snap.valuePresent)
                            snap.value = QString::fromUtf8(v.value().GetString().data(),
                                                           static_cast<int>(v.value().GetString().size()));
                    }
                    break;
                }
                case PoDoFo::PdfFieldType::CheckBox: {
                    if (auto* c = dynamic_cast<PoDoFo::PdfCheckBox*>(&field)) {
                        snap.valuePresent = dict.HasKey("V");
                        snap.value = c->IsChecked() ? QStringLiteral("Yes") : QStringLiteral("Off");
                    }
                    break;
                }
                default: {
                    // Other types: capture the raw /V string form when present.
                    if (const PoDoFo::PdfObject* v = dict.FindKey("V"); v && v->IsString()) {
                        snap.valuePresent = true;
                        snap.value = QString::fromUtf8(v->GetString().GetString().data(),
                                                       static_cast<int>(v->GetString().GetString().size()));
                    }
                    break;
                }
            }
            break; // duplicates: first occurrence wins (documented policy)
        }
    } catch (const PoDoFo::PdfError& e) {
        qWarning() << "captureFieldSnapshot error:" << e.what();
        snap.found = false;
    }
    return snap;
}

bool FormManager::applyFieldSnapshot(const QString &pdfFilePath, const FormFieldSnapshot &target, const QString &outputPath)
{
    if (!target.found) {
        qWarning() << "applyFieldSnapshot: refusing to apply a not-found snapshot for" << target.name;
        return false;
    }
    QString err;
    const bool ok = runFormSaveTransaction(
        pdfFilePath, outputPath,
        [&](PoDoFo::PdfMemDocument& doc) {
            auto* acroForm = doc.GetAcroForm();
            if (!acroForm)
                throw SaveAbort{QStringLiteral("no AcroForm in %1").arg(pdfFilePath)};

            bool found = false;
            for (unsigned i = 0; i < acroForm->GetFieldCount(); ++i) {
                auto& field = acroForm->GetFieldAt(i);
                if (QString::fromStdString(field.GetFullName()) != target.name) continue;
                found = true;
                PoDoFo::PdfDictionary& dict = field.GetDictionary();

                // /TU — present in the snapshot wins; absence removes the key.
                if (target.tooltipPresent)
                    dict.AddKey(PoDoFo::PdfName("TU"), PoDoFo::PdfString(target.tooltip.toStdString()));
                else
                    dict.RemoveKey("TU");

                // /Ff bit 2 — read-modify-write so unrelated flag bits survive.
                int flags = 0;
                if (const PoDoFo::PdfObject* ffObj = dict.FindKey("Ff"); ffObj && ffObj->IsNumber())
                    flags = static_cast<int>(ffObj->GetNumber());
                if (target.required) flags |= (1 << 1);
                else                 flags &= ~(1 << 1);
                dict.AddKey(PoDoFo::PdfName("Ff"), PoDoFo::PdfVariant(static_cast<int64_t>(flags)));

                // /V — one transactional write for the types the panel edits;
                // absence vs explicitly-empty is preserved exactly.
                switch (field.GetType()) {
                    case PoDoFo::PdfFieldType::TextBox: {
                        if (auto* t = dynamic_cast<PoDoFo::PdfTextBox*>(&field)) {
                            if (target.valuePresent)
                                t->SetText(PoDoFo::PdfString(target.value.toStdString()));
                            else
                                dict.RemoveKey("V");
                        }
                        break;
                    }
                    case PoDoFo::PdfFieldType::CheckBox: {
                        if (auto* c = dynamic_cast<PoDoFo::PdfCheckBox*>(&field)) {
                            if (target.valuePresent)
                                c->SetChecked(target.value == QLatin1String("Yes"));
                            else
                                dict.RemoveKey("V");
                        }
                        break;
                    }
                    default:
                        break; // documented: /V of other field types is not rewritten
                }
                break; // duplicates: first occurrence wins (documented policy)
            }
            if (!found)
                throw SaveAbort{QStringLiteral("field not found: %1").arg(target.name)};
        },
        [&](PoDoFo::PdfMemDocument& reopened) {
            const PoDoFo::PdfField* f = findFieldByName(reopened, target.name);
            if (!f) return false;
            const PoDoFo::PdfDictionary& dict = (f->GetObject)().GetDictionary();

            if (target.tooltipPresent) {
                const PoDoFo::PdfObject* tu = dict.FindKey("TU");
                if (!tu || !tu->IsString()
                    || std::string(tu->GetString().GetString().data(), tu->GetString().GetString().size())
                           != target.tooltip.toStdString()) return false;
            } else if (dict.HasKey("TU")) {
                return false;
            }

            const int flags = [&] {
                const PoDoFo::PdfObject* ff = dict.FindKey("Ff");
                return (ff && ff->IsNumber()) ? static_cast<int>(ff->GetNumber()) : 0;
            }();
            if (((flags & (1 << 1)) != 0) != target.required) return false;

            switch (f->GetType()) {
                case PoDoFo::PdfFieldType::TextBox: {
                    auto* t = dynamic_cast<const PoDoFo::PdfTextBox*>(f);
                    if (!t) return false;
                    auto v = t->GetText(); // nullable: non-const accessors
                    if (target.valuePresent) {
                        if (!v.has_value()
                            || std::string(v.value().GetString().data(), v.value().GetString().size())
                                   != target.value.toStdString()) return false;
                    } else if (v.has_value()) {
                        return false; // /V must be absent
                    }
                    break;
                }
                case PoDoFo::PdfFieldType::CheckBox: {
                    auto* c = dynamic_cast<const PoDoFo::PdfCheckBox*>(f);
                    if (!c) return false;
                    if (target.valuePresent) {
                        if (c->IsChecked() != (target.value == QLatin1String("Yes"))) return false;
                    } else if (dict.HasKey("V")) {
                        return false;
                    }
                    break;
                }
                default:
                    break;
            }
            return true;
        },
        &err);
    if (!ok) qWarning() << "applyFieldSnapshot error:" << err;
    return ok;
}

FormManager::FormManager() : d(std::make_unique<Private>())
{
}

FormManager::~FormManager() = default;

bool FormManager::extractFormFields(const QString &pdfFilePath)
{
    qDebug() << "Extracting AcroForm objects from:" << pdfFilePath;
    try {
        PoDoFo::PdfMemDocument doc;
        doc.Load(pdfFilePath.toUtf8().constData());
        auto* acroForm = doc.GetAcroForm();
        if (acroForm) {
            for (unsigned i = 0; i < acroForm->GetFieldCount(); ++i) {
                auto& field = acroForm->GetFieldAt(i);
                QString name = QString::fromStdString(field.GetFullName());
                qDebug() << "Found field:" << name;
            }
            return true;
        } else {
            qDebug() << "No AcroForm found.";
            return false;
        }
    } catch (const PoDoFo::PdfError& e) {
        qDebug() << "PoDoFo error during form extraction.";
        return false;
    }
}

bool FormManager::fillForm(const QString &pdfFilePath, const QVariantMap &fieldData, const QString &outputPath, bool lockFields, QStringList *unsupportedFields)
{
    qDebug() << "Filling form data at:" << outputPath;

    // §9.6 P0: requested fields that could not be applied (unknown name, or a
    // type fillForm cannot set) are reported back instead of vanishing quietly.
    QStringList appliedNames;

    const bool ok = runFormSaveTransaction(
        pdfFilePath, outputPath,
        [&](PoDoFo::PdfMemDocument& doc) {
            auto* acroForm = doc.GetAcroForm();
            if (!acroForm) {
                throw SaveAbort{QStringLiteral("no AcroForm found in document")};
            }
            for (unsigned i = 0; i < acroForm->GetFieldCount(); ++i) {
                auto& field = acroForm->GetFieldAt(i);
                QString name = QString::fromStdString(field.GetFullName());
                if (!fieldData.contains(name)) continue;
                appliedNames.append(name);

                QVariant val = fieldData.value(name);

                switch (field.GetType()) {
                    case PoDoFo::PdfFieldType::TextBox: {
                        auto* textField = dynamic_cast<PoDoFo::PdfTextBox*>(&field);
                        if (textField) {
                            textField->SetText(PoDoFo::PdfString(val.toString().toStdString()));
                        }
                        break;
                    }
                    case PoDoFo::PdfFieldType::CheckBox: {
                        auto* checkBox = dynamic_cast<PoDoFo::PdfCheckBox*>(&field);
                        if (checkBox) {
                            checkBox->SetChecked(val.toBool());
                        }
                        break;
                    }
                    case PoDoFo::PdfFieldType::ComboBox: {
                        auto* comboBox = dynamic_cast<PoDoFo::PdfComboBox*>(&field);
                        if (comboBox) {
                            int idx = val.toInt();
                            if (idx >= 0 && idx < static_cast<int>(comboBox->GetItemCount())) {
                                comboBox->SetSelectedIndex(idx);
                            }
                        }
                        break;
                    }
                    case PoDoFo::PdfFieldType::ListBox: {
                        auto* listBox = dynamic_cast<PoDoFo::PdfListBox*>(&field);
                        if (listBox) {
                            int idx = val.toInt();
                            if (idx >= 0 && idx < static_cast<int>(listBox->GetItemCount())) {
                                listBox->SetSelectedIndex(idx);
                            }
                        }
                        break;
                    }
                    default:
                        qDebug() << "Skipping unsupported field type for:" << name;
                        if (unsupportedFields) unsupportedFields->append(name);
                        break;
                }

                if (lockFields) field.SetReadOnly(true); // §9.6 P0: only the explicit fill+lock path locks; default-value edits must not silently lock fields
            }
        },
        // Validate the applied values on the reopened candidate. Text and
        // checkbox state round-trip exactly; combo/list selection is
        // viewer-semantic (/V//I), so those are gated on presence + type.
        [&](PoDoFo::PdfMemDocument& reopened) {
            for (const QString& name : appliedNames) {
                const PoDoFo::PdfField* f = findFieldByName(reopened, name);
                if (!f) return false;
                const QVariant val = fieldData.value(name);
                switch (f->GetType()) {
                    case PoDoFo::PdfFieldType::TextBox: {
                        auto* t = dynamic_cast<const PoDoFo::PdfTextBox*>(f);
                        if (!t) return false;
                        auto text = t->GetText(); // nullable: non-const accessors
                        if (!text.has_value()) return false;
                        if (std::string(text.value().GetString().data(), text.value().GetString().size())
                                != val.toString().toStdString()) return false;
                        break;
                    }
                    case PoDoFo::PdfFieldType::CheckBox: {
                        auto* c = dynamic_cast<const PoDoFo::PdfCheckBox*>(f);
                        if (!c || c->IsChecked() != val.toBool()) return false;
                        break;
                    }
                    default:
                        break;
                }
            }
            return true;
        },
        nullptr);

    if (!ok) {
        qWarning() << "fillForm: safe save failed; no destination was modified";
        return false;
    }

    // §9.6 P0: make silent no-ops visible — every requested field that was
    // not applied (unknown name, or Radio/PushButton which fillForm cannot
    // set) is reported back to the caller instead of vanishing quietly.
    if (unsupportedFields) {
        for (auto it = fieldData.constBegin(); it != fieldData.constEnd(); ++it) {
            if (!appliedNames.contains(it.key()) && !unsupportedFields->contains(it.key()))
                unsupportedFields->append(it.key());
        }
    }
    return true;
}

bool FormManager::hasXfaForms(const QString &pdfFilePath)
{
    try {
        PoDoFo::PdfMemDocument doc;
        doc.Load(pdfFilePath.toUtf8().constData());
        auto* acroForm = doc.GetAcroForm();
        if (acroForm) {
            auto dict = acroForm->GetDictionary();
            if (dict.HasKey("XFA")) {
                return true;
            }
        }
    } catch (...) {
        return false;
    }
    return false;
}

bool FormManager::addTextField(const QString &pdfFilePath, int pageIndex, const QRectF &rect, const QString &fieldName, const QString &outputPath)
{
    QString err;
    const bool ok = runFormSaveTransaction(
        pdfFilePath, outputPath,
        [&](PoDoFo::PdfMemDocument& doc) {
            if (pageIndex < 0 || static_cast<unsigned>(pageIndex) >= doc.GetPages().GetCount())
                throw SaveAbort{QStringLiteral("invalid page index")};
            PoDoFo::PdfPage& page = doc.GetPages().GetPageAt(pageIndex);
            PoDoFo::Rect pdfRect(rect.x(), page.GetMediaBox().Height - rect.y() - rect.height(), rect.width(), rect.height());
            auto& field = page.CreateField<PoDoFo::PdfTextBox>(fieldName.toStdString(), pdfRect);
            field.SetText(PoDoFo::PdfString(""));
        },
        [&](PoDoFo::PdfMemDocument& reopened) {
            return fieldIsOfType(reopened, fieldName, PoDoFo::PdfFieldType::TextBox);
        },
        &err);
    if (!ok) qWarning() << "Error adding text field:" << err;
    else qDebug() << "Added text field" << fieldName << "on page" << pageIndex;
    return ok;
}

bool FormManager::addDateField(const QString &pdfFilePath, int pageIndex, const QRectF &rect, const QString &fieldName, const QString &outputPath)
{
    QString err;
    const bool ok = runFormSaveTransaction(
        pdfFilePath, outputPath,
        [&](PoDoFo::PdfMemDocument& doc) {
            if (pageIndex < 0 || static_cast<unsigned>(pageIndex) >= doc.GetPages().GetCount())
                throw SaveAbort{QStringLiteral("invalid page index")};
            PoDoFo::PdfPage& page = doc.GetPages().GetPageAt(pageIndex);
            PoDoFo::Rect pdfRect(rect.x(), page.GetMediaBox().Height - rect.y() - rect.height(), rect.width(), rect.height());

            auto& field = page.CreateField<PoDoFo::PdfTextBox>(fieldName.toStdString(), pdfRect);
            field.SetText(PoDoFo::PdfString(""));

            // Setup AA dictionary for formatting and keystroke
            PoDoFo::PdfDictionary aaDict;

            PoDoFo::PdfDictionary formatAction;
            formatAction.AddKey(PoDoFo::PdfName("S"), PoDoFo::PdfName("JavaScript"));
            formatAction.AddKey(PoDoFo::PdfName("JS"), PoDoFo::PdfString("AFDate_FormatEx(\"yyyy-mm-dd\");"));
            aaDict.AddKey(PoDoFo::PdfName("F"), formatAction);

            PoDoFo::PdfDictionary keystrokeAction;
            keystrokeAction.AddKey(PoDoFo::PdfName("S"), PoDoFo::PdfName("JavaScript"));
            keystrokeAction.AddKey(PoDoFo::PdfName("JS"), PoDoFo::PdfString("AFDate_KeystrokeEx(\"yyyy-mm-dd\");"));
            aaDict.AddKey(PoDoFo::PdfName("K"), keystrokeAction);

            field.GetDictionary().AddKey(PoDoFo::PdfName("AA"), aaDict);
        },
        [&](PoDoFo::PdfMemDocument& reopened) {
            return fieldIsOfType(reopened, fieldName, PoDoFo::PdfFieldType::TextBox);
        },
        &err);
    if (!ok) qWarning() << "Error adding date field:" << err;
    else qDebug() << "Added date field" << fieldName << "on page" << pageIndex;
    return ok;
}

bool FormManager::addNumericField(const QString &pdfFilePath, int pageIndex, const QRectF &rect, const QString &fieldName, const QString &outputPath)
{
    QString err;
    const bool ok = runFormSaveTransaction(
        pdfFilePath, outputPath,
        [&](PoDoFo::PdfMemDocument& doc) {
            if (pageIndex < 0 || static_cast<unsigned>(pageIndex) >= doc.GetPages().GetCount())
                throw SaveAbort{QStringLiteral("invalid page index")};
            PoDoFo::PdfPage& page = doc.GetPages().GetPageAt(pageIndex);
            PoDoFo::Rect pdfRect(rect.x(), page.GetMediaBox().Height - rect.y() - rect.height(), rect.width(), rect.height());

            auto& field = page.CreateField<PoDoFo::PdfTextBox>(fieldName.toStdString(), pdfRect);
            field.SetText(PoDoFo::PdfString(""));

            // Setup AA dictionary for formatting and keystroke
            PoDoFo::PdfDictionary aaDict;

            PoDoFo::PdfDictionary formatAction;
            formatAction.AddKey(PoDoFo::PdfName("S"), PoDoFo::PdfName("JavaScript"));
            formatAction.AddKey(PoDoFo::PdfName("JS"), PoDoFo::PdfString("AFNumber_Format(2, 0, 0, 0, \"\", true);"));
            aaDict.AddKey(PoDoFo::PdfName("F"), formatAction);

            PoDoFo::PdfDictionary keystrokeAction;
            keystrokeAction.AddKey(PoDoFo::PdfName("S"), PoDoFo::PdfName("JavaScript"));
            keystrokeAction.AddKey(PoDoFo::PdfName("JS"), PoDoFo::PdfString("AFNumber_Keystroke(2, 0, 0, 0, \"\", true);"));
            aaDict.AddKey(PoDoFo::PdfName("K"), keystrokeAction);

            field.GetDictionary().AddKey(PoDoFo::PdfName("AA"), aaDict);
        },
        [&](PoDoFo::PdfMemDocument& reopened) {
            return fieldIsOfType(reopened, fieldName, PoDoFo::PdfFieldType::TextBox);
        },
        &err);
    if (!ok) qWarning() << "Error adding numeric field:" << err;
    else qDebug() << "Added numeric field" << fieldName << "on page" << pageIndex;
    return ok;
}

bool FormManager::addCheckBox(const QString &pdfFilePath, int pageIndex, const QRectF &rect, const QString &fieldName, const QString &outputPath)
{
    QString err;
    const bool ok = runFormSaveTransaction(
        pdfFilePath, outputPath,
        [&](PoDoFo::PdfMemDocument& doc) {
            if (pageIndex < 0 || static_cast<unsigned>(pageIndex) >= doc.GetPages().GetCount())
                throw SaveAbort{QStringLiteral("invalid page index")};
            PoDoFo::PdfPage& page = doc.GetPages().GetPageAt(pageIndex);
            PoDoFo::Rect pdfRect(rect.x(), page.GetMediaBox().Height - rect.y() - rect.height(), rect.width(), rect.height());
            auto& field = page.CreateField<PoDoFo::PdfCheckBox>(fieldName.toStdString(), pdfRect);
            field.SetChecked(false);
        },
        [&](PoDoFo::PdfMemDocument& reopened) {
            return fieldIsOfType(reopened, fieldName, PoDoFo::PdfFieldType::CheckBox);
        },
        &err);
    if (!ok) qWarning() << "Error adding checkbox:" << err;
    else qDebug() << "Added checkbox" << fieldName << "on page" << pageIndex;
    return ok;
}

bool FormManager::addRadioButton(const QString &pdfFilePath, int pageIndex, const QRectF &rect, const QString &fieldName, const QString &outputPath)
{
    QString err;
    const bool ok = runFormSaveTransaction(
        pdfFilePath, outputPath,
        [&](PoDoFo::PdfMemDocument& doc) {
            if (pageIndex < 0 || static_cast<unsigned>(pageIndex) >= doc.GetPages().GetCount())
                throw SaveAbort{QStringLiteral("invalid page index")};
            PoDoFo::PdfPage& page = doc.GetPages().GetPageAt(pageIndex);
            PoDoFo::Rect pdfRect(rect.x(), page.GetMediaBox().Height - rect.y() - rect.height(), rect.width(), rect.height());

            // PoDoFo 0.10: Radio buttons should use RadiosInUnison
            // We'll create a generic checkbox-like radio but set flags if supported or just cast
            // For simplicity we create a radio button if CreateField supports it.
            auto& field = page.CreateField<PoDoFo::PdfRadioButton>(fieldName.toStdString(), pdfRect);

            // Add flags if needed
            // PoDoFo doesn't expose RadiosInUnison in high-level sometimes, so we set via dictionary:
            // PoDoFo::PdfDictionary& dict = field.GetDictionary();
            // 1 << 25 is RadiosInUnison in PDF spec for fields
            int flags = 0;
            const PoDoFo::PdfObject* ffObj = field.GetDictionary().FindKey("Ff");
            if (ffObj && ffObj->IsNumber()) {
                flags = static_cast<int>(ffObj->GetNumber());
            }
            flags |= (1 << 25); // RadiosInUnison
            field.GetDictionary().AddKey(PoDoFo::PdfName("Ff"), PoDoFo::PdfVariant(static_cast<int64_t>(flags)));
        },
        [&](PoDoFo::PdfMemDocument& reopened) {
            return fieldIsOfType(reopened, fieldName, PoDoFo::PdfFieldType::RadioButton);
        },
        &err);
    if (!ok) qWarning() << "Error adding radio button:" << err;
    else qDebug() << "Added radio button" << fieldName << "on page" << pageIndex;
    return ok;
}

bool FormManager::addDropdown(const QString &pdfFilePath, int pageIndex, const QRectF &rect, const QString &fieldName, const QStringList &options, const QString &outputPath)
{
    QString err;
    const bool ok = runFormSaveTransaction(
        pdfFilePath, outputPath,
        [&](PoDoFo::PdfMemDocument& doc) {
            if (pageIndex < 0 || static_cast<unsigned>(pageIndex) >= doc.GetPages().GetCount())
                throw SaveAbort{QStringLiteral("invalid page index")};
            PoDoFo::PdfPage& page = doc.GetPages().GetPageAt(pageIndex);
            PoDoFo::Rect pdfRect(rect.x(), page.GetMediaBox().Height - rect.y() - rect.height(), rect.width(), rect.height());

            auto& field = page.CreateField<PoDoFo::PdfComboBox>(fieldName.toStdString(), pdfRect);
            for (const QString &opt : options) {
                field.InsertItem(PoDoFo::PdfString(opt.toStdString()));
            }
            if (!options.isEmpty()) {
                field.SetSelectedIndex(0);
            }

            // Set Edit flag (1 << 18) for combobox
            int flags = 0;
            const PoDoFo::PdfObject* ffObj = field.GetDictionary().FindKey("Ff");
            if (ffObj && ffObj->IsNumber()) {
                flags = static_cast<int>(ffObj->GetNumber());
            }
            flags |= (1 << 18); // Edit flag
            field.GetDictionary().AddKey(PoDoFo::PdfName("Ff"), PoDoFo::PdfVariant(static_cast<int64_t>(flags)));
        },
        [&](PoDoFo::PdfMemDocument& reopened) {
            return fieldIsOfType(reopened, fieldName, PoDoFo::PdfFieldType::ComboBox);
        },
        &err);
    if (!ok) qWarning() << "Error adding dropdown:" << err;
    else qDebug() << "Added dropdown" << fieldName << "with" << options.size() << "options on page" << pageIndex;
    return ok;
}

bool FormManager::addListBox(const QString &pdfFilePath, int pageIndex, const QRectF &rect, const QString &fieldName, const QStringList &options, bool multiSelect, const QString &outputPath)
{
    QString err;
    const bool ok = runFormSaveTransaction(
        pdfFilePath, outputPath,
        [&](PoDoFo::PdfMemDocument& doc) {
            if (pageIndex < 0 || static_cast<unsigned>(pageIndex) >= doc.GetPages().GetCount())
                throw SaveAbort{QStringLiteral("invalid page index")};
            PoDoFo::PdfPage& page = doc.GetPages().GetPageAt(pageIndex);
            PoDoFo::Rect pdfRect(rect.x(), page.GetMediaBox().Height - rect.y() - rect.height(), rect.width(), rect.height());

            auto& field = page.CreateField<PoDoFo::PdfListBox>(fieldName.toStdString(), pdfRect);
            for (const QString &opt : options) {
                field.InsertItem(PoDoFo::PdfString(opt.toStdString()));
            }

            if (multiSelect) {
                int flags = 0;
                const PoDoFo::PdfObject* ffObj = field.GetDictionary().FindKey("Ff");
                if (ffObj && ffObj->IsNumber()) {
                    flags = static_cast<int>(ffObj->GetNumber());
                }
                flags |= (1 << 21); // MultiSelect flag
                field.GetDictionary().AddKey(PoDoFo::PdfName("Ff"), PoDoFo::PdfVariant(static_cast<int64_t>(flags)));
            }
        },
        [&](PoDoFo::PdfMemDocument& reopened) {
            return fieldIsOfType(reopened, fieldName, PoDoFo::PdfFieldType::ListBox);
        },
        &err);
    if (!ok) qWarning() << "Error adding ListBox:" << err;
    else qDebug() << "Added ListBox" << fieldName << "on page" << pageIndex;
    return ok;
}


bool FormManager::createButton(const QString &pdfFilePath, int pageIndex, const QRectF &rect, const QString &caption, const QString &action, const QString &outputPath)
{
    QString err;
    const bool ok = runFormSaveTransaction(
        pdfFilePath, outputPath,
        [&](PoDoFo::PdfMemDocument& doc) {
            if (pageIndex < 0 || static_cast<unsigned>(pageIndex) >= doc.GetPages().GetCount())
                throw SaveAbort{QStringLiteral("invalid page index")};
            PoDoFo::PdfPage& page = doc.GetPages().GetPageAt(pageIndex);
            PoDoFo::Rect pdfRect(rect.x(), page.GetMediaBox().Height - rect.y() - rect.height(), rect.width(), rect.height());

            auto& field = page.CreateField<PoDoFo::PdfPushButton>(caption.toStdString(), pdfRect);

            PoDoFo::PdfDictionary mkDict;
            mkDict.AddKey(PoDoFo::PdfName("CA"), PoDoFo::PdfString(caption.toStdString()));
            field.GetDictionary().AddKey(PoDoFo::PdfName("MK"), mkDict);

            int flags = (1 << 16);
            field.GetDictionary().AddKey(PoDoFo::PdfName("Ff"), PoDoFo::PdfVariant(static_cast<int64_t>(flags)));

            if (!action.isEmpty()) {
                PoDoFo::PdfDictionary actionDict;
                actionDict.AddKey(PoDoFo::PdfName("S"), PoDoFo::PdfName("JavaScript"));
                actionDict.AddKey(PoDoFo::PdfName("JS"), PoDoFo::PdfString(action.toStdString()));
                field.GetDictionary().AddKey(PoDoFo::PdfName("A"), actionDict);
            }
        },
        [&](PoDoFo::PdfMemDocument& reopened) {
            return fieldIsOfType(reopened, caption, PoDoFo::PdfFieldType::PushButton);
        },
        &err);
    if (!ok) qWarning() << "Error adding button:" << err;
    else qDebug() << "Added button on page" << pageIndex;
    return ok;
}

// ── addCalculatedField ────────────────────────────────────────────────────────
// Creates a read-only text field whose value is computed by the supplied
// JavaScript/AcroForm calculation expression. The calculation runs via the
// /AA /C (Calculate) additional action; the field is registered in the AcroForm
// /CO array so conforming viewers recompute it in document order.
bool FormManager::addCalculatedField(const QString &pdfFilePath, int pageIndex, const QRectF &rect,
                                     const QString &fieldName, const QString &expression,
                                     const QString &outputPath)
{
    QString err;
    const bool ok = runFormSaveTransaction(
        pdfFilePath, outputPath,
        [&](PoDoFo::PdfMemDocument& doc) {
            if (pageIndex < 0 || static_cast<unsigned>(pageIndex) >= doc.GetPages().GetCount())
                throw SaveAbort{QStringLiteral("invalid page index")};
            PoDoFo::PdfPage& page = doc.GetPages().GetPageAt(pageIndex);
            PoDoFo::Rect pdfRect(rect.x(), page.GetMediaBox().Height - rect.y() - rect.height(), rect.width(), rect.height());

            auto& field = page.CreateField<PoDoFo::PdfTextBox>(fieldName.toStdString(), pdfRect);
            field.SetText(PoDoFo::PdfString(""));

            // /AA dictionary: /C (Calculate) JavaScript action plus a /F (Format)
            // action so the computed result displays as a 2-decimal number.
            PoDoFo::PdfDictionary aaDict;

            PoDoFo::PdfDictionary calcAction;
            calcAction.AddKey(PoDoFo::PdfName("S"), PoDoFo::PdfName("JavaScript"));
            calcAction.AddKey(PoDoFo::PdfName("JS"), PoDoFo::PdfString(expression.toStdString()));
            aaDict.AddKey(PoDoFo::PdfName("C"), calcAction);

            PoDoFo::PdfDictionary formatAction;
            formatAction.AddKey(PoDoFo::PdfName("S"), PoDoFo::PdfName("JavaScript"));
            formatAction.AddKey(PoDoFo::PdfName("JS"), PoDoFo::PdfString("AFNumber_Format(2, 0, 0, 0, \"\", true);"));
            aaDict.AddKey(PoDoFo::PdfName("F"), formatAction);

            field.GetDictionary().AddKey(PoDoFo::PdfName("AA"), aaDict);

            // A calculated field must be read-only so users can't overtype the result.
            int flags = 0;
            const PoDoFo::PdfObject* ffObj = field.GetDictionary().FindKey("Ff");
            if (ffObj && ffObj->IsNumber()) flags = static_cast<int>(ffObj->GetNumber());
            flags |= (1 << 0); // ReadOnly
            field.GetDictionary().AddKey(PoDoFo::PdfName("Ff"), PoDoFo::PdfVariant(static_cast<int64_t>(flags)));

            // Register the field in the AcroForm /CO calculation-order array so
            // viewers know to recalculate it. Append to any existing /CO entries.
            auto* acroForm = doc.GetAcroForm();
            if (acroForm) {
                PoDoFo::PdfArray coArr;
                const PoDoFo::PdfObject* existing = acroForm->GetDictionary().FindKey("CO");
                if (existing && existing->IsArray()) coArr = existing->GetArray();
                coArr.Add((field.GetObject)().GetIndirectReference());
                acroForm->GetDictionary().AddKey(PoDoFo::PdfName("CO"), coArr);
            }
        },
        [&](PoDoFo::PdfMemDocument& reopened) {
            const PoDoFo::PdfField* f = findFieldByName(reopened, fieldName);
            return f && f->GetType() == PoDoFo::PdfFieldType::TextBox
                   && f->GetDictionary().FindKey("AA")
                   && f->GetDictionary().GetKey("AA")->GetDictionary().HasKey("C");
        },
        &err);
    if (!ok) qWarning() << "Error adding calculated field:" << err;
    else qDebug() << "Added calculated field" << fieldName << "on page" << pageIndex
                  << "expr:" << expression;
    return ok;
}

QList<FieldSuggestion> FormManager::autoDetectFields(const QString &pdfFilePath, int pageIndex)
{
    // §9.6 P0: real content-aware heuristic (replaces the former stub that
    // returned three hardcoded dummy fields regardless of document content).
    // Heuristic: scan the page's content stream for label-like text runs —
    // ending in ':' or containing an underscore blank ('___') — and suggest
    // a Text field positioned right after the label. Results are suggestions
    // only: the UI places them for review before commit.
    QList<FieldSuggestion> suggestions;
    try {
        PoDoFo::PdfMemDocument doc;
        doc.Load(pdfFilePath.toUtf8().constData());
        if (pageIndex < 0 || static_cast<unsigned>(pageIndex) >= doc.GetPages().GetCount())
            return suggestions;

        PoDoFo::PdfPage& page = doc.GetPages().GetPageAt(pageIndex);
        PoDoFo::PdfContentStreamReader reader(page);

        double currentX = 0, currentY = 0;
        double currentFontSize = 10.0;
        const double pageWidth = page.GetMediaBox().Width;
        const double pageHeight = page.GetMediaBox().Height;
        int autoIndex = 0;

        PoDoFo::PdfContent content;
        while (reader.TryReadNext(content)) {
            if (content.GetType() != PoDoFo::PdfContentType::Operator) continue;
            std::string_view kw = content.GetKeyword();
            const auto& stack = content.GetStack();

            if (kw == "Tm" && stack.size() >= 6) {
                // PdfVariantStack is LIFO: for 'a b c d e f Tm', e=X and f=Y
                // sit at stack[1] / stack[0].
                if (stack[1].IsNumberOrReal()) currentX = stack[1].GetReal();
                if (stack[0].IsNumberOrReal()) currentY = stack[0].GetReal();
            } else if ((kw == "Td" || kw == "TD") && stack.size() >= 2) {
                // 'tx ty Td': ty was pushed last → stack[0]; tx → stack[1].
                if (stack[1].IsNumberOrReal()) currentX += stack[1].GetReal();
                if (stack[0].IsNumberOrReal()) currentY += stack[0].GetReal();
            } else if (kw == "Tf" && stack.size() >= 2) {
                // 'name size Tf': size on top → stack[0].
                if (stack[0].IsNumberOrReal()) currentFontSize = stack[0].GetReal();
            } else {
                QString text;
                if (kw == "Tj" && stack.size() >= 1 && stack[0].IsString())
                    text = QString::fromStdString(std::string(stack[0].GetString().GetString()));
                else if (kw == "TJ" && stack.size() >= 1 && stack[0].IsArray()) {
                    for (const auto& item : stack[0].GetArray())
                        if (item.IsString())
                            text += QString::fromStdString(std::string(item.GetString().GetString()));
                }
                if (text.isEmpty()) continue;

                const bool isLabel = text.trimmed().endsWith(QLatin1Char(':'));
                const bool hasBlank = text.contains(QStringLiteral("___"));
                if (!isLabel && !hasBlank) continue;
                if (autoIndex >= 20) break; // safety cap

                FieldSuggestion s;
                s.type = QStringLiteral("Text");
                s.suggestedName = QStringLiteral("AutoField%1").arg(++autoIndex);
                const double fieldW = qMin(180.0, qMax(80.0, pageWidth - currentX - currentFontSize * 2));
                if (fieldW < 40.0) continue; // no room on this line
                if (hasBlank) {
                    // The underscores ARE the blank: replace that run in place.
                    s.rect = QRectF(currentX, currentY,
                                    qMax(fieldW, text.length() * currentFontSize * 0.55),
                                    currentFontSize * 1.4);
                } else {
                    // Label ends with ':': place the entry box after it.
                    s.rect = QRectF(currentX + text.length() * currentFontSize * 0.5,
                                    currentY, fieldW, currentFontSize * 1.4);
                }
                // Clamp inside the page.
                s.rect.setWidth(qMin(s.rect.width(), pageWidth - s.rect.x() - 4));
                s.rect.setHeight(qMin(s.rect.height(), pageHeight - s.rect.y() - 4));
                if (s.rect.width() < 20 || s.rect.height() < 8) continue;
                suggestions.append(s);
            }
        }
    } catch (const PoDoFo::PdfError& e) {
        qWarning() << "autoDetectFields error:" << e.what();
    }
    return suggestions;
}

// §9.6 P0: persist the properties panel's Required flag and Tooltip as real
// PDF metadata — /Ff bit position 2 (Required) and /TU (tooltip / UI text) —
// so what the user sets in the panel survives save/reload and is honored by
// other viewers. Previously these were collected by the UI but discarded.
bool FormManager::setFieldMetadata(const QString &pdfFilePath, const QString &fieldName,
                                   const QString &tooltip, bool required,
                                   const QString &outputPath)
{
    QString err;
    const bool ok = runFormSaveTransaction(
        pdfFilePath, outputPath,
        [&](PoDoFo::PdfMemDocument& doc) {
            auto* acroForm = doc.GetAcroForm();
            if (!acroForm) throw SaveAbort{QStringLiteral("no AcroForm found in document")};

            bool found = false;
            for (unsigned i = 0; i < acroForm->GetFieldCount(); ++i) {
                auto& field = acroForm->GetFieldAt(i);
                if (QString::fromStdString(field.GetFullName()) != fieldName) continue;
                found = true;
                PoDoFo::PdfDictionary& dict = field.GetDictionary();

                // /TU — tooltip text for accessibility/UI (ISO 32000-1 §12.7.3.1).
                if (!tooltip.isEmpty())
                    dict.AddKey(PoDoFo::PdfName("TU"), PoDoFo::PdfString(tooltip.toStdString()));
                else
                    dict.RemoveKey("TU");

                // /Ff bit position 2 (value 2) = Required.
                int flags = 0;
                const PoDoFo::PdfObject* ffObj = dict.FindKey("Ff");
                if (ffObj && ffObj->IsNumber()) flags = static_cast<int>(ffObj->GetNumber());
                if (required) flags |= (1 << 1);
                else          flags &= ~(1 << 1);
                dict.AddKey(PoDoFo::PdfName("Ff"), PoDoFo::PdfVariant(static_cast<int64_t>(flags)));
                break;
            }
            if (!found) throw SaveAbort{QStringLiteral("field not found: %1").arg(fieldName)};
        },
        [&](PoDoFo::PdfMemDocument& reopened) {
            const PoDoFo::PdfField* f = findFieldByName(reopened, fieldName);
            if (!f) return false;
            const PoDoFo::PdfDictionary& fieldDict = (f->GetObject)().GetDictionary();
            if (tooltip.isEmpty()) {
                if (fieldDict.HasKey("TU")) return false;
            } else {
                const PoDoFo::PdfObject* tu = fieldDict.FindKey("TU");
                if (!tu || !tu->IsString()
                    || std::string(tu->GetString().GetString().data(), tu->GetString().GetString().size())
                           != tooltip.toStdString()) return false;
            }
            const PoDoFo::PdfObject* ff = fieldDict.FindKey("Ff");
            const int flags = (ff && ff->IsNumber()) ? static_cast<int>(ff->GetNumber()) : 0;
            return ((flags & (1 << 1)) != 0) == required;
        },
        &err);
    if (!ok) qWarning() << "Error setting field metadata:" << err;
    return ok;
}


bool FormManager::exportFormData(const QString &pdfFilePath, const QString &outputPath, const QString &format)
{
    qDebug() << "Exporting form data from" << pdfFilePath << "to" << outputPath << "in format" << format;
    try {
        PoDoFo::PdfMemDocument doc;
        doc.Load(pdfFilePath.toUtf8().constData());
        auto* acroForm = doc.GetAcroForm();
        if (!acroForm) return false;

        QVariantMap data;
        for (unsigned i = 0; i < acroForm->GetFieldCount(); ++i) {
            auto& field = acroForm->GetFieldAt(i);
            QString name = QString::fromStdString(field.GetFullName());
            QString value;

            if (auto* txt = dynamic_cast<PoDoFo::PdfTextBox*>(&field)) {
                auto textOpt = txt->GetText();
                if (textOpt.has_value()) {
                    auto str = textOpt.value().GetString();
                    value = QString::fromUtf8(str.data(), str.size());
                }
            } else if (auto* cb = dynamic_cast<PoDoFo::PdfCheckBox*>(&field)) {
                value = cb->IsChecked() ? "Yes" : "Off";
            } else if (auto* combo = dynamic_cast<PoDoFo::PdfComboBox*>(&field)) {
                if (combo->GetSelectedIndex() >= 0 && combo->GetSelectedIndex() < static_cast<int>(combo->GetItemCount())) {
                    auto str = combo->GetItem(combo->GetSelectedIndex()).GetString();
                    value = QString::fromUtf8(str.data(), str.size());
                }
            } else if (auto* lb = dynamic_cast<PoDoFo::PdfListBox*>(&field)) {
                if (lb->GetSelectedIndex() >= 0 && lb->GetSelectedIndex() < static_cast<int>(lb->GetItemCount())) {
                    auto str = lb->GetItem(lb->GetSelectedIndex()).GetString();
                    value = QString::fromUtf8(str.data(), str.size());
                }
            } else if (auto* rb = dynamic_cast<PoDoFo::PdfRadioButton*>(&field)) {
                value = rb->IsChecked() ? "Yes" : "Off";
            }
            data.insert(name, value);
        }

        QFile outFile(outputPath);
        if (!outFile.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
        QTextStream out(&outFile);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        out.setCodec("UTF-8");
#endif

        if (format.toLower() == "csv") {
            out << "FieldName,FieldValue\n";
            for (auto it = data.cbegin(); it != data.cend(); ++it) {
                QString v = it.value().toString();
                v.replace("\"", "\"\"");
                out << "\"" << it.key() << "\",\"" << v << "\"\n";
            }
        } else if (format.toLower() == "fdf") {
            out << "%FDF-1.2\n1 0 obj\n<< /FDF << /Fields [\n";
            for (auto it = data.cbegin(); it != data.cend(); ++it) {
                // A-04: the ad-hoc replace() escaped ( and ) but NOT backslash, so a
                // field value containing '\' produced an invalid PDF literal string
                // (e.g. a trailing '\' escaped the closing ')' and broke the FDF
                // structure; "a\b" became the backspace escape). Route through the
                // canonical escaper which escapes '\' first, then ( and ), in the
                // correct order. Field names/values can come from an attacker-supplied
                // AcroForm, so this is the §6 "raw user strings -> always escape" rule.
                out << "<< /T (" << QString::fromStdString(pdfEscapeLiteralString(it.key()))
                    << ") /V (" << QString::fromStdString(pdfEscapeLiteralString(it.value().toString()))
                    << ") >>\n";
            }
            out << "] >> >>\nendobj\ntrailer << /Root 1 0 R >>\n%%EOF\n";
        }
        return true;
    } catch (const PoDoFo::PdfError& e) {
        qWarning() << "Error exporting form data:" << e.what();
        return false;
    }
}

bool FormManager::importFormData(const QString &pdfFilePath, const QString &dataFilePath, const QString &outputPath, QStringList *unsupportedFields)
{
    qDebug() << "Importing form data from" << dataFilePath << "into" << pdfFilePath << "saving to" << outputPath;

    QFile inFile(dataFilePath);
    if (!inFile.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    QTextStream in(&inFile);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    in.setCodec("UTF-8");
#endif
    QString content = in.readAll();

    QVariantMap data;
    if (content.startsWith("%FDF")) {
        QRegularExpression re("<<\\s*/T\\s*\\((.*?)\\)\\s*/V\\s*\\((.*?)\\)\\s*>>");
        QRegularExpressionMatchIterator i = re.globalMatch(content);
        while (i.hasNext()) {
            QRegularExpressionMatch match = i.next();
            QString k = match.captured(1);
            QString v = match.captured(2);
            k.replace("\\)", ")").replace("\\(", "(");
            v.replace("\\)", ")").replace("\\(", "(");
            data.insert(k, v);
        }
    } else {
        // Simple CSV parser
        QStringList lines = content.split('\n', Qt::SkipEmptyParts);
        for (int i = 1; i < lines.size(); ++i) {
            QString line = lines[i].trimmed();
            if (line.isEmpty()) continue;
            QStringList parts = line.split("\",\"");
            if (parts.size() >= 2) {
                QString k = parts[0];
                if (k.startsWith("\"")) k = k.mid(1);
                QString v = parts[1];
                if (v.endsWith("\"")) v = v.mid(0, v.length() - 1);
                v.replace("\"\"", "\"");
                data.insert(k, v);
            }
        }
    }

    // R01: import lands on the same transactional boundary via fillForm
    // (import + flatten paths must not direct-write either).
    return fillForm(pdfFilePath, data, outputPath, /*lockFields=*/true, unsupportedFields);
}

bool FormManager::flattenForm(const QString &pdfFilePath, const QString &outputPath)
{
    qDebug() << "Flattening form:" << pdfFilePath;
    bool hadAcroForm = false;
    bool removed = false;

    const bool ok = runFormSaveTransaction(
        pdfFilePath, outputPath,
        [&](PoDoFo::PdfMemDocument& doc) {
            auto* acroForm = doc.GetAcroForm();
            hadAcroForm = (acroForm != nullptr);

            if (acroForm) {
                const PoDoFo::PdfFont* font = doc.GetFonts().SearchFont("Helvetica");
                if (!font) {
                    font = &doc.GetFonts().GetStandard14Font(PoDoFo::PdfStandard14FontType::Helvetica);
                }

                for (unsigned i = 0; i < acroForm->GetFieldCount(); ++i) {
                    auto& field = acroForm->GetFieldAt(i);
                    PoDoFo::Rect rect = field.GetDictionary().HasKey("Rect") ? PoDoFo::Rect::FromArray(field.GetDictionary().GetKey("Rect")->GetArray()) : PoDoFo::Rect(0,0,0,0);

                    // Try to find the page this field is on
                    PoDoFo::PdfPage* page = nullptr;
                    auto* pObj = field.GetDictionary().FindKey("P");
                    if (pObj) {
                        if (pObj->IsReference()) pObj = &doc.GetObjects().MustGetObject(pObj->GetIndirectReference());
                        for (unsigned pi = 0; pi < doc.GetPages().GetCount(); ++pi) {
                            if ((doc.GetPages().GetPageAt(pi).GetObject)().GetIndirectReference() == pObj->GetIndirectReference()) {
                                page = &doc.GetPages().GetPageAt(pi);
                                break;
                            }
                        }
                    }

                    if (!page) {
                        // Fallback: search annotations of all pages
                        for (unsigned pi = 0; pi < doc.GetPages().GetCount(); ++pi) {
                            auto& p = doc.GetPages().GetPageAt(pi);
                            auto& annos = p.GetAnnotations();
                            for (unsigned ai = 0; ai < annos.GetCount(); ++ai) {
                                if ((annos.GetAnnotAt(ai).GetObject)().GetIndirectReference() == (field.GetObject)().GetIndirectReference()) {
                                    page = &p;
                                    break;
                                }
                            }
                            if (page) break;
                        }
                    }

                    if (page) {
                        QString value;
                        if (auto* txt = dynamic_cast<PoDoFo::PdfTextBox*>(&field)) {
                            auto textOpt = txt->GetText();
                            if (textOpt.has_value()) {
                                auto str = textOpt.value().GetString();
                                value = QString::fromUtf8(str.data(), str.size());
                            }
                        } else if (auto* cb = dynamic_cast<PoDoFo::PdfCheckBox*>(&field)) {
                            if (cb->IsChecked()) value = "X";
                        } else if (auto* combo = dynamic_cast<PoDoFo::PdfComboBox*>(&field)) {
                            if (combo->GetSelectedIndex() >= 0 && combo->GetSelectedIndex() < static_cast<int>(combo->GetItemCount())) {
                                auto str = combo->GetItem(combo->GetSelectedIndex()).GetString();
                                value = QString::fromUtf8(str.data(), str.size());
                            }
                        } else if (auto* lb = dynamic_cast<PoDoFo::PdfListBox*>(&field)) {
                            if (lb->GetSelectedIndex() >= 0 && lb->GetSelectedIndex() < static_cast<int>(lb->GetItemCount())) {
                                auto str = lb->GetItem(lb->GetSelectedIndex()).GetString();
                                value = QString::fromUtf8(str.data(), str.size());
                            }
                        } else if (auto* rb = dynamic_cast<PoDoFo::PdfRadioButton*>(&field)) {
                            if (rb->IsChecked()) value = "X";
                        }

                        if (!value.isEmpty()) {
                            PoDoFo::PdfPainter painter;
                            painter.SetCanvas(*page);
                            painter.TextState.SetFont(*font, 12.0);
                            painter.GraphicsState.SetNonStrokingColor(PoDoFo::PdfColor(0.0, 0.0, 0.0));

                            // Draw text slightly offset within the rect
                            double y = rect.Y + (rect.Height - 12.0) / 2.0;
                            painter.DrawText(value.toUtf8().constData(), rect.X + 2.0, y);
                            painter.FinishDrawing();
                        }
                    }

                    field.SetReadOnly(true);
                    int annotFlags = static_cast<int>(PoDoFo::PdfAnnotationFlags::Print) | static_cast<int>(PoDoFo::PdfAnnotationFlags::Locked);
                    field.GetDictionary().AddKey(PoDoFo::PdfName("F"), PoDoFo::PdfVariant(static_cast<int64_t>(annotFlags)));
                }
            }

            removed = false;
            auto& catalog = doc.GetCatalog();
            if (catalog.GetDictionary().HasKey("AcroForm")) {
                catalog.GetDictionary().RemoveKey("AcroForm");
                removed = true;
            }
            if (doc.GetTrailer().GetDictionary().HasKey("AcroForm")) {
                doc.GetTrailer().GetDictionary().RemoveKey("AcroForm");
                removed = true;
            }

            // Remove Widget annotations from pages
            for (unsigned pi = 0; pi < doc.GetPages().GetCount(); ++pi) {
                auto& page = doc.GetPages().GetPageAt(pi);
                auto& annos = page.GetAnnotations();
                std::vector<unsigned> toRemove;
                for (unsigned ai = 0; ai < annos.GetCount(); ++ai) {
                    auto& anno = annos.GetAnnotAt(ai);
                    auto* subtype = anno.GetDictionary().FindKey("Subtype");
                    if (subtype && subtype->IsName() && subtype->GetName().GetString() == "Widget") {
                        toRemove.push_back(ai);
                    }
                }
                for (auto it = toRemove.rbegin(); it != toRemove.rend(); ++it) {
                    annos.RemoveAnnotAt(*it);
                    removed = true;
                }
            }

            // Preserve the historical contract: nothing to flatten → fail
            // without writing anything.
            if (!removed && !hadAcroForm)
                throw SaveAbort{QStringLiteral("no AcroForm to flatten")};
        },
        // The requested change IS the AcroForm removal: the flattened candidate
        // must carry no /AcroForm and no Widget annotations.
        [&](PoDoFo::PdfMemDocument& reopened) {
            if (reopened.GetCatalog().GetDictionary().HasKey("AcroForm")) return false;
            if (reopened.GetTrailer().GetDictionary().HasKey("AcroForm")) return false;
            for (unsigned pi = 0; pi < reopened.GetPages().GetCount(); ++pi) {
                auto& annos = reopened.GetPages().GetPageAt(pi).GetAnnotations();
                for (unsigned ai = 0; ai < annos.GetCount(); ++ai) {
                    auto* subtype = annos.GetAnnotAt(ai).GetDictionary().FindKey("Subtype");
                    if (subtype && subtype->IsName() && subtype->GetName().GetString() == "Widget")
                        return false;
                }
            }
            return true;
        },
        nullptr);

    if (!ok) {
        qWarning() << "PoDoFo error during form flattening: nothing persisted";
        return false;
    }
    return true;
}

// ── removeFieldByName ─────────────────────────────────────────────────────────
// Locates the named AcroForm field by full name, removes it from the /Fields
// array AND removes the corresponding Widget annotation from the page, then
// persists through the shared R01 save boundary.  Returns true on success.
bool FormManager::removeFieldByName(const QString &pdfFilePath,
                                    const QString &fieldName,
                                    const QString &outputPath)
{
    QString err;
    const bool ok = runFormSaveTransaction(
        pdfFilePath, outputPath,
        [&](PoDoFo::PdfMemDocument& doc) {
            auto* acroForm = doc.GetAcroForm();
            if (!acroForm)
                throw SaveAbort{QStringLiteral("no AcroForm in %1").arg(pdfFilePath)};

            // Find the field index by full name
            int fieldIdx = -1;
            PoDoFo::PdfReference fieldRef;
            for (unsigned i = 0; i < acroForm->GetFieldCount(); ++i) {
                auto& f = acroForm->GetFieldAt(i);
                if (QString::fromStdString(f.GetFullName()) == fieldName) {
                    fieldIdx = static_cast<int>(i);
                    fieldRef = (f.GetObject)().GetIndirectReference();
                    break;
                }
            }

            if (fieldIdx < 0)
                throw SaveAbort{QStringLiteral("field not found: %1").arg(fieldName)};

            // Remove the Widget annotation from every page that references this field
            for (unsigned pi = 0; pi < doc.GetPages().GetCount(); ++pi) {
                auto& page = doc.GetPages().GetPageAt(pi);
                auto& annos = page.GetAnnotations();
                for (unsigned ai = annos.GetCount(); ai-- > 0;) {
                    if ((annos.GetAnnotAt(ai).GetObject)().GetIndirectReference() == fieldRef) {
                        annos.RemoveAnnotAt(ai);
                        break;
                    }
                }
            }

            // Remove from AcroForm /Fields
            acroForm->RemoveFieldAt(static_cast<unsigned>(fieldIdx));
        },
        [&](PoDoFo::PdfMemDocument& reopened) {
            return findFieldByName(reopened, fieldName) == nullptr;
        },
        &err);
    if (!ok) qWarning() << "removeFieldByName error:" << err;
    else qDebug() << "Removed field" << fieldName << "and saved to" << outputPath;
    return ok;
}

// ── updateFieldRect ───────────────────────────────────────────────────────────
// Updates the /Rect of the named field's widget annotation to newRect.
// newRect is in Qt widget coordinates (origin top-left of the page); this
// function converts to PDF coordinates (origin bottom-left) using pageIndex to
// determine the page height.
bool FormManager::updateFieldRect(const QString &pdfFilePath,
                                  const QString &fieldName,
                                  int pageIndex,
                                  const QRectF &newRect,
                                  const QString &outputPath)
{
    QString err;
    const bool ok = runFormSaveTransaction(
        pdfFilePath, outputPath,
        [&](PoDoFo::PdfMemDocument& doc) {
            if (pageIndex < 0 || static_cast<unsigned>(pageIndex) >= doc.GetPages().GetCount())
                throw SaveAbort{QStringLiteral("invalid page index %1").arg(pageIndex)};

            auto* acroForm = doc.GetAcroForm();
            if (!acroForm)
                throw SaveAbort{QStringLiteral("no AcroForm in %1").arg(pdfFilePath)};

            double pageH = doc.GetPages().GetPageAt(pageIndex).GetMediaBox().Height;

            // Convert Qt (top-left origin) to PDF (bottom-left origin)
            PoDoFo::Rect pdfRect(
                newRect.x(),
                pageH - newRect.y() - newRect.height(),
                newRect.width(),
                newRect.height()
            );

            PoDoFo::PdfArray rectArr;
            rectArr.Add(PoDoFo::PdfVariant(pdfRect.X));
            rectArr.Add(PoDoFo::PdfVariant(pdfRect.Y));
            rectArr.Add(PoDoFo::PdfVariant(pdfRect.X + pdfRect.Width));
            rectArr.Add(PoDoFo::PdfVariant(pdfRect.Y + pdfRect.Height));

            bool found = false;
            for (unsigned i = 0; i < acroForm->GetFieldCount(); ++i) {
                auto& f = acroForm->GetFieldAt(i);
                if (QString::fromStdString(f.GetFullName()) == fieldName) {
                    f.GetDictionary().AddKey(PoDoFo::PdfName("Rect"), rectArr);
                    found = true;
                    break;
                }
            }
            if (!found)
                throw SaveAbort{QStringLiteral("field not found: %1").arg(fieldName)};
        },
        [&](PoDoFo::PdfMemDocument& reopened) {
            if (pageIndex < 0 || static_cast<unsigned>(pageIndex) >= reopened.GetPages().GetCount())
                return false;
            const PoDoFo::PdfField* f = findFieldByName(reopened, fieldName);
            if (!f) return false;
            const PoDoFo::PdfObject* rectObj = f->GetDictionary().FindKey("Rect");
            if (!rectObj || !rectObj->IsArray() || rectObj->GetArray().GetSize() != 4) return false;
            // Serialized real numbers keep limited precision — compare loosely.
            const auto approx = [](const PoDoFo::PdfObject& o, double v) {
                const double num = o.IsNumber() ? o.GetNumber() : -1e30;
                return qAbs(num - v) < 0.01;
            };
            // Expect the PDF-coordinate (bottom-left origin) form of newRect.
            const double pageH = reopened.GetPages().GetPageAt(pageIndex).GetMediaBox().Height;
            const double pdfY = pageH - newRect.y() - newRect.height();
            const PoDoFo::PdfArray& a = rectObj->GetArray();
            return approx(a[0], newRect.x())
                && approx(a[1], pdfY)
                && approx(a[3], pdfY + newRect.height());
        },
        &err);
    if (!ok) qWarning() << "updateFieldRect error:" << err;
    else qDebug() << "Updated rect for field" << fieldName << "on page" << pageIndex;
    return ok;
}

// ── listFields ────────────────────────────────────────────────────────────────
// Returns the full names of all AcroForm fields in the document.
QStringList FormManager::listFields(const QString &pdfFilePath)
{
    QStringList names;
    try {
        PoDoFo::PdfMemDocument doc;
        doc.Load(pdfFilePath.toUtf8().constData());
        auto* acroForm = doc.GetAcroForm();
        if (!acroForm) return names;
        for (unsigned i = 0; i < acroForm->GetFieldCount(); ++i) {
            names.append(QString::fromStdString(acroForm->GetFieldAt(i).GetFullName()));
        }
    } catch (const PoDoFo::PdfError& e) {
        qWarning() << "listFields error:" << e.what();
    }
    return names;
}

// ── setTabOrder ───────────────────────────────────────────────────────────────
// Writes the AcroForm /CO (calculation order) array with field references in
// orderedNames order.  This array is used by PDF readers to determine the tab
// sequence.  Fields not in orderedNames are appended at the end of the CO array.
// R01: persists through the shared boundary — the old fixed-".tmp" +
// remove-destination + ignore-rename-failure pattern is gone.
bool FormManager::setTabOrder(const QString &pdfFilePath,
                              const QStringList &orderedNames,
                              const QString &outputPath)
{
    QString err;
    const bool ok = runFormSaveTransaction(
        pdfFilePath, outputPath,
        [&](PoDoFo::PdfMemDocument& doc) {
            auto* acroForm = doc.GetAcroForm();
            if (!acroForm)
                throw SaveAbort{QStringLiteral("no AcroForm in %1").arg(pdfFilePath)};

            // Build a name→reference map
            std::map<std::string, PoDoFo::PdfReference> refMap;
            for (unsigned i = 0; i < acroForm->GetFieldCount(); ++i) {
                auto& f = acroForm->GetFieldAt(i);
                refMap[f.GetFullName()] = (f.GetObject)().GetIndirectReference();
            }

            // Build /CO array in the requested order
            PoDoFo::PdfArray coArr;
            // First: fields in orderedNames
            for (const QString& name : orderedNames) {
                auto it = refMap.find(name.toStdString());
                if (it != refMap.end()) {
                    coArr.Add(it->second);
                    refMap.erase(it);
                }
            }
            // Then: any remaining fields not in the ordered list
            for (const auto& kv : refMap) {
                coArr.Add(kv.second);
            }

            acroForm->GetDictionary().AddKey(PoDoFo::PdfName("CO"), coArr);
        },
        [&](PoDoFo::PdfMemDocument& reopened) {
            auto* acroForm = reopened.GetAcroForm();
            if (!acroForm) return false;
            // Recompute the expected /CO on the REOPENED document: object
            // numbers change between serializations, so refs are resolved
            // from the candidate's own field table.
            std::map<std::string, PoDoFo::PdfReference> refMap;
            for (unsigned i = 0; i < acroForm->GetFieldCount(); ++i) {
                auto& f = acroForm->GetFieldAt(i);
                refMap[f.GetFullName()] = (f.GetObject)().GetIndirectReference();
            }
            PoDoFo::PdfArray expected;
            for (const QString& name : orderedNames) {
                auto it = refMap.find(name.toStdString());
                if (it != refMap.end()) {
                    expected.Add(it->second);
                    refMap.erase(it);
                }
            }
            for (const auto& kv : refMap) expected.Add(kv.second);

            const PoDoFo::PdfObject* co = acroForm->GetDictionary().FindKey("CO");
            if (!co || !co->IsArray()) return false;
            const PoDoFo::PdfArray& actual = co->GetArray();
            if (actual.GetSize() != expected.GetSize()) return false;
            for (unsigned i = 0; i < expected.GetSize(); ++i) {
                if (!actual[i].IsReference() || !(actual[i].GetReference() == expected[i].GetReference()))
                    return false;
            }
            return true;
        },
        &err);
    if (!ok) qWarning() << "setTabOrder error:" << err;
    else qDebug() << "setTabOrder: wrote" << orderedNames.size() << "ordered entries to /CO";
    return ok;
}
