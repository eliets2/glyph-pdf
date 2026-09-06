// SPDX-License-Identifier: Apache-2.0
#include "Ribbon.h"
#include "RibbonModel.h"
#include "util/GpTheme.h"
#include "util/Icons.h"
#include <QSet>

#include <algorithm>

#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QShortcut>
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

    // ── U02: tab row = tabs + active-task label + collapse chevron ──
    auto* tabRow = new QWidget(this);
    tabRow->setObjectName("ribbonTabRow");
    auto* tabRowLay = new QHBoxLayout(tabRow);
    tabRowLay->setContentsMargins(0, 0, 2, 0);
    tabRowLay->setSpacing(4);
    tabRowLay->addWidget(_tabs, 1);

    // Active task stays visible while collapsed (plan U02 requirement).
    // Vertical Ignored policies keep the extra row content from growing the
    // tab row: collapse must shrink the ribbon by exactly the body height.
    _activeTaskLabel = new QLabel(tabRow);
    _activeTaskLabel->setObjectName("ribbonActiveTaskLabel");
    _activeTaskLabel->setProperty("mono", true);
    _activeTaskLabel->setAlignment(Qt::AlignVCenter | Qt::AlignRight);
    _activeTaskLabel->setAccessibleName(tr("Active task"));
    _activeTaskLabel->setVisible(false);
    _activeTaskLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Ignored);
    tabRowLay->addWidget(_activeTaskLabel);

    _collapseBtn = new QToolButton(tabRow);
    _collapseBtn->setObjectName("ribbonCollapseBtn");
    _collapseBtn->setText(QChar(0x25B4));   // ▴ — click collapses (▾ when collapsed)
    _collapseBtn->setCheckable(true);
    _collapseBtn->setAutoRaise(true);
    _collapseBtn->setFocusPolicy(Qt::TabFocus);
    _collapseBtn->setToolTip(tr("Collapse the ribbon (Ctrl+F1)"));
    _collapseBtn->setAccessibleName(tr("Collapse ribbon"));
    _collapseBtn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Ignored);
    tabRowLay->addWidget(_collapseBtn);

    connect(_collapseBtn, &QToolButton::toggled, this, [this](bool checked) {
        setCollapsed(checked);   // no-op when states already agree
    });

    // Microsoft's documented Ctrl+F1 collapse toggle.
    auto* collapseShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_F1), this);
    connect(collapseShortcut, &QShortcut::activated, this, [this]() {
        setCollapsed(!_collapsed);
    });

    // Double-click on the tab bar toggles the ribbon.
    _tabs->installEventFilter(this);

    connect(_tabs, &QTabBar::currentChanged, this, [this](int idx) {
        // Fluent v1 simplification: selecting a tab with the mouse expands
        // the ribbon and stays expanded. The TaskStateSync raiseTab() channel
        // (guarded) must not change the expansion state.
        if (_collapsed && !_raiseInProgress)
            setCollapsed(false);

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

    outer->addWidget(tabRow);
    outer->addWidget(_bodyStack);
}

bool Ribbon::eventFilter(QObject* watched, QEvent* event) {
    if (watched == _tabs && event->type() == QEvent::MouseButtonDblClick) {
        setCollapsed(!_collapsed);
        return true;
    }
    return QWidget::eventFilter(watched, event);
}

// ── U02 collapse ─────────────────────────────────────────────────────────────

void Ribbon::setCollapsed(bool collapsed) {
    if (_collapsed == collapsed) return;
    _collapsed = collapsed;

    _bodyStack->setVisible(!collapsed);
    _activeTaskLabel->setVisible(collapsed);
    if (collapsed) {
        _activeTaskLabel->setText(collapsedLabel());
    }
    _collapseBtn->setText(collapsed ? QChar(0x25BE)   // ▾
                                    : QChar(0x25B4)); // ▴
    _collapseBtn->setChecked(collapsed);
    updateGeometry();
    emit collapsedChanged(collapsed);
}

void Ribbon::raiseTab(const QString& tabName) {
    const int idx = tabIndexFor(tabName);
    if (idx < 0) return;
    _raiseInProgress = true;
    _tabs->setCurrentIndex(idx);   // lazy-build + tabChanged handled by the ctor lambda
    _raiseInProgress = false;
}

int Ribbon::tabIndexFor(const QString& tabName) const {
    const auto& defs = RibbonModel::tabs();
    for (int i = 0; i < defs.size(); ++i)
        if (defs.at(i).name == tabName)
            return i;
    return -1;
}

QString Ribbon::activeTabName() const {
    return _tabs->tabText(_tabs->currentIndex());
}

int Ribbon::bodyHeight() const {
    return _bodyStack->isVisible() ? _bodyStack->height() : 0;
}

QString Ribbon::collapsedLabel() const {
    QString tool = _activeTool;
    if (auto* b = _buttons.value(_activeTool))
        tool = b->text();
    return tr("%1 \xC2\xB7 %2").arg(activeTabName(), tool);   // "Tab · Tool"
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

        // AR-8 D3: planned tools are NOT rendered (hidden, not disabled).
        // Their ToolId enum entries, RibbonModel definitions, and plannedTools()
        // registry are all preserved — re-enable with one line by removing
        // the id from plannedTools() when the feature ships.
        const QSet<QString>& planned = RibbonModel::plannedTools();

        QVector<Tool> bigs, smalls;
        for (const auto& t : grp.tools) {
            if (planned.contains(t.id)) continue;  // hide, not disable
            (t.big ? bigs : smalls).append(t);
        }

        // A1: if every tool in this group is planned/hidden, skip the entire
        // group frame so an empty titled box is never shown.
        if (bigs.isEmpty() && smalls.isEmpty()) {
            delete group;
            continue;
        }

        for (const auto& t : bigs) {
            auto* b = makeTool(t.id, t.label, t.icon, true);
            if (t.id == _activeTool) b->setProperty("active", true);
            const QString id = t.id;
            connect(b, &QToolButton::clicked, this, [this, id]() {
                setActiveTool(id);
                emit toolActivated(id);
            });
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
                const QString id = t.id;
                connect(b, &QToolButton::clicked, this, [this, id]() {
                    setActiveTool(id);
                    emit toolActivated(id);
                });
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
    // Keep the collapsed active-task line current.
    if (_collapsed)
        _activeTaskLabel->setText(collapsedLabel());
}

} // namespace gp
