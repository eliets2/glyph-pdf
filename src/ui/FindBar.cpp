// SPDX-License-Identifier: Apache-2.0
#include "ui/FindBar.h"
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QCheckBox>
#include <QComboBox>
#include <QKeyEvent>
#include <QShowEvent>

FindBar::FindBar(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("findBar");
    setAccessibleName(tr("Find bar"));
    setAccessibleDescription(tr("Searches the current document and can replace or redact matches"));

    // --- Search row ---
    searchInput = new QLineEdit(this);
    searchInput->setPlaceholderText(tr("Find in document..."));
    searchInput->setClearButtonEnabled(true);
    searchInput->setAccessibleName(tr("Find text"));

    btnPrev = new QPushButton(tr("Prev"), this);
    btnNext = new QPushButton(tr("Next"), this);
    btnPrev->setAccessibleName(tr("Find previous"));
    btnNext->setAccessibleName(tr("Find next"));

    caseCheckBox = new QCheckBox(tr("Aa"), this);
    caseCheckBox->setToolTip(tr("Match Case"));
    wordsCheckBox = new QCheckBox(tr("W"), this);
    wordsCheckBox->setToolTip(tr("Whole Words"));
    regexCheckBox = new QCheckBox(tr(".*"), this);
    regexCheckBox->setToolTip(tr("Regular Expression"));

    scopeCombo = new QComboBox(this);
    scopeCombo->addItem(tr("Document Text"), ScopeDocumentText);
    scopeCombo->addItem(tr("Comments"), ScopeComments);
    scopeCombo->addItem(tr("Bookmarks"), ScopeBookmarks);
    scopeCombo->addItem(tr("All"), ScopeAll);
    scopeCombo->setToolTip(tr("Search Scope"));
    scopeCombo->setAccessibleName(tr("Search scope"));

    resultLabel = new QLabel(this);
    resultLabel->setObjectName("findResultLabel");
    resultLabel->setAccessibleName(tr("Find result status"));

    btnClose = new QPushButton(tr("×"), this);
    btnClose->setFixedWidth(28);
    btnClose->setAccessibleName(tr("Close find bar"));

    auto *searchRow = new QHBoxLayout;
    searchRow->setContentsMargins(0, 0, 0, 0);
    searchRow->setSpacing(2);
    searchRow->addWidget(searchInput, 1);
    searchRow->addWidget(caseCheckBox);
    searchRow->addWidget(wordsCheckBox);
    searchRow->addWidget(regexCheckBox);
    searchRow->addWidget(scopeCombo);
    searchRow->addWidget(btnPrev);
    searchRow->addWidget(btnNext);
    searchRow->addWidget(resultLabel);
    searchRow->addWidget(btnClose);

    // --- Replace row ---
    replaceInput = new QLineEdit(this);
    replaceInput->setPlaceholderText(tr("Replace with..."));
    replaceInput->setClearButtonEnabled(true);
    replaceInput->setAccessibleName(tr("Replacement text"));

    btnReplace = new QPushButton(tr("Replace"), this);
    btnReplaceAll = new QPushButton(tr("Replace All"), this);
    btnRedactAll = new QPushButton(tr("Redact All"), this);
    btnRedactAll->setObjectName("destructiveButton");
    btnRedactAll->setStyleSheet("background-color: #800; color: white;");
    btnReplace->setAccessibleName(tr("Replace current match"));
    btnReplaceAll->setAccessibleName(tr("Replace all matches"));
    btnRedactAll->setAccessibleName(tr("Redact all matches"));

    auto *replaceRow = new QHBoxLayout;
    replaceRow->setContentsMargins(0, 0, 0, 0);
    replaceRow->setSpacing(2);
    replaceRow->addWidget(replaceInput, 1);
    replaceRow->addWidget(btnReplace);
    replaceRow->addWidget(btnReplaceAll);
    replaceRow->addWidget(btnRedactAll);
    replaceRow->addStretch();

    auto *col = new QVBoxLayout(this);
    col->setContentsMargins(4, 2, 4, 2);
    col->setSpacing(2);
    col->addLayout(searchRow);
    col->addLayout(replaceRow);

    // --- Connections ---
    connect(btnNext, &QPushButton::clicked, this, &FindBar::onNext);
    connect(btnPrev, &QPushButton::clicked, this, &FindBar::onPrev);
    connect(searchInput, &QLineEdit::returnPressed, this, &FindBar::onNext);
    connect(searchInput, &QLineEdit::textChanged, this, &FindBar::updateButtonStates);
    connect(btnClose, &QPushButton::clicked, this, &FindBar::closeRequested);

    connect(btnReplace, &QPushButton::clicked, [this]() {
        if (searchInput->text().trimmed().isEmpty()) return;
        emit replaceRequested(searchInput->text(), replaceInput->text(),
                              caseCheckBox->isChecked(), wordsCheckBox->isChecked(),
                              regexCheckBox->isChecked());
    });
    connect(btnReplaceAll, &QPushButton::clicked, [this]() {
        if (searchInput->text().trimmed().isEmpty()) return;
        emit replaceAllRequested(searchInput->text(), replaceInput->text(),
                                 caseCheckBox->isChecked(), wordsCheckBox->isChecked(),
                                 regexCheckBox->isChecked());
    });
    connect(btnRedactAll, &QPushButton::clicked, [this]() {
        const QString text = searchInput->text().trimmed();
        if (!text.isEmpty())
            emit redactAllRequested(text, caseCheckBox->isChecked(), wordsCheckBox->isChecked());
    });

    connect(regexCheckBox, &QCheckBox::toggled, [this](bool checked) {
        wordsCheckBox->setEnabled(!checked);
        if (checked) wordsCheckBox->setChecked(false);
    });

    updateButtonStates();
}

void FindBar::setMatchCount(int current, int total)
{
    if (total == 0)
        resultLabel->setText(searchInput->text().isEmpty() ? QString() : tr("No matches"));
    else
        resultLabel->setText(tr("%1 of %2").arg(current).arg(total));
}

void FindBar::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    searchInput->setFocus(Qt::ShortcutFocusReason);
    searchInput->selectAll();
}

void FindBar::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        emit closeRequested();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

void FindBar::updateButtonStates()
{
    const bool hasText = !searchInput->text().trimmed().isEmpty();
    btnPrev->setEnabled(hasText);
    btnNext->setEnabled(hasText);
    btnReplace->setEnabled(hasText);
    btnReplaceAll->setEnabled(hasText);
    btnRedactAll->setEnabled(hasText);
    if (!hasText) resultLabel->clear();
}

void FindBar::onNext()
{
    if (searchInput->text().trimmed().isEmpty()) return;
    emit searchRequested(searchInput->text(), true,
                         caseCheckBox->isChecked(), wordsCheckBox->isChecked(),
                         regexCheckBox->isChecked(), scopeCombo->currentData().toInt());
}

void FindBar::onPrev()
{
    if (searchInput->text().trimmed().isEmpty()) return;
    emit searchRequested(searchInput->text(), false,
                         caseCheckBox->isChecked(), wordsCheckBox->isChecked(),
                         regexCheckBox->isChecked(), scopeCombo->currentData().toInt());
}
