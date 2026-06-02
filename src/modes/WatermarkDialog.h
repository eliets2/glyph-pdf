// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QDialog>

class QLineEdit;
class QSpinBox;
class QComboBox;
class QRadioButton;
class QToolButton;
class QLabel;
class QTabWidget;
class QButtonGroup;
struct AppContext;

namespace gp {
class LabeledSlider;

class WatermarkDialog : public QDialog {
    Q_OBJECT
public:
    explicit WatermarkDialog(const AppContext* ctx, QWidget* parent = nullptr);

private slots:
    void onApply();
    void updatePreview();

private:
    void buildTextTab(QWidget* tab);
    void buildImageTab(QWidget* tab);
    void buildPageRange(QWidget* container);

    const AppContext* _ctx = nullptr;

    // Text tab
    QLineEdit*     _textInput     = nullptr;
    QComboBox*     _fontCombo     = nullptr;
    QSpinBox*      _fontSizeSpin  = nullptr;
    LabeledSlider* _opacitySlider = nullptr;
    QSpinBox*      _rotationSpin  = nullptr;
    QToolButton*   _colorBtn      = nullptr;

    // Image tab
    QLineEdit*     _imagePath        = nullptr;
    LabeledSlider* _imgOpacitySlider = nullptr;
    LabeledSlider* _imgScaleSlider   = nullptr;

    // Shared
    QTabWidget*    _tabs       = nullptr;
    QButtonGroup*  _posGroup   = nullptr;
    QRadioButton*  _allPages   = nullptr;
    QRadioButton*  _customPages = nullptr;
    QSpinBox*      _pageFrom   = nullptr;
    QSpinBox*      _pageTo     = nullptr;

    // Preview
    QLabel* _previewLabel = nullptr;
};

} // namespace gp
