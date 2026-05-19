#pragma once
#include <QWidget>

namespace gp {

class CompareMode : public QWidget {
    Q_OBJECT
public:
    explicit CompareMode(QWidget* parent = nullptr);
};

} // namespace gp
