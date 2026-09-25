// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QFrame>
#include <QString>
#include <functional>
#include <QFutureWatcher>

#include "engines/AccessibilityChecker.h"
#include "engines/AccessibilityFixes.h"
#include "engines/AccessibilityTagger.h"

class QLabel;
class QPushButton;
class QVBoxLayout;
class QWidget;

namespace gp {

// ── T2-4 accessibility: the checker + tagging panel ──────────────────────────
//
// Runs gp::scanAccessibility() on the open document (off the GUI thread) and
// lists the gaps it finds with severity + an honest whyNot. P2 adds the
// "Tag Document…" action: a read-only pre-flight (cluster table + image
// prompt list, rendered inline) followed by the injected tagging runner —
// the panel stays a view; the shell owns resident-document coordination.
//
// HONESTY CONTRACT (mirrored in the engine headers, pinned in tests):
//   * The checker never certifies PDF/UA — a findings-free report is "no
//     gaps found by these checks", not a claim of accessibility.
//   * Auto-tagging builds a best-effort structure tree from layout
//     heuristics; A TAGGED DOCUMENT IS NOT A CONFORMING DOCUMENT. It does
//     not verify reading order against intent, does not produce table/list
//     structure, and issues no conformance verdict of any kind.
//   * Large documents: per-object findings are a bounded sample; truncation
//     is disclosed (totals shown), never silent.
class AccessibilityPanel : public QFrame {
    Q_OBJECT
public:
    explicit AccessibilityPanel(QWidget* parent = nullptr);
    ~AccessibilityPanel() override;

    // ARC06 (same discipline as PdfAValidationPanel): the one production-path
    // setter — the identity every scan result must match to be displayed.
    QString currentDocumentPath() const { return m_currentDocPath; }

    // Read access for tests / wiring.
    const A11yReport& lastReport() const { return m_lastReport; }

    // D2 cheap fixes: the fix runner is INJECTED — the panel never mutates
    // documents itself; the shell owns resident-document coordination and the
    // FormManager seam for /TU. Without a runner no fix affordances exist
    // (never a dead control).
    void setFixRunner(
        std::function<A11yFixOutcome(const A11yFixRequest&)> runner);

    // P2 tagging: the tag runner is INJECTED the same way (the shell wraps
    // gp::tagDocumentAccessibility with viewer-handle coordination). Without
    // a runner the Tag action does not exist (never a dead control).
    void setTagRunner(std::function<TaggerReport(const QString& path)> runner);

    // PR-review §3.1: the read-only gate is INJECTED too — the panel stays a
    // view and never holds the DocumentSession; the shell's predicate asks
    // EditPolicy::mutationBlocked(session). A read-only session refuses the
    // Tag action up front (honest message) AND the shell's runner refuses
    // again at the write boundary (defense in depth). Without a gate the
    // click-time check is skipped (tests / hosts that guarantee mutability).
    void setReadOnlyGate(std::function<bool()> gate);

public slots:
    void setDocument(const QString& path);
    // One fix, end to end: run it, report the outcome honestly in the status
    // line, and on success re-scan (setDocument on the same identity).
    void applyFix(const A11yFixRequest& request);

signals:
    void scanCompleted();
    // Emitted when a fix succeeded (message is user-presentable) so the host
    // can surface it in the status bar.
    void documentMutated(const QString& message);
    // Emitted once the tagging pre-flight confirmation surface is rendered
    // (test seam, same spirit as scanCompleted).
    void tagPreflightReady();
    // Emitted when the tagging run delivered (test seam for the async wait).
    void tagRunFinished();

private slots:
    void runScan();
    void onScanFinished();
    void onTagClicked();
    void onPreflightFinished();
    void onApplyClicked();
    void onTagCancelClicked();
    void onTagFinished();

private:
    void updateDisplay(const A11yReport& report);
    void updateTagActionState();
    void clearFindings();
    void hideTagConfirmation();
    // Inline editor row (combo for /Lang, line edit for /Alt and /TU)
    // inserted under the finding; Apply routes an A11yFixRequest to the
    // injected runner.
    void showEditorForFinding(int findingIndex);

    QString m_currentDocPath;
    QString m_submittedScanPath;   // ARC06 identity tie for in-flight scans
    A11yReport m_lastReport;
    std::function<A11yFixOutcome(const A11yFixRequest&)> m_fixRunner;
    std::function<TaggerReport(const QString& path)> m_tagRunner;
    std::function<bool()> m_readOnlyGate;   // PR-review §3.1 (injected)

    QFutureWatcher<A11yReport>* m_scanWatcher = nullptr;
    QFutureWatcher<TaggerPreflight>* m_preflightWatcher = nullptr;
    QFutureWatcher<TaggerReport>* m_tagWatcher = nullptr;
    QString m_submittedTagPath;    // ARC06 identity tie for the tagging flow

    QLabel* m_statusLabel = nullptr;
    QLabel* m_disclosureLabel = nullptr;
    QLabel* m_findingsHeading = nullptr;
    QWidget* m_findingsList = nullptr;
    QVBoxLayout* m_findingsLayout = nullptr;
    QPushButton* m_scanBtn = nullptr;
    QPushButton* m_tagBtn = nullptr;
    QWidget* m_tagConfirm = nullptr;
    QLabel* m_tagSummary = nullptr;
    QPushButton* m_tagApplyBtn = nullptr;
};

} // namespace gp
