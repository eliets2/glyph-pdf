#include "ui/FindBar.h"
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QHBoxLayout>
#include <QCheckBox>
#include <QKeyEvent>
#include <QShowEvent>

FindBar::FindBar(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("findBar");
    setAccessibleName(tr("Find bar"));
    setAccessibleDescription(tr("Searches the current document and can redact all matches"));

    searchInput = new QLineEdit(this);
    searchInput->setPlaceholderText(tr("Find in document..."));
    searchInput->setClearButtonEnabled(true);
    searchInput->setAccessibleName(tr("Find text"));
    searchInput->setAccessibleDescription(tr("Enter text to find in the current document"));

    btnPrev = new QPushButton(tr("Previous"), this);
    btnNext = new QPushButton(tr("Next"), this);
    btnRedactAll = new QPushButton(tr("Redact All"), this);
    btnRedactAll->setObjectName("destructiveButton");
    btnRedactAll->setStyleSheet("background-color: #800; color: white;");
    btnClose = new QPushButton(tr("Close"), this);
    btnPrev->setAccessibleName(tr("Find previous"));
    btnNext->setAccessibleName(tr("Find next"));
    btnRedactAll->setAccessibleName(tr("Redact all matches"));
    btnRedactAll->setAccessibleDescription(tr("Permanently redacts every match for the entered search text"));
    btnClose->setAccessibleName(tr("Close find bar"));
    
    caseCheckBox = new QCheckBox(tr("Match Case"), this);
    wordsCheckBox = new QCheckBox(tr("Whole Words"), this);
    caseCheckBox->setAccessibleDescription(tr("Search with case-sensitive matching"));
    wordsCheckBox->setAccessibleDescription(tr("Search complete words only"));
    
    resultLabel = new QLabel(this);
    resultLabel->setObjectName("findResultLabel");
    resultLabel->setAccessibleName(tr("Find result status"));

    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->addWidget(searchInput);
    layout->addWidget(caseCheckBox);
    layout->addWidget(wordsCheckBox);
    layout->addWidget(btnPrev);
    layout->addWidget(btnNext);
    layout->addWidget(btnRedactAll);
    layout->addWidget(resultLabel);
    layout->addWidget(btnClose);

    connect(btnNext, &QPushButton::clicked, [this]() {
        emit searchRequested(searchInput->text(), true, caseCheckBox->isChecked(), wordsCheckBox->isChecked());
    });
    connect(btnPrev, &QPushButton::clicked, [this]() {
        emit searchRequested(searchInput->text(), false, caseCheckBox->isChecked(), wordsCheckBox->isChecked());
    });
    connect(btnRedactAll, &QPushButton::clicked, [this]() {
        const QString text = searchInput->text().trimmed();
        if (!text.isEmpty())
            emit redactAllRequested(text, caseCheckBox->isChecked(), wordsCheckBox->isChecked());
    });
    connect(searchInput, &QLineEdit::returnPressed, btnNext, &QPushButton::click);
    connect(searchInput, &QLineEdit::textChanged, this, &FindBar::updateButtonStates);
    connect(btnClose, &QPushButton::clicked, this, &FindBar::closeRequested);
    updateButtonStates();
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
    btnRedactAll->setEnabled(hasText);
}
