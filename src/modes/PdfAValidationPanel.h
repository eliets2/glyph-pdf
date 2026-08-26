// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QFrame>
#include <QString>
#include "engines/VeraPdfValidator.h"
#include <functional>
#include <QFutureWatcher>

namespace gp {

class PdfAValidationReport;

/// §9.14: tagged-PDF reading-order analysis (exposed for tests).
struct ReadingOrderResult {
    bool tagged = false;
    int elementCount = 0;
    QStringList issues;
};
ReadingOrderResult analyzeReadingOrder(const QString& path);

class PdfAValidationPanel : public QFrame {
    Q_OBJECT
public:
    explicit PdfAValidationPanel(QWidget* parent = nullptr);

public:
    void setExportPdfACallback(
        std::function<bool(const QString& outputPath, int conformanceLevel)> cb);

public slots:
    void setDocument(const QString& path, PdfAConformance level = PdfAConformance::PDF_A_2B);

private:
    void runValidation();
    void updateDisplay(const PdfAValidationReport& report);
    void onExportReportClicked();
    void onCheckReadingOrder();   // §9.14 tagged-PDF reading-order check

    // AR-7 D2: slot called on GUI thread when the off-thread validation finishes.
    void onValidationFinished();

    QString m_currentDocPath;
    PdfAConformance m_currentConformance{PdfAConformance::PDF_A_2B};

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
};

} // namespace gp
