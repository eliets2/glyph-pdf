#pragma once
#include <QDialog>
#include "core/interfaces/IPdfEditorEngine.h"

class QLineEdit;
class QSpinBox;
class QComboBox;
class QRadioButton;

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

private:
    void applyPreset(int index);
    void updateRangeEnabled();

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

    int _pageCount = 0;
};

} // namespace gp
