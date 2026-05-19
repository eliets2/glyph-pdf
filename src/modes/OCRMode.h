#pragma once
#include <QWidget>

namespace gp {

// OCR Verification Mode: top toolbar + info strip + 4-pane splitter.
class OCRMode : public QWidget {
    Q_OBJECT
public:
    explicit OCRMode(QWidget* parent = nullptr);
};

} // namespace gp
