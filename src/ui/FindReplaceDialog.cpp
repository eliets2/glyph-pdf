// SPDX-License-Identifier: Apache-2.0
#include "ui/FindReplaceDialog.h"
#include "engines/TextMatchFinder.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLineEdit>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QSet>
#include <utility>

FindReplaceDialog::FindReplaceDialog(QWidget* parent)
    : QDialog(parent)
{
    setObjectName(QStringLiteral("findReplaceDialog"));   // R15: test seam
    setWindowTitle(tr("Find & Replace"));
    setModal(false);

    auto* grid = new QGridLayout;
    grid->addWidget(new QLabel(tr("Find:"), this), 0, 0);
    m_search = new QLineEdit(this);
    m_search->setObjectName(QStringLiteral("frSearch"));
    grid->addWidget(m_search, 0, 1, 1, 2);

    grid->addWidget(new QLabel(tr("Replace with:"), this), 1, 0);
    m_replace = new QLineEdit(this);
    m_replace->setObjectName(QStringLiteral("frReplace"));
    grid->addWidget(m_replace, 1, 1, 1, 2);

    auto* opts = new QHBoxLayout;
    m_matchCase = new QCheckBox(tr("Match case"), this);
    m_matchCase->setObjectName(QStringLiteral("frMatchCase"));
    m_wholeWords = new QCheckBox(tr("Whole words"), this);
    m_wholeWords->setObjectName(QStringLiteral("frWholeWords"));
    m_useRegex = new QCheckBox(tr("Regular expression"), this);
    m_useRegex->setObjectName(QStringLiteral("frRegex"));
    opts->addWidget(m_matchCase);
    opts->addWidget(m_wholeWords);
    opts->addWidget(m_useRegex);
    opts->addStretch();
    grid->addLayout(opts, 2, 0, 1, 3);

    m_scope = new QComboBox(this);
    m_scope->setObjectName(QStringLiteral("frScope"));
    m_scope->addItems({tr("All pages"), tr("Current page"), tr("Range…")});
    m_range = new QLineEdit(this);
    m_range->setObjectName(QStringLiteral("frRange"));
    m_range->setPlaceholderText(tr("e.g. 2-5"));
    m_range->setEnabled(false);
    grid->addWidget(new QLabel(tr("Scope:"), this), 3, 0);
    grid->addWidget(m_scope, 3, 1);
    grid->addWidget(m_range, 3, 2);
    connect(m_scope, &QComboBox::currentIndexChanged, this, [this](int) {
        updateScopeEnabled();
        recount();
    });
    connect(m_range, &QLineEdit::textChanged, this, [this]() { recount(); });

    auto* btns = new QHBoxLayout;
    m_countBtn = new QPushButton(tr("Count"), this);
    m_countBtn->setObjectName(QStringLiteral("frCount"));
    m_countBtn->setDefault(true);
    m_replaceAllBtn = new QPushButton(tr("Replace All"), this);
    m_replaceAllBtn->setObjectName(QStringLiteral("frReplaceAll"));
    auto* closeBtn = new QPushButton(tr("Close"), this);
    btns->addWidget(m_countBtn);
    btns->addWidget(m_replaceAllBtn);
    btns->addStretch();
    btns->addWidget(closeBtn);

    m_matchSummary = new QLabel(this);
    m_matchSummary->setObjectName(QStringLiteral("frMatchSummary"));

    // Moat M8 honesty: the disclosure is part of the dialog, not a tooltip.
    auto* honesty = new QLabel(
        tr("Replacements are drawn at the match position in the match's font size "
           "(standard font). The surrounding layout does not reflow — replacements "
           "that change the text width are reported after apply."), this);
    honesty->setObjectName(QStringLiteral("frHonesty"));
    honesty->setWordWrap(true);

    m_details = new QPlainTextEdit(this);
    m_details->setObjectName(QStringLiteral("frDetails"));
    m_details->setReadOnly(true);
    m_details->setFixedHeight(96);
    m_details->setPlaceholderText(tr("Outcome and geometry warnings appear here."));

    auto* col = new QVBoxLayout(this);
    col->addLayout(grid);
    col->addLayout(btns);
    col->addWidget(m_matchSummary);
    col->addWidget(honesty);
    col->addWidget(m_details);

    connect(m_countBtn, &QPushButton::clicked, this, [this]() { recount(); });
    connect(m_replaceAllBtn, &QPushButton::clicked, this, [this]() { applyReplace(); });
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::close);
    connect(m_search, &QLineEdit::textChanged, this, [this]() { recount(); });
    connect(m_matchCase, &QCheckBox::toggled, this, [this]() { recount(); });
    connect(m_wholeWords, &QCheckBox::toggled, this, [this]() { recount(); });
    connect(m_useRegex, &QCheckBox::toggled, this, [this]() { recount(); });

    recount();
}

