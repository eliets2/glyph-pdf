// SPDX-License-Identifier: Apache-2.0
#include "engines/FormManager.h"
#include "engines/podofo/PdfStringEscape.h"
#include <memory>
#include <QDebug>
#include <podofo/podofo.h>
#include <QString>
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>

class FormManager::Private {
public:
    // PoDoFo AcroForm extraction state
};

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
    try {
        PoDoFo::PdfMemDocument doc;
        doc.Load(pdfFilePath.toUtf8().constData());
        auto* acroForm = doc.GetAcroForm();
        if (!acroForm) {
            qDebug() << "No AcroForm found in document.";
            return false;
        }

        QStringList appliedNames;
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

        doc.Save(outputPath.toUtf8().constData());
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
    } catch (const PoDoFo::PdfError& e) {
        qWarning() << "PoDoFo error during form fill:" << e.what();
        return false;
    }
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
    try {
        PoDoFo::PdfMemDocument doc;
        doc.Load(pdfFilePath.toUtf8().constData());
        if (pageIndex < 0 || static_cast<unsigned>(pageIndex) >= doc.GetPages().GetCount()) return false;

        PoDoFo::PdfPage& page = doc.GetPages().GetPageAt(pageIndex);
        PoDoFo::Rect pdfRect(rect.x(), page.GetMediaBox().Height - rect.y() - rect.height(), rect.width(), rect.height());
        
        auto& field = page.CreateField<PoDoFo::PdfTextBox>(fieldName.toStdString(), pdfRect);
        field.SetText(PoDoFo::PdfString(""));

        doc.Save(outputPath.toUtf8().constData());
        qDebug() << "Added text field" << fieldName << "on page" << pageIndex;
        return true;
    } catch (const PoDoFo::PdfError& e) {
        qWarning() << "Error adding text field:" << e.what();
        return false;
    }
}

bool FormManager::addDateField(const QString &pdfFilePath, int pageIndex, const QRectF &rect, const QString &fieldName, const QString &outputPath)
{
    try {
        PoDoFo::PdfMemDocument doc;
        doc.Load(pdfFilePath.toUtf8().constData());
        if (pageIndex < 0 || static_cast<unsigned>(pageIndex) >= doc.GetPages().GetCount()) return false;

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

        doc.Save(outputPath.toUtf8().constData());
        qDebug() << "Added date field" << fieldName << "on page" << pageIndex;
        return true;
    } catch (const PoDoFo::PdfError& e) {
        qWarning() << "Error adding date field:" << e.what();
        return false;
    }
}

bool FormManager::addNumericField(const QString &pdfFilePath, int pageIndex, const QRectF &rect, const QString &fieldName, const QString &outputPath)
{
    try {
        PoDoFo::PdfMemDocument doc;
        doc.Load(pdfFilePath.toUtf8().constData());
        if (pageIndex < 0 || static_cast<unsigned>(pageIndex) >= doc.GetPages().GetCount()) return false;

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

        doc.Save(outputPath.toUtf8().constData());
        qDebug() << "Added numeric field" << fieldName << "on page" << pageIndex;
        return true;
    } catch (const PoDoFo::PdfError& e) {
        qWarning() << "Error adding numeric field:" << e.what();
        return false;
    }
}

bool FormManager::addCheckBox(const QString &pdfFilePath, int pageIndex, const QRectF &rect, const QString &fieldName, const QString &outputPath)
{
    try {
        PoDoFo::PdfMemDocument doc;
        doc.Load(pdfFilePath.toUtf8().constData());
        if (pageIndex < 0 || static_cast<unsigned>(pageIndex) >= doc.GetPages().GetCount()) return false;

        PoDoFo::PdfPage& page = doc.GetPages().GetPageAt(pageIndex);
        PoDoFo::Rect pdfRect(rect.x(), page.GetMediaBox().Height - rect.y() - rect.height(), rect.width(), rect.height());

        auto& field = page.CreateField<PoDoFo::PdfCheckBox>(fieldName.toStdString(), pdfRect);
        field.SetChecked(false);

        doc.Save(outputPath.toUtf8().constData());
        qDebug() << "Added checkbox" << fieldName << "on page" << pageIndex;
        return true;
    } catch (const PoDoFo::PdfError& e) {
        qWarning() << "Error adding checkbox:" << e.what();
        return false;
    }
}

