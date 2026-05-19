#pragma once
#include <QMenuBar>

namespace gp {
class MenuBar : public QMenuBar {
    Q_OBJECT
public:
    explicit MenuBar(QWidget* parent = nullptr);
};
} // namespace gp
