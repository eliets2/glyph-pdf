// SPDX-License-Identifier: Apache-2.0
#include "RedactMode.h"
#include "util/GpTheme.h"
#include "ui/PdfViewerWidget.h"
#include "core/AppContext.h"
#include "core/interfaces/IPdfEditorEngine.h"
#include "engines/PatternRedactor.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressDialog>
#include <QPushButton>
#include <QRadioButton>
#include <QRegularExpression>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>

namespace gp {

RedactMode::RedactMode(QWidget* parent) : QWidget(parent) {
    auto* col = new QVBoxLayout(this);
    col->setContentsMargins(0, 0, 0, 0);
    col->setSpacing(0);

    // ── Toolbar ────────────────────────────────────────────────────────────
    auto* tb = new QFrame;
    tb->setProperty("role", "modeToolbar");
    tb->setFixedHeight(Theme::ToolbarH);
    auto* row = new QHBoxLayout(tb);
    row->setContentsMargins(10, 0, 10, 0);
    row->setSpacing(6);

    auto* monoLab = new QLabel(tr("REDACT"));
    monoLab->setProperty("mono", true);
    row->addWidget(monoLab);

    auto makePill = [&](const QString& text, bool active = false) -> QToolButton* {
        auto* btn = new QToolButton;
        btn->setText(text);
        btn->setProperty("variant", "ghost");
        btn->setCheckable(true);
        btn->setChecked(active);
        return btn;
    };

    m_pillMarkRegion  = makePill(tr("Mark Region"), true);
    m_pillMarkPattern = makePill(tr("Mark by Pattern \xe2\x96\xbe")); // ▾
    m_pillMarkAll     = makePill(tr("Mark All Occurrences"));

    row->addWidget(m_pillMarkRegion);
    row->addWidget(m_pillMarkPattern);
    row->addWidget(m_pillMarkAll);
    row->addStretch(1);

    m_applyBtn = new QToolButton;
    m_applyBtn->setText(tr("Apply All Redactions"));
    m_applyBtn->setProperty("variant", "danger");
    row->addWidget(m_applyBtn);

    auto* exitBtn = new QToolButton;
    exitBtn->setText(tr("Cancel"));
    exitBtn->setProperty("variant", "ghost");
    row->addWidget(exitBtn);

    col->addWidget(tb);

    // ── Canvas ────────────────────────────────────────────────────────────
    auto* canvas = new PdfViewerWidget;
    col->addWidget(canvas, 1);

    // ── Pattern configuration panel ───────────────────────────────────────
    auto* cfgFrame = new QFrame;
    cfgFrame->setProperty("role", "patternConfig");
    auto* cfgLayout = new QVBoxLayout(cfgFrame);
    cfgLayout->setContentsMargins(12, 8, 12, 8);
    cfgLayout->setSpacing(8);

    buildPatternSection(cfgFrame);
    buildScopeSection(cfgFrame);

    // match count + action row
    auto* actionRow = new QHBoxLayout;
    m_matchCountLabel = new QLabel(tr("Select a pattern to preview matches."));
    m_matchCountLabel->setProperty("role", "infoStrip");
    actionRow->addWidget(m_matchCountLabel);
    actionRow->addStretch(1);

    m_previewBtn = new QToolButton;
    m_previewBtn->setText(tr("Preview Matches"));
    actionRow->addWidget(m_previewBtn);

    m_clearBtn = new QToolButton;
    m_clearBtn->setText(tr("Clear Marks"));
    actionRow->addWidget(m_clearBtn);

    cfgLayout->addLayout(actionRow);
    col->addWidget(cfgFrame);

    // ── Info strip ────────────────────────────────────────────────────────
    auto* info = new QFrame;
    info->setProperty("role", "infoStrip");
    info->setFixedHeight(Theme::InfoStripH);
    auto* irow = new QHBoxLayout(info);
    irow->setContentsMargins(12, 0, 12, 0);
    irow->setSpacing(14);

    auto makeInfoLabel = [](const QString& s) -> QLabel* {
        auto* lbl = new QLabel(s);
        lbl->setProperty("role", "infoStrip");
        return lbl;
    };
    irow->addWidget(makeInfoLabel(tr("— REGIONS MARKED")));
    irow->addWidget(makeInfoLabel(tr("— TEXT \xc2\xb7 — IMAGE \xc2\xb7 — PATTERN")));
    irow->addWidget(makeInfoLabel(tr("ESTIMATED — CHARACTERS REMOVED")));
    irow->addStretch(1);
    col->addWidget(info);

    // ── Signal connections ────────────────────────────────────────────────
    connect(m_pillMarkPattern, &QToolButton::clicked, this, [this](bool checked) {
        // Show/hide the config panel based on toggle
        if (auto* cfg = findChild<QFrame*>("patternConfig")) {
            Q_UNUSED(cfg);
        }
        if (checked) {
            m_pillMarkRegion->setChecked(false);
            m_pillMarkAll->setChecked(false);
        }
    });

    connect(m_pillMarkRegion, &QToolButton::clicked, this, [this]() {
        m_pillMarkPattern->setChecked(false);
        m_pillMarkAll->setChecked(false);
        m_pillMarkRegion->setChecked(true);
    });

    connect(m_pillMarkAll, &QToolButton::clicked, this, [this]() {
        m_pillMarkPattern->setChecked(false);
        m_pillMarkRegion->setChecked(false);
        m_pillMarkAll->setChecked(true);
    });

    connect(m_patternCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RedactMode::onPatternChanged);
    connect(m_regexEdit, &QLineEdit::textChanged,
            this, &RedactMode::onRegexTextChanged);
    connect(m_previewBtn, &QToolButton::clicked, this, &RedactMode::onPreviewMatches);
    connect(m_applyBtn,   &QToolButton::clicked, this, &RedactMode::onApplyRedactions);
    connect(m_clearBtn,   &QToolButton::clicked, this, &RedactMode::onClearMarks);

    connect(exitBtn, &QToolButton::clicked, this, [canvas]() {
        Q_UNUSED(canvas);
    });

    connect(m_scopeCurrentPage, &QRadioButton::toggled, this, &RedactMode::onScopeChanged);
    connect(m_scopeAllPages,    &QRadioButton::toggled, this, &RedactMode::onScopeChanged);
    connect(m_scopeRange,       &QRadioButton::toggled, this, &RedactMode::onScopeChanged);

    // Trigger initial state
    onPatternChanged(m_patternCombo->currentIndex());
    onScopeChanged();
}

void RedactMode::buildPatternSection(QWidget* host) {
    auto* layout = qobject_cast<QVBoxLayout*>(host->layout());
    if (!layout) return;

    auto* patternRow = new QHBoxLayout;
    auto* patternLabel = new QLabel(tr("Pattern:"));
    patternLabel->setProperty("mono", true);
    patternRow->addWidget(patternLabel);

    m_patternCombo = new QComboBox;
    const QStringList names = PatternRedactor::availablePatterns();
    for (const QString& n : names) {
        m_patternCombo->addItem(n, n);
    }
    m_patternCombo->addItem(tr("Custom regex \xe2\x80\xa6"), QStringLiteral("custom"));
    patternRow->addWidget(m_patternCombo, 1);
    layout->addLayout(patternRow);

    // Custom regex entry — hidden until "custom" is selected
    m_regexEdit = new QLineEdit;
    m_regexEdit->setPlaceholderText(tr("Enter regular expression (Qt syntax)"));
    m_regexEdit->setVisible(false);
    layout->addWidget(m_regexEdit);
}

void RedactMode::buildScopeSection(QWidget* host) {
    auto* layout = qobject_cast<QVBoxLayout*>(host->layout());
    if (!layout) return;

    auto* scopeRow = new QHBoxLayout;
    auto* scopeLabel = new QLabel(tr("Scope:"));
    scopeLabel->setProperty("mono", true);
    scopeRow->addWidget(scopeLabel);

    m_scopeCurrentPage = new QRadioButton(tr("Current page"));
    m_scopeAllPages    = new QRadioButton(tr("All pages"));
    m_scopeRange       = new QRadioButton(tr("Page range:"));
    m_scopeAllPages->setChecked(true);

    m_pageRangeEdit = new QLineEdit;
    m_pageRangeEdit->setPlaceholderText(tr("e.g. 1-3, 5, 7-9"));
    m_pageRangeEdit->setEnabled(false);
    m_pageRangeEdit->setMaximumWidth(150);

    scopeRow->addWidget(m_scopeCurrentPage);
    scopeRow->addWidget(m_scopeAllPages);
    scopeRow->addWidget(m_scopeRange);
    scopeRow->addWidget(m_pageRangeEdit);
    scopeRow->addStretch(1);
    layout->addLayout(scopeRow);
}

void RedactMode::setAppContext(const AppContext* ctx) {
    m_ctx = ctx;
}

void RedactMode::activateCustomRegex(const QString& initialPattern) {
    // Select the "Custom regex" item (last in combo)
    const int customIdx = m_patternCombo->count() - 1;
    m_patternCombo->setCurrentIndex(customIdx);
    m_pillMarkPattern->setChecked(true);
    m_pillMarkRegion->setChecked(false);
    m_pillMarkAll->setChecked(false);

    if (!initialPattern.isEmpty()) {
        m_regexEdit->setText(initialPattern);
    }
    m_regexEdit->setFocus();
}

void RedactMode::onPatternChanged(int index) {
    if (!m_patternCombo) return;
    const QString key = m_patternCombo->itemData(index).toString();
    const bool isCustom = (key == QLatin1String("custom"));
    if (m_regexEdit) m_regexEdit->setVisible(isCustom);
    m_matchCountLabel->setText(tr("Select a pattern to preview matches."));
}

void RedactMode::onRegexTextChanged(const QString& text) {
    if (text.isEmpty()) {
        m_regexEdit->setStyleSheet(QString());
        m_matchCountLabel->setText(tr("Select a pattern to preview matches."));
        return;
    }
    QRegularExpression rx(text);
    if (!rx.isValid()) {
        m_regexEdit->setStyleSheet(QStringLiteral("border: 2px solid red;"));
        m_regexEdit->setToolTip(rx.errorString());
    } else {
        m_regexEdit->setStyleSheet(QString());
        m_regexEdit->setToolTip(QString());
    }
}

QRegularExpression RedactMode::currentRegex() const {
    if (!m_patternCombo) return QRegularExpression();
    const QString key = m_patternCombo->currentData().toString();
    if (key == QLatin1String("custom")) {
        return QRegularExpression(m_regexEdit ? m_regexEdit->text() : QString());
    }
    return PatternRedactor::namedPattern(key);
}

std::pair<int,int> RedactMode::resolvePageRange() const {
    // Returns 0-based inclusive [start, end]; -1 signals "all pages" to caller
    if (m_scopeAllPages && m_scopeAllPages->isChecked()) {
        return {-1, -1};
    }
    if (m_scopeCurrentPage && m_scopeCurrentPage->isChecked()) {
        // Current page — caller must resolve the actual page index from the viewer
        return {0, 0};
    }
    // Range expression — simple parse: "start-end" or single number
    if (m_pageRangeEdit) {
        const QString expr = m_pageRangeEdit->text().trimmed();
        if (!expr.isEmpty()) {
            const int dash = expr.indexOf('-');
            bool startOk = false, endOk = false;
            int startPage = 0, endPage = 0;
            if (dash > 0) {
                startPage = expr.left(dash).trimmed().toInt(&startOk) - 1;
                endPage   = expr.mid(dash + 1).trimmed().toInt(&endOk) - 1;
            } else {
                startPage = expr.toInt(&startOk) - 1;
                endPage = startPage;
                endOk = startOk;
            }
            if (startOk && endOk && startPage >= 0 && endPage >= startPage) {
                return {startPage, endPage};
            }
        }
    }
    return {-1, -1};
}

void RedactMode::showMatchCount(int count) {
    if (count == 0) {
        m_matchCountLabel->setText(tr("No matches found."));
    } else {
        m_matchCountLabel->setText(tr("%1 match(es) found.").arg(count));
    }
}

void RedactMode::onPreviewMatches() {
    if (!m_ctx || !m_ctx->pdfEditor) {
        QMessageBox::warning(this, tr("Redact"), tr("No document is open."));
        return;
    }

    const QRegularExpression rx = currentRegex();
    if (!rx.isValid()) {
        QMessageBox::warning(this, tr("Redact"), tr("The regular expression is invalid:\n%1").arg(rx.errorString()));
        return;
    }
    if (rx.pattern().isEmpty()) {
        QMessageBox::warning(this, tr("Redact"), tr("Please select a pattern or enter a custom regular expression."));
        return;
    }

    const QString pdfPath = m_ctx->pdfEditor->currentFile();
    if (pdfPath.isEmpty()) {
        QMessageBox::warning(this, tr("Redact"), tr("No document path available."));
        return;
    }

    const auto [startPage, endPage] = resolvePageRange();

    // For preview: count matches across the requested scope (single page or first page if all)
    int previewPage = 0;
    if (startPage >= 0) {
        previewPage = startPage;
    }
    const QList<QRectF> matches = PatternRedactor::findMatches(pdfPath, previewPage, rx);
    showMatchCount(matches.size());
}

void RedactMode::onApplyRedactions() {
    if (!m_ctx || !m_ctx->pdfEditor) {
        QMessageBox::warning(this, tr("Redact"), tr("No document is open."));
        return;
    }

    const QRegularExpression rx = currentRegex();
    if (!rx.isValid()) {
        QMessageBox::warning(this, tr("Redact"), tr("The regular expression is invalid:\n%1").arg(rx.errorString()));
        return;
    }
    if (rx.pattern().isEmpty()) {
        QMessageBox::warning(this, tr("Redact"), tr("Please select a pattern or enter a custom regular expression."));
        return;
    }

    const QString pdfPath = m_ctx->pdfEditor->currentFile();
    if (pdfPath.isEmpty()) {
        QMessageBox::warning(this, tr("Redact"), tr("No document path available."));
        return;
    }

    const auto [startPage, endPage] = resolvePageRange();

    // Confirm action
    const int answer = QMessageBox::question(
        this,
        tr("Apply Redactions"),
        tr("This will permanently remove matched content. Redaction cannot be undone.\n\nContinue?"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (answer != QMessageBox::Yes) return;

    bool success = m_ctx->pdfEditor->applyPatternRedactions(rx, startPage, endPage);
    if (!success) {
        QMessageBox::critical(this, tr("Redaction Failed"),
            tr("Pattern redaction failed. The document has not been modified.\n\n%1")
                .arg(m_ctx->pdfEditor->lastError().userMessage));
    } else {
        m_matchCountLabel->setText(tr("Redaction applied successfully."));
    }
}

void RedactMode::onClearMarks() {
    m_matchCountLabel->setText(tr("Select a pattern to preview matches."));
}

void RedactMode::onScopeChanged() {
    const bool rangeSelected = m_scopeRange && m_scopeRange->isChecked();
    if (m_pageRangeEdit) m_pageRangeEdit->setEnabled(rangeSelected);
}

} // namespace gp