void FindReplaceDialog::updateScopeEnabled() {
    if (m_range)
        m_range->setEnabled(m_scope && m_scope->currentIndex() == 2);
}

void FindReplaceDialog::setDocumentContext(const QString& docPath, int pageCount,
                                           int currentPage, const ReplaceInvoker& invoker) {
    m_docPath = docPath;
    m_pageCount = pageCount;
    m_currentPage = currentPage;
    m_invoker = invoker;
    recount();
}

ReplaceOptions FindReplaceDialog::currentOptions() const {
    ReplaceOptions options;
    options.searchText = m_search ? m_search->text() : QString();
    options.replaceText = m_replace ? m_replace->text() : QString();
    options.matchCase = m_matchCase && m_matchCase->isChecked();
    options.wholeWords = m_wholeWords && m_wholeWords->isChecked();
    options.useRegex = m_useRegex && m_useRegex->isChecked();
    // packa-F1: an unusable scope is its OWN state — never the same empty
    // page list that means "all pages" downstream.
    options.scopeValid = scopeRefusal().isEmpty();

    const int scope = m_scope ? m_scope->currentIndex() : 0;
    if (scope == 1) {
        // Current page (1-based display → 0-based scope).
        if (m_currentPage >= 1 && (m_pageCount <= 0 || m_currentPage <= m_pageCount))
            options.pages.append(m_currentPage - 1);
    } else if (scope == 2) {
        // Range "a-b", 1-based inclusive; mirrored into 0-based page ids.
        const QRegularExpression rx(QStringLiteral("^\\s*(\\d+)\\s*-\\s*(\\d+)\\s*$"));
        const auto m = rx.match(m_range ? m_range->text() : QString());
        if (m.hasMatch()) {
            int a = m.captured(1).toInt();
            int b = m.captured(2).toInt();
            if (a > b) std::swap(a, b);
            for (int p = qMax(1, a); p <= b; ++p) {
                if (m_pageCount > 0 && p > m_pageCount) break;
                options.pages.append(p - 1);
            }
        }
    }
    // scope 0 (All pages): pages stays empty (= whole document).
    return options;
}

// WP-R07: an unusable scope must be REFUSED explicitly — an empty page list
// means "all pages" downstream, so a malformed (or entirely out-of-document)
// range must never flow through as one. The dialog says no in plain text and
// leaves the document alone.
QString FindReplaceDialog::scopeRefusal() const {
    const int scope = m_scope ? m_scope->currentIndex() : 0;
    if (scope == 1) {
        if (m_pageCount > 0 && (m_currentPage < 1 || m_currentPage > m_pageCount))
            return tr("The current page (page %1) is outside this %2-page document — replace scope refused.")
                        .arg(m_currentPage).arg(m_pageCount);
        return QString();
    }
    if (scope == 2) {
        const QString range = m_range ? m_range->text() : QString();
        const QRegularExpression rx(QStringLiteral("^\\s*(\\d+)\\s*-\\s*(\\d+)\\s*$"));
        const auto m = rx.match(range);
        if (!m.hasMatch())
            return tr("The page range must look like 2-5 — replace scope refused.");
        int a = m.captured(1).toInt();
        int b = m.captured(2).toInt();
        if (a > b) std::swap(a, b);
        if (m_pageCount > 0 && (b < 1 || a > m_pageCount))
            return tr("The page range %1 is outside this %2-page document — replace scope refused.")
                        .arg(range.simplified()).arg(m_pageCount);
        return QString();
    }
    return QString();
}

