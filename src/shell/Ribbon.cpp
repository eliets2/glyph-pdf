// SPDX-License-Identifier: Apache-2.0
#include "Ribbon.h"
#include "RibbonModel.h"
#include "util/GpTheme.h"
#include "util/Icons.h"
#include <QSet>

#include <algorithm>

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QStackedWidget>
#include <QStyle>
#include <QTabBar>
#include <QToolButton>
#include <QVBoxLayout>

namespace gp {

Ribbon::Ribbon(QWidget* parent) : QWidget(parent) {
    setAccessibleName(tr("Ribbon toolbar"));
    setAccessibleDescription(tr("Main toolbar with tool groups organized in tabs"));

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    _tabs = new QTabBar(this);
    _tabs->setObjectName("ribbonTabs");
    _tabs->setExpanding(false);
    _tabs->setDrawBase(false);
    _tabs->setFocusPolicy(Qt::TabFocus);
    _tabs->setAccessibleName(tr("Ribbon tabs"));
    for (const auto& def : RibbonModel::tabs()) {
        _tabs->addTab(def.name);
    }

    _bodyStack = new QStackedWidget(this);
    _bodyStack->setObjectName("ribbonBody");
    _bodyStack->setFixedHeight(Theme::RibbonBodyH);

    for (int i = 0; i < RibbonModel::tabs().size(); ++i) {
        if (i == 0) {
            _bodyStack->addWidget(buildBody(0));
        } else {
            _bodyStack->addWidget(new QWidget(this));
        }
    }

    connect(_tabs, &QTabBar::currentChanged, this, [this](int idx) {
        QWidget* current = _bodyStack->widget(idx);
        if (current && !current->layout()) {
            QWidget* realBody = buildBody(idx);
            _bodyStack->removeWidget(current);
            current->deleteLater();
            _bodyStack->insertWidget(idx, realBody);
        }
        _bodyStack->setCurrentIndex(idx);
        emit tabChanged(RibbonModel::tabs().at(idx).name);
    });

    outer->addWidget(_tabs);
    outer->addWidget(_bodyStack);
}

QToolButton* Ribbon::makeTool(const QString& id, const QString& label,
                              const QString& icon, bool big) {
    auto* btn = new QToolButton;
    btn->setText(label);
    btn->setIcon(Icons::get(icon));
    btn->setIconSize(QSize(big ? 22 : 16, big ? 22 : 16));
    btn->setToolButtonStyle(big ? Qt::ToolButtonTextUnderIcon : Qt::ToolButtonTextBesideIcon);
    btn->setProperty("variant", "tool");
    btn->setProperty("size",   big ? "big" : "row");
    btn->setProperty("toolId", id);
    btn->setAutoRaise(true);
    btn->setCheckable(false);
    btn->setFocusPolicy(Qt::TabFocus);
    btn->setAccessibleName(label);
    btn->setAccessibleDescription(tr("Activate %1 tool").arg(label));

    return btn;
}

QWidget* Ribbon::buildBody(int tabIdx) {
    const auto& def = RibbonModel::tabs().at(tabIdx);
    auto* host = new QWidget;
    auto* row  = new QHBoxLayout(host);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(0);

    for (int g = 0; g < def.groups.size(); ++g) {
        const auto& grp = def.groups.at(g);
        auto* group = new QFrame;
        group->setProperty("role", "ribbonGroup");
        auto* col = new QVBoxLayout(group);
        col->setContentsMargins(0, 0, 0, 0);
        col->setSpacing(0);

        auto* bodyHolder = new QWidget;
        auto* bodyRow = new QHBoxLayout(bodyHolder);
        bodyRow->setContentsMargins(8, 4, 8, 2);
        bodyRow->setSpacing(2);

        QVector<Tool> bigs, smalls;
        for (const auto& t : grp.tools) (t.big ? bigs : smalls).append(t);

        const QSet<QString>& planned = RibbonModel::plannedTools();

        for (const auto& t : bigs) {
            auto* b = makeTool(t.id, t.label, t.icon, true);
            if (t.id == _activeTool) b->setProperty("active", true);
            if (planned.contains(t.id)) {
                b->setEnabled(false);
                b->setToolTip(tr("Planned for a future release"));
            } else {
                const QString id = t.id;
                connect(b, &QToolButton::clicked, this, [this, id]() {
                    setActiveTool(id);
                    emit toolActivated(id);
                });
            }
            _buttons.insert(t.id, b);
            bodyRow->addWidget(b);
        }

        for (int s = 0; s < smalls.size(); s += 3) {
            auto* colW = new QWidget;
            auto* colLay = new QVBoxLayout(colW);
            colLay->setContentsMargins(0, 0, 0, 0);
            colLay->setSpacing(1);
            for (int k = s; k < std::min(s + 3, static_cast<int>(smalls.size())); ++k) {
                const auto& t = smalls.at(k);
                auto* b = makeTool(t.id, t.label, t.icon, false);
                if (t.id == _activeTool) b->setProperty("active", true);
                if (planned.contains(t.id)) {
                    b->setEnabled(false);
                    b->setToolTip(tr("Planned for a future release"));
                } else {
                    const QString id = t.id;
                    connect(b, &QToolButton::clicked, this, [this, id]() {
                        setActiveTool(id);
                        emit toolActivated(id);
                    });
                }
                _buttons.insert(t.id, b);
                colLay->addWidget(b);
            }
            colLay->addStretch();
            bodyRow->addWidget(colW);
        }

        col->addWidget(bodyHolder, 1);

        auto* title = new QLabel(grp.title);
        title->setProperty("role", "ribbonGroupTitle");
        title->setAlignment(Qt::AlignCenter);
        title->setFixedHeight(16);
        col->addWidget(title);

        row->addWidget(group);
    }
    row->addStretch(1);

    auto* scroller = new QScrollArea;
    scroller->setWidget(host);
    scroller->setWidgetResizable(true);
    scroller->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroller->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroller->setFrameShape(QFrame::NoFrame);
    return scroller;
}

void Ribbon::setActiveTool(const QString& toolId) {
    if (_activeTool == toolId) return;
    if (auto* prev = _buttons.value(_activeTool)) {
        prev->setProperty("active", false);
        prev->style()->unpolish(prev);
        prev->style()->polish(prev);
    }
    _activeTool = toolId;
    if (auto* next = _buttons.value(toolId)) {
        next->setProperty("active", true);
        next->style()->unpolish(next);
        next->style()->polish(next);
    }
}

} // namespace gp
