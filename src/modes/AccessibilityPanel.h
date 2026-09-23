// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QFrame>
#include <QString>
#include <functional>
#include <QFutureWatcher>

#include "engines/AccessibilityChecker.h"
#include "engines/AccessibilityFixes.h"

class QLabel;
class QPushButton;
class QVBoxLayout;

namespace gp {

// ── T2-4 accessibility P1: the checker panel ─────────────────────────────────
//
// Runs gp::scanAccessibility() on the open document (off the GUI thread) and
// lists the gaps it finds with severity + an honest whyNot.
//
// HONESTY CONTRACT (mirrored in the engine header, pinned in tests):
//   * DETECTION + DISCLOSURE only. This panel never certifies PDF/UA — a
//     findings-free report is "no gaps found by these checks", not a claim
//     of accessibility.
//   * Content tagging (structure-tree construction) is NOT available in P1;
//     the disclosure says so next to every report.
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

private slots:
    void runScan();
    void onScanFinished();

private:
    void updateDisplay(const A11yReport& report);
    void clearFindings();
    // Inline editor row (combo for /Lang, line edit for /Alt and /TU)
    // inserted under the finding; Apply routes an A11yFixRequest to the
    // injected runner.
    void showEditorForFinding(int findingIndex);

    QString m_currentDocPath;
    QString m_submittedScanPath;   // ARC06 identity tie for in-flight scans
    A11yReport m_lastReport;
    std::function<A11yFixOutcome(const A11yFixRequest&)> m_fixRunner;

    QFutureWatcher<A11yReport>* m_scanWatcher = nullptr;

    QLabel* m_statusLabel = nullptr;
    QLabel* m_disclosureLabel = nullptr;
    QLabel* m_findingsHeading = nullptr;
    QWidget* m_findingsList = nullptr;
    QVBoxLayout* m_findingsLayout = nullptr;
    QPushButton* m_scanBtn = nullptr;
};

} // namespace gp
