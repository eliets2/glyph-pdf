// SPDX-License-Identifier: Apache-2.0
#include "ui/PermissionsDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QCheckBox>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QLabel>
#include <QMessageBox>

PermissionsDialog::PermissionsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Document Permissions"));
    setAccessibleName(tr("Document permissions dialog"));

    m_userPasswordEdit = new QLineEdit(this);
    m_userPasswordEdit->setEchoMode(QLineEdit::Password);
    m_userPasswordEdit->setPlaceholderText(tr("Required to open the document"));

    m_ownerPasswordEdit = new QLineEdit(this);
    m_ownerPasswordEdit->setEchoMode(QLineEdit::Password);
    m_ownerPasswordEdit->setPlaceholderText(tr("Required to change permissions"));

    m_printCheck = new QCheckBox(tr("Allow Printing"), this);
    m_printCheck->setChecked(true);
    
    m_printHighCheck = new QCheckBox(tr("Allow High Quality Printing"), this);
    m_printHighCheck->setChecked(true);

    m_copyCheck = new QCheckBox(tr("Allow Copying text and graphics"), this);
    m_copyCheck->setChecked(true);
    
    m_modifyCheck = new QCheckBox(tr("Allow Modification of contents"), this);
    m_modifyCheck->setChecked(false);

    m_annotateCheck = new QCheckBox(tr("Allow Annotations"), this);
    m_annotateCheck->setChecked(false);

    m_fillFormsCheck = new QCheckBox(tr("Allow Filling Forms"), this);
    m_fillFormsCheck->setChecked(false);

    m_accessibilityCheck = new QCheckBox(tr("Allow Content Extraction for Accessibility"), this);
    m_accessibilityCheck->setChecked(true);

    m_assembleCheck = new QCheckBox(tr("Allow Document Assembly"), this);
    m_assembleCheck->setChecked(false);

    QFormLayout *formLayout = new QFormLayout;
    formLayout->addRow(tr("User Password:"), m_userPasswordEdit);
    formLayout->addRow(tr("Owner Password:"), m_ownerPasswordEdit);

    QLabel *helpLabel = new QLabel(tr("Configure document permissions. A permissions (owner) password is required to restrict permissions."), this);
    helpLabel->setObjectName("dialogHelpText");
    helpLabel->setWordWrap(true);

    QVBoxLayout *permsLayout = new QVBoxLayout;
    permsLayout->addWidget(m_printCheck);
    permsLayout->addWidget(m_printHighCheck);
    permsLayout->addWidget(m_copyCheck);
    permsLayout->addWidget(m_modifyCheck);
    permsLayout->addWidget(m_annotateCheck);
    permsLayout->addWidget(m_fillFormsCheck);
    permsLayout->addWidget(m_accessibilityCheck);
    permsLayout->addWidget(m_assembleCheck);

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
        bool hasRestrictions = !m_printCheck->isChecked() || !m_printHighCheck->isChecked() || 
                               !m_copyCheck->isChecked() || !m_modifyCheck->isChecked() ||
                               !m_annotateCheck->isChecked() || !m_fillFormsCheck->isChecked() ||
                               !m_accessibilityCheck->isChecked() || !m_assembleCheck->isChecked();
        if (ownerPassword.isEmpty() && hasRestrictions) {
            QMessageBox::warning(this, tr("Owner Password Required"), tr("Set a permissions password when restricting permissions."));
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

QString PermissionsDialog::userPassword() const { return m_userPasswordEdit->text(); }
QString PermissionsDialog::ownerPassword() const { return m_ownerPasswordEdit->text(); }

DocumentPermissions PermissionsDialog::permissions() const {
    DocumentPermissions perms;
    perms.print = m_printCheck->isChecked();
    perms.printHighQuality = m_printHighCheck->isChecked();
    perms.copy = m_copyCheck->isChecked();
    perms.modify = m_modifyCheck->isChecked();
    perms.annotate = m_annotateCheck->isChecked();
    perms.fillForms = m_fillFormsCheck->isChecked();
    perms.accessibility = m_accessibilityCheck->isChecked();
    perms.assemble = m_assembleCheck->isChecked();
    return perms;
}
