// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QFrame>
#include <QHash>

class QToolButton;

namespace gp {

// U02: the bottom screen-nav strip. Items are built from the TaskNav table
// (no "01"-style numbering — titles are human task names); ids and the
// screenSelected signal are unchanged and remain the navigation interface.
class ScreenNav : public QFrame {
    Q_OBJECT
public:
    explicit ScreenNav(QWidget* parent = nullptr);
    void setActive(const QString& id);
    QString active() const { return _active; }
signals:
    void screenSelected(const QString& id);
private:
    QHash<QString, QToolButton*> _items;
    QString _active;
};

} // namespace gp
