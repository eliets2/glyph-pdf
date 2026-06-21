// SPDX-License-Identifier: Apache-2.0
#include "ui/EncryptionDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QCheckBox>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QLabel>
#include <QMessageBox>

EncryptionDialog::EncryptionDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Encrypt Document"));
    setAccessibleName(tr("Encrypt document dialog"));

    // ── Passwords ─────────────────────────────────────────────────────────
    m_userPasswordEdit = new QLineEdit(this);
    m_userPasswordEdit->setEchoMode(QLineEdit::Password);
    m_userPasswordEdit->setPlaceholderText(tr("Required to open the document"));
    m_userPasswordEdit->setAccessibleName(tr("Open password"));
    m_userPasswordEdit->setAccessibleDescription(
        tr("Password required to open the encrypted document"));

    // AR-8 D4: confirm-password field so users cannot set a typo they cannot recover from.
    m_userPasswordConfirmEdit = new QLineEdit(this);
    m_userPasswordConfirmEdit->setEchoMode(QLineEdit::Password);
    m_userPasswordConfirmEdit->setPlaceholderText(tr("Re-enter the open password"));
    m_userPasswordConfirmEdit->setAccessibleName(tr("Confirm open password"));
    m_userPasswordConfirmEdit->setAccessibleDescription(
        tr("Re-enter the open password to confirm — must match the field above"));

    m_ownerPasswordEdit = new QLineEdit(this);
    m_ownerPasswordEdit->setEchoMode(QLineEdit::Password);
    m_ownerPasswordEdit->setPlaceholderText(tr("Required to change permissions (optional)"));
    m_ownerPasswordEdit->setAccessibleName(tr("Permissions password"));
    m_ownerPasswordEdit->setAccessibleDescription(
        tr("Password required to change document permissions"));

    QFormLayout *formLayout = new QFormLayout;
    formLayout->addRow(tr("Open Password:"),    m_userPasswordEdit);
    formLayout->addRow(tr("Confirm Password:"), m_userPasswordConfirmEdit);
    formLayout->addRow(tr("Owner Password:"),   m_ownerPasswordEdit);

    // ── AR-8 D4: "lost password = unrecoverable" warning ──────────────────
    auto* warningLabel = new QLabel(
        tr("<b>Warning:</b> Encrypted PDFs cannot be opened without the correct password. "
           "There is no recovery option if the password is lost. "
           "Store your password in a secure password manager before encrypting."),
        this);
    warningLabel->setObjectName("encryptionWarningLabel");
    warningLabel->setWordWrap(true);
    warningLabel->setStyleSheet("color: #c8442b; font-size: 11px;");

    // ── Permissions ────────────────────────────────────────────────────────
    // AR-8 D4: disable the permissions group until an owner password is entered.
    // Permissions restrictions are meaningless without an owner password because
    // any application that opens the document with the user password alone could
    // modify it.
    m_permsGroup = new QGroupBox(tr("Permissions (requires owner password)"), this);
    m_permsGroup->setEnabled(false);
    m_permsGroup->setToolTip(
        tr("Enter an owner password above to enable permission restrictions"));

    auto* permsLayout = new QVBoxLayout(m_permsGroup);
    m_printCheck = new QCheckBox(tr("Allow Printing"), this);
    m_printCheck->setChecked(true);
    m_copyCheck = new QCheckBox(tr("Allow Copying"), this);
    m_copyCheck->setChecked(true);
    m_modifyCheck = new QCheckBox(tr("Allow Modification"), this);
    m_modifyCheck->setChecked(false);
    permsLayout->addWidget(m_printCheck);
    permsLayout->addWidget(m_copyCheck);
    permsLayout->addWidget(m_modifyCheck);

    // Enable permissions group when the owner password is non-empty.
    connect(m_ownerPasswordEdit, &QLineEdit::textChanged, this, [this](const QString& text) {
        m_permsGroup->setEnabled(!text.isEmpty());
        if (text.isEmpty()) {
            // Reset to defaults when owner password is cleared — restrictions without
            // an owner password are unenforceable.
            m_printCheck->setChecked(true);
            m_copyCheck->setChecked(true);
            m_modifyCheck->setChecked(false);
        }
    });

    // ── Buttons ────────────────────────────────────────────────────────────
    QDialogButtonBox *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

    connect(buttonBox, &QDialogButtonBox::accepted, this, [this]() {
        const QString userPassword    = m_userPasswordEdit->text();
        const QString confirmPassword = m_userPasswordConfirmEdit->text();
        const QString ownerPassword   = m_ownerPasswordEdit->text();

        if (userPassword.isEmpty()) {
            QMessageBox::warning(this, tr("Password Required"),
                tr("Enter an open password before encrypting the document."));
            m_userPasswordEdit->setFocus(Qt::OtherFocusReason);
            return;
        }
        if (userPassword.size() < 8) {
            QMessageBox::warning(this, tr("Weak Password"),
                tr("Use at least 8 characters for the open password."));
            m_userPasswordEdit->setFocus(Qt::OtherFocusReason);
            return;
        }
        // AR-8 D4: confirm-password check.
        if (userPassword != confirmPassword) {
            QMessageBox::warning(this, tr("Passwords Do Not Match"),
                tr("The open password and confirmation do not match. "
                   "Re-enter both fields carefully."));
            m_userPasswordConfirmEdit->clear();
            m_userPasswordEdit->setFocus(Qt::OtherFocusReason);
            return;
        }
        bool hasRestrictions = !m_printCheck->isChecked() || !m_copyCheck->isChecked()
                               || !m_modifyCheck->isChecked();
        if (ownerPassword.isEmpty() && hasRestrictions) {
            QMessageBox::warning(this, tr("Owner Password Required"),
                tr("Set a permissions password when restricting printing, copying, or modification."));
            m_ownerPasswordEdit->setFocus(Qt::OtherFocusReason);
            return;
        }
        if (!ownerPassword.isEmpty() && ownerPassword == userPassword) {
            QMessageBox::warning(this, tr("Use Different Passwords"),
                tr("Use a different permissions password so document restrictions are not "
                   "unlocked by the open password."));
            m_ownerPasswordEdit->setFocus(Qt::OtherFocusReason);
            return;
        }
        accept();
    });
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // ── Layout ─────────────────────────────────────────────────────────────
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(formLayout);
    mainLayout->addSpacing(8);
    mainLayout->addWidget(warningLabel);
    mainLayout->addSpacing(8);
    mainLayout->addWidget(m_permsGroup);
    mainLayout->addSpacing(10);
    mainLayout->addWidget(buttonBox);
}

QString EncryptionDialog::userPassword()  const { return m_userPasswordEdit->text(); }
QString EncryptionDialog::ownerPassword() const { return m_ownerPasswordEdit->text(); }
bool    EncryptionDialog::canPrint()      const { return m_printCheck->isChecked(); }
bool    EncryptionDialog::canCopy()       const { return m_copyCheck->isChecked(); }
bool    EncryptionDialog::canModify()     const { return m_modifyCheck->isChecked(); }
