// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QDialog>
#include <QStringList>

class QListWidget;
class QDialogButtonBox;

class RecoveryDialog : public QDialog
{
    Q_OBJECT
public:
    enum Result {
        Recover,
        Discard,
        Later
    };

    explicit RecoveryDialog(const QStringList &orphanedFiles, QWidget *parent = nullptr);
    ~RecoveryDialog() override;

    QStringList selectedFiles() const;

private:
    QListWidget *m_listWidget;
};
