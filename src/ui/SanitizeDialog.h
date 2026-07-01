// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QDialog>
#include "core/interfaces/IPdfEditorEngine.h"

class QCheckBox;
class QLabel;
class QGroupBox;

class SanitizeDialog : public QDialog {
    Q_OBJECT
public:
    explicit SanitizeDialog(IPdfEditorEngine* engine, QWidget* parent = nullptr);
    ~SanitizeDialog() override;

    SanitizeOptions getOptions() const;

private slots:
    void updateEstimate();
    void toggleSelectiveMode(bool enabled);

private:
    IPdfEditorEngine* m_engine;

    QCheckBox* m_selectiveModeCheckbox;
    QGroupBox* m_optionsGroup;

    QCheckBox* m_chkMetadata;
    QCheckBox* m_chkHiddenText;
    QCheckBox* m_chkOptionalContent;
    QCheckBox* m_chkBookmarks;
    QCheckBox* m_chkEmbeddedFiles;
    QCheckBox* m_chkFormValues;
    QCheckBox* m_chkAnnotations;
    QCheckBox* m_chkDangerousActions;

    QLabel* m_estimateLabel;
};
