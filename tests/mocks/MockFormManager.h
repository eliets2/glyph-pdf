#pragma once
#include "core/interfaces/IFormManager.h"
#include <QMap>

class MockFormManager : public IFormManager {
public:
    bool extractFormFields(const QString &pdfFilePath) override {
        m_lastFilePath = pdfFilePath;
        ++m_extractCalls;
        return m_extractResult;
    }

    bool fillForm(const QString &pdfFilePath, const QVariantMap &fieldData, const QString &outputPath) override {
        m_lastFilePath = pdfFilePath;
        m_lastOutputPath = outputPath;
        m_lastFieldData = fieldData;
        ++m_fillCalls;
        return m_fillResult;
    }

    bool hasXfaForms(const QString &pdfFilePath) override {
        m_lastFilePath = pdfFilePath;
        return m_hasXfaResult;
    }

    bool addTextField(const QString &pdfFilePath, int pageIndex, const QRectF &rect,
                      const QString &fieldName, const QString &outputPath) override {
        m_lastFilePath = pdfFilePath;
        m_lastPageIndex = pageIndex;
        m_lastRect = rect;
        m_lastFieldName = fieldName;
        m_lastOutputPath = outputPath;
        ++m_addFieldCalls;
        return m_addFieldResult;
    }

    bool addCheckBox(const QString &pdfFilePath, int pageIndex, const QRectF &rect,
                     const QString &fieldName, const QString &outputPath) override {
        m_lastFilePath = pdfFilePath;
        m_lastPageIndex = pageIndex;
        m_lastRect = rect;
        m_lastFieldName = fieldName;
        m_lastOutputPath = outputPath;
        ++m_addFieldCalls;
        return m_addFieldResult;
    }

    bool addRadioButton(const QString &pdfFilePath, int pageIndex, const QRectF &rect,
                        const QString &fieldName, const QString &outputPath) override {
        m_lastFilePath = pdfFilePath;
        m_lastPageIndex = pageIndex;
        m_lastRect = rect;
        m_lastFieldName = fieldName;
        m_lastOutputPath = outputPath;
        ++m_addFieldCalls;
        return m_addFieldResult;
    }

    bool addDropdown(const QString &pdfFilePath, int pageIndex, const QRectF &rect,
                     const QString &fieldName, const QStringList &options, const QString &outputPath) override {
        m_lastFilePath = pdfFilePath;
        m_lastPageIndex = pageIndex;
        m_lastRect = rect;
        m_lastFieldName = fieldName;
        m_lastOptions = options;
        m_lastOutputPath = outputPath;
        ++m_addFieldCalls;
        return m_addFieldResult;
    }

    // Test helpers -- configurable return values
    bool m_extractResult = true;
    bool m_fillResult = true;
    bool m_hasXfaResult = false;
    bool m_addFieldResult = true;

    // Test helpers -- call tracking
    int m_extractCalls = 0;
    int m_fillCalls = 0;
    int m_addFieldCalls = 0;
    QString m_lastFilePath;
    QString m_lastOutputPath;
    QString m_lastFieldName;
    QVariantMap m_lastFieldData;
    QStringList m_lastOptions;
    QRectF m_lastRect;
    int m_lastPageIndex = -1;
};
