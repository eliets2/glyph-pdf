// SPDX-License-Identifier: Apache-2.0
#ifndef ENCRYPTIONDIALOG_H
#define ENCRYPTIONDIALOG_H

#include <QDialog>

QT_BEGIN_NAMESPACE
class QLineEdit;
class QCheckBox;
QT_END_NAMESPACE

class EncryptionDialog : public QDialog
{
    Q_OBJECT
public:
    explicit EncryptionDialog(QWidget *parent = nullptr);

    QString userPassword() const;
    QString ownerPassword() const;
    bool canPrint() const;
    bool canCopy() const;
    bool canModify() const;

private:
    QLineEdit *m_userPasswordEdit;
    QLineEdit *m_ownerPasswordEdit;
    QCheckBox *m_printCheck;
    QCheckBox *m_copyCheck;
    QCheckBox *m_modifyCheck;
};

#endif // ENCRYPTIONDIALOG_H
