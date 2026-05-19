#include "ui/SignatureDialog.h"
#include "util/GpTheme.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QFileDialog>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QMessageBox>

SignatureDialog::SignatureDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Digital Signature"));
    setMinimumWidth(450);
    setAccessibleName(tr("Digital signature dialog"));

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(15);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    QLabel *titleLabel = new QLabel(tr("Apply Digital Signature"));
    titleLabel->setStyleSheet(QString("font-size: 16px; font-weight: bold; color: %1;").arg(gp::Theme::accent().name()));
    mainLayout->addWidget(titleLabel);

    QLabel *trustLabel = new QLabel(tr("Choose a .p12 or .pfx certificate from a trusted source. The certificate password unlocks the private key used to sign this document; it is not stored by Glyph PDF."));
    trustLabel->setObjectName("dialogHelpText");
    trustLabel->setWordWrap(true);
    mainLayout->addWidget(trustLabel);

    QFormLayout *formLayout = new QFormLayout();
    formLayout->setLabelAlignment(Qt::AlignRight);
    formLayout->setSpacing(10);

    m_certPathEdit = new QLineEdit();
    m_certPathEdit->setPlaceholderText(tr("Select .p12 or .pfx certificate..."));
    m_certPathEdit->setAccessibleName(tr("Certificate file"));
    m_certPathEdit->setAccessibleDescription(tr("Path to the signing certificate file"));
    
    QPushButton *browseBtn = new QPushButton(tr("Browse..."));
    browseBtn->setAccessibleName(tr("Browse for certificate"));
    connect(browseBtn, &QPushButton::clicked, this, &SignatureDialog::browseCertificate);
    
    QHBoxLayout *certLayout = new QHBoxLayout();
    certLayout->addWidget(m_certPathEdit);
    certLayout->addWidget(browseBtn);
    
    m_passwordEdit = new QLineEdit();
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setPlaceholderText(tr("Certificate Password"));
    m_passwordEdit->setAccessibleName(tr("Certificate password"));
    m_passwordEdit->setAccessibleDescription(tr("Password used to unlock the certificate private key"));

    m_reasonEdit = new QLineEdit();
    m_reasonEdit->setPlaceholderText(tr("e.g. I am the author of this document"));
    m_reasonEdit->setAccessibleName(tr("Signing reason"));

    m_locationEdit = new QLineEdit();
    m_locationEdit->setPlaceholderText(tr("e.g. New York, USA"));
    m_locationEdit->setAccessibleName(tr("Signing location"));

    formLayout->addRow(tr("Certificate:"), certLayout);
    formLayout->addRow(tr("Password:"), m_passwordEdit);
    formLayout->addRow(tr("Reason:"), m_reasonEdit);
    formLayout->addRow(tr("Location:"), m_locationEdit);

    mainLayout->addLayout(formLayout);

    QLabel *validationLabel = new QLabel(tr("Recipients trust the signature only when the certificate chain is trusted. Use Validate Signature after signing to confirm status."));
    validationLabel->setObjectName("dialogHelpText");
    validationLabel->setWordWrap(true);
    mainLayout->addWidget(validationLabel);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttonBox, &QDialogButtonBox::accepted, this, [this]() {
        if (m_certPathEdit->text().trimmed().isEmpty()) {
            QMessageBox::warning(this, tr("Certificate Required"), tr("Select a .p12 or .pfx certificate before signing."));
            m_certPathEdit->setFocus(Qt::OtherFocusReason);
            return;
        }
        if (m_passwordEdit->text().isEmpty()) {
            QMessageBox::warning(this, tr("Password Required"), tr("Enter the certificate password to unlock the signing key."));
            m_passwordEdit->setFocus(Qt::OtherFocusReason);
            return;
        }
        accept();
    });
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);

    // Styling
    setStyleSheet(QString(
        "QDialog { background-color: #1E1E1E; }"
        "QLabel { color: #DDDDDD; }"
        "QLabel#dialogHelpText { color: #A8ABB0; line-height: 130%; }"
        "QLineEdit { background-color: #2D2D2D; color: white; border: 1px solid #444; padding: 5px; border-radius: 3px; }"
        "QLineEdit:focus { border: 1px solid %1; }"
        "QPushButton { background-color: %1; color: black; border: none; padding: 5px 15px; border-radius: 3px; font-weight: bold; }"
        "QPushButton:hover { background-color: #FFD700; }"
    ).arg(gp::Theme::accent().name()));
}

QString SignatureDialog::certificatePath() const { return m_certPathEdit->text(); }
QString SignatureDialog::password() const { return m_passwordEdit->text(); }
QString SignatureDialog::reason() const { return m_reasonEdit->text(); }
QString SignatureDialog::location() const { return m_locationEdit->text(); }

void SignatureDialog::browseCertificate()
{
    QString path = QFileDialog::getOpenFileName(this, tr("Select Certificate"), QString(), tr("Certificates (*.p12 *.pfx)"));
    if (!path.isEmpty()) {
        m_certPathEdit->setText(path);
    }
}
