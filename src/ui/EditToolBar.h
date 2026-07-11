// SPDX-License-Identifier: Apache-2.0
#ifndef EDITTOOLBAR_H
#define EDITTOOLBAR_H

#include <QToolBar>
#include "core/PdfEnums.h"

class EditToolBar : public QToolBar
{
    Q_OBJECT

public:
    explicit EditToolBar(const QString &title, QWidget *parent = nullptr);

    // §9.2 Wave 1B: called by EditController when EditText/EditImage mode is
    // entered via a non-toolbar path (ribbon/menu/Edit-menu Cut-Copy-Delete),
    // so the format/opacity sub-controls stay in sync even when this toolbar's
    // own tool buttons weren't what triggered the mode change.
    void setActiveMode(ToolMode mode);

signals:
    void activeToolChanged(ToolMode mode);
    void textFormatChanged(const QString &fontFamily, int fontSize, const QColor &color, bool bold, bool italic, int alignment);
    // §9.2 Wave 2B item 3: opacity control shared by the text-edit and
    // image-edit toolbars (same widget, shown for both modes). 0.0-1.0.
    void opacityChanged(double opacity);

private:
    void createActions();
    void updateFormatVisibility(ToolMode mode);
    void emitFormatChanged();

    QAction *handToolAct;
    QAction *selectTextAct;
    QAction *editTextAct;
    QAction *editObjectAct;
    QAction *addTextFieldAct;
    QAction *addCheckboxAct;

    class QFontComboBox *fontFamilyCombo;
    class QComboBox *fontSizeCombo;
    QAction *boldAct;
    QAction *italicAct;
    QAction *alignLeftAct;
    QAction *alignCenterAct;
    QAction *alignRightAct;
    QAction *colorAct;
    QColor currentColor = Qt::black;

    QWidget *formatWidget;

    // §9.2 Wave 2B item 3: opacity control (shown for EditText and EditImage).
    QWidget *opacityWidget;
    class QComboBox *opacityCombo;
};

#endif // EDITTOOLBAR_H
