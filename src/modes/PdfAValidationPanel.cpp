#include "PdfAValidationPanel.h"
#include "util/GpTheme.h"
#include "util/Badge.h"

#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QTextStream>
#include <QVBoxLayout>

namespace gp {

// ---------------------------------------------------------------------------
// Helper: build a single violation row widget
// ---------------------------------------------------------------------------
static QWidget* issueRow(const QString& rule, const QString& descr, bool err) {
    auto* w = new QFrame;
    w->setStyleSheet(QString("background:#1a1b1e; border:1px solid #393b40; border-left:3px solid %1; padding:8px 10px; margin-bottom:6px;")
        .arg(err ? "#c8442b" : "#ff8c42"));
    auto* h = new QHBoxLayout(w);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(8);
    auto* dot = new QLabel(err ? "✕" : "▲");
    dot->setStyleSheet(QString("color:%1;font-weight:700;").arg(err ? "#c8442b" : "#ff8c42"));
    auto* lbl = new QLabel(QString("<b style='color:#dfe1e5;font-family:JetBrains Mono;font-size:10px;'>%1</b> · %2").arg(rule, descr));
    lbl->setTextFormat(Qt::RichText);
    lbl->setWordWrap(true);
    auto* jump = new QPushButton(QObject::tr("JUMP"));
    jump->setStyleSheet("font-family:JetBrains Mono; font-size:9.5px; color:#ff8c42; border:1px solid rgba(255,140,66,0.33); padding:1px 6px;");
    h->addWidget(dot);
    h->addWidget(lbl, 1);
    h->addWidget(jump);
    return w;
}

// ---------------------------------------------------------------------------
// Constructor — builds the static skeleton; dynamic content via updateDisplay()
// ---------------------------------------------------------------------------
PdfAValidationPanel::PdfAValidationPanel(QWidget* parent) : QFrame(parent) {
    setObjectName("rightSidebar");
    setFixedWidth(Theme::RightPaneW);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0,0,0,0); outer->setSpacing(0);

    // Header bar
    auto* head = new QFrame; head->setProperty("role","modeToolbar"); head->setFixedHeight(32);
    auto* hr = new QHBoxLayout(head); hr->setContentsMargins(10,0,10,0);
    auto* t = new QLabel(tr("PDF/A · VALIDATION")); t->setStyleSheet("font-weight:600;letter-spacing:1.2px;");
    hr->addWidget(t); hr->addStretch(1);
    outer->addWidget(head);

    auto* scroll = new QScrollArea; scroll->setWidgetResizable(true); scroll->setFrameShape(QFrame::NoFrame);
    auto* body = new QWidget;
    auto* col = new QVBoxLayout(body);
    col->setContentsMargins(12,12,12,12); col->setSpacing(8);

    // Status label — shows validation result or "unavailable" message
    m_statusLabel = new QLabel(tr("No document loaded."));
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setProperty("mono", true);
    col->addWidget(m_statusLabel);

    // Issues heading (hidden until validation runs)
    m_issuesHeading = new QLabel;
    m_issuesHeading->setProperty("mono", true);
    m_issuesHeading->hide();
    col->addWidget(m_issuesHeading);

    // Dynamic issues list container
    m_issuesList = new QWidget;
    m_issuesLayout = new QVBoxLayout(m_issuesList);
    m_issuesLayout->setContentsMargins(0,0,0,0);
    m_issuesLayout->setSpacing(0);
    m_issuesList->hide();
    col->addWidget(m_issuesList);

    // Action buttons
    m_fixBtn = new QPushButton(tr("Fix Automatically"));
    m_fixBtn->setEnabled(false);
    m_fixBtn->setToolTip(tr("Automatic fix — planned for v1.1.0"));

    auto* conv = new QPushButton(tr("Convert to PDF/A-2B"));
    conv->setStyleSheet("background:#ff8c42;color:#1a1b1e;border:1px solid #ff8c42;font-weight:600;padding:8px 12px;");

    m_exportBtn = new QPushButton(tr("Export Report"));

    col->addWidget(m_fixBtn);
    col->addWidget(conv);
    col->addWidget(m_exportBtn);
    col->addStretch(1);

    scroll->setWidget(body);
    outer->addWidget(scroll, 1);

    // Wire buttons
    connect(conv, &QPushButton::clicked, this, [this]() {
        if (m_currentDocPath.isEmpty()) {
            QMessageBox::information(this, tr("No Document"),
                tr("Open a PDF document first, then switch to PDF/A validation mode."));
        } else {
            QMessageBox::information(this, tr("Convert to PDF/A-2B"),
                tr("Use File > Export As PDF/A to convert the current document.\n"
                   "Direct conversion from the validation panel is planned for v1.1.0."));
        }
    });

    connect(m_exportBtn, &QPushButton::clicked, this, &PdfAValidationPanel::onExportReportClicked);
}

