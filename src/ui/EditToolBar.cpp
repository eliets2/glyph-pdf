// SPDX-License-Identifier: Apache-2.0
#include "ui/EditToolBar.h"
#include "util/Icons.h"
#include "util/GpTheme.h"

#include <QAction>
#include <QActionGroup>
#include <QFontComboBox>
#include <QComboBox>
#include <QColorDialog>
#include <QHBoxLayout>
#include <QWidget>
#include <QToolButton>
#include <QLabel>

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

    // Formatting widget
    formatWidget = new QWidget(this);
    QHBoxLayout *fmtLayout = new QHBoxLayout(formatWidget);
    fmtLayout->setContentsMargins(0, 0, 0, 0);
    fmtLayout->setSpacing(4);

    fontFamilyCombo = new QFontComboBox(formatWidget);
    fmtLayout->addWidget(fontFamilyCombo);

    fontSizeCombo = new QComboBox(formatWidget);
    fontSizeCombo->addItems({"8", "9", "10", "11", "12", "14", "16", "18", "20", "24", "36", "48", "72"});
    fontSizeCombo->setCurrentText("12");
    fmtLayout->addWidget(fontSizeCombo);

    boldAct = new QAction(gp::Icons::get("bold"), tr("Bold"), this);
    boldAct->setCheckable(true);
    QToolButton *btnBold = new QToolButton(formatWidget);
    btnBold->setDefaultAction(boldAct);
    fmtLayout->addWidget(btnBold);

    italicAct = new QAction(gp::Icons::get("italic"), tr("Italic"), this);
    italicAct->setCheckable(true);
    QToolButton *btnItalic = new QToolButton(formatWidget);
    btnItalic->setDefaultAction(italicAct);
    fmtLayout->addWidget(btnItalic);

    QActionGroup *alignGroup = new QActionGroup(this);
    alignLeftAct = new QAction(gp::Icons::get("align-left"), tr("Align Left"), this);
    alignCenterAct = new QAction(gp::Icons::get("align-center"), tr("Align Center"), this);
    alignRightAct = new QAction(gp::Icons::get("align-right"), tr("Align Right"), this);
    alignLeftAct->setCheckable(true); alignCenterAct->setCheckable(true); alignRightAct->setCheckable(true);
    alignLeftAct->setChecked(true);
    alignGroup->addAction(alignLeftAct); alignGroup->addAction(alignCenterAct); alignGroup->addAction(alignRightAct);

    QToolButton *btnAlignL = new QToolButton(formatWidget); btnAlignL->setDefaultAction(alignLeftAct);
    QToolButton *btnAlignC = new QToolButton(formatWidget); btnAlignC->setDefaultAction(alignCenterAct);
    QToolButton *btnAlignR = new QToolButton(formatWidget); btnAlignR->setDefaultAction(alignRightAct);
    fmtLayout->addWidget(btnAlignL); fmtLayout->addWidget(btnAlignC); fmtLayout->addWidget(btnAlignR);

    colorAct = new QAction(gp::Icons::get("circle"), tr("Color"), this);
    QToolButton *btnColor = new QToolButton(formatWidget);
    btnColor->setDefaultAction(colorAct);
    fmtLayout->addWidget(btnColor);

    addWidget(formatWidget);
    formatWidget->hide(); // hidden by default

    // §9.2 Wave 2B item 3: opacity control, shared by the text-edit and
    // image-edit toolbars. Reuses the ExtGState /ca /CA mechanism already
    // built for watermarks (PoDoFoBackend::editTextInline/setImageOpacity).
    opacityWidget = new QWidget(this);
    QHBoxLayout *opLayout = new QHBoxLayout(opacityWidget);
    opLayout->setContentsMargins(0, 0, 0, 0);
    opLayout->setSpacing(4);
    QLabel *opacityLabel = new QLabel(tr("Opacity:"), opacityWidget);
    opLayout->addWidget(opacityLabel);
    opacityCombo = new QComboBox(opacityWidget);
    opacityCombo->addItem(tr("100%"), 100);
    opacityCombo->addItem(tr("90%"), 90);
    opacityCombo->addItem(tr("75%"), 75);
    opacityCombo->addItem(tr("50%"), 50);
    opacityCombo->addItem(tr("25%"), 25);
    opacityCombo->addItem(tr("10%"), 10);
    opacityCombo->setCurrentIndex(0);
    opacityCombo->setToolTip(tr("Opacity applied to newly edited text or the selected image"));
    opLayout->addWidget(opacityCombo);
    addWidget(opacityWidget);
    opacityWidget->hide(); // hidden by default

    connect(opacityCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        const double opacity = opacityCombo->itemData(index).toInt() / 100.0;
        emit opacityChanged(opacity);
    });

    connect(this, &EditToolBar::activeToolChanged, this, &EditToolBar::updateFormatVisibility);

    // Connect format signals
    connect(fontFamilyCombo, &QFontComboBox::currentFontChanged, this, &EditToolBar::emitFormatChanged);
    connect(fontSizeCombo, &QComboBox::currentTextChanged, this, &EditToolBar::emitFormatChanged);
    connect(boldAct, &QAction::toggled, this, &EditToolBar::emitFormatChanged);
    connect(italicAct, &QAction::toggled, this, &EditToolBar::emitFormatChanged);
    connect(alignLeftAct, &QAction::toggled, this, &EditToolBar::emitFormatChanged);
    connect(alignCenterAct, &QAction::toggled, this, &EditToolBar::emitFormatChanged);
    connect(alignRightAct, &QAction::toggled, this, &EditToolBar::emitFormatChanged);
    connect(colorAct, &QAction::triggered, this, [this]() {
        QColor c = QColorDialog::getColor(currentColor, this, tr("Select Text Color"));
        if (c.isValid()) {
            currentColor = c;
            emitFormatChanged();
        }
    });
}

void EditToolBar::updateFormatVisibility(ToolMode mode)
{
    if (mode == ToolMode::EditText) {
        formatWidget->show();
    } else {
        formatWidget->hide();
    }

    // §9.2 Wave 2B item 3: opacity is meaningful for both inline text edits
    // and image placement, so it stays visible across both modes.
    if (mode == ToolMode::EditText || mode == ToolMode::EditImage) {
        opacityWidget->show();
    } else {
        opacityWidget->hide();
    }
}

void EditToolBar::setActiveMode(ToolMode mode)
{
    updateFormatVisibility(mode);
}

void EditToolBar::emitFormatChanged()
{
    int alignment = 0; // 0=left, 1=center, 2=right
    if (alignCenterAct->isChecked()) alignment = 1;
    else if (alignRightAct->isChecked()) alignment = 2;

    emit textFormatChanged(
        fontFamilyCombo->currentFont().family(),
        fontSizeCombo->currentText().toInt(),
        currentColor,
        boldAct->isChecked(),
        italicAct->isChecked(),
        alignment
    );
}
