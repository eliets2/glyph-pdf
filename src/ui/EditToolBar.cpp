#include "ui/EditToolBar.h"
#include "util/Icons.h"
#include "util/GpTheme.h"

#include <QAction>
#include <QActionGroup>

EditToolBar::EditToolBar(const QString &title, QWidget *parent)
    : QToolBar(title, parent)
{
    createActions();
}

void EditToolBar::createActions()
{
    QActionGroup *toolGroup = new QActionGroup(this);

    auto makeToolAction = [&](const QString &label,
                               const QString &iconName,
                               QActionGroup *group,
                               bool checked = false) -> QAction* {
        QAction *a = new QAction(label, this);
        a->setIcon(gp::Icons::get(iconName));
        a->setCheckable(true);
        a->setChecked(checked);
        group->addAction(a);
        addAction(a);
        // Swap to accent icon when checked
        QObject::connect(a, &QAction::toggled, a, [a, iconName](bool on) {
            a->setIcon(on ? gp::Icons::get(iconName, gp::Theme::accent()) : gp::Icons::get(iconName));
        });
        return a;
    };

    handToolAct    = makeToolAction(tr("Hand Tool"),        "hand",         toolGroup, true);
    selectTextAct  = makeToolAction(tr("Select Text"),      "type",         toolGroup);
    editTextAct    = makeToolAction(tr("Edit Text"),        "edit",         toolGroup);
    editObjectAct  = makeToolAction(tr("Edit Image/Object"),"move",         toolGroup);

    addSeparator();

    addTextFieldAct = makeToolAction(tr("Add Text Field"),  "input",        toolGroup);
    addCheckboxAct  = makeToolAction(tr("Add Checkbox"),    "check-square", toolGroup);

    connect(handToolAct,    &QAction::triggered, [this]{ emit activeToolChanged(ToolMode::HandTool); });
    connect(selectTextAct,  &QAction::triggered, [this]{ emit activeToolChanged(ToolMode::SelectText); });
    connect(editTextAct,    &QAction::triggered, [this]{ emit activeToolChanged(ToolMode::EditText); });
    connect(editObjectAct,  &QAction::triggered, [this]{ emit activeToolChanged(ToolMode::EditObject); });
    connect(addTextFieldAct,&QAction::triggered, [this]{ emit activeToolChanged(ToolMode::AddTextField); });
    connect(addCheckboxAct, &QAction::triggered, [this]{ emit activeToolChanged(ToolMode::AddCheckbox); });
}
