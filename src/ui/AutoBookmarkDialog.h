// SPDX-License-Identifier: Apache-2.0
#ifndef AUTOBOOKMARKDIALOG_H
#define AUTOBOOKMARKDIALOG_H

#include <QDialog>
#include "engines/HeadingOutlineDetector.h"

class QLabel;
class QPushButton;
class QTableWidget;

// ── T2-9: auto-bookmarks preview ────────────────────────────────────────────
// Detects heading candidates (font-size/bold heuristics + TOC-page patterns)
// and presents them for review. NOTHING is written until the user confirms:
// rows can be unchecked and titles edited inline. The heuristic basis is
// disclosed in the dialog itself (moat honesty rule).
class AutoBookmarkDialog : public QDialog {
    Q_OBJECT
public:
    explicit AutoBookmarkDialog(const QString& pdfPath, QWidget* parent = nullptr);

    // The checked (and possibly re-titled) candidates, in display order.
    QList<HeadingOutlineDetector::Candidate> acceptedCandidates() const;

    // The outline tree the current selection produces (delegates to the
    // detector's nesting; kept as a member so tests pin the same path).
    QList<OutlineEntry> buildTree() const;

    QString candidateCountText() const;

private:
    QString m_pdfPath;
    QTableWidget* m_table = nullptr;
    QLabel* m_disclosure = nullptr;
    QLabel* m_countLabel = nullptr;
    QPushButton* m_okBtn = nullptr;
    QPushButton* m_cancelBtn = nullptr;
};

#endif // AUTOBOOKMARKDIALOG_H
