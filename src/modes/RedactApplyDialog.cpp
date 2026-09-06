// SPDX-License-Identifier: Apache-2.0
#include "modes/RedactApplyDialog.h"
#include "util/GpTheme.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

namespace gp {

namespace {
QString plural(int n, const QString& one, const QString& many)
{
    return n == 1 ? one : many.arg(n);
}

// The destination must differ from the source (the old controller path saved
// IN PLACE over the original — U05 removes that flow). Case-insensitive on
// Windows, mirroring SecurityController::sanitizeDocument's canonical compare.
bool sameFile(const QString& a, const QString& b)
{
    if (a.isEmpty() || b.isEmpty()) return false;
    const QFileInfo ia(a);
    const QFileInfo ib(b);
    const QString ca = ia.canonicalFilePath().isEmpty() ? ia.absoluteFilePath() : ia.canonicalFilePath();
    const QString cb = ib.canonicalFilePath().isEmpty() ? ib.absoluteFilePath() : ib.canonicalFilePath();
    return QString::compare(ca, cb, Qt::CaseInsensitive) == 0;
}
} // namespace

// ── RedactApplyDialog ────────────────────────────────────────────────────────

RedactApplyDialog::RedactApplyDialog(const RedactApplyPlan& plan, QWidget* parent)
    : QDialog(parent), m_plan(plan)
{
    setObjectName(QStringLiteral("redactApplyDialog"));
    setWindowTitle(tr("Apply Redactions"));
    setModal(true);
    setMinimumWidth(520);
    buildUi();
    refreshState();
}

void RedactApplyDialog::buildUi()
{
    auto* col = new QVBoxLayout(this);
    col->setSpacing(10);

    // ── Summary: document + marks, "marked for removal" wording ────────────
    auto* summaryFrame = new QFrame(this);
    summaryFrame->setProperty("role", "redactSummary");
    auto* scol = new QVBoxLayout(summaryFrame);
    scol->setContentsMargins(12, 10, 12, 10);
    scol->setSpacing(6);

    const QString docName = QFileInfo(m_plan.sourcePath).fileName();
    m_docLabel = new QLabel(
        tr("Document: %1%2").arg(docName,
            m_plan.sourcePageCount > 0
                ? tr(" \xe2\x80\x94 %1").arg(plural(m_plan.sourcePageCount, tr("%1 page"), tr("%1 pages")))
                : QString()),
        this);
    scol->addWidget(m_docLabel);

    m_summaryLabel = new QLabel(this);
    m_summaryLabel->setObjectName(QStringLiteral("redactApplySummaryLabel"));
    m_summaryLabel->setWordWrap(true);
    scol->addWidget(m_summaryLabel);

    if (!m_plan.marksPerPage.isEmpty()) {
        QStringList perPage;
        for (auto it = m_plan.marksPerPage.constBegin(); it != m_plan.marksPerPage.constEnd(); ++it)
            perPage << tr("Page %1: %2").arg(it.key() + 1).arg(it.value());
        m_marksDetailLabel = new QLabel(perPage.join(tr(", ")), this);
        m_marksDetailLabel->setWordWrap(true);
        m_marksDetailLabel->setStyleSheet(QString("color:%1;").arg(gp::Theme::fg2().name()));
        scol->addWidget(m_marksDetailLabel);
    }

    // The wording contract: canvas marks are "marked for removal"; the saved
    // output is only "applied" once the operation commits.
    auto* wording = new QLabel(
        tr("The black boxes on the canvas are marks for removal. Nothing is changed until "
           "you press Apply Redactions; the content is excised only in the saved output, "
           "and the original file is never modified."), this);
    wording->setWordWrap(true);
    wording->setStyleSheet(QString("color:%1;").arg(gp::Theme::fg2().name()));
    scol->addWidget(wording);
    col->addWidget(summaryFrame);

    // ── Destinations ────────────────────────────────────────────────────────
    auto makeDestRow = [this, &col](const QString& caption, QLineEdit** edit, QPushButton** browse) {
        auto* row = new QHBoxLayout;
        auto* lab = new QLabel(caption, this);
        lab->setMinimumWidth(150);
        row->addWidget(lab);
        *edit = new QLineEdit(this);
        row->addWidget(*edit, 1);
        *browse = new QPushButton(tr("Browse\xe2\x80\xa6"), this);
        row->addWidget(*browse);
        col->addLayout(row);
    };
    QPushButton* destBrowse = nullptr;
    QPushButton* sanitizedBrowse = nullptr;
    makeDestRow(tr("Redacted output:"), &m_destinationEdit, &destBrowse);
    m_destinationEdit->setObjectName(QStringLiteral("redactApplyDestinationEdit"));
    // Prefill the caller's defaults (<base>_redacted.pdf / <base>_redacted_sanitized.pdf)
    // — the picker must offer the real default destinations, not empty edits.
    m_destinationEdit->setText(m_plan.destinationPath);

    makeDestRow(tr("Sanitized copy:"), &m_sanitizedDestinationEdit, &sanitizedBrowse);
    m_sanitizedDestinationEdit->setObjectName(QStringLiteral("redactApplySanitizedDestinationEdit"));
    m_sanitizedDestinationEdit->setText(m_plan.sanitizedDestinationPath);

    connect(destBrowse, &QPushButton::clicked, this, &RedactApplyDialog::browseDestination);
    connect(sanitizedBrowse, &QPushButton::clicked, this, &RedactApplyDialog::browseSanitizedDestination);

    m_sanitizeCheck = new QCheckBox(
        tr("Also produce a fully sanitized copy (metadata, attachments, JavaScript)"), this);
    m_sanitizeCheck->setObjectName(QStringLiteral("redactApplySanitizeCheck"));
    // The mode panel's sanitize choice is the dialog's starting state — the
    // dialog's own checkbox is a separate widget and starts unchecked unless
    // initialized here (U05: a silently unchecked box produced "Completed"
    // runs with no sanitized copy).
    m_sanitizeCheck->setChecked(m_plan.sanitize);
    col->addWidget(m_sanitizeCheck);

    // ── §9.8 P1: optional overlay text printed on the burn-in boxes ────────
    // Legal/FOIA reviewers expect the WHY on the box itself. Empty edit =
    // plain black boxes (the previous behavior, unchanged).
    auto* overlayRow = new QHBoxLayout;
    auto* overlayLab = new QLabel(tr("Overlay text:"), this);
    overlayLab->setMinimumWidth(150);
    overlayRow->addWidget(overlayLab);
    m_overlayEdit = new QLineEdit(this);
    m_overlayEdit->setObjectName(QStringLiteral("redactApplyOverlayEdit"));
    m_overlayEdit->setPlaceholderText(
        tr("Optional — e.g. a reason code, printed on each black box"));
    m_overlayEdit->setToolTip(tr(
        "Printed centered in white (7pt) on every redaction box in the saved "
        "output. Boxes too small to fit the text stay plain. Leave empty for "
        "black boxes with no label."));
    overlayRow->addWidget(m_overlayEdit, 1);
    col->addLayout(overlayRow);

    // ── Validation feedback + buttons ──────────────────────────────────────
    m_warningLabel = new QLabel(this);
    m_warningLabel->setObjectName(QStringLiteral("redactApplyWarningLabel"));
    m_warningLabel->setWordWrap(true);
    m_warningLabel->setStyleSheet(QStringLiteral("color:#ef4444;"));
    col->addWidget(m_warningLabel);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_okButton = buttons->button(QDialogButtonBox::Ok);
    m_okButton->setObjectName(QStringLiteral("redactApplyOkButton"));
    m_okButton->setText(tr("Apply Redactions"));
    m_okButton->setProperty("variant", "danger");
    m_okButton->setDefault(true);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    col->addWidget(buttons);

    // ── Wiring ──────────────────────────────────────────────────────────────
    connect(m_sanitizeCheck, &QCheckBox::toggled, this, &RedactApplyDialog::refreshState);
    // Editing either destination re-validates; Browse dialogs already confirm
    // overwrites natively.
    connect(m_destinationEdit, &QLineEdit::textChanged, this, &RedactApplyDialog::refreshState);
    connect(m_sanitizedDestinationEdit, &QLineEdit::textChanged, this, &RedactApplyDialog::refreshState);
}

QString RedactApplyDialog::pickPdfFile(const QString& currentPath)
{
    return QFileDialog::getSaveFileName(this, tr("Choose Output File"),
        currentPath.isEmpty() ? QFileInfo(m_plan.sourcePath).absolutePath() : currentPath,
        tr("PDF Files (*.pdf)"));
}

void RedactApplyDialog::browseDestination()
{
    const QString picked = pickPdfFile(m_destinationEdit->text());
    if (!picked.isEmpty()) m_destinationEdit->setText(picked); // QFileDialog already
                                                               // confirms overwrites
}

void RedactApplyDialog::browseSanitizedDestination()
{
    const QString picked = pickPdfFile(m_sanitizedDestinationEdit->text());
    if (!picked.isEmpty()) m_sanitizedDestinationEdit->setText(picked);
}

void RedactApplyDialog::refreshState()
{
    // Summary line — the "N marks on M pages" contract.
    const int pageCount = m_plan.marksPerPage.size();
    m_summaryLabel->setText(
        tr("%1, marked for removal.").arg(
            QStringLiteral("%1 on %2")
                .arg(plural(m_plan.markCount, tr("%1 mark"), tr("%1 marks")))
                .arg(plural(pageCount, tr("%1 page"), tr("%1 pages")))));

    // Plan validation: OK is disabled with a reason while invalid.
    QStringList problems;
    const QString dest = m_destinationEdit->text().trimmed();
    if (dest.isEmpty()) problems << tr("Choose a destination for the redacted output.");
    if (sameFile(dest, m_plan.sourcePath))
        problems << tr("The destination must not be the original document.");
    if (m_sanitizeCheck->isChecked()) {
        const QString sdest = m_sanitizedDestinationEdit->text().trimmed();
        if (sdest.isEmpty()) problems << tr("Choose a destination for the sanitized copy.");
        if (sameFile(sdest, m_plan.sourcePath))
            problems << tr("The sanitized copy must not overwrite the original document.");
        if (sameFile(sdest, dest))
            problems << tr("The sanitized copy must be a separate file from the redacted output.");
    }
    m_sanitizedDestinationEdit->setEnabled(m_sanitizeCheck->isChecked());
    m_warningLabel->setText(problems.join(QLatin1Char('\n')));
    m_okButton->setEnabled(problems.isEmpty());
}

void RedactApplyDialog::setDestinationPath(const QString& path) { m_destinationEdit->setText(path); }
void RedactApplyDialog::setSanitizedDestinationPath(const QString& path) { m_sanitizedDestinationEdit->setText(path); }
void RedactApplyDialog::setSanitizeChecked(bool on) { m_sanitizeCheck->setChecked(on); }

// §9.8 P1: the overlay text is optional and never validated — any non-empty
// value is carried onto the boxes; whitespace-only collapses to empty (no
// overlay) in the operation itself.
void RedactApplyDialog::setOverlayText(const QString& text)
{
    if (m_overlayEdit) m_overlayEdit->setText(text);
}

QString RedactApplyDialog::summaryText() const
{
    if (!m_summaryLabel) return QString();
    QString text = m_summaryLabel->text();
    if (m_docLabel) text += QLatin1Char('\n') + m_docLabel->text();
    return text;
}

RedactApplyPlan RedactApplyDialog::plan() const
{
    RedactApplyPlan p = m_plan;
    p.destinationPath = m_destinationEdit->text().trimmed();
    p.sanitizedDestinationPath = m_sanitizedDestinationEdit->text().trimmed();
    p.sanitize = m_sanitizeCheck->isChecked();
    p.overlayText = m_overlayEdit ? m_overlayEdit->text().trimmed() : QString();
    return p;
}

// ── RedactResultPresenter ────────────────────────────────────────────────────

namespace RedactResultPresenter {

QString bannerText(const RedactResult& result)
{
    switch (result.outcome) {
    case RedactOutcome::Completed: {
        QString text = QObject::tr("Redactions applied to saved output: %1")
                           .arg(QFileInfo(result.destination).fileName());
        if (!result.sanitizedDestination.isEmpty())
            text += QObject::tr("; sanitized copy: %1").arg(QFileInfo(result.sanitizedDestination).fileName());
        return text;
    }
    case RedactOutcome::PartialRedactedOnly:
        return QObject::tr("Redacted copy saved; sanitization FAILED \xe2\x80\x94 see the redaction report.");
    case RedactOutcome::Failed:
        return QObject::tr("Redaction failed \xe2\x80\x94 the original file was not modified.");
    case RedactOutcome::Canceled:
        return QObject::tr("Redaction canceled \xe2\x80\x94 no output was written; marks preserved.");
    }
    return QString();
}

QString detailText(const RedactResult& result)
{
    switch (result.outcome) {
    case RedactOutcome::Completed: {
        QString text = QObject::tr("The redacted copy has been saved to:\n%1").arg(result.destination);
        if (!result.sanitizedDestination.isEmpty())
            text += QObject::tr("\n\nA fully sanitized copy (metadata, attachments, JavaScript removed) "
                       "has been saved to:\n%1").arg(result.sanitizedDestination);
        return text;
    }
    case RedactOutcome::PartialRedactedOnly:
        return QObject::tr("Redacted copy saved to:\n%1\n\n"
                  "Sanitization FAILED: %2\n\n"
                  "The saved file still contains document metadata, attachments and embedded "
                  "JavaScript. Keep the redacted file, retry sanitization, or discard the "
                  "output.").arg(result.destination, result.error);
    case RedactOutcome::Failed:
        return QObject::tr("Redaction failed. The original file was not modified and no output was "
                  "written.\n\n%1\n\nStage: %2").arg(result.error, result.failedStage);
    case RedactOutcome::Canceled:
        return QObject::tr("Redaction canceled. No output was written and the original file was not "
                  "modified. Your redaction marks are preserved.");
    }
    return QString();
}

MarkDecision present(QWidget* parent, const RedactResult& result)
{
    switch (result.outcome) {
    case RedactOutcome::Completed:
        QMessageBox::information(parent, QObject::tr("Redaction Complete"), detailText(result));
        return MarkDecision::ClearMarks;

    case RedactOutcome::PartialRedactedOnly: {
        // Labeled partial-result dialog — never a generic success banner.
        QMessageBox box(parent);
        box.setIcon(QMessageBox::Warning);
        box.setWindowTitle(QObject::tr("Redaction Partially Complete \xe2\x80\x94 Sanitization Failed"));
        box.setText(detailText(result));
        QPushButton* retry  = box.addButton(QObject::tr("Retry Sanitize"), QMessageBox::ActionRole);
        QPushButton* keep   = box.addButton(QObject::tr("Keep Redacted File"), QMessageBox::AcceptRole);
        QPushButton* discard = box.addButton(QObject::tr("Discard Output"), QMessageBox::DestructiveRole);
        Q_UNUSED(keep);
        box.setDefaultButton(retry);
        box.exec();

        if (box.clickedButton() == retry) {
            QString err;
            if (RedactOperation::sanitizeCommittedFile(result.destination,
                                                       result.sanitizedDestination, &err)) {
                QMessageBox::information(parent, QObject::tr("Sanitization Complete"),
                    QObject::tr("The sanitized copy has been saved to:\n%1")
                        .arg(result.sanitizedDestination));
                return MarkDecision::ClearMarks;
            }
            QMessageBox::warning(parent, QObject::tr("Sanitize Failed"),
                QObject::tr("Sanitization failed again:\n%1\n\nThe redacted file remains at:\n%2")
                    .arg(err, result.destination));
            return MarkDecision::ClearMarks; // redacted artifact kept; marks' effect is saved
        }
        if (box.clickedButton() == discard) {
            // The redacted file is the only redacted copy — delete only after
            // an explicit confirmation, and never touch the source.
            const int answer = QMessageBox::warning(
                parent, QObject::tr("Discard Redacted Output"),
                QObject::tr("Delete the redacted copy\n%1\n\nThe original document will not be "
                   "modified, and your redaction marks are kept so you can retry.")
                    .arg(result.destination),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (answer == QMessageBox::Yes)
                QFile::remove(result.destination);
            return MarkDecision::RetainMarks;
        }
        return MarkDecision::ClearMarks; // Keep Redacted File
    }

    case RedactOutcome::Failed:
        QMessageBox::critical(parent, QObject::tr("Redaction Failed"), detailText(result));
        return MarkDecision::RetainMarks;

    case RedactOutcome::Canceled:
        QMessageBox::information(parent, QObject::tr("Redaction Canceled"), detailText(result));
        return MarkDecision::RetainMarks;
    }
    return MarkDecision::RetainMarks;
}

} // namespace RedactResultPresenter
} // namespace gp
