#pragma once
#include <QFrame>

namespace gp {

class PdfAValidationPanel : public QFrame {
    Q_OBJECT
public:
    explicit PdfAValidationPanel(QWidget* parent = nullptr);
};

} // namespace gp
