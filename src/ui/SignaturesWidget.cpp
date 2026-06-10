// SPDX-License-Identifier: Apache-2.0
#include "ui/SignaturesWidget.h"
#include "util/GpTheme.h"
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
    // T-03: use fg2 token (dim secondary text) instead of hardcoded dark hex.
    m_emptyLabel->setStyleSheet(
        QString("color: %1; padding: 20px;").arg(gp::Theme::fg2().name()));
    layout->addWidget(m_emptyLabel);

    m_listWidget = new QListWidget(this);
    // T-03: replace hardcoded dark hex with GpTheme tokens for list item borders
    // and selection background so the widget works in Light and High-Contrast.
    m_listWidget->setStyleSheet(
        QString("QListWidget { background: transparent; border: none; }"
                "QListWidget::item { border-bottom: 1px solid %1; padding: 10px; }"
                "QListWidget::item:selected { background: %2; }")
            .arg(gp::Theme::bg3().name(), gp::Theme::bg2().name()));
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
            // T-03: use palette role for primary text rather than hardcoded "white".
            nameLabel->setStyleSheet("font-weight: bold;");

            QLabel *statusLabel = new QLabel(tr("Status: %1").arg(info.trustStatus));
            // T-03 / ER-5: three-way colour mapping so UntrustedChain is visually
            // distinct from both ValidWithDSS (green) and Invalid/Revoked/Forged (red).
            //   ValidWithDSS               → okGreen  (#4ec9b0)
            //   UntrustedChain             → warning   (#F59E0B, amber)
            //   Invalid / Revoked / Forged → danger    (#c8442b, red)
            if (info.trustStatus == QLatin1String("UntrustedChain") ||
                info.trustStatus == QLatin1String("ValidWithUnsignedChanges")) {
                statusLabel->setStyleSheet(
                    QString("color: %1;").arg(gp::Theme::warning().name()));
            } else if (info.isValid) {
                statusLabel->setStyleSheet(
                    QString("color: %1;").arg(gp::Theme::okGreen().name()));
            } else {
                statusLabel->setStyleSheet(
                    QString("color: %1;").arg(gp::Theme::danger().name()));
            }

            QLabel *reasonLabel = new QLabel(info.reason);
            // T-03: use fg2 token (dim secondary text).
            reasonLabel->setStyleSheet(
                QString("font-size: 10px; color: %1;").arg(gp::Theme::fg2().name()));

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
