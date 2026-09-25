// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QFrame>
#include <QString>
#include "engines/VeraPdfValidator.h"
#include <functional>
#include <QFutureWatcher>

namespace gp {

class PdfAValidationReport;

/// §9.14 P1: how many positions a structure element may drift between its
/// document (structure) position and its visual position before the
/// reading-order check reports it.
///
/// HEURISTIC, not a conformance rule: a triage aid per common PDF/UA
/// practice. Legitimately tagged documents can shuffle an element by a
/// couple of slots (wrapper elements, decorative ordering), so small
/// displacements are not reported to keep the signal low-noise. Human review
/// remains authoritative: neither a flagged nor a clean result is, by
/// itself, a PDF/UA verdict. The boundary (2 = not flagged, 3 = flagged) is
/// pinned by tests/TestReadingOrderThreshold.cpp.
inline constexpr int kReadingOrderSlotTolerance = 2;

/// §9.14: tagged-PDF reading-order analysis (exposed for tests).
struct ReadingOrderResult {
    bool tagged = false;
    int elementCount = 0;
    QStringList issues;
    /// 0-based page for each issue (-1 when unknown); parallel to `issues`.
    QList<int> issuePages;
};
ReadingOrderResult analyzeReadingOrder(const QString& path);

class PdfAValidationPanel : public QFrame {
    Q_OBJECT
public:
    explicit PdfAValidationPanel(QWidget* parent = nullptr);
    ~PdfAValidationPanel() override;

public:
    void setExportPdfACallback(
        std::function<bool(const QString& outputPath, int conformanceLevel)> cb);

    // ARC06 (TEAM-ARCHITECTURE-REVIEW-2026-09-07): the identity the panel is
    // currently bound to — the one input every validation/reading-order/
    // export action below operates on. Set only by setDocument().
    QString currentDocumentPath() const { return m_currentDocPath; }

public slots:
    void setDocument(const QString& path, PdfAConformance level = PdfAConformance::PDF_A_2B);

private:
    void runValidation();
    void updateDisplay(const PdfAValidationReport& report);
    void onExportReportClicked();
    void onCheckReadingOrder();   // §9.14 tagged-PDF reading-order check

    // AR-7 D2: slot called on GUI thread when the off-thread validation finishes.
    void onValidationFinished();

    // §9.14: slot called on GUI thread when the off-thread reading-order
    // analysis finishes (same async pattern as the veraPDF validation above —
    // analyzeReadingOrder parses and walks the whole structure tree, which can
    // freeze the UI on large/deeply-tagged documents).
    void onReadingOrderFinished();

    QString m_currentDocPath;
    PdfAConformance m_currentConformance{PdfAConformance::PDF_A_2B};

    // ARC06: the document identity each in-flight async worker was SUBMITTED
    // for. A finished result whose submitted identity no longer matches the
    // current document is discarded, so a slow A result can never populate a
    // B panel (setDocument also cancels both watchers first — the tags are
    // the identity check that makes the discard explicit rather than a side
    // effect of cancellation timing).
    QString m_submittedValidationPath;
    QString m_submittedReadingOrderPath;

    std::function<bool(const QString&, int)> m_exportPdfACallback;
    QMetaObject::Connection m_fixBtnConn;

    // Dynamic UI elements updated by updateDisplay()
    class QLabel* m_statusLabel{nullptr};
    class QLabel* m_issuesHeading{nullptr};
    class QWidget* m_issuesList{nullptr};
    class QVBoxLayout* m_issuesLayout{nullptr};
    class QPushButton* m_fixBtn{nullptr};
    class QPushButton* m_exportBtn{nullptr};
    class QPushButton* m_readingOrderBtn{nullptr};

    // AR-7 D2: off-thread veraPDF worker.
    QFutureWatcher<PdfAValidationReport>* m_validationWatcher{nullptr};
    // §9.14: off-thread reading-order worker.
    QFutureWatcher<ReadingOrderResult>* m_readingOrderWatcher{nullptr};
};

} // namespace gp
