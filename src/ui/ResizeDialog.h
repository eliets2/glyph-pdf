// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QDialog>
#include <QSizeF>

class QComboBox;
class QDoubleSpinBox;

namespace gp {

class ResizeDialog : public QDialog {
    Q_OBJECT
public:
    explicit ResizeDialog(QWidget* parent = nullptr);
    QSizeF selectedSize() const;

private:
    QComboBox* _presetCombo;
    QDoubleSpinBox* _widthSpin;
    QDoubleSpinBox* _heightSpin;
    QComboBox* _unitCombo;

    void updateSpinsFromPreset();
};

} // namespace gp
