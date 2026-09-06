// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QWidget>
#include <QRegularExpression>
#include "engines/RedactOperation.h"

struct AppContext;
class PdfViewerWidget;
class QProgressDialog;
class QComboBox;
class QCheckBox;
class QLineEdit;
class QLabel;
class QToolButton;
class QRadioButton;
class QListWidget;
class QStackedWidget;

namespace gp {

class ModeController;

class RedactMode : public QWidget {
    Q_OBJECT
public:
    explicit RedactMode(QWidget* parent = nullptr);

    // Called by ModeController after construction (same pattern as PagesMode/BatchMode).
    void setAppContext(const AppContext* ctx);
    // Gives RedactMode visibility of the main viewer so "Current page" scope is real.
    void setViewer(PdfViewerWidget* viewer);

    // Pre-select "Custom regex" and populate the regex line edit.
    void activateCustomRegex(const QString& initialPattern = {});

    // §9.8 P1: Foxit-style word-list import (pure, unit-testable seams).
    // readWordList reads a .txt file (one term per line, blank lines skipped,
    // duplicates removed) and refuses files larger than maxBytes with an
    // honest error; wordListToPattern regex-escapes every term and joins the
    // branches with '|'. The combined pattern lands in the custom-regex edit
    // for REVIEW before any marking happens.
    static QStringList readWordList(const QString& path, qint64 maxBytes, QString* errorOut);
    static QString wordListToPattern(const QStringList& terms);

signals:
    /// §9.8 P0: user-facing status text surfaced on the main status bar.
    void statusMessageRequested(const QString& message);
    /// §9.8 P1: the panel's Cancel/Exit control — the mode-exit contract.
    /// The host (via ModeController's relay) returns to the standard canvas;
    /// placed redaction marks stay on the viewer and remain recoverable.
    void exitRequested();

private slots:
    void onPatternChanged(int index);
    void onRegexTextChanged(const QString& text);
    void onPreviewMatches();
    void onApplyRedactions();
    void onClearMarks();
    void onScopeChanged();
    void onMarkRegion();          // §9.8 P0
    void onMarkAllOccurrences();  // §9.8 P0
    void onImportWordList();      // §9.8 P1

private:
    void buildPatternSection(QWidget* host);
    void buildScopeSection(QWidget* host);
    QRegularExpression currentRegex() const;
    // Returns {startPage, endPage} 0-based, inclusive; -1 means invalid / whole-doc sentinel
    QList<int> resolvePageRange() const;
    void showMatchCount(int count);
    // U05: run the ONE transactional redaction operation behind this entry
    // path — progress + cancel between pages, marks cleared only after the
    // output is committed and kept (shared with SecurityController's path).
    void runRedactOperation(const gp::RedactRequest& request);

    // toolbar pills (stored so D4 can pre-check the pattern pill)
    QToolButton* m_pillMarkRegion  = nullptr;
    QToolButton* m_pillMarkPattern = nullptr;
    QToolButton* m_pillMarkAll     = nullptr;

    // pattern section
    QComboBox*   m_patternCombo    = nullptr;
    QLineEdit*   m_regexEdit       = nullptr;  // shown only when "Custom regex" selected
    QToolButton* m_importListBtn   = nullptr;  // §9.8 P1: word-list import (beside the edit)
    QLabel*      m_matchCountLabel = nullptr;

    // scope
    QRadioButton* m_scopeCurrentPage = nullptr;
    QRadioButton* m_scopeAllPages    = nullptr;
    QRadioButton* m_scopeRange       = nullptr;
    QLineEdit*    m_pageRangeEdit    = nullptr;

    // action buttons
    QToolButton* m_previewBtn  = nullptr;
    QToolButton* m_applyBtn    = nullptr;
    QToolButton* m_clearBtn    = nullptr;

    // §9.8 P0: bundle the full hidden-data scrub into the Apply flow
    QCheckBox*   m_chkSanitizeCopy = nullptr;

    // U05: progress dialog of the running transactional redaction. Deleted only
    // when the NEXT operation starts (or with this widget) — never from the
    // operation's finished handler: a modal QProgressDialog::setValue() pumps
    // the event loop, so a deleteLater delivered inside that pump frees the
    // dialog under the still-executing setValue frame (use-after-free).
    QProgressDialog* m_redactProgress = nullptr;

    const AppContext*  m_ctx    = nullptr;
    PdfViewerWidget*   m_viewer = nullptr;
};

} // namespace gp
