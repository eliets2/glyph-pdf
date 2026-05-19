#include "ui/AnnotationToolBar.h"
#include "util/Icons.h"
#include "util/GpTheme.h"

#include <QAction>
#include <QActionGroup>
#include <QColorDialog>
#include <QMenu>
#include <QSpinBox>
#include <QLabel>

AnnotationToolBar::AnnotationToolBar(const QString &title, QWidget *parent)
    : QToolBar(title, parent)
{
    createActions();
}

void AnnotationToolBar::createActions()
{
    QActionGroup *toolGroup = new QActionGroup(this);

    auto makeToolAction = [&](const QString &label,
                               const QString &iconName,
                               QActionGroup *group) -> QAction* {
        QAction *a = new QAction(label, this);
        a->setIcon(gp::Icons::get(iconName));
        a->setCheckable(true);
        group->addAction(a);
        addAction(a);
        return a;
    };

    selectAct    = makeToolAction(tr("Select"),    "cursor",         toolGroup);
    highlightAct = makeToolAction(tr("Highlight"), "highlight",      toolGroup);
    underlineAct = makeToolAction(tr("Underline"), "underline",      toolGroup);
    drawAct      = makeToolAction(tr("Draw"),      "pencil",         toolGroup);
    textAct      = makeToolAction(tr("Text Box"),  "text-box",       toolGroup);
    commentAct   = makeToolAction(tr("Comment"),   "message-square", toolGroup);
    redactAct    = makeToolAction(tr("Redact"),    "redact",         toolGroup);
    signAct      = makeToolAction(tr("Sign"),      "pen-tool",       toolGroup);
    rectAct      = makeToolAction(tr("Rectangle"), "square",         toolGroup);
    ellipseAct   = makeToolAction(tr("Ellipse"),   "circle",         toolGroup);
    lineAct      = makeToolAction(tr("Line"),      "minus",          toolGroup);
    arrowAct     = makeToolAction(tr("Arrow"),     "arrow-right",    toolGroup);

    // Update icon to accent when checked
    auto applyCheckedStyle = [](QAction *act, const QString &iconName) {
        QObject::connect(act, &QAction::toggled, act, [act, iconName](bool checked) {
            act->setIcon(checked ? gp::Icons::get(iconName, gp::Theme::accent())
                                 : gp::Icons::get(iconName));
        });
    };
    applyCheckedStyle(selectAct,    "cursor");
    applyCheckedStyle(highlightAct, "highlight");
    applyCheckedStyle(underlineAct, "underline");
    applyCheckedStyle(drawAct,      "pencil");
    applyCheckedStyle(textAct,      "text-box");
    applyCheckedStyle(commentAct,   "message-square");
    applyCheckedStyle(redactAct,    "redact");
    applyCheckedStyle(signAct,      "pen-tool");
    applyCheckedStyle(rectAct,      "square");
    applyCheckedStyle(ellipseAct,   "circle");
    applyCheckedStyle(lineAct,      "minus");
    applyCheckedStyle(arrowAct,     "arrow-right");

    deleteAct = new QAction(tr("Delete"), this);
    deleteAct->setIcon(gp::Icons::get("trash", gp::Theme::danger()));
    addAction(deleteAct);

    // Signals
    connect(selectAct,    &QAction::triggered, [this]{ emit activeToolChanged(ToolMode::EditObject); });
    connect(highlightAct, &QAction::triggered, [this]{ emit activeToolChanged(ToolMode::Highlight); });
    connect(underlineAct, &QAction::triggered, [this]{ emit activeToolChanged(ToolMode::Underline); });
    connect(drawAct,      &QAction::triggered, [this]{ emit activeToolChanged(ToolMode::DrawFreehand); });
    connect(textAct,      &QAction::triggered, [this]{ emit activeToolChanged(ToolMode::AddTextBox); });
    connect(commentAct,   &QAction::triggered, [this]{ emit activeToolChanged(ToolMode::AddComment); });
    connect(redactAct,    &QAction::triggered, [this]{ emit activeToolChanged(ToolMode::Redact); });
    connect(signAct,      &QAction::triggered, [this]{ emit activeToolChanged(ToolMode::AddSignature); });
    connect(rectAct,      &QAction::triggered, [this]{ emit activeToolChanged(ToolMode::DrawRectangle); });
    connect(ellipseAct,   &QAction::triggered, [this]{ emit activeToolChanged(ToolMode::DrawEllipse); });
    connect(lineAct,      &QAction::triggered, [this]{ emit activeToolChanged(ToolMode::DrawLine); });
    connect(arrowAct,     &QAction::triggered, [this]{ emit activeToolChanged(ToolMode::DrawArrow); });
    connect(deleteAct,    &QAction::triggered, this, &AnnotationToolBar::deleteRequested);

    addSeparator();

    // ── Color picker ──────────────────────────────────────────────────
    QAction *colorAct = new QAction(tr("Color"), this);
    colorAct->setIcon(gp::Icons::get("palette"));
    QMenu *colorMenu = new QMenu(this);

    auto addColor = [&](const QString &name, const QColor &color) {
        QAction *a = colorMenu->addAction(name);
        connect(a, &QAction::triggered, [this, color]{ emit colorChanged(color); });
    };
    addColor(tr("Yellow"), QColor("#FACC15"));
    addColor(tr("Red"),    QColor("#EF4444"));
    addColor(tr("Green"),  QColor("#22C55E"));
    addColor(tr("Blue"),   QColor("#5BA3FF"));
    addColor(tr("Cyan"),   QColor("#06B6D4"));
    addColor(tr("Purple"), QColor("#A855F7"));
    addColor(tr("Orange"), QColor("#F97316"));
    colorMenu->addSeparator();
    QAction *customAct = colorMenu->addAction(tr("Custom…"));
    connect(customAct, &QAction::triggered, [this]{
        QColor c = QColorDialog::getColor(QColor("#FACC15"), this, tr("Annotation Color"));
        if (c.isValid()) emit colorChanged(c);
    });

    colorAct->setMenu(colorMenu);
    addAction(colorAct);

    // ── Thickness ────────────────────────────────────────────────────
    addSeparator();
    auto *sizeLabel = new QLabel(tr("  Size: "), this);
    sizeLabel->setStyleSheet("color: #808080; font-size: 11px;");
    addWidget(sizeLabel);

    QSpinBox *sizeBox = new QSpinBox(this);
    sizeBox->setRange(1, 20);
    sizeBox->setValue(2);
    sizeBox->setFixedWidth(52);
    connect(sizeBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &AnnotationToolBar::thicknessChanged);
    addWidget(sizeBox);
}
