#pragma once
#include <QDialog>

namespace gp {

class CompressDialog : public QDialog {
    Q_OBJECT
public:
    explicit CompressDialog(QWidget* parent = nullptr);
};

} // namespace gp
