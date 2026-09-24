// SPDX-License-Identifier: Apache-2.0
#pragma once

// ── KEEP decision (residexec lane 2026-09-23, closes the archaeologist's
// open revival question — SWEEP-W3-ARCHAEOLOGIST §2 rows 49-50) ────────────
// This class is NOT compiled into the app (removed from the build in 6aac22c;
// the authoritative markup surface is the ribbon Comment tab, pinned live by
// tests/TestAnnotationToolBar.cpp per e5a5f01). It is deliberately kept as
// the revival base for the floating-toolbar parity gap: the 13-tool ribbon
// consolidation explicitly framed it as the re-entry point. Re-decide at the
// parity (P2) pass; deletion then needs the parity lane's sign-off plus this
// header removed together with its .cpp.
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
    QAction *strikeoutAct;
    QAction *squigglyAct;
    QAction *stampAct;
    QAction *calloutAct;
    QAction *deleteAct;
};
