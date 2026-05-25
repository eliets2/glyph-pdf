#pragma once
#include <QMenuBar>

class QMenu;

namespace gp {
class MenuBar : public QMenuBar {
    Q_OBJECT
public:
    explicit MenuBar(QWidget* parent = nullptr);

    /** Rebuild the recent files submenu from QSettings. */
    void refreshRecentFiles();

private:
    QMenu* m_recentMenu = nullptr;
};
} // namespace gp
