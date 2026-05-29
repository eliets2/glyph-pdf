#pragma once
#include <QDialog>
#include "core/interfaces/IPdfEditorEngine.h"

class QLineEdit;
class QSpinBox;
class QComboBox;

namespace gp {

class BatesNumberingDialog : public QDialog {
    Q_OBJECT
public:
    explicit BatesNumberingDialog(QWidget* parent = nullptr);
    BatesNumberingOptions options() const;

private:
    QLineEdit* _prefixEdit;
    QLineEdit* _suffixEdit;
    QSpinBox* _startSpin;
    QSpinBox* _digitsSpin;
    QComboBox* _fontCombo;
    QSpinBox* _sizeSpin;
    QComboBox* _positionCombo;
};

} // namespace gp
