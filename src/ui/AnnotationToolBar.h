// SPDX-License-Identifier: Apache-2.0
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
    // Wave 1A §9.3: Strikeout/Squiggly/Stamp/Callout were the 4 of 10 markup types
    // missing from this toolbar versus the Ribbon's Comment tab (which has all 10).
    QAction *strikeoutAct;
    QAction *squigglyAct;
    QAction *drawAct;
    QAction *textAct;
    QAction *commentAct;
    QAction *stampAct;
    QAction *calloutAct;
    QAction *redactAct;
    QAction *signAct;
    QAction *rectAct;
    QAction *ellipseAct;
    QAction *lineAct;
    QAction *arrowAct;
    QAction *deleteAct;
};
