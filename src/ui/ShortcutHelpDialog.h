#pragma once
#include <QDialog>

namespace gp {

class ShortcutHelpDialog : public QDialog {
    Q_OBJECT
public:
    explicit ShortcutHelpDialog(QWidget* parent = nullptr);
};

} // namespace gp
