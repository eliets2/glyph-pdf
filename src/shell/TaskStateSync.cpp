// SPDX-License-Identifier: Apache-2.0
#include "TaskStateSync.h"
#include "TaskNav.h"
#include "ScreenNav.h"
#include "StatusBar.h"
#include "ModeStrip.h"
#include "Ribbon.h"

namespace gp {

TaskStateSync::TaskStateSync(ScreenNav* nav, StatusBar* status, ModeStrip* strip, Ribbon* ribbon,
                             QObject* parent)
    : QObject(parent), _nav(nav), _status(status), _strip(strip), _ribbon(ribbon) {}

void TaskStateSync::apply(const QString& screenId) {
    const TaskSpec* spec = TaskNav::forScreen(screenId);

    _current = screenId;

    // 1. ScreenNav checked-state.
    if (_nav) _nav->setActive(screenId);

    // 2. Status bar current-task line (the slim bar shows it in the details
    //    affordance, not as a bar cell). The id is passed through — the bar
    //    resolves the TaskNav title itself ("" maps to "Standard").
    if (_status) _status->setScreen(screenId);

    // 3. Mode pill — only when the task names one ("" = leave unchanged).
    if (_strip && spec && spec->modePill && *spec->modePill)
        _strip->setMode(QString::fromLatin1(spec->modePill));

    // 4. Ribbon tab — only when the task names one. The sync channel never
    //    changes the ribbon's expansion state.
    if (_ribbon && spec && spec->ribbonTab && *spec->ribbonTab)
        _ribbon->raiseTab(QString::fromLatin1(spec->ribbonTab));

    emit applied(screenId);
}

} // namespace gp
