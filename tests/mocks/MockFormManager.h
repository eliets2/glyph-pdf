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

    bool fillForm(const QString &pdfFilePath, const QVariantMap &fieldData, const QString &outputPath, bool lockFields) override {
        m_lastFilePath = pdfFilePath;
        m_lastOutputPath = outputPath;
        m_lastFieldData = fieldData;
        m_lastLockFields = lockFields;
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

    bool addCalculatedField(const QString &pdfFilePath, int pageIndex, const QRectF &rect,
                            const QString &fieldName, const QString &expression,
                            const QString &outputPath) override {
        m_lastFilePath = pdfFilePath;
        m_lastPageIndex = pageIndex;
        m_lastRect = rect;
        m_lastFieldName = fieldName;
        m_lastExpression = expression;
        m_lastOutputPath = outputPath;
        ++m_addFieldCalls;
        return m_addFieldResult;
    }

    bool setFieldMetadata(const QString &pdfFilePath, const QString &fieldName,
                          const QString &tooltip, bool required,
                          const QString &outputPath) override {
        m_lastFilePath = pdfFilePath;
        m_lastFieldName = fieldName;
        m_lastTooltip = tooltip;
        m_lastRequired = required;
        m_lastOutputPath = outputPath;
        ++m_setMetadataCalls;
        return m_setMetadataResult;
    }

    // R02 (F09): snapshot capture/apply stubs — the mock keeps no document,
    // so capture resolves the field as NOT found (explicit missing-field
    // resolution) and apply refuses to write.
    FormFieldSnapshot captureFieldSnapshot(const QString &pdfFilePath, const QString &fieldName) override {
        m_lastFilePath = pdfFilePath;
        m_lastFieldName = fieldName;
        FormFieldSnapshot snap;
        snap.name = fieldName;
        snap.found = false;
        ++m_captureSnapshotCalls;
        return snap;
    }

    bool applyFieldSnapshot(const QString &pdfFilePath, const FormFieldSnapshot &target,
                            const QString &outputPath) override {
        m_lastFilePath = pdfFilePath;
        m_lastOutputPath = outputPath;
        m_lastSnapshot = target;
        ++m_applySnapshotCalls;
        return m_applySnapshotResult;
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
    QString m_lastExpression;
    QVariantMap m_lastFieldData;
    bool m_lastLockFields = true;
    QString m_lastTooltip;
    bool m_lastRequired = false;
    int m_setMetadataCalls = 0;
    bool m_setMetadataResult = true;
    QStringList m_lastOptions;
    QRectF m_lastRect;
    int m_lastPageIndex = -1;

    // R02 snapshot seam
    FormFieldSnapshot m_lastSnapshot;
    bool m_applySnapshotResult = true;
    int m_captureSnapshotCalls = 0;
    int m_applySnapshotCalls = 0;
};
