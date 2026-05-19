#pragma once

#include <QToolBar>
#include "core/PdfEnums.h"

class AnnotationToolBar : public QToolBar
{
    Q_OBJECT

public:
    explicit AnnotationToolBar(const QString &title, QWidget *parent = nullptr);

signals:
    void activeToolChanged(ToolMode mode);
    void colorChanged(const QColor &color);
    void thicknessChanged(int thickness);
    void deleteRequested();

private:
    void createActions();

    QAction *selectAct;
    QAction *highlightAct;
    QAction *underlineAct;
    QAction *drawAct;
    QAction *textAct;
    QAction *commentAct;
    QAction *redactAct;
    QAction *signAct;
    QAction *rectAct;
    QAction *ellipseAct;
    QAction *lineAct;
    QAction *arrowAct;
    QAction *deleteAct;
};
