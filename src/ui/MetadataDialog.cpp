#include "ui/MetadataDialog.h"
#include <QFormLayout>
#include <QLineEdit>
#include <QDialogButtonBox>
#include <QVBoxLayout>

MetadataDialog::MetadataDialog(const PdfMetadata &metadata, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Edit XMP Metadata"));
    resize(400, 250);

    titleEdit = new QLineEdit(metadata.title, this);
    authorEdit = new QLineEdit(metadata.author, this);
    subjectEdit = new QLineEdit(metadata.subject, this);
    keywordsEdit = new QLineEdit(metadata.keywords, this);
    creatorEdit = new QLineEdit(metadata.creator, this);
    producerEdit = new QLineEdit(metadata.producer, this);

    QFormLayout *form = new QFormLayout;
    form->addRow(tr("Title:"), titleEdit);
    form->addRow(tr("Author:"), authorEdit);
    form->addRow(tr("Subject:"), subjectEdit);
    form->addRow(tr("Keywords (comma separated):"), keywordsEdit);
    form->addRow(tr("Creator:"), creatorEdit);
    form->addRow(tr("Producer:"), producerEdit);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(form);
    mainLayout->addWidget(buttonBox);
}

PdfMetadata MetadataDialog::metadata() const
{
    PdfMetadata meta;
    meta.title = titleEdit->text();
    meta.author = authorEdit->text();
    meta.subject = subjectEdit->text();
    meta.keywords = keywordsEdit->text();
    meta.creator = creatorEdit->text();
    meta.producer = producerEdit->text();
    return meta;
}
