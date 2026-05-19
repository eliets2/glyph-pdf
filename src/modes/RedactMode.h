#pragma once
#include <QWidget>

namespace gp {

class RedactMode : public QWidget {
    Q_OBJECT
public:
    explicit RedactMode(QWidget* parent = nullptr);
};

} // namespace gp
