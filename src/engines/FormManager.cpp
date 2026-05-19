#include "engines/FormManager.h"
#include <memory>
#include <QDebug>
#include <podofo/podofo.h>
#include <QString>

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

        page.CreateField<PoDoFo::PdfCheckBox>(fieldName.toStdString(), pdfRect);

        doc.Save(outputPath.toUtf8().constData());
        qDebug() << "Added radio button (as checkbox)" << fieldName << "on page" << pageIndex;
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

        doc.Save(outputPath.toUtf8().constData());
        qDebug() << "Added dropdown" << fieldName << "with" << options.size() << "options on page" << pageIndex;
        return true;
    } catch (const PoDoFo::PdfError& e) {
        qWarning() << "Error adding dropdown:" << e.what();
        return false;
    }
}
