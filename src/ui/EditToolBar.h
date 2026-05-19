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

private:
    void createActions();

    QAction *handToolAct;
    QAction *selectTextAct;
    QAction *editTextAct;
    QAction *editObjectAct;
    QAction *addTextFieldAct;
    QAction *addCheckboxAct;
};

#endif // EDITTOOLBAR_H