bool FormManager::addRadioButton(const QString &pdfFilePath, int pageIndex, const QRectF &rect, const QString &fieldName, const QString &outputPath)
{
    try {
        PoDoFo::PdfMemDocument doc;
        doc.Load(pdfFilePath.toUtf8().constData());
        if (pageIndex < 0 || static_cast<unsigned>(pageIndex) >= doc.GetPages().GetCount()) return false;

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

        doc.Save(outputPath.toUtf8().constData());
        qDebug() << "Added radio button" << fieldName << "on page" << pageIndex;
        return true;
    } catch (const PoDoFo::PdfError& e) {
        qWarning() << "Error adding radio button:" << e.what();
        return false;
    }
}

bool FormManager::addDropdown(const QString &pdfFilePath, int pageIndex, const QRectF &rect, const QString &fieldName, const QStringList &options, const QString &outputPath)
{
    try {
        PoDoFo::PdfMemDocument doc;
        doc.Load(pdfFilePath.toUtf8().constData());
        if (pageIndex < 0 || static_cast<unsigned>(pageIndex) >= doc.GetPages().GetCount()) return false;

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

        doc.Save(outputPath.toUtf8().constData());
        qDebug() << "Added dropdown" << fieldName << "with" << options.size() << "options on page" << pageIndex;
        return true;
    } catch (const PoDoFo::PdfError& e) {
        qWarning() << "Error adding dropdown:" << e.what();
        return false;
    }
}

bool FormManager::addListBox(const QString &pdfFilePath, int pageIndex, const QRectF &rect, const QString &fieldName, const QStringList &options, bool multiSelect, const QString &outputPath)
{
    try {
        PoDoFo::PdfMemDocument doc;
        doc.Load(pdfFilePath.toUtf8().constData());
        if (pageIndex < 0 || static_cast<unsigned>(pageIndex) >= doc.GetPages().GetCount()) return false;

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

        doc.Save(outputPath.toUtf8().constData());
        qDebug() << "Added ListBox" << fieldName << "on page" << pageIndex;
        return true;
    } catch (const PoDoFo::PdfError& e) {
        qWarning() << "Error adding ListBox:" << e.what();
        return false;
    }
}


bool FormManager::createButton(const QString &pdfFilePath, int pageIndex, const QRectF &rect, const QString &caption, const QString &action, const QString &outputPath)
{
    try {
        PoDoFo::PdfMemDocument doc;
        doc.Load(pdfFilePath.toUtf8().constData());
        if (pageIndex < 0 || static_cast<unsigned>(pageIndex) >= doc.GetPages().GetCount()) return false;

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

        doc.Save(outputPath.toUtf8().constData());
        qDebug() << "Added button on page" << pageIndex;
        return true;
    } catch (const PoDoFo::PdfError& e) {
        qWarning() << "Error adding button:" << e.what();
        return false;
    }
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
    try {
        PoDoFo::PdfMemDocument doc;
        doc.Load(pdfFilePath.toUtf8().constData());
        if (pageIndex < 0 || static_cast<unsigned>(pageIndex) >= doc.GetPages().GetCount()) return false;

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
            coArr.Add(field.GetObject().GetIndirectReference());
            acroForm->GetDictionary().AddKey(PoDoFo::PdfName("CO"), coArr);
        }

        doc.Save(outputPath.toUtf8().constData());
        qDebug() << "Added calculated field" << fieldName << "on page" << pageIndex
                 << "expr:" << expression;
        return true;
    } catch (const PoDoFo::PdfError& e) {
        qWarning() << "Error adding calculated field:" << e.what();
        return false;
    }
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
    try {
        PoDoFo::PdfMemDocument doc;
        doc.Load(pdfFilePath.toUtf8().constData());
        auto* acroForm = doc.GetAcroForm();
        if (!acroForm) return false;

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
        if (!found) return false;

        doc.Save(outputPath.toUtf8().constData());
        return true;
    } catch (const PoDoFo::PdfError& e) {
        qWarning() << "Error setting field metadata:" << e.what();
        return false;
    }
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

    return fillForm(pdfFilePath, data, outputPath, /*lockFields=*/true, unsupportedFields);
}

