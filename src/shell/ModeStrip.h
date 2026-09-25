// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QFrame>
#include <QHash>
#include <QTime>
#include <QFutureWatcher>
#include <QDateTime>
#include <atomic>

class QToolButton;
class QLabel;
struct AppContext;

namespace gp {

// View/Edit/Comment/Form/Protect pills + status meta on the right.
// U02: the right cluster also carries the compact "Tools" chooser — every
// specialized task, built from the TaskNav table (one list, zero drift).
class ModeStrip : public QFrame {
    Q_OBJECT
public:
    explicit ModeStrip(QWidget* parent = nullptr);
    void init(const AppContext* ctx);
    void setMode(const QString& id);
    QString mode() const { return _active; }
    void setTheme(int themeMode); // 0=Dark 1=Light

    void updateLabels();

public slots:
    void setAutosaveTime(const QDateTime& time);
    void setSignatureStatus(int signedCount, int totalCount);

signals:
    void modeChanged(const QString& id);
    void themeToggleRequested();
    void aiToggleRequested();
    /// A task was chosen in the "Tools" chooser (host routes to activateScreen).
    void taskSelected(const QString& id);

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

    // AR-7 D2: signature validation runs on a worker thread to avoid freezing the UI.
    // _sigWatcher fires onSignatureValidationDone() on the GUI thread when finished.
    QFutureWatcher<QPair<int,int>>* _sigWatcher = nullptr;
    std::atomic<bool> _sigValidationInFlight{false};

private slots:
    void onSignatureValidationDone();
};

} // namespace gp
