#include "ui/RecoveryDialog.h"
#include <QListWidget>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QLabel>
#include <QFileInfo>
#include <QDateTime>
#include <QPushButton>

static QString formatSize(qint64 bytes)
{
    double size = bytes;
    QStringList list = {"B", "KB", "MB", "GB"};
    int index = 0;
    while (size >= 1024.0 && index < list.size() - 1) {
        size /= 1024.0;
        index++;
    }
    return QString::number(size, 'f', index == 0 ? 0 : 1) + " " + list[index];
}

static QString formatSizeDiff(qint64 diff)
{
    if (diff == 0) return "no size change";
    if (diff > 0) return "+" + formatSize(diff);
    return "-" + formatSize(-diff);
}

RecoveryDialog::RecoveryDialog(const QStringList &orphanedFiles, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Crash Recovery"));
    resize(600, 350);

    auto *layout = new QVBoxLayout(this);
    
    auto *titleLabel = new QLabel(tr("GlyphPDF detected unsaved changes from a previous session. Select the documents you want to recover:"), this);
    titleLabel->setWordWrap(true);
    layout->addWidget(titleLabel);

    m_listWidget = new QListWidget(this);
    for (const auto &file : orphanedFiles) {
        QString autosavePath = file + ".autosave.pdf";
        QFileInfo origInfo(file);
        QFileInfo autoInfo(autosavePath);

        QString displayName = origInfo.fileName();
        if (displayName.isEmpty()) displayName = file;

        QString timeStr = autoInfo.lastModified().toString("yyyy-MM-dd hh:mm:ss");
        qint64 sizeDiff = autoInfo.size() - origInfo.size();
        QString diffStr = formatSizeDiff(sizeDiff);

        QString labelText = QString("%1\n   Path: %2\n   Autosaved: %3 · %4")
                                .arg(displayName, file, timeStr, diffStr);

        auto *item = new QListWidgetItem(labelText, m_listWidget);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Checked);
        item->setData(Qt::UserRole, file);
    }
    layout->addWidget(m_listWidget);

    auto *btnBox = new QDialogButtonBox(this);
    auto *btnRecover = btnBox->addButton(tr("Recover Selected"), QDialogButtonBox::AcceptRole);
    auto *btnDiscard = btnBox->addButton(tr("Discard All"), QDialogButtonBox::RejectRole);
    auto *btnLater = btnBox->addButton(tr("Decide Later"), QDialogButtonBox::HelpRole);

    btnRecover->setDefault(true);

    connect(btnRecover, &QPushButton::clicked, this, [this]() { done(Recover); });
    connect(btnDiscard, &QPushButton::clicked, this, [this]() { done(Discard); });
    connect(btnLater, &QPushButton::clicked, this, [this]() { done(Later); });

    layout->addWidget(btnBox);
}

RecoveryDialog::~RecoveryDialog() = default;

QStringList RecoveryDialog::selectedFiles() const
{
    QStringList selected;
    for (int i = 0; i < m_listWidget->count(); ++i) {
        auto *item = m_listWidget->item(i);
        if (item->checkState() == Qt::Checked) {
            selected.append(item->data(Qt::UserRole).toString());
        }
    }
    return selected;
}
