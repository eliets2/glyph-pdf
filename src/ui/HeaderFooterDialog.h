#pragma once
#include <QDialog>
#include "core/interfaces/IPdfEditorEngine.h"

class QLineEdit;
class QComboBox;
class QSpinBox;

namespace gp {

class HeaderFooterDialog : public QDialog {
    Q_OBJECT
public:
    explicit HeaderFooterDialog(QWidget* parent = nullptr);
    HeaderFooterOptions options() const;

private:
    QLineEdit* _textEdit;
    QComboBox* _fontCombo;
    QSpinBox* _sizeSpin;
    QComboBox* _positionCombo;
};

} // namespace gp
