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

bool FormManager::fillForm(const QString &pdfFilePath, const QVariantMap &fieldData, const QString &outputPath)
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

        for (unsigned i = 0; i < acroForm->GetFieldCount(); ++i) {
            auto& field = acroForm->GetFieldAt(i);
            QString name = QString::fromStdString(field.GetFullName());
            if (!fieldData.contains(name)) continue;

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
                    break;
            }

            field.SetReadOnly(true);
        }

        doc.Save(outputPath.toUtf8().constData());
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

QList<FieldSuggestion> FormManager::autoDetectFields(const QString &pdfFilePath, int pageIndex)
{
    QList<FieldSuggestion> suggestions;
    suggestions.append({QRectF(100, 200, 200, 25), "Text", "AutoName"});
    suggestions.append({QRectF(100, 250, 200, 25), "Date", "AutoDate"});
    suggestions.append({QRectF(100, 300, 20, 20), "Checkbox", "AutoCheck"});
    return suggestions;
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

bool FormManager::importFormData(const QString &pdfFilePath, const QString &dataFilePath, const QString &outputPath)
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

    return fillForm(pdfFilePath, data, outputPath);
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
                    if (pObj->IsReference()) pObj = &doc.GetObjects().MustGetObject(pObj->GetReference());
                    for (unsigned pi = 0; pi < doc.GetPages().GetCount(); ++pi) {
                        if (doc.GetPages().GetPageAt(pi).GetObject().GetReference() == pObj->GetReference()) {
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
                            if (annos.GetAnnotAt(ai).GetObject().GetReference() == field.GetObject().GetReference()) {
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
