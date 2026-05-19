#pragma once
#include <QFrame>
#include <QHash>

class QToolButton;

namespace gp {

class ScreenNav : public QFrame {
    Q_OBJECT
public:
    explicit ScreenNav(QWidget* parent = nullptr);
    void setActive(const QString& id);
signals:
    void screenSelected(const QString& id);
private:
    QHash<QString, QToolButton*> _items;
    QString _active;
};

} // namespace gp