bool FormManager::flattenForm(const QString &pdfFilePath, const QString &outputPath)
{
    qDebug() << "Flattening form:" << pdfFilePath;
    try {
        PoDoFo::PdfMemDocument doc;
        doc.Load(pdfFilePath.toUtf8().constData());
        
        auto* acroForm = doc.GetAcroForm();
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
                        if (doc.GetPages().GetPageAt(pi).GetObject().GetIndirectReference() == pObj->GetIndirectReference()) {
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
                            if (annos.GetAnnotAt(ai).GetObject().GetIndirectReference() == field.GetObject().GetIndirectReference()) {
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

        bool removed = false;
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

        if (removed || acroForm) {
            doc.Save(outputPath.toUtf8().constData());
            return true;
        }
        return false;
    } catch (const PoDoFo::PdfError& e) {
        qWarning() << "PoDoFo error during form flattening:" << e.what();
        return false;
    }
}

// ── removeFieldByName ─────────────────────────────────────────────────────────
// Locates the named AcroForm field by full name, removes it from the /Fields
// array AND removes the corresponding Widget annotation from the page, then
// saves to outputPath.  Returns true on success.
bool FormManager::removeFieldByName(const QString &pdfFilePath,
                                    const QString &fieldName,
                                    const QString &outputPath)
{
    try {
        PoDoFo::PdfMemDocument doc;
        doc.Load(pdfFilePath.toUtf8().constData());

        auto* acroForm = doc.GetAcroForm();
        if (!acroForm) {
            qWarning() << "removeFieldByName: no AcroForm in" << pdfFilePath;
            return false;
        }

        // Find the field index by full name
        int fieldIdx = -1;
        PoDoFo::PdfReference fieldRef;
        for (unsigned i = 0; i < acroForm->GetFieldCount(); ++i) {
            auto& f = acroForm->GetFieldAt(i);
            if (QString::fromStdString(f.GetFullName()) == fieldName) {
                fieldIdx = static_cast<int>(i);
                fieldRef = f.GetObject().GetIndirectReference();
                break;
            }
        }

        if (fieldIdx < 0) {
            qWarning() << "removeFieldByName: field not found:" << fieldName;
            return false;
        }

        // Remove the Widget annotation from every page that references this field
        for (unsigned pi = 0; pi < doc.GetPages().GetCount(); ++pi) {
            auto& page = doc.GetPages().GetPageAt(pi);
            auto& annos = page.GetAnnotations();
            for (unsigned ai = annos.GetCount(); ai-- > 0;) {
                if (annos.GetAnnotAt(ai).GetObject().GetIndirectReference() == fieldRef) {
                    annos.RemoveAnnotAt(ai);
                    break;
                }
            }
        }

        // Remove from AcroForm /Fields
        acroForm->RemoveFieldAt(static_cast<unsigned>(fieldIdx));

        doc.Save(outputPath.toUtf8().constData());
        qDebug() << "Removed field" << fieldName << "and saved to" << outputPath;
        return true;
    } catch (const PoDoFo::PdfError& e) {
        qWarning() << "removeFieldByName error:" << e.what();
        return false;
    }
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
    try {
        PoDoFo::PdfMemDocument doc;
        doc.Load(pdfFilePath.toUtf8().constData());

        if (pageIndex < 0 || static_cast<unsigned>(pageIndex) >= doc.GetPages().GetCount()) {
            qWarning() << "updateFieldRect: invalid pageIndex" << pageIndex;
            return false;
        }

        auto* acroForm = doc.GetAcroForm();
        if (!acroForm) {
            qWarning() << "updateFieldRect: no AcroForm in" << pdfFilePath;
            return false;
        }

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

        if (!found) {
            qWarning() << "updateFieldRect: field not found:" << fieldName;
            return false;
        }

        doc.Save(outputPath.toUtf8().constData());
        qDebug() << "Updated rect for field" << fieldName << "on page" << pageIndex;
        return true;
    } catch (const PoDoFo::PdfError& e) {
        qWarning() << "updateFieldRect error:" << e.what();
        return false;
    }
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
bool FormManager::setTabOrder(const QString &pdfFilePath,
                              const QStringList &orderedNames,
                              const QString &outputPath)
{
    QString tempOut = (pdfFilePath == outputPath) ? outputPath + ".tmp" : outputPath;
    try {
        {
            PoDoFo::PdfMemDocument doc;
            doc.Load(pdfFilePath.toUtf8().constData());

            auto* acroForm = doc.GetAcroForm();
            if (!acroForm) {
                qWarning() << "setTabOrder: no AcroForm in" << pdfFilePath;
                return false;
            }

            // Build a name→reference map
            std::map<std::string, PoDoFo::PdfReference> refMap;
            for (unsigned i = 0; i < acroForm->GetFieldCount(); ++i) {
                auto& f = acroForm->GetFieldAt(i);
                refMap[f.GetFullName()] = f.GetObject().GetIndirectReference();
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
            
            doc.Save(tempOut.toUtf8().constData());
            qDebug() << "setTabOrder: wrote" << coArr.GetSize() << "entries to /CO";
        }
        
        if (pdfFilePath == outputPath) {
            QFile::remove(outputPath);
            QFile::rename(tempOut, outputPath);
        }
        return true;
    } catch (const PoDoFo::PdfError& e) {
        qWarning() << "setTabOrder error:" << e.what();
        return false;
    }
}
