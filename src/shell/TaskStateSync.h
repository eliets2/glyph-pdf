// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QObject>
#include <QString>

namespace gp {

class ScreenNav;
class StatusBar;
class ModeStrip;
class Ribbon;

// U02 — the SINGLE writer of visible navigation state.
//
// One navigation event (a screen activation) reaches every visible-state
// widget through exactly this object: ScreenNav checked-state, the status
// bar's current-task line, the ModeStrip pill (only when the task names one)
// and the Ribbon tab (only when the task names one). Nothing else may call
// ScreenNav::setActive / ModeStrip::setMode / Ribbon::raiseTab for navigation
// sync — that is the anti-drift rule (research Q-U02, Module 3).
//
// Plain QObject, headless-constructible with widget stubs (null members are
// guarded); data comes from the TaskNav table, never from callers.
class TaskStateSync : public QObject {
    Q_OBJECT
public:
    TaskStateSync(ScreenNav* nav, StatusBar* status, ModeStrip* strip, Ribbon* ribbon,
                  QObject* parent = nullptr);

    /// Full sync from TaskNav::forScreen(screenId). Idempotent.
    void apply(const QString& screenId);

    QString currentScreen() const { return _current; }

signals:
    /// Emitted once per apply() — observation seam for tests.
    void applied(const QString& screenId);

private:
    ScreenNav*  _nav    = nullptr;
    StatusBar*  _status = nullptr;
    ModeStrip*  _strip  = nullptr;
    Ribbon*     _ribbon = nullptr;
    QString     _current;
};

} // namespace gp
