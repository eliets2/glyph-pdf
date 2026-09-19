// SPDX-License-Identifier: Apache-2.0
#include "AccessibilityPanel.h"

#include "util/GpTheme.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
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

// The bounded language list for the /Lang fix — a SHORT curated set with
// BCP-47 tags; the user picks, the tool never guesses the document language.
struct LangOption { const char* tag; const char* label; };
const QVector<LangOption> kLanguages = {
    { "en",    "English" },
    { "de-DE", "Deutsch" },
    { "fr-FR", "Français" },
    { "es-ES", "Español" },
    { "it-IT", "Italiano" },
    { "pt-PT", "Português" },
    { "nl-NL", "Nederlands" },
    { "pl-PL", "Polski" },
    { "tr-TR", "Türkçe" },
    { "ru-RU", "Русский" },
    { "ar",    "العربية" },
    { "ja",    "日本語" },
    { "zh-CN", "中文（简体）" },
};

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

void AccessibilityPanel::setFixRunner(
    std::function<A11yFixOutcome(const A11yFixRequest&)> runner) {
    m_fixRunner = std::move(runner);
}

void AccessibilityPanel::applyFix(const A11yFixRequest& request) {
    if (!m_fixRunner) return;   // no runner → no fix affordances, no path here
    const A11yFixOutcome out = m_fixRunner(request);
    if (out.ok) {
        emit documentMutated(out.message);
        m_statusLabel->setText(tr("Fix applied — re-running check…"));
        // Re-scan the SAME identity so the user sees the finding disappear.
        setDocument(m_currentDocPath);
    } else {
        // Honest refusal — say why, change nothing.
        m_statusLabel->setText(tr("Fix not applied: %1").arg(out.message));
    }
}

void AccessibilityPanel::showEditorForFinding(int findingIndex) {
    if (findingIndex < 0 || findingIndex >= m_lastReport.findings.size()) return;
    const A11yFinding& f = m_lastReport.findings.at(findingIndex);

    // Map the finding to a concrete fix request. targetId is the
    // machine-readable name set by the engine (image resource name or fully
    // qualified field name).
    A11yFixRequest req;
    req.page = f.page;
    req.resourceName = f.targetId;
    req.fieldName = f.targetId;
    if (f.checkId == QStringLiteral("doc-language"))
        req.kind = A11yFixKind::SetLanguage;
    else if (f.checkId == QStringLiteral("display-doc-title"))
        req.kind = A11yFixKind::EnableDisplayDocTitle;
    else if (f.checkId == QStringLiteral("image-alt"))
        req.kind = A11yFixKind::SetImageAltText;
    else if (f.checkId == QStringLiteral("field-tu"))
        req.kind = A11yFixKind::SetFieldTu;
    else
        return;   // struct-tree / doc-title: no cheap fix in P1

    auto* editor = new QFrame;
    editor->setStyleSheet(
        "background:#141518; border:1px solid #393b40; padding:6px 8px; margin-bottom:6px;");
    auto* h = new QHBoxLayout(editor);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(6);

    QComboBox* combo = nullptr;
    QLineEdit* edit = nullptr;
    if (req.kind == A11yFixKind::SetLanguage) {
        combo = new QComboBox(editor);
        combo->setObjectName(QStringLiteral("a11yLanguageCombo"));
        for (const LangOption& l : kLanguages)
            combo->addItem(QString::fromUtf8(l.label), QString::fromUtf8(l.tag));
        combo->setToolTip(tr("Pick the document's language — the tool never guesses it"));
        h->addWidget(new QLabel(tr("Language:"), editor), 0);
        h->addWidget(combo, 1);
    } else {
        edit = new QLineEdit(editor);
        edit->setObjectName(QStringLiteral("a11yAltTextEdit"));
        edit->setPlaceholderText(req.kind == A11yFixKind::SetFieldTu
                                     ? tr("Describe what the field is for")
                                     : tr("Describe the image for screen readers"));
        h->addWidget(edit, 1);
    }

    auto* apply = new QPushButton(tr("Apply"), editor);
    apply->setObjectName(QStringLiteral("a11yApplyFixButton"));
    auto* cancel = new QPushButton(tr("Cancel"), editor);
    h->addWidget(apply);
    h->addWidget(cancel);

    const int row = findingIndex + 1;  // insert directly under the finding
    m_findingsLayout->insertWidget(row, editor);

    // Values are read from the LIVE widgets at click time (never captured by
    // reference into a transient frame).
    connect(apply, &QPushButton::clicked, this, [this, req, combo, edit, editor]() {
        A11yFixRequest finalReq = req;
        if (finalReq.kind == A11yFixKind::SetLanguage) {
            if (!combo) return;
            finalReq.language = combo->currentData().toString();
        } else {
            if (!edit) return;
            finalReq.text = edit->text().trimmed();
            if (finalReq.text.isEmpty())
                return;   // never write an empty /Alt or /TU
        }
        editor->deleteLater();
        applyFix(finalReq);
    });
    connect(cancel, &QPushButton::clicked, editor, [editor]() {
        editor->deleteLater();
    });
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

    int findingIndex = -1;
    for (const A11yFinding& f : report.findings) {
        ++findingIndex;
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

        // D2 cheap fixes: only when a runner is wired, and only for the four
        // fixable findings. No runner → no buttons (never dead controls).
        if (m_fixRunner) {
            const bool fixable =
                f.checkId == QStringLiteral("doc-language") ||
                f.checkId == QStringLiteral("display-doc-title") ||
                f.checkId == QStringLiteral("image-alt") ||
                f.checkId == QStringLiteral("field-tu");
            if (fixable) {
                auto* fixBtn = new QPushButton(tr("FIX"), w);
                fixBtn->setObjectName(
                    QStringLiteral("a11yFixButton_%1").arg(findingIndex));
                if (f.checkId == QStringLiteral("display-doc-title")) {
                    // "title from /Info": needs a title to exist. Without one
                    // the button is disabled and says why — honest refusal.
                    const bool hasTitle = report.hasDocTitle;
                    fixBtn->setEnabled(hasTitle);
                    fixBtn->setToolTip(hasTitle
                        ? tr("Show the document title in the window title bar")
                        : tr("No /Title in this document — set one under "
                             "Document Properties first"));
                    const A11yFixRequest req{A11yFixKind::EnableDisplayDocTitle,
                                             QString(), -1, QString(), QString(), QString()};
                    connect(fixBtn, &QPushButton::clicked, this, [this, req]() {
                        applyFix(req);
                    });
                } else {
                    fixBtn->setToolTip(tr("Fix this gap"));
                    const int idx = findingIndex;
                    connect(fixBtn, &QPushButton::clicked, this, [this, idx]() {
                        showEditorForFinding(idx);
                    });
                }
                h->addWidget(fixBtn);
            }
        }
        m_findingsLayout->addWidget(w);
    }
    m_findingsList->show();
}

} // namespace gp
