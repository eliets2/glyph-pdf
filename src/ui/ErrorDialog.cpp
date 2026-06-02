// SPDX-License-Identifier: Apache-2.0
#include "ErrorDialog.h"
#include "util/GpTheme.h"

#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

namespace gp {

// ── colours per severity ────────────────────────────────────────────────
static const char* severityColor(ErrorInfo::Severity s) {
    switch (s) {
        case ErrorInfo::Info:     return "#5b9bd5";
        case ErrorInfo::Warning:  return "#ff8c42";
        case ErrorInfo::Error:    return "#c8442b";
        case ErrorInfo::Critical: return "#e02020";
    }
    return "#c8442b";
}

static const char* severityIcon(ErrorInfo::Severity s) {
    switch (s) {
        case ErrorInfo::Info:     return "ℹ";
        case ErrorInfo::Warning:  return "⚠";
        case ErrorInfo::Error:    return "✕";
        case ErrorInfo::Critical: return "⛔";
    }
    return "✕";
}

// ── ctor ────────────────────────────────────────────────────────────────
ErrorDialog::ErrorDialog(const ErrorInfo& info, QWidget* parent)
    : QDialog(parent)
{
    setProperty("role", "modal");
    setWindowTitle(info.severityString());
    setMinimumWidth(420);
    setMaximumWidth(600);
    buildUi(info);
}

void ErrorDialog::buildUi(const ErrorInfo& info) {
    auto* col = new QVBoxLayout(this);
    col->setContentsMargins(0, 0, 0, 0);
    col->setSpacing(0);

    // ── Header band ─────────────────────────────────────────────────
    auto* head = new QFrame;
    head->setStyleSheet(
        QString("background:%1; padding:10px 14px;").arg(severityColor(info.severity)));
    auto* hr = new QHBoxLayout(head);
    hr->setContentsMargins(0, 0, 0, 0);
    auto* hdrIcon = new QLabel(severityIcon(info.severity));
    hdrIcon->setStyleSheet("font-size:18px; color:#fff;");
    auto* hdrTitle = new QLabel(info.severityString());
    hdrTitle->setStyleSheet("font-weight:700; letter-spacing:1.2px; color:#fff;");
    hr->addWidget(hdrIcon);
    hr->addWidget(hdrTitle);
    hr->addStretch(1);
    col->addWidget(head);

    // ── Body ────────────────────────────────────────────────────────
    auto* body = new QWidget;
    auto* bl = new QVBoxLayout(body);
    bl->setContentsMargins(16, 14, 16, 8);
    bl->setSpacing(10);

    m_message = new QLabel(info.userMessage);
    m_message->setWordWrap(true);
    m_message->setStyleSheet("font-size:13px;");
    bl->addWidget(m_message);

    // Source file / page context
    if (!info.sourceFile.isEmpty()) {
        QString ctx = tr("File: %1").arg(info.sourceFile);
        if (info.sourcePage >= 0)
            ctx += tr("  ·  Page %1").arg(info.sourcePage + 1);
        auto* src = new QLabel(ctx);
        src->setProperty("mono", true);
        src->setStyleSheet("font-size:10px; color:#888;");
        bl->addWidget(src);
    }

    // Technical details (collapsed by default)
    if (!info.technicalDetails.isEmpty()) {
        m_toggleBtn = new QPushButton(tr("Show Details ▾"));
        m_toggleBtn->setFlat(true);
        m_toggleBtn->setStyleSheet("text-align:left; padding:2px 0; color:#5b9bd5; font-size:11px;");
        bl->addWidget(m_toggleBtn);

        m_details = new QTextEdit;
        m_details->setReadOnly(true);
        m_details->setPlainText(info.technicalDetails);
        m_details->setMaximumHeight(120);
        m_details->setStyleSheet(
            "background:#1a1b1e; border:1px solid #393b40; font-family:'JetBrains Mono';"
            "font-size:10px; padding:6px;");
        m_details->setVisible(false);
        bl->addWidget(m_details);

        connect(m_toggleBtn, &QPushButton::clicked, this, [this]() {
            bool show = !m_details->isVisible();
            m_details->setVisible(show);
            m_toggleBtn->setText(show ? tr("Hide Details ▴") : tr("Show Details ▾"));
            adjustSize();
        });
    }

    col->addWidget(body, 1);

    // ── Footer buttons ──────────────────────────────────────────────
    auto* foot = new QFrame;
    foot->setProperty("role", "modalFoot");
    auto* fr = new QHBoxLayout(foot);
    fr->setContentsMargins(12, 8, 12, 8);

    // Export Log (left-aligned)
    if (info.suggestedActions & ErrorInfo::ExportLog) {
        m_exportBtn = new QPushButton(tr("Export Log"));
        m_exportBtn->setStyleSheet("padding:6px 14px;");
        fr->addWidget(m_exportBtn);
        connect(m_exportBtn, &QPushButton::clicked, this, [this, info]() {
            onExportLog(info);
        });
    }

    fr->addStretch(1);

    // Skip
    if (info.suggestedActions & ErrorInfo::Skip) {
        m_skipBtn = new QPushButton(tr("Skip"));
        m_skipBtn->setStyleSheet("padding:6px 14px;");
        fr->addWidget(m_skipBtn);
        connect(m_skipBtn, &QPushButton::clicked, this, [this]() {
            emit skipRequested();
            done(SkipResult);
        });
    }

    // Retry
    if (info.suggestedActions & ErrorInfo::Retry) {
        m_retryBtn = new QPushButton(tr("Retry"));
        m_retryBtn->setStyleSheet(
            "background:#ff8c42; color:#1a1b1e; border:1px solid #ff8c42;"
            "font-weight:600; padding:6px 18px;");
        fr->addWidget(m_retryBtn);
        connect(m_retryBtn, &QPushButton::clicked, this, [this]() {
            emit retryRequested();
            accept();
        });
    }

    // OK (always present)
    m_okBtn = new QPushButton(tr("OK"));
    m_okBtn->setStyleSheet("padding:6px 18px;");
    m_okBtn->setDefault(true);
    fr->addWidget(m_okBtn);
    connect(m_okBtn, &QPushButton::clicked, this, &QDialog::reject);

    col->addWidget(foot);
}

void ErrorDialog::onExportLog(const ErrorInfo& info) {
    QString path = QFileDialog::getSaveFileName(
        this, tr("Export Error Log"), {},
        tr("JSON (*.json);;CSV (*.csv);;All Files (*)"));
    if (path.isEmpty()) return;

    ErrorLog log;
    log.append(info);

    bool ok = path.endsWith(".csv", Qt::CaseInsensitive)
                  ? log.exportCsv(path)
                  : log.exportJson(path);
    if (!ok) {
        auto* lbl = new QLabel(tr("Failed to write log file."));
        lbl->setStyleSheet("color:#c8442b;");
        // Show inline — don't spawn another dialog
        layout()->addWidget(lbl);
    }
}

// ── Static convenience ──────────────────────────────────────────────────

int ErrorDialog::show(const ErrorInfo& info, QWidget* parent) {
    ErrorDialog dlg(info, parent);
    return dlg.exec();
}

void ErrorDialog::showError(const QString& msg, QWidget* parent) {
    ErrorDialog::show(ErrorInfo::error(msg), parent);
}

} // namespace gp
