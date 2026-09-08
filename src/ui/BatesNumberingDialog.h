// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QDialog>
#include "core/interfaces/IPdfEditorEngine.h"

class QLineEdit;
class QSpinBox;
class QComboBox;
class QRadioButton;
class QListWidget;
class QPushButton;

namespace gp {

class BatesNumberingDialog : public QDialog {
    Q_OBJECT
public:
    explicit BatesNumberingDialog(QWidget* parent = nullptr);

    /// Inform the dialog of the document's page count so the range spin boxes
    /// can be bounded and defaulted sensibly. Optional; defaults to a generous
    /// upper bound (the backend clamps the range regardless).
    void setPageCount(int pageCount);

    BatesNumberingOptions options() const;

    // ── §9.9 P1: cross-document batch ────────────────────────────────────
    // When a non-empty file list is set, Accepted means "number these files
    // as ONE continuous sequence, in list order": document N+1 starts at
    // document N's last number + 1. Each file is written as
    // `<stem>_bated.pdf` next to its input; the inputs are not modified.
    // An empty list keeps the legacy behavior (stamp the open document).
    void setBatchFiles(const QStringList& files);
    QStringList batchFiles() const;

private:
    void applyPreset(int index);
    void updateRangeEnabled();
    void addBatchFiles();
    void removeSelectedBatchFiles();

    QComboBox* _presetCombo;
    QLineEdit* _prefixEdit;
    QLineEdit* _suffixEdit;
    QSpinBox* _startSpin;
    QSpinBox* _digitsSpin;
    QComboBox* _fontCombo;
    QSpinBox* _sizeSpin;
    QComboBox* _positionCombo;

    // Page-range selection
    QRadioButton* _rangeAllRadio;
    QRadioButton* _rangeCustomRadio;
    QSpinBox* _fromSpin;
    QSpinBox* _toSpin;

    // §9.9 P1: cross-document batch file list
    QListWidget* _batchList;
    QPushButton* _batchAddBtn;
    QPushButton* _batchRemoveBtn;

    int _pageCount = 0;
};

} // namespace gp
