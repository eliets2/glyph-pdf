// SPDX-License-Identifier: Apache-2.0
#include "RedactMode.h"
#include "RedactApplyDialog.h"
#include "util/GpTheme.h"
#include "ui/PdfViewerWidget.h"
#include "core/AppContext.h"
#include "core/interfaces/IPdfEditorEngine.h"
#include "engines/PatternRedactor.h"
#include "modes/PagesMode.h"
#include <podofo/podofo.h>

#include <QCheckBox>
#include <QComboBox>
#include <QFileInfo>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPointer>
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

    // O1: "Mark Region" and "Mark All Occurrences" place real redaction marks
    // (via PatternRedactor geometry) for the Apply pipeline to burn in.
    m_pillMarkRegion->setVisible(true);
    m_pillMarkAll->setVisible(true);

    // Only the wired pill is shown.
    row->addWidget(m_pillMarkRegion);
    row->addWidget(m_pillMarkPattern);
    row->addWidget(m_pillMarkAll);
    row->addStretch(1);

    m_applyBtn = new QToolButton;
    m_applyBtn->setText(tr("Apply All Redactions"));
    m_applyBtn->setProperty("variant", "danger");
    row->addWidget(m_applyBtn);

    // §9.8 P0: a black box is meaningless if the same PII survives in
    // metadata, attachments, embedded JavaScript, or the name tree — offer
    // the full hidden-data scrub on the saved copy (default ON), running the
    // same sanitizeDocument() pass as Security ▸ Sanitize Document.
    m_chkSanitizeCopy = new QCheckBox(tr("Sanitize copy (metadata, attachments, JS)"));
    m_chkSanitizeCopy->setObjectName(QStringLiteral("redactChkSanitizeCopy"));
    m_chkSanitizeCopy->setChecked(true);
    m_chkSanitizeCopy->setToolTip(tr(
        "Runs the full hidden-data scrub on the saved copy: document metadata, "
        "XMP, attachments, JavaScript actions, bookmarks and form values."));
    row->addWidget(m_chkSanitizeCopy);

    // §9.8 P0: state the compliance differentiator on the redaction surface
    // (muted, one line — the whole pipeline is in-process).
    auto* localClaim = new QLabel(PagesMode::localFirstClaim());
    localClaim->setObjectName("redactLocalClaimLabel");
    localClaim->setWordWrap(true);
    localClaim->setStyleSheet(QString("color:%1; font-size:8pt;")
                                  .arg(gp::Theme::fg2().name()));
    col->addWidget(localClaim);

    // §9.8 P1: the Cancel/Exit control is RESTORED and honestly wired (AR-8 D3
    // had only hidden the button whose connection was a no-op lambda — the
    // missing affordance stayed missing). Clicking emits exitRequested(); the
    // host returns to the standard canvas via ModeController's relay. Placed
    // redaction marks are NOT touched — they live on the viewer and remain
    // recoverable when the user re-enters the mode.
    auto* exitBtn = new QToolButton;
    exitBtn->setObjectName(QStringLiteral("redactBtnCancel"));
    exitBtn->setText(tr("Cancel"));
    exitBtn->setProperty("variant", "ghost");
    exitBtn->setToolTip(tr(
        "Exit redaction. Placed marks are kept on the document — reopen the "
        "Redaction task to review or apply them."));
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

    connect(m_pillMarkRegion, &QToolButton::clicked, this, &RedactMode::onMarkRegion);

    connect(m_pillMarkAll, &QToolButton::clicked, this, &RedactMode::onMarkAllOccurrences);

    connect(m_patternCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RedactMode::onPatternChanged);
    connect(m_regexEdit, &QLineEdit::textChanged,
            this, &RedactMode::onRegexTextChanged);
    connect(m_previewBtn, &QToolButton::clicked, this, &RedactMode::onPreviewMatches);
    connect(m_applyBtn,   &QToolButton::clicked, this, &RedactMode::onApplyRedactions);
    connect(m_clearBtn,   &QToolButton::clicked, this, &RedactMode::onClearMarks);

    // §9.8 P1: Cancel exits the mode via the exitRequested contract (the host
    // snaps navigation back to the standard canvas); the viewer's marks and
    // the tool state stay exactly as they are.
    connect(exitBtn, &QToolButton::clicked, this, &RedactMode::exitRequested);

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

