// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QFrame>
#include <QHash>
#include <QTime>

#include <QDateTime>

class QToolButton;
class QLabel;
struct AppContext;

namespace gp {

// View/Edit/Comment/Form/Protect pills + status meta on the right.
class ModeStrip : public QFrame {
    Q_OBJECT
public:
    explicit ModeStrip(QWidget* parent = nullptr);
    void init(const AppContext* ctx);
    void setMode(const QString& id);
    void setTheme(int themeMode); // 0=Dark 1=Light

    void updateLabels();

public slots:
    void setAutosaveTime(const QDateTime& time);
    void setSyncStatus(const QString& status);
    void setSignatureStatus(int signedCount, int totalCount);

signals:
    void modeChanged(const QString& id);
    void themeToggleRequested();
    void aiToggleRequested();

private:
    QHash<QString, QToolButton*> _pills;
    QString _active;

    const AppContext* _ctx = nullptr;
    QLabel* _autosaveLabel = nullptr;
    QLabel* _syncLabel = nullptr;
    QLabel* _signLabel = nullptr;
    QTime _lastSavedTime;
    QString _lastPath;
    int _cachedValidSigs = 0;
    int _cachedTotalSigs = 0;
};

} // namespace gp
