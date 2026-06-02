// SPDX-License-Identifier: Apache-2.0
#include "ShortcutHelpDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTreeWidget>
#include <QPushButton>
#include <QHeaderView>

namespace gp {

ShortcutHelpDialog::ShortcutHelpDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Keyboard Shortcuts"));
    setMinimumSize(520, 480);
    setProperty("role", "modal");
    setAccessibleName(tr("Keyboard Shortcuts"));
    setAccessibleDescription(tr("Lists all available keyboard shortcuts grouped by category"));

    auto* col = new QVBoxLayout(this);

    auto* title = new QLabel(tr("Keyboard Shortcuts"));
    title->setProperty("role", "ribbonGroupTitle");
    col->addWidget(title);

    auto* tree = new QTreeWidget;
    tree->setHeaderLabels({tr("Action"), tr("Shortcut")});
    tree->header()->setStretchLastSection(true);
    tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    tree->setAccessibleName(tr("Shortcut list"));
    tree->setRootIsDecorated(true);
    tree->setAlternatingRowColors(true);

    struct Entry { QString action; QString shortcut; };
    struct Group { QString name; QVector<Entry> entries; };

    const QVector<Group> groups = {
        {tr("File"), {
            {tr("Open"),              QStringLiteral("Ctrl+O")},
            {tr("Save"),              QStringLiteral("Ctrl+S")},
            {tr("Save As"),           QStringLiteral("Ctrl+Shift+S")},
            {tr("Print"),             QStringLiteral("Ctrl+P")},
            {tr("Close"),             QStringLiteral("Ctrl+W")},
        }},
        {tr("Edit"), {
            {tr("Undo"),              QStringLiteral("Ctrl+Z")},
            {tr("Redo"),              QStringLiteral("Ctrl+Y")},
            {tr("Find / Replace"),    QStringLiteral("Ctrl+F")},
            {tr("Select All"),        QStringLiteral("Ctrl+A")},
            {tr("Delete Selected"),   QStringLiteral("Delete")},
        }},
        {tr("View"), {
            {tr("Zoom In"),           QStringLiteral("Ctrl++")},
            {tr("Zoom Out"),          QStringLiteral("Ctrl+-")},
            {tr("Fit Width"),         QStringLiteral("Ctrl+1")},
            {tr("Fit Page"),          QStringLiteral("Ctrl+2")},
            {tr("Fullscreen"),        QStringLiteral("F11")},
            {tr("Toggle Theme"),      QStringLiteral("Ctrl+Shift+T")},
        }},
        {tr("Navigate"), {
            {tr("Next Page"),         QStringLiteral("Page Down")},
            {tr("Previous Page"),     QStringLiteral("Page Up")},
            {tr("First Page"),        QStringLiteral("Ctrl+Home")},
            {tr("Last Page"),         QStringLiteral("Ctrl+End")},
            {tr("Go Back"),           QStringLiteral("Alt+Left")},
            {tr("Go Forward"),        QStringLiteral("Alt+Right")},
            {tr("Go to Page"),        QStringLiteral("Ctrl+G")},
        }},
        {tr("Accessibility"), {
            {tr("Cycle Major Regions"), QStringLiteral("F6")},
            {tr("Reverse Cycle"),       QStringLiteral("Shift+F6")},
            {tr("Show Shortcuts"),      QStringLiteral("F1")},
            {tr("Close Panel/Dialog"),  QStringLiteral("Escape")},
        }},
    };

    for (const auto& group : groups) {
        auto* parent = new QTreeWidgetItem(tree, {group.name});
        parent->setExpanded(true);
        for (const auto& entry : group.entries) {
            new QTreeWidgetItem(parent, {entry.action, entry.shortcut});
        }
    }

    col->addWidget(tree, 1);

    auto* foot = new QHBoxLayout;
    foot->addStretch();
    auto* close = new QPushButton(tr("Close"));
    close->setAccessibleName(tr("Close shortcuts dialog"));
    connect(close, &QPushButton::clicked, this, &QDialog::accept);
    foot->addWidget(close);
    col->addLayout(foot);
}

} // namespace gp
