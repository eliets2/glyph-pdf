// SPDX-License-Identifier: Apache-2.0
#ifndef ENCRYPTIONDIALOG_H
#define ENCRYPTIONDIALOG_H

#include <QDialog>

QT_BEGIN_NAMESPACE
class QLineEdit;
class QCheckBox;
class QGroupBox;
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
    QLineEdit *m_userPasswordConfirmEdit;   // AR-8 D4: confirm-password field
    QLineEdit *m_ownerPasswordEdit;
    QGroupBox *m_permsGroup;                 // AR-8 D4: disabled until owner password set
    QCheckBox *m_printCheck;
    QCheckBox *m_copyCheck;
    QCheckBox *m_modifyCheck;
};

#endif // ENCRYPTIONDIALOG_H
