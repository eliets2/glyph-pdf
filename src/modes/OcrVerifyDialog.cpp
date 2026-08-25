// SPDX-License-Identifier: Apache-2.0
#include "OcrVerifyDialog.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QToolButton>
#include <QVBoxLayout>

namespace gp {

OcrVerifyDialog::OcrVerifyDialog(QWidget* parent) : QDialog(parent)
{
    // FineReader's Verify dialog is a floating tool window; the editor stays
    // visible and interactive behind it.
    setWindowFlag(Qt::Tool, true);
    setWindowTitle(tr("Verify Text"));
    setModal(false);

    auto* lay = new QVBoxLayout(this);

    // Magnified crop of the current word (B6 renderer feeds this).
    m_cropLabel = new QLabel;
    m_cropLabel->setObjectName("ocrVerifyCrop");
    m_cropLabel->setAlignment(Qt::AlignCenter);
    m_cropLabel->setMinimumSize(220, 90);
    m_cropLabel->setStyleSheet(
        "background:#e8e6df; border:1px solid #000; padding:8px;");
    lay->addWidget(m_cropLabel);

    // Why this word was flagged.
    m_reasonLabel = new QLabel;
    m_reasonLabel->setObjectName("ocrVerifyReason");
    lay->addWidget(m_reasonLabel);

    // Position in the queue ("word 3 of 12").
    m_posLabel = new QLabel;
    m_posLabel->setProperty("mono", true);
    lay->addWidget(m_posLabel);

    // Editable correction field.
    m_edit = new QLineEdit;
    m_edit->setObjectName("ocrVerifyEdit");
    lay->addWidget(m_edit);

    // B9: ranked correction candidates; picking one fills the edit field.
    m_suggestions = new QListWidget;
    m_suggestions->setObjectName("ocrVerifySuggestions");
    m_suggestions->setMaximumHeight(96);
    lay->addWidget(m_suggestions);
    connect(m_suggestions, &QListWidget::itemClicked, this, [this](QListWidgetItem *it) {
        if (it) m_edit->setText(it->text());
    });

    auto* btnRow = new QHBoxLayout;
    m_btnConfirm = new QToolButton;
    m_btnConfirm->setObjectName("ocrVerifyConfirm");
    m_btnConfirm->setText(tr("Confirm"));
    m_btnSkip = new QToolButton;
    m_btnSkip->setObjectName("ocrVerifySkip");
    m_btnSkip->setText(tr("Skip"));
    btnRow->addWidget(m_btnConfirm);
    btnRow->addWidget(m_btnSkip);

    // B10: teach the checker this word (stops future flagging).
    m_btnAddDict = new QToolButton;
    m_btnAddDict->setObjectName("ocrVerifyAddDict");
    m_btnAddDict->setText(tr("Add to Dictionary"));
    btnRow->addWidget(m_btnAddDict);

    // B4: session-wide skip / replace.
    m_btnSkipAll = new QToolButton;
    m_btnSkipAll->setObjectName("ocrVerifySkipAll");
    m_btnSkipAll->setText(tr("Skip All"));
    btnRow->addWidget(m_btnSkipAll);
    m_btnReplAll = new QToolButton;
    m_btnReplAll->setObjectName("ocrVerifyReplaceAll");
    m_btnReplAll->setText(tr("Replace All"));
    btnRow->addWidget(m_btnReplAll);

    btnRow->addStretch(1);
    lay->addLayout(btnRow);

    connect(m_btnConfirm, &QToolButton::clicked, this, &OcrVerifyDialog::onConfirm);
    connect(m_btnSkip,    &QToolButton::clicked, this, &OcrVerifyDialog::onSkip);
    connect(m_btnAddDict, &QToolButton::clicked,
            this, &OcrVerifyDialog::onAddToDictionary);
    connect(m_btnSkipAll, &QToolButton::clicked, this, &OcrVerifyDialog::onSkipAll);
    connect(m_btnReplAll, &QToolButton::clicked, this, &OcrVerifyDialog::onReplaceAll);
}

void OcrVerifyDialog::setItems(const QList<Item> &items)
{
    m_items = items;
    m_cursor = items.isEmpty() ? -1 : 0;
    showItem();
}

int OcrVerifyDialog::remaining() const
{
    if (m_cursor < 0) return 0;
    return m_items.size() - m_cursor;
}

void OcrVerifyDialog::showItem()
{
    if (m_cursor < 0 || m_cursor >= m_items.size()) {
        // Queue exhausted.
        m_cropLabel->clear();
        m_reasonLabel->setText(tr("No more uncertain words on this page."));
        m_posLabel->setText(tr("done"));
        m_edit->clear();
        m_edit->setEnabled(false);
        m_btnConfirm->setEnabled(false);
        m_btnSkip->setEnabled(false);
        m_btnAddDict->setEnabled(false);
        m_suggestions->clear();
        emit finished();
        return;
    }

    const Item &it = m_items.at(m_cursor);
    if (it.crop.isNull())
        m_cropLabel->setText(it.text.toHtmlEscaped());
    else
        m_cropLabel->setPixmap(QPixmap::fromImage(it.crop));

    m_reasonLabel->setText(
        tr("Low-confidence characters (%1%)").arg(it.confidence));
    m_posLabel->setText(tr("word %1 of %2")
                            .arg(m_cursor + 1).arg(m_items.size()));
    m_edit->setText(it.text);
    m_edit->setEnabled(true);
    m_btnConfirm->setEnabled(true);
    m_btnSkip->setEnabled(true);
    m_btnAddDict->setEnabled(true);

    // B9: show ranked suggestions for this word.
    m_suggestions->clear();
    for (const QString &s : it.suggestions)
        m_suggestions->addItem(s);

    m_edit->selectAll();
    m_edit->setFocus();
}

void OcrVerifyDialog::onConfirm()
{
    if (m_cursor < 0 || m_cursor >= m_items.size()) return;
    const Item &it = m_items.at(m_cursor);
    emit confirmRequested(it.wordIndex, m_edit->text());
    ++m_cursor;
    showItem();
}

void OcrVerifyDialog::onSkip()
{
    if (m_cursor < 0 || m_cursor >= m_items.size()) return;
    emit skipRequested(m_items.at(m_cursor).wordIndex);
    ++m_cursor;
    showItem();
}

void OcrVerifyDialog::onAddToDictionary()
{
    if (m_cursor < 0 || m_cursor >= m_items.size()) return;
    // The corrected spelling (edit field) is what becomes known.
    emit addToDictionaryRequested(m_edit->text().trimmed());
}

void OcrVerifyDialog::onSkipAll()
{
    if (m_cursor < 0 || m_cursor >= m_items.size()) return;
    const QString token = m_edit->text();
    emit skipAllRequested(token);
    // Every remaining occurrence of this token is now suppressed.
    while (m_cursor < m_items.size() &&
           m_items.at(m_cursor).text == token)
        ++m_cursor;
    showItem();
}

void OcrVerifyDialog::onReplaceAll()
{
    if (m_cursor < 0 || m_cursor >= m_items.size()) return;
    const QString from = m_items.at(m_cursor).text;
    const QString to   = m_edit->text();
    emit replaceAllRequested(from, to);
    // Remaining occurrences of `from` are all replaced in one go.
    while (m_cursor < m_items.size() &&
           m_items.at(m_cursor).text == from)
        ++m_cursor;
    showItem();
}

} // namespace gp
