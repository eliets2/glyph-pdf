// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QFrame>
#include <QString>
#include "engines/VeraPdfValidator.h"

namespace gp {

class PdfAValidationReport;

class PdfAValidationPanel : public QFrame {
    Q_OBJECT
public:
    explicit PdfAValidationPanel(QWidget* parent = nullptr);

public slots:
    void setDocument(const QString& path, PdfAConformance level = PdfAConformance::PDF_A_2B);

private:
    void runValidation();
    void updateDisplay(const PdfAValidationReport& report);
    void onExportReportClicked();

    QString m_currentDocPath;
    PdfAConformance m_currentConformance{PdfAConformance::PDF_A_2B};

    // Dynamic UI elements updated by updateDisplay()
    class QLabel* m_statusLabel{nullptr};
    class QLabel* m_issuesHeading{nullptr};
    class QWidget* m_issuesList{nullptr};
    class QVBoxLayout* m_issuesLayout{nullptr};
    class QPushButton* m_fixBtn{nullptr};
    class QPushButton* m_exportBtn{nullptr};
};

} // namespace gp
