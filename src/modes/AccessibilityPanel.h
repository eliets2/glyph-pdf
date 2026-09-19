// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QFrame>
#include <QString>
#include <functional>
#include <QFutureWatcher>

#include "engines/AccessibilityChecker.h"

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

public slots:
    void setDocument(const QString& path);

signals:
    void scanCompleted();

private slots:
    void runScan();
    void onScanFinished();

private:
    void updateDisplay(const A11yReport& report);
    void clearFindings();

    QString m_currentDocPath;
    QString m_submittedScanPath;   // ARC06 identity tie for in-flight scans
    A11yReport m_lastReport;

    QFutureWatcher<A11yReport>* m_scanWatcher = nullptr;

    QLabel* m_statusLabel = nullptr;
    QLabel* m_disclosureLabel = nullptr;
    QLabel* m_findingsHeading = nullptr;
    QWidget* m_findingsList = nullptr;
    QVBoxLayout* m_findingsLayout = nullptr;
    QPushButton* m_scanBtn = nullptr;
};

} // namespace gp