void FindReplaceDialog::recount() {
    if (!m_matchSummary) return;
    updateActionAvailability();
    if (m_docPath.isEmpty()) {
        m_matchSummary->setText(tr("No document is open."));
        return;
    }
    const QString refusal = scopeRefusal();
    if (!refusal.isEmpty()) {
        m_matchSummary->setText(refusal);
        return;
    }
    ReplaceOptions options = currentOptions();
    if (options.searchText.isEmpty()) {
        m_matchSummary->setText(tr("Enter text to search for."));
        return;
    }
    const QRegularExpression rx = TextMatchFinder::buildPattern(
        options.searchText, options.matchCase, options.wholeWords, options.useRegex);
    if (!rx.isValid()) {
        m_matchSummary->setText(tr("Invalid regular expression: %1").arg(rx.errorString()));
        return;
    }
    QList<int> pages = options.pages;
    if (pages.isEmpty() && m_pageCount > 0) {
        for (int p = 0; p < m_pageCount; ++p) pages.append(p);
    }
    const QList<TextMatch> matches = TextMatchFinder::findMatches(m_docPath, pages, rx);
    QSet<int> pagesHit;
    for (const auto& m : matches) pagesHit.insert(m.pageIndex);
    m_matchSummary->setText(tr("%1 match(es) on %2 page(s) — count shown before replace.")
                                .arg(matches.size()).arg(pagesHit.size()));
}

QString FindReplaceDialog::matchSummaryText() const {
    return m_matchSummary ? m_matchSummary->text() : QString();
}

// packa-F1: Count/Replace are DISABLED until the scope is usable. A refused
// scope must not be clickable (and the mouse path on a disabled button is a
// no-op) — applyReplace()'s textual refusal stays as defense in depth.
void FindReplaceDialog::updateActionAvailability() {
    const bool usable = !m_docPath.isEmpty() && scopeRefusal().isEmpty();
    if (m_countBtn) m_countBtn->setEnabled(usable);
    if (m_replaceAllBtn) m_replaceAllBtn->setEnabled(usable);
}

void FindReplaceDialog::applyReplace() {
    if (!m_invoker) return;
    const QString refusal = scopeRefusal();
    if (!refusal.isEmpty()) {
        m_outcome = refusal;
        if (m_details) m_details->setPlainText(refusal);
        return;
    }
    const ReplaceOutcome out = m_invoker(currentOptions());
    m_outcome = out.message;
    if (m_details) {
        QString report = out.message;
        if (out.ok && out.requested > 0) {
            report += QStringLiteral("\n") +
                tr("Geometry: %1 of %2 replacement(s) changed the text width.")
                    .arg(out.widthChanged).arg(out.applied);
            const auto warnings = TextMatchFinder::reflowWarnings(out.matches, m_replace ? m_replace->text() : QString());
            if (!warnings.isEmpty())
                report += QStringLiteral("\n") +
                    tr("Length differs in %1 case(s) (first: %2, '%3' → '%4').")
                        .arg(warnings.size())
                        .arg(warnings.first().pageIndex + 1)
                        .arg(warnings.first().matched, warnings.first().replacement);
        }
        m_details->setPlainText(report);
    }
}

QString FindReplaceDialog::outcomeText() const {
    return m_details ? m_details->toPlainText() : m_outcome;
}