// ---------------------------------------------------------------------------
// Public slot — set current document and trigger validation
// ---------------------------------------------------------------------------
void PdfAValidationPanel::setDocument(const QString& path, PdfAConformance level) {
    m_currentDocPath = path;
    m_currentConformance = level;
    runValidation();
}

// ---------------------------------------------------------------------------
// Run veraPDF validation and refresh the display
// ---------------------------------------------------------------------------
void PdfAValidationPanel::runValidation() {
    if (m_currentDocPath.isEmpty()) {
        m_statusLabel->setText(tr("No document loaded."));
        m_issuesHeading->hide();
        m_issuesList->hide();
        return;
    }

    m_statusLabel->setText(tr("Validating…"));

    auto report = VeraPdfValidator::validate(m_currentDocPath, m_currentConformance);
    updateDisplay(report);
}

// ---------------------------------------------------------------------------
// Update the panel UI from a PdfAValidationReport
// ---------------------------------------------------------------------------
void PdfAValidationPanel::updateDisplay(const PdfAValidationReport& report) {
    // Clear previous violation rows
    QLayoutItem* item;
    while ((item = m_issuesLayout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }
    m_issuesList->hide();
    m_issuesHeading->hide();

    if (!report.validatorAvailable) {
        m_statusLabel->setText(
            tr("Validator unavailable — build with -DVERAPDF_CLI_PATH=<path> to enable real validation."));
        m_exportBtn->setEnabled(false);
        return;
    }

    if (!report.errorMessage.isEmpty()) {
        m_statusLabel->setText(tr("Validation error: %1").arg(report.errorMessage));
        m_exportBtn->setEnabled(false);
        return;
    }

    m_exportBtn->setEnabled(true);

    if (report.isValid) {
        QString level = report.conformanceLevel.isEmpty()
            ? tr("PDF/A")
            : report.conformanceLevel;
        m_statusLabel->setText(tr("✓ Conforms to %1").arg(level));
        return;
    }

    // Violations found
    int n = report.violations.size();
    m_statusLabel->setText(tr("✗ %1 violation(s) found").arg(n));

    if (n > 0) {
        m_issuesHeading->setText(tr("ISSUES · %1").arg(n));
        m_issuesHeading->show();

        for (const RuleViolation& v : report.violations) {
            QString label = v.pageNumber >= 0
                ? tr("%1 · Page %2").arg(v.description).arg(v.pageNumber)
                : v.description;
            bool isError = (v.severity == "error");
            m_issuesLayout->addWidget(issueRow(v.ruleId, label, isError));
        }
        m_issuesList->show();
    }
}

// ---------------------------------------------------------------------------
// Export Report — write violations to a JSON file chosen via QFileDialog
// ---------------------------------------------------------------------------
void PdfAValidationPanel::onExportReportClicked() {
    if (m_currentDocPath.isEmpty()) {
        QMessageBox::information(this, tr("Export Report"),
            tr("No document to export a report for."));
        return;
    }

    QString dest = QFileDialog::getSaveFileName(
        this,
        tr("Export Validation Report"),
        QString(),
        tr("JSON files (*.json);;Text files (*.txt);;All files (*)"));

    if (dest.isEmpty()) return;

    auto report = VeraPdfValidator::validate(m_currentDocPath, m_currentConformance);

    QFile file(dest);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Export Failed"),
            tr("Could not write to %1").arg(dest));
        return;
    }

    QTextStream out(&file);
    out << "{\n";
    out << "  \"conformanceLevel\": \"" << report.conformanceLevel << "\",\n";
    out << "  \"isValid\": " << (report.isValid ? "true" : "false") << ",\n";
    out << "  \"validatorAvailable\": " << (report.validatorAvailable ? "true" : "false") << ",\n";
    if (!report.errorMessage.isEmpty()) {
        QString escaped = report.errorMessage;
        escaped.replace("\\", "\\\\").replace("\"", "\\\"");
        out << "  \"errorMessage\": \"" << escaped << "\",\n";
    }
    out << "  \"violations\": [\n";
    const auto& viols = report.violations;
    for (int i = 0; i < viols.size(); ++i) {
        const RuleViolation& v = viols[i];
        QString desc = v.description;
        desc.replace("\\", "\\\\").replace("\"", "\\\"");
        out << "    {\n";
        out << "      \"ruleId\": \"" << v.ruleId << "\",\n";
        out << "      \"clause\": \"" << v.clause << "\",\n";
        out << "      \"description\": \"" << desc << "\",\n";
        out << "      \"pageNumber\": " << v.pageNumber << ",\n";
        out << "      \"severity\": \"" << v.severity << "\"\n";
        out << "    }" << (i + 1 < viols.size() ? "," : "") << "\n";
    }
    out << "  ]\n";
    out << "}\n";

    file.close();
    QMessageBox::information(this, tr("Report Exported"),
        tr("Validation report written to:\n%1").arg(dest));
}

} // namespace gp
