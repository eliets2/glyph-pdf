#include "ui/DocumentPropertiesWidget.h"

#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QFileInfo>
#include <QDateTime>

static QLabel* makeValueLabel(QWidget *parent)
{
    auto *l = new QLabel(parent);
    l->setTextInteractionFlags(Qt::TextSelectableByMouse);
    l->setWordWrap(true);
    return l;
}

DocumentPropertiesWidget::DocumentPropertiesWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(10);

    auto *fileGroup = new QGroupBox(tr("File Information"), this);
    auto *fileForm = new QFormLayout(fileGroup);
    m_filePath = makeValueLabel(this);
    m_fileSize = makeValueLabel(this);
    m_pageCount = makeValueLabel(this);
    fileForm->addRow(tr("File Path:"), m_filePath);
    fileForm->addRow(tr("File Size:"), m_fileSize);
    fileForm->addRow(tr("Page Count:"), m_pageCount);

    auto *metaGroup = new QGroupBox(tr("Document Metadata"), this);
    auto *metaForm = new QFormLayout(metaGroup);
    m_title = makeValueLabel(this);
    m_author = makeValueLabel(this);
    m_subject = makeValueLabel(this);
    m_keywords = makeValueLabel(this);
    m_creator = makeValueLabel(this);
    m_producer = makeValueLabel(this);
    m_creationDate = makeValueLabel(this);
    m_modDate = makeValueLabel(this);

    metaForm->addRow(tr("Title:"), m_title);
    metaForm->addRow(tr("Author:"), m_author);
    metaForm->addRow(tr("Subject:"), m_subject);
    metaForm->addRow(tr("Keywords:"), m_keywords);
    metaForm->addRow(tr("Creator:"), m_creator);
    metaForm->addRow(tr("Producer:"), m_producer);
    metaForm->addRow(tr("Creation Date:"), m_creationDate);
    metaForm->addRow(tr("Modification Date:"), m_modDate);

    layout->addWidget(fileGroup);
    layout->addWidget(metaGroup);
    layout->addStretch(1);

    clearUi();
}

void DocumentPropertiesWidget::clearUi()
{
    const auto dash = tr("—");
    m_filePath->setText(dash);
    m_fileSize->setText(dash);
    m_pageCount->setText(dash);
    m_title->setText(dash);
    m_author->setText(dash);
    m_subject->setText(dash);
    m_keywords->setText(dash);
    m_creator->setText(dash);
    m_producer->setText(dash);
    m_creationDate->setText(dash);
    m_modDate->setText(dash);
}

void DocumentPropertiesWidget::setDocument(QPdfDocument *document, const QString &filePath)
{
    if (!document || filePath.isEmpty()) {
        clearUi();
        return;
    }
    populate(document, filePath);
}

void DocumentPropertiesWidget::populate(QPdfDocument *document, const QString &filePath)
{
    QFileInfo fi(filePath);
    m_filePath->setText(fi.absoluteFilePath());

    const qint64 sizeBytes = fi.exists() ? fi.size() : 0;
    if (sizeBytes > 1048576)
        m_fileSize->setText(QString("%1 MB").arg(sizeBytes / 1048576.0, 0, 'f', 2));
    else if (sizeBytes > 1024)
        m_fileSize->setText(QString("%1 KB").arg(sizeBytes / 1024.0, 0, 'f', 1));
    else
        m_fileSize->setText(fi.exists() ? QString("%1 bytes").arg(sizeBytes) : tr("—"));

    m_pageCount->setText(QString::number(document->pageCount()));

    auto md = [&](QPdfDocument::MetaDataField f) {
        const auto v = document->metaData(f).toString();
        return v.isEmpty() ? tr("—") : v;
    };

    m_title->setText(md(QPdfDocument::MetaDataField::Title));
    m_author->setText(md(QPdfDocument::MetaDataField::Author));
    m_subject->setText(md(QPdfDocument::MetaDataField::Subject));
    m_keywords->setText(md(QPdfDocument::MetaDataField::Keywords));
    m_creator->setText(md(QPdfDocument::MetaDataField::Creator));
    m_producer->setText(md(QPdfDocument::MetaDataField::Producer));

    const QDateTime creationDate = document->metaData(QPdfDocument::MetaDataField::CreationDate).toDateTime();
    m_creationDate->setText(creationDate.isValid() ? creationDate.toString("yyyy-MM-dd hh:mm:ss") : tr("—"));

    const QDateTime modDate = document->metaData(QPdfDocument::MetaDataField::ModificationDate).toDateTime();
    m_modDate->setText(modDate.isValid() ? modDate.toString("yyyy-MM-dd hh:mm:ss") : tr("—"));
}