void RedactMode::setViewer(PdfViewerWidget* viewer) {
    m_viewer = viewer;
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

QList<int> RedactMode::resolvePageRange() const {
    if (m_scopeAllPages && m_scopeAllPages->isChecked()) {
        return QList<int>();  // empty = all pages
    }
    if (m_scopeCurrentPage && m_scopeCurrentPage->isChecked()) {
        // Use the viewer's real current page (0-based).  Fall back to page 0 only
        // when no viewer has been injected (e.g. unit-test context).
        const int page = (m_viewer && m_viewer->isLoaded()) ? m_viewer->currentPage() : 0;
        return QList<int>{ page };
    }
    if (m_pageRangeEdit) {
        const QString expr = m_pageRangeEdit->text().trimmed();
        if (expr.isEmpty()) {
            return QList<int>{-2}; // Sentinel for invalid
        }
        // Get page count from the engine's loaded document
        int totalPages = 0;
        if (m_ctx && m_ctx->pdfEditor) {
            // Load the PDF quickly to get page count via PoDoFo
            const QString pdfPath = m_ctx->pdfEditor->currentFile();
            if (!pdfPath.isEmpty()) {
                try {
                    PoDoFo::PdfMemDocument doc;
                    doc.Load(pdfPath.toUtf8().constData());
                    totalPages = static_cast<int>(doc.GetPages().GetCount());
                } catch (...) {}
            }
        }
        if (totalPages <= 0) {
            return QList<int>{-2}; // Can't determine page count
        }
        QList<int> pages = PagesMode::parsePageRange(expr, totalPages);
        if (pages.isEmpty()) {
            return QList<int>{-2}; // Sentinel for invalid
        }
        return pages;
    }
    return QList<int>();
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

    const QList<int> pages = resolvePageRange();
    if (pages.size() == 1 && pages.first() == -2) {
        // Invalid range
        showMatchCount(0);
        return;
    }

    // For preview: show matches on the explicitly requested page(s).
    // When scope is "all pages" (empty list) or a custom range, preview the first
    // page in the resolved set.  When scope is "current page" the list contains
    // the real current page index from the viewer.
    int previewPage = (m_viewer && m_viewer->isLoaded()) ? m_viewer->currentPage() : 0;
    if (!pages.isEmpty()) {
        previewPage = pages.first();
    }
    const QList<QRectF> matches = PatternRedactor::findMatches(pdfPath, previewPage, rx);
    showMatchCount(matches.size());
}

void RedactMode::onApplyRedactions() {
    if (!m_ctx || !m_ctx->pdfEditor) {
        QMessageBox::warning(this, tr("Redact"), tr("No document is open."));
        return;
    }
    if (!m_viewer) {
        QMessageBox::warning(this, tr("Redact"), tr("No document is open."));
        return;
    }

    // §9.8 P0 (DEFECT 1): "Apply All Redactions" must burn in EVERY placed mark
    // (Mark Region / Mark All Occurrences), not just re-run the regex. The
    // mark-based path is the single authoritative boundary — it gathers all
    // ToolMode::Redact annotations, guards signed documents (ER-2), and excises
    // via the same engine pipeline SecurityController uses.
    const QList<AnnotationItem> marks = m_viewer->annotations();
    bool hasMarks = false;
    for (const auto& a : marks) {
        if (a.mode == ToolMode::Redact) { hasMarks = true; break; }
    }
    if (!hasMarks) {
        QMessageBox::warning(this, tr("Redact"),
            tr("No redaction marks are placed. Use Mark Region or Mark All Occurrences first."));
        return;
    }

    const QString pdfPath = m_ctx->pdfEditor->currentFile();
    if (pdfPath.isEmpty()) {
        QMessageBox::warning(this, tr("Redact"), tr("No document path available."));
        return;
    }

    // ER-2: refuse to redact a signed document in place (leaks excised bytes
    // into revision 1). The engine enforces this too; we surface it up front.
    if (m_ctx->pdfEditor->hasPdfSignatures()) {
        QMessageBox::critical(
            this, tr("Cannot Redact Signed Document"),
            tr("This document is digitally signed.\n\n"
               "Applying redactions and saving in place would leave the original "
               "content recoverable from the PDF revision history.\n\n"
               "To redact permanently:\n"
               "1. File > Save As — save an unsigned copy.\n"
               "2. Open the copy and apply redactions."));
        return;
    }

    // U05: pre-mutation summary dialog — mark/page counts, the actual
    // sanitization choice, and destination pickers with defaults and normal
    // overwrite handling. Replaces the plain confirm box and the old
    // fixed-destination direct save (RedactMode.cpp `_redacted.pdf` default is
    // preserved as the dialog's default).
    RedactApplyPlan plan;
    plan.sourcePath = pdfPath;
    const QFileInfo fi(pdfPath);
    plan.destinationPath = fi.absolutePath() + QLatin1Char('/')
        + fi.completeBaseName() + QStringLiteral("_redacted.pdf");
    plan.sanitizedDestinationPath = fi.absolutePath() + QLatin1Char('/')
        + fi.completeBaseName() + QStringLiteral("_redacted_sanitized.pdf");
    plan.sourcePageCount = m_viewer->isLoaded() ? m_viewer->pageCount() : 0;
    plan.sanitize = m_chkSanitizeCopy && m_chkSanitizeCopy->isChecked();
    for (const auto& a : marks) {
        if (a.mode == ToolMode::Redact) {
            ++plan.markCount;
            ++plan.marksPerPage[a.pageIndex];
        }
    }

    RedactApplyDialog dlg(plan, this);
    if (dlg.exec() != QDialog::Accepted) return; // nothing mutated
    const RedactApplyPlan chosen = dlg.plan();

    RedactRequest request;
    request.sourcePath = chosen.sourcePath;
    request.destinationPath = chosen.destinationPath;
    for (const auto& a : marks) {
        if (a.mode == ToolMode::Redact)
            request.redactionsByPage[a.pageIndex].append(a.rect);
    }
    request.sanitize = chosen.sanitize;
    request.sanitizedDestinationPath = chosen.sanitizedDestinationPath;
    // §9.8 P1: optional reason text printed on the burn-in boxes (empty =
    // current behavior).
    request.overlayText = chosen.overlayText;

    runRedactOperation(request);
}

// U05: the ONE transactional redaction operation behind this entry path. The
// live document is never mutated (the operation runs on a disposable private
// engine and a unique temp candidate); marks are cleared only after the output
// is committed AND kept, and partial failure is presented by the shared
// labeled presenter — never by a generic success banner.
void RedactMode::runRedactOperation(const RedactRequest& request) {
    // Delete the PREVIOUS operation's progress dialog here — never from the
    // finished handler below. A modal QProgressDialog::setValue() pumps the
    // event loop (Qt: "if (isModal() ...) processEvents()"), so a deleteLater
    // delivered inside that pump frees the dialog under the still-executing
    // setValue frame (use-after-free in reset()). close() from the finished
    // handler is safe; nothing can be mid-setValue at the start of a new run.
    if (m_redactProgress) {
        m_redactProgress->deleteLater();
        m_redactProgress = nullptr;
    }
    auto* progress = new QProgressDialog(tr("Applying redactions..."), tr("Cancel"),
                                         0, request.redactionsByPage.size(), this);
    m_redactProgress = progress;
    progress->setWindowModality(Qt::WindowModal);
    progress->setMinimumDuration(0);
    progress->show();

    auto* op = new RedactOperation(request, this);
    connect(progress, &QProgressDialog::canceled, op, &RedactOperation::cancel);
    connect(op, &RedactOperation::stageChanged, this,
            [progress](RedactStage stage, int pagesDone, int pagesTotal) {
                if (stage == RedactStage::Redacting) {
                    progress->setRange(0, pagesTotal);
                    progress->setValue(pagesDone);
                }
            });

    QPointer<RedactMode> self(this);
    QPointer<PdfViewerWidget> viewer(m_viewer);
    connect(op, &RedactOperation::finished, this,
            [self, progress, viewer](const RedactResult& result) {
                // Close only — deletion is deferred to the next runRedactOperation
                // (see the comment there for the QProgressDialog pump hazard).
                progress->close();
                if (!self) return;
                const auto decision = RedactResultPresenter::present(self, result);
                // Marks are cleared only once the redacted output is committed
                // AND kept; Failed / Canceled / Discard keep them recoverable.
                const bool committedAndKept =
                    result.outcome == RedactOutcome::Completed
                    || (result.outcome == RedactOutcome::PartialRedactedOnly
                        && decision == RedactResultPresenter::MarkDecision::ClearMarks);
                if (committedAndKept && viewer) {
                    const QList<AnnotationItem> annos = viewer->annotations();
                    QList<AnnotationItem> remaining;
                    for (const auto& a : annos) {
                        if (a.mode != ToolMode::Redact) remaining.append(a);
                    }
                    viewer->setAnnotations(remaining);
                }
                emit self->statusMessageRequested(RedactResultPresenter::bannerText(result));
            });
    op->start();
}

void RedactMode::onClearMarks() {
    // Audit 9.8 P0: make Clear Marks actually remove placed redaction marks
    // (was a no-op that only reset the status label).
    if (!m_viewer) {
        m_matchCountLabel->setText(tr("No document open."));
        return;
    }
    const QList<AnnotationItem> annos = m_viewer->annotations();
    QList<AnnotationItem> remaining;
    remaining.reserve(annos.size());
    int removed = 0;
    for (const auto& anno : annos) {
        if (anno.mode == ToolMode::Redact) ++removed;
        else remaining.append(anno);
    }
    if (removed == 0) {
        m_matchCountLabel->setText(tr("No redaction marks to clear."));
        return;
    }
    m_viewer->setAnnotations(remaining);
    m_matchCountLabel->setText(tr("Cleared %1 redaction mark(s).").arg(removed));
}

void RedactMode::onScopeChanged() {
    const bool rangeSelected = m_scopeRange && m_scopeRange->isChecked();
    if (m_pageRangeEdit) m_pageRangeEdit->setEnabled(rangeSelected);
}

// §9.8 P0: Mark Region activates the canvas drag-placement path — the same
// ToolMode::Redact drag used by the ribbon Mark button.
void RedactMode::onMarkRegion() {
    m_pillMarkPattern->setChecked(false);
    m_pillMarkAll->setChecked(false);
    m_pillMarkRegion->setChecked(true);
    if (m_viewer) {
        m_viewer->setToolMode(ToolMode::Redact);
        emit statusMessageRequested(tr("Drag on the page to mark a redaction region."));
    }
}

// §9.8 P0: Mark All Occurrences places redaction marks for every regex match
// in the selected page range, via the same PatternRedactor geometry the Apply
// pipeline consumes.
void RedactMode::onMarkAllOccurrences() {
    m_pillMarkPattern->setChecked(false);
    m_pillMarkRegion->setChecked(false);
    m_pillMarkAll->setChecked(true);
    if (!m_viewer || !m_ctx) return;
    const QString path = m_viewer->filePath();
    if (path.isEmpty()) return;
    const QRegularExpression rx = currentRegex();
    if (!rx.isValid()) {
        emit statusMessageRequested(tr("Invalid pattern — cannot mark occurrences."));
        return;
    }
    // §9.8 F1: resolvePageRange returns an explicit LIST of 0-based pages
    // ("1-3, 5" → {0,1,2,4}), not a [start,end] pair — treating it as a range
    // marked (and later destroyed on Apply) pages the user never selected,
    // and the {-2} invalid-range sentinel fell through to "all pages".
    // Empty list = all-pages scope, expanded here because
    // PatternRedactor::findMatches treats an empty list as "nothing to do".
    QList<int> pages = resolvePageRange();
    if (pages.size() == 1 && pages.first() == -2) {
        emit statusMessageRequested(tr("Invalid page range — cannot mark occurrences."));
        return;
    }
    if (pages.isEmpty()) {
        const int pageCount = m_viewer->pageCount();
        for (int p = 0; p < pageCount; ++p) pages.append(p);
    }

    const auto matches = PatternRedactor::findMatches(path, pages, rx);
    QList<AnnotationItem> annos = m_viewer->annotations();
    int placed = 0;
    for (auto it = matches.constBegin(); it != matches.constEnd(); ++it) {
        for (const QRectF& r : it.value()) {
            AnnotationItem a;
            a.mode = ToolMode::Redact;
            a.pageIndex = it.key();
            a.rect = r;
            annos.append(a);
            ++placed;
        }
    }
    m_viewer->setAnnotations(annos);
    emit statusMessageRequested(
        tr("Marked %1 occurrence(s) across %2 page(s). Review, then Apply.")
            .arg(placed).arg(matches.size()));
}

} // namespace gp
