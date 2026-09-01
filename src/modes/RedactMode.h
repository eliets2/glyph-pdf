// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QWidget>
#include <QRegularExpression>

struct AppContext;
class PdfViewerWidget;
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

signals:
    /// §9.8 P0: user-facing status text surfaced on the main status bar.
    void statusMessageRequested(const QString& message);

private slots:
    void onPatternChanged(int index);
    void onRegexTextChanged(const QString& text);
    void onPreviewMatches();
    void onApplyRedactions();
    void onClearMarks();
    void onScopeChanged();
    void onMarkRegion();          // §9.8 P0
    void onMarkAllOccurrences();  // §9.8 P0

private:
    void buildPatternSection(QWidget* host);
    void buildScopeSection(QWidget* host);
    QRegularExpression currentRegex() const;
    // Returns {startPage, endPage} 0-based, inclusive; -1 means invalid / whole-doc sentinel
    QList<int> resolvePageRange() const;
    void showMatchCount(int count);

    // toolbar pills (stored so D4 can pre-check the pattern pill)
    QToolButton* m_pillMarkRegion  = nullptr;
    QToolButton* m_pillMarkPattern = nullptr;
    QToolButton* m_pillMarkAll     = nullptr;

    // pattern section
    QComboBox*   m_patternCombo    = nullptr;
    QLineEdit*   m_regexEdit       = nullptr;  // shown only when "Custom regex" selected
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

    const AppContext*  m_ctx    = nullptr;
    PdfViewerWidget*   m_viewer = nullptr;
};

} // namespace gp
