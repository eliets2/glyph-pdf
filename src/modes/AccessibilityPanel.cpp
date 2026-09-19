// SPDX-License-Identifier: Apache-2.0
#include "AccessibilityPanel.h"

#include "util/GpTheme.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QStringList>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>

namespace gp {

namespace {

// Severity accent colors (same palette as the PDF/A panel's issue rows).
QString severityColor(A11ySeverity s) {
    switch (s) {
        case A11ySeverity::High:   return QStringLiteral("#c8442b");
        case A11ySeverity::Medium: return QStringLiteral("#ff8c42");
        case A11ySeverity::Low:    return QStringLiteral("#8a8f98");
    }
    return QStringLiteral("#8a8f98");
}

QString severityName(A11ySeverity s) {
    switch (s) {
        case A11ySeverity::High:   return AccessibilityPanel::tr("HIGH");
        case A11ySeverity::Medium: return AccessibilityPanel::tr("MEDIUM");
        case A11ySeverity::Low:    return AccessibilityPanel::tr("LOW");
    }
    return {};
}

} // namespace

AccessibilityPanel::AccessibilityPanel(QWidget* parent) : QFrame(parent) {
    setObjectName("rightSidebar");
    setFixedWidth(Theme::RightPaneW);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    // Header bar (GpMainWindow right-panel idiom).
    auto* head = new QFrame;
    head->setProperty("role", "modeToolbar");
    head->setFixedHeight(32);
    auto* hr = new QHBoxLayout(head);
    hr->setContentsMargins(10, 0, 10, 0);
    auto* t = new QLabel(tr("ACCESSIBILITY · CHECKER"));
    t->setStyleSheet("font-weight:600;letter-spacing:1.2px;");
    hr->addWidget(t);
    hr->addStretch(1);
    outer->addWidget(head);

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* body = new QWidget;
    auto* col = new QVBoxLayout(body);
    col->setContentsMargins(12, 12, 12, 12);
    col->setSpacing(8);

    m_statusLabel = new QLabel(tr("No document loaded."));
    m_statusLabel->setObjectName(QStringLiteral("a11yStatusLabel"));
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setProperty("mono", true);
    col->addWidget(m_statusLabel);

    // The honesty box — ALWAYS visible. It bounds what a report means.
    m_disclosureLabel = new QLabel(tr(
        "Detection only. This report shows what a tagged document needs — "
        "it never certifies PDF/UA. Content tagging (building a structure "
        "tree) is not yet available in this version."));
    m_disclosureLabel->setObjectName(QStringLiteral("a11yDisclosureLabel"));
    m_disclosureLabel->setWordWrap(true);
    m_disclosureLabel->setStyleSheet(
        "color:#b0b4bb; font-size:9.5px; padding:6px 8px;"
        "border:1px solid #393b40; background:#1a1b1e;");
    col->addWidget(m_disclosureLabel);

    m_findingsHeading = new QLabel;
    m_findingsHeading->setProperty("mono", true);
    m_findingsHeading->hide();
    col->addWidget(m_findingsHeading);

    m_findingsList = new QWidget;
    m_findingsLayout = new QVBoxLayout(m_findingsList);
    m_findingsLayout->setContentsMargins(0, 0, 0, 0);
    m_findingsLayout->setSpacing(0);
    m_findingsList->hide();
    col->addWidget(m_findingsList);

    m_scanBtn = new QPushButton(tr("Run Check"));
    m_scanBtn->setObjectName(QStringLiteral("a11yRunButton"));
    m_scanBtn->setEnabled(false);  // honest empty state: no document, no scan
    m_scanBtn->setToolTip(tr("Scan the open document for accessibility gaps"));
    col->addWidget(m_scanBtn);
    col->addStretch(1);

    scroll->setWidget(body);
    outer->addWidget(scroll, 1);

    connect(m_scanBtn, &QPushButton::clicked, this, &AccessibilityPanel::runScan);
}

AccessibilityPanel::~AccessibilityPanel() {
    // Bounded async lifetime — a destroyed panel must not leave an in-flight
    // scan delivering into it (same as PdfAValidationPanel).
    if (m_scanWatcher && m_scanWatcher->isRunning()) {
        m_scanWatcher->cancel();
        m_scanWatcher->waitForFinished();
    }
}

void AccessibilityPanel::setDocument(const QString& path) {
    m_currentDocPath = path;
    if (m_scanWatcher && m_scanWatcher->isRunning()) {
        m_scanWatcher->cancel();
        m_scanWatcher->waitForFinished();
    }
    // A document change invalidates the previous report — do not let an old
    // report describe the new identity, even before the new scan lands.
    m_lastReport = A11yReport{};
    if (path.isEmpty()) {
        m_statusLabel->setText(tr("No document loaded."));
        m_findingsHeading->hide();
        m_findingsList->hide();
        m_scanBtn->setEnabled(false);
        return;
    }
    m_scanBtn->setEnabled(true);
    runScan();
}

void AccessibilityPanel::runScan() {
    if (m_currentDocPath.isEmpty()) return;

    m_statusLabel->setText(tr("Scanning…"));
    if (!m_scanWatcher) {
        m_scanWatcher = new QFutureWatcher<A11yReport>(this);
        connect(m_scanWatcher, &QFutureWatcher<A11yReport>::finished,
                this, &AccessibilityPanel::onScanFinished);
    }
    if (m_scanWatcher->isRunning()) {
        m_scanWatcher->cancel();
        m_scanWatcher->waitForFinished();
    }

    const QString path = m_currentDocPath;
    m_submittedScanPath = path;
    m_scanWatcher->setFuture(
        QtConcurrent::run([path]() { return scanAccessibility(path); }));
}

void AccessibilityPanel::onScanFinished() {
    if (!m_scanWatcher || m_scanWatcher->isCanceled()) return;
    // ARC06 identity tie: a result for a document that is no longer current
    // is discarded, never displayed.
    if (m_submittedScanPath != m_currentDocPath) return;
    updateDisplay(m_scanWatcher->result());
    emit scanCompleted();
}

void AccessibilityPanel::clearFindings() {
    QLayoutItem* item;
    while ((item = m_findingsLayout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }
    m_findingsList->hide();
    m_findingsHeading->hide();
}

void AccessibilityPanel::updateDisplay(const A11yReport& report) {
    clearFindings();
    m_lastReport = report;

    if (!report.loadOk) {
        m_statusLabel->setText(tr("Cannot scan: %1").arg(report.loadError));
        return;
    }

    const int n = report.findings.size();
    if (n == 0) {
        // Honest wording — never "accessible", never "PDF/UA conformant".
        m_statusLabel->setText(
            tr("✓ No gaps found by these checks. This is not a PDF/UA "
               "verdict — content tagging is not checked."));
        return;
    }

    QString extra;
    if (report.truncated()) {
        extra = tr(" (showing %1 of %2 images, %3 of %4 fields — report "
                   "truncated)")
                    .arg(report.imagesReported)
                    .arg(report.imagesTotal)
                    .arg(report.fieldsReported)
                    .arg(report.fieldsTotal);
    }
    m_statusLabel->setText(tr("✗ %1 gap(s) found%2").arg(n).arg(extra));

    m_findingsHeading->setText(tr("GAPS · %1").arg(n));
    m_findingsHeading->show();

    for (const A11yFinding& f : report.findings) {
        auto* w = new QFrame;
        w->setStyleSheet(
            QString("background:#1a1b1e; border:1px solid #393b40;"
                    " border-left:3px solid %1; padding:8px 10px; margin-bottom:6px;")
                .arg(severityColor(f.severity)));
        auto* h = new QHBoxLayout(w);
        h->setContentsMargins(0, 0, 0, 0);
        h->setSpacing(8);
        auto* dot = new QLabel(f.severity == A11ySeverity::High
                                   ? QStringLiteral("✕")
                                   : QStringLiteral("▲"));
        dot->setStyleSheet(QString("color:%1;font-weight:700;")
                               .arg(severityColor(f.severity)));
        auto* lbl = new QLabel(QString(
            "<b style='color:#dfe1e5;font-family:JetBrains Mono;font-size:10px;'>%1</b> · %2<br>"
            "<span style='color:#b0b4bb;'>%3</span>")
            .arg(severityName(f.severity), f.where.toHtmlEscaped(),
                 f.whyNot.toHtmlEscaped()));
        lbl->setTextFormat(Qt::RichText);
        lbl->setWordWrap(true);
        h->addWidget(dot);
        h->addWidget(lbl, 1);
        m_findingsLayout->addWidget(w);
    }
    m_findingsList->show();
}

} // namespace gp
