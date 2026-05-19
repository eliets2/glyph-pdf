#pragma once
#include <QDialog>

namespace gp {

class WatermarkDialog : public QDialog {
    Q_OBJECT
public:
    explicit WatermarkDialog(QWidget* parent = nullptr);
};

} // namespace gp
