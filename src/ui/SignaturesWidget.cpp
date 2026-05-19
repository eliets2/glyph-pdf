#include "ui/SignaturesWidget.h"
#include <QLabel>
#include <QVBoxLayout>
#include <QListWidget>
#include <QListWidgetItem>
#include <QDebug>

SignaturesWidget::SignaturesWidget(ISignatureManager *signMgr, QWidget *parent)
    : QWidget(parent), m_signManager(signMgr)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_emptyLabel = new QLabel(tr("No digital signatures detected."), this);
    m_emptyLabel->setWordWrap(true);
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setStyleSheet("color: #64748B; padding: 20px;");
    layout->addWidget(m_emptyLabel);

    m_listWidget = new QListWidget(this);
    m_listWidget->setStyleSheet(QString(
        "QListWidget { background: transparent; border: none; }"
        "QListWidget::item { border-bottom: 1px solid #333; padding: 10px; }"
        "QListWidget::item:selected { background: #2D2D2D; }"
    ));
    m_listWidget->hide();
    layout->addWidget(m_listWidget);
}

void SignaturesWidget::setDocumentFile(const QString &filePath)
{
    clear();

    if (filePath.isEmpty()) {
        m_emptyLabel->setText(tr("Open a document to view signatures."));
        m_emptyLabel->show();
        m_listWidget->hide();
        return;
    }

    QList<SignatureInfo> sigs = m_signManager->validateSignatures(filePath);

    if (sigs.isEmpty()) {
        m_emptyLabel->setText(tr("No digital signatures detected."));
        m_emptyLabel->show();
        m_listWidget->hide();
    } else {
        m_emptyLabel->hide();
        m_listWidget->show();

        for (const auto &info : sigs) {
            auto *item = new QListWidgetItem(m_listWidget);
            QWidget *widget = new QWidget();
            QVBoxLayout *itemLayout = new QVBoxLayout(widget);
            itemLayout->setContentsMargins(5, 5, 5, 5);
            itemLayout->setSpacing(2);

            QLabel *nameLabel = new QLabel(info.signerName.isEmpty() ? info.fieldName : info.signerName);
            nameLabel->setStyleSheet("font-weight: bold; color: white;");
            
            QLabel *statusLabel = new QLabel(tr("Status: %1").arg(info.trustStatus));
            if (info.isValid) {
                statusLabel->setStyleSheet("color: #10B981;"); // Green
            } else {
                statusLabel->setStyleSheet("color: #EF4444;"); // Red
            }

            QLabel *reasonLabel = new QLabel(info.reason);
            reasonLabel->setStyleSheet("font-size: 10px; color: #94A3B8;");

            itemLayout->addWidget(nameLabel);
            itemLayout->addWidget(statusLabel);
            if (!info.reason.isEmpty()) itemLayout->addWidget(reasonLabel);

            item->setSizeHint(widget->sizeHint());
            m_listWidget->setItemWidget(item, widget);
        }
    }
}

void SignaturesWidget::clear()
{
    m_listWidget->clear();
}
