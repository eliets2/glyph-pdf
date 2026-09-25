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

signals:
    void activeToolChanged(ToolMode mode);
    void textFormatChanged(const QString &fontFamily, int fontSize, const QColor &color, bool bold, bool italic, int alignment);
    // Opacity (0..1), letter spacing (pt, PDF Tc) and line spacing (x the
    // normal pitch) for the next inline text edit.
    void textStyleChanged(double opacity, double letterSpacing, double lineSpacing);

private:
    void createActions();
    void updateFormatVisibility(ToolMode mode);
    void emitFormatChanged();
    void emitStyleChanged();

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
    class QComboBox *opacityCombo;
    class QComboBox *letterSpacingCombo;
    class QComboBox *lineSpacingCombo;

    QWidget *formatWidget;
};

#endif // EDITTOOLBAR_H
