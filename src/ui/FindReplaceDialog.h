// SPDX-License-Identifier: Apache-2.0
#ifndef FINDREPLACEDIALOG_H
#define FINDREPLACEDIALOG_H

#include <QDialog>
#include <functional>
#include "engines/TextMatchFinder.h" // ReplaceOptions / ReplaceOutcome / TextMatch

class QLineEdit;
class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QPlainTextEdit;
class QTimer;

// ── T2-2: Find & Replace ────────────────────────────────────────────────────
// The full find+replace surface (Edit > Find & Replace, Ctrl+H). Competing
// editors ship this as a dedicated pane; the thin FindBar stays the
// quick-search/navigation surface and its Replace All reuses the SAME
// EditController pipeline — there is no second replace implementation.
//
// Honesty contract (research pack T2-2, moat M8):
//  * the live match count is shown BEFORE any replace happens;
//  * replacement is in-place: the matched glyphs are excised and the
//    replacement is drawn at the match position in the match's font size
//    (standard font) — the surrounding layout does NOT reflow, and the
//    dialog says so in plain text;
//  * every replacement whose measured width differs from the match is
//    reported after apply ("changed the text width — check page N").
//
// The dialog holds NO engine state: the document facts (path / page count /
// current page) are pushed in by the host, and the replace mutation goes
// through ONE injected invoker (the host wires EditController::
// replaceAllInDocument). Tests inject a recording lambda instead — the
// dialog never needs a MainWindow.
class FindReplaceDialog : public QDialog {
    Q_OBJECT
public:
    using ReplaceInvoker =
        std::function<ReplaceOutcome(const ReplaceOptions&)>;

    explicit FindReplaceDialog(QWidget* parent = nullptr);

    // Host wiring. docPath empty ⇒ the dialog reports "no document open".
    void setDocumentContext(const QString& docPath, int pageCount, int currentPage,
                            const ReplaceInvoker& invoker);

    // Assemble the current options from the widgets (scope resolved to the
    // 0-based inclusive page list; empty list = all pages).
    ReplaceOptions currentOptions() const;

    // Re-run the finder over the scoped pages and refresh the match summary.
    void recount();
    QString matchSummaryText() const;

    // Run the replace through the injected invoker and report the outcome —
    // including the measured width-change warnings — in the details pane.
    void applyReplace();
    QString outcomeText() const;

private:
    void updateScopeEnabled();

    // packa-F1: Count/Replace enabled only with a document open AND a usable
    // scope (see scopeRefusal). Called from recount()/setDocumentContext so
    // every scope edit re-evaluates immediately.
    void updateActionAvailability();

    // packa-F4: the CHEAP parts of a recount — button availability plus the
    // terminal messages (no document / refused scope / empty search / bad
    // regex) — with no PDF parsing. Returns true when the expensive matching
    // part is warranted.
    bool recountUiOnly();

    // packa-F4: typing path — immediate recountUiOnly(), deferred matching
    // (one debounced recount per pause, not one per keystroke).
    void scheduleRecount();

    // WP-R07: non-empty when the SELECTED scope cannot be honored (malformed
    // range, range entirely outside the document, out-of-range current page).
    // The dialog refuses the scope explicitly instead of silently widening it
    // to the whole document (an empty page list means "all pages" downstream).
    QString scopeRefusal() const;

    QLineEdit* m_search = nullptr;
    QLineEdit* m_replace = nullptr;
    QCheckBox* m_matchCase = nullptr;
    QCheckBox* m_wholeWords = nullptr;
    QCheckBox* m_useRegex = nullptr;
    QComboBox* m_scope = nullptr;      // All pages / Current page / Range
    QLineEdit* m_range = nullptr;      // "2-5" (1-based, inclusive)
    QLabel* m_matchSummary = nullptr;
    QPushButton* m_countBtn = nullptr;
    QPushButton* m_replaceAllBtn = nullptr;
    QPlainTextEdit* m_details = nullptr;
    QTimer* m_recountDebounce = nullptr;   // packa-F4 (250ms single shot)

    QString m_docPath;
    int m_pageCount = 0;
    int m_currentPage = 1;             // 1-based
    ReplaceInvoker m_invoker;
    QString m_outcome;
};

#endif // FINDREPLACEDIALOG_H
