// SPDX-License-Identifier: Apache-2.0
#include "SanitizeSummaryDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

SanitizeSummaryDialog::SanitizeSummaryDialog(const SanitizePlan &plan, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Sanitize Document — Review What Will Be Removed"));
    setModal(true);
    setMinimumWidth(560);

    auto *lay = new QVBoxLayout(this);
    auto *intro = new QLabel(
        tr("GlyphPDF found hidden data that a sanitized copy should not carry. "
           "Checked categories are removed from the saved copy; unchecked "
           "categories are kept exactly as they are."), this);
    intro->setWordWrap(true);
    lay->addWidget(intro);

    for (const auto &c : plan.categories) {
        if (c.count <= 0) continue;
        QString label = QStringLiteral("%1 — %2 item%3")
                            .arg(sanitizeCategoryLabel(c.category),
                                 QString::number(c.count),
                                 c.count == 1 ? QString() : QStringLiteral("s"));
        if (!c.items.isEmpty())
            label += QStringLiteral(": ") + c.items.join(QStringLiteral(", "));
        auto *box = new QCheckBox(label, this);
        box->setChecked(true);
        connect(box, &QCheckBox::toggled, this,
                &SanitizeSummaryDialog::refreshCommitControls);
        lay->addWidget(box);
        m_boxes.insert(int(c.category), box);
        m_counts.insert(int(c.category), c.count);
    }

    m_remainingLabel = new QLabel(this);
    m_remainingLabel->setWordWrap(true);
    lay->addWidget(m_remainingLabel);

    auto *note = new QLabel(
        tr("The saved copy is always rewritten and its file identity (/ID) is "
           "refreshed — that part cannot be disabled. Removal happens only in "
           "the saved copy; the open document is not modified."), this);
    note->setWordWrap(true);
    QFont small = note->font();
    small.setPointSize(qMax(8, small.pointSize() - 1));
    note->setFont(small);
    lay->addWidget(note);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                                         this);
    m_commitButton = buttons->button(QDialogButtonBox::Ok);
    connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
        // Honest refusal: a zero-checked commit removes nothing, so it is not
        // a sanitize. Say so plainly and keep the dialog open — never silently
        // no-op, never silently close.
        QString reason;
        if (!canCommit(&reason)) {
            QMessageBox::warning(this, tr("Sanitize Document"), reason);
            return;
        }
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    lay->addWidget(buttons);

    refreshCommitControls();
}

SanitizeCategories SanitizeSummaryDialog::selectedCategories() const
{
    SanitizeCategories selected;
    for (auto it = m_boxes.constBegin(); it != m_boxes.constEnd(); ++it)
        if (it.value()->isChecked())
            selected |= SanitizeCategory(it.key());
    return selected;
}

bool SanitizeSummaryDialog::canCommit(QString *refusalReason) const
{
    if (m_boxes.isEmpty()) {
        // The classify walk found nothing — a commit would only rewrite the
        // file while removing nothing.
        if (refusalReason)
            *refusalReason = tr("Nothing removable was found in this document, "
                                "so there is nothing to sanitize. The document "
                                "is left unchanged.");
        return false;
    }
    if (!selectedCategories()) {
        if (refusalReason)
            *refusalReason = tr("No category is checked — nothing would be removed, "
                                "so this commit would change nothing. Check at least "
                                "one category to remove, or Cancel to keep the "
                                "document unchanged.");
        return false;
    }
    return true;
}

void SanitizeSummaryDialog::refreshCommitControls()
{
    int removedItems = 0;
    int removedCategories = 0;
    QStringList kept;
    for (auto it = m_boxes.constBegin(); it != m_boxes.constEnd(); ++it) {
        const int count = m_counts.value(it.key());
        const QString label = sanitizeCategoryLabel(SanitizeCategory(it.key()));
        if (it.value()->isChecked()) {
            ++removedCategories;
            removedItems += count;
        } else {
            kept << QStringLiteral("%1 (%2)").arg(label, QString::number(count));
        }
    }

    if (m_commitButton) {
        m_commitButton->setText(removedItems > 0
            ? tr("Remove %n item(s) and Save…", "", removedItems)
            : tr("Remove nothing and Save…"));
    }

    if (m_remainingLabel) {
        if (m_boxes.isEmpty())
            m_remainingLabel->setText(tr("Nothing was found to remove."));
        else if (kept.isEmpty())
            m_remainingLabel->setText(
                tr("Every found category (%1 across %2 categories) will be removed.")
                    .arg(QString::number(removedItems),
                         QString::number(removedCategories)));
        else
            m_remainingLabel->setText(
                tr("Will remain unchanged: %1.").arg(kept.join(QStringLiteral(", "))));
    }
}
