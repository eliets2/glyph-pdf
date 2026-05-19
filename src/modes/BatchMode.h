#pragma once
#include <QWidget>

namespace gp {

class BatchMode : public QWidget {
    Q_OBJECT
public:
    explicit BatchMode(QWidget* parent = nullptr);
};

} // namespace gp
