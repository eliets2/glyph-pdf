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
};

#endif // EDITTOOLBAR_H
