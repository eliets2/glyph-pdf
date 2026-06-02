// SPDX-License-Identifier: Apache-2.0
#include "ui/EncryptionDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QCheckBox>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QLabel>
#include <QMessageBox>

EncryptionDialog::EncryptionDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Encrypt Document"));
    setAccessibleName(tr("Encrypt document dialog"));

    m_userPasswordEdit = new QLineEdit(this);
    m_userPasswordEdit->setEchoMode(QLineEdit::Password);
    m_userPasswordEdit->setPlaceholderText(tr("Required to open the document"));
    m_userPasswordEdit->setAccessibleName(tr("Open password"));
    m_userPasswordEdit->setAccessibleDescription(tr("Password required to open the encrypted document"));

    m_ownerPasswordEdit = new QLineEdit(this);
    m_ownerPasswordEdit->setEchoMode(QLineEdit::Password);
    m_ownerPasswordEdit->setPlaceholderText(tr("Required to change permissions"));
    m_ownerPasswordEdit->setAccessibleName(tr("Permissions password"));
    m_ownerPasswordEdit->setAccessibleDescription(tr("Password required to change document permissions"));

    m_printCheck = new QCheckBox(tr("Allow Printing"), this);
    m_printCheck->setChecked(true);
    
    m_copyCheck = new QCheckBox(tr("Allow Copying"), this);
    m_copyCheck->setChecked(true);
    
    m_modifyCheck = new QCheckBox(tr("Allow Modification"), this);
    m_modifyCheck->setChecked(false);

    QFormLayout *formLayout = new QFormLayout;
    formLayout->addRow(tr("User Password:"), m_userPasswordEdit);
    formLayout->addRow(tr("Owner Password:"), m_ownerPasswordEdit);

    QLabel *helpLabel = new QLabel(tr("Use a strong open password. The permissions password should be different if you want to restrict printing, copying, or modification."), this);
    helpLabel->setObjectName("dialogHelpText");
    helpLabel->setWordWrap(true);

    QVBoxLayout *permsLayout = new QVBoxLayout;
    permsLayout->addWidget(m_printCheck);
    permsLayout->addWidget(m_copyCheck);
    permsLayout->addWidget(m_modifyCheck);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, [this]() {
        const QString userPassword = m_userPasswordEdit->text();
        const QString ownerPassword = m_ownerPasswordEdit->text();
        if (userPassword.isEmpty()) {
            QMessageBox::warning(this, tr("Password Required"), tr("Enter an open password before encrypting the document."));
            m_userPasswordEdit->setFocus(Qt::OtherFocusReason);
            return;
        }
        if (userPassword.size() < 8) {
            QMessageBox::warning(this, tr("Weak Password"), tr("Use at least 8 characters for the open password."));
            m_userPasswordEdit->setFocus(Qt::OtherFocusReason);
            return;
        }
        bool hasRestrictions = !m_printCheck->isChecked() || !m_copyCheck->isChecked() || !m_modifyCheck->isChecked();
        if (ownerPassword.isEmpty() && hasRestrictions) {
            QMessageBox::warning(this, tr("Owner Password Required"), tr("Set a permissions password when restricting printing, copying, or modification."));
            m_ownerPasswordEdit->setFocus(Qt::OtherFocusReason);
            return;
        }
        if (!ownerPassword.isEmpty() && ownerPassword == userPassword) {
            QMessageBox::warning(this, tr("Use Different Passwords"), tr("Use a different permissions password so document restrictions are not unlocked by the open password."));
            m_ownerPasswordEdit->setFocus(Qt::OtherFocusReason);
            return;
        }
        accept();
    });
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(formLayout);
    mainLayout->addWidget(helpLabel);
    mainLayout->addSpacing(10);
    mainLayout->addLayout(permsLayout);
    mainLayout->addSpacing(10);
    mainLayout->addWidget(buttonBox);
}

QString EncryptionDialog::userPassword() const { return m_userPasswordEdit->text(); }
QString EncryptionDialog::ownerPassword() const { return m_ownerPasswordEdit->text(); }
bool EncryptionDialog::canPrint() const { return m_printCheck->isChecked(); }
bool EncryptionDialog::canCopy() const { return m_copyCheck->isChecked(); }
bool EncryptionDialog::canModify() const { return m_modifyCheck->isChecked(); }
