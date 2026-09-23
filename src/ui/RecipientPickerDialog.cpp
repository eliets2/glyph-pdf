// SPDX-License-Identifier: Apache-2.0
// N17 — recipient picker implementation. See RecipientPickerDialog.h.
#include "RecipientPickerDialog.h"

#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/sha.h>

namespace {
// (no local helpers — OpenSSL calls are guarded inline below)
} // namespace

RecipientCertInfo RecipientCertInfo::fromFile(const QString &path)
{
    RecipientCertInfo info;
    info.filePath = path;

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return info;                       // invalid — missing/unreadable
    const QByteArray data = f.readAll();
    f.close();

    // DER first, then PEM — the exact acceptance order of the engine's
    // encryptWithCertificate, so anything the dialog accepts the engine reads.
    const unsigned char *p = reinterpret_cast<const unsigned char *>(data.constData());
    X509 *cert = d2i_X509(nullptr, &p, data.size());
    if (!cert) {
        BIO *bio = BIO_new_mem_buf(data.constData(), data.size());
        if (bio) {
            cert = PEM_read_bio_X509(bio, nullptr, nullptr, nullptr);
            BIO_free(bio);
        }
    }
    if (!cert)
        return info;                       // invalid — not a certificate

    // Subject: prefer the RFC2253 one-liner (CN included), fall back to CN.
    char *subj = X509_NAME_oneline(X509_get_subject_name(cert), nullptr, 0);
    if (subj) {
        info.subject = QString::fromLatin1(subj);
        OPENSSL_free(subj);
    }
    {
        X509_NAME *name = X509_get_subject_name(cert);
        const int cnLoc = X509_NAME_get_index_by_NID(name, NID_commonName, -1);
        if (cnLoc >= 0) {
            X509_NAME_ENTRY *entry = X509_NAME_get_entry(name, cnLoc);
            ASN1_STRING *cn = X509_NAME_ENTRY_get_data(entry);
            info.subject = QString::fromUtf8(reinterpret_cast<const char *>(ASN1_STRING_get0_data(cn)),
                                             ASN1_STRING_length(cn));
        }
    }

    // Fingerprint: SHA-256 over the DER encoding, uppercase hex.
    const int derLen = i2d_X509(cert, nullptr);
    if (derLen > 0) {
        QByteArray derBuf(derLen, Qt::Uninitialized);
        unsigned char *derPtr = reinterpret_cast<unsigned char *>(derBuf.data());
        if (i2d_X509(cert, &derPtr) == derLen) {
            unsigned char digest[SHA256_DIGEST_LENGTH];
            SHA256(reinterpret_cast<const unsigned char *>(derBuf.constData()),
                   static_cast<size_t>(derLen), digest);
            for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i)
                info.fingerprintHex += QString::fromLatin1("%1")
                                           .arg(digest[i], 2, 16, QLatin1Char('0'))
                                           .toUpper();
        }
    }
    X509_free(cert);
    return info;
}

RecipientPickerDialog::RecipientPickerDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Encrypt with Recipient Certificates"));
    setModal(true);
    resize(560, 420);

    auto *lay = new QVBoxLayout(this);

    lay->addWidget(new QLabel(
        tr("Encrypt the document's session key to one or more recipient "
           "certificates. Each listed recipient will be able to open the "
           "encrypted document with their credential; nobody else — including "
           "holders of no certificate — can."), this));

    // ER-3 disclosure: honest, permanent (never hidden behind a help toggle).
    m_disclosureLabel = new QLabel(
        tr("How recipients work: every recipient receives its own CMS envelope, "
           "and all envelopes wrap the SAME session key. Re-encrypting a "
           "document that is already encrypted for multiple recipients changes "
           "that session key — recipients you do not re-specify here will be "
           "locked out and must be granted access again."), this);
    m_disclosureLabel->setObjectName(QStringLiteral("recipientDisclosureLabel"));
    m_disclosureLabel->setWordWrap(true);
    lay->addWidget(m_disclosureLabel);

    m_list = new QListWidget(this);
    m_list->setObjectName(QStringLiteral("recipientList"));
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setAccessibleName(tr("Recipients"));
    lay->addWidget(m_list, 1);

    m_errorLabel = new QLabel(this);
    m_errorLabel->setObjectName(QStringLiteral("recipientErrorLabel"));
    m_errorLabel->setWordWrap(true);
    m_errorLabel->setStyleSheet(QStringLiteral("color: #b00020;"));
    m_errorLabel->hide();
    lay->addWidget(m_errorLabel);

    auto *buttons = new QHBoxLayout;
    m_addButton = new QPushButton(tr("Add Certificates…"), this);
    m_addButton->setObjectName(QStringLiteral("recipientAddButton"));
    m_removeButton = new QPushButton(tr("Remove Selected"), this);
    m_removeButton->setObjectName(QStringLiteral("recipientRemoveButton"));
    m_clearButton = new QPushButton(tr("Remove All"), this);
    m_clearButton->setObjectName(QStringLiteral("recipientClearButton"));
    buttons->addWidget(m_addButton);
    buttons->addWidget(m_removeButton);
    buttons->addWidget(m_clearButton);
    buttons->addStretch(1);
    lay->addLayout(buttons);

    auto *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    box->setObjectName(QStringLiteral("recipientButtonBox"));
    lay->addWidget(box);

    connect(m_addButton, &QPushButton::clicked, this, &RecipientPickerDialog::browseCertificates);
    connect(m_removeButton, &QPushButton::clicked, this, &RecipientPickerDialog::removeSelected);
    connect(m_clearButton, &QPushButton::clicked, this, &RecipientPickerDialog::clearRecipients);
    connect(box, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(this, &RecipientPickerDialog::recipientsChanged,
            this, &RecipientPickerDialog::updateAcceptEnabled);
    updateAcceptEnabled();
}

RecipientPickerDialog::~RecipientPickerDialog() = default;

bool RecipientPickerDialog::addRecipientPaths(const QStringList &paths)
{
    bool anyAccepted = false;
    QString lastReason;
    for (const QString &path : paths) {
        const RecipientCertInfo info = RecipientCertInfo::fromFile(path);
        if (!info.isValid()) {
            // Honest refusal: name the file, keep it OUT of the list, keep OK
            // gated. Never silently drop.
            lastReason = tr("%1 is not a readable X.509 certificate (DER or PEM "
                            "expected). It was NOT added.")
                             .arg(QFileInfo(path).fileName());
            continue;
        }
        if (m_certPaths.contains(path)) {
            lastReason = tr("%1 is already in the recipient list.")
                             .arg(QFileInfo(path).fileName());
            continue;
        }
        m_certPaths.append(path);
        // Row identity comes from the CERTIFICATE (subject + fingerprint), not
        // the file name — pins that the dialog actually parsed the cert.
        m_list->addItem(QStringLiteral("%1 — SHA-256 %2…%3")
                            .arg(info.subject,
                                 info.fingerprintHex.left(16),
                                 info.fingerprintHex.right(8)));
        anyAccepted = true;
    }
    if (!lastReason.isEmpty()) {
        m_errorLabel->setText(lastReason);
        m_errorLabel->show();
    } else {
        m_errorLabel->hide();
    }
    emit recipientsChanged();
    return anyAccepted;
}

void RecipientPickerDialog::clearRecipients()
{
    m_certPaths.clear();
    m_list->clear();
    emit recipientsChanged();
}

void RecipientPickerDialog::browseCertificates()
{
    // Native multi-select. Tests never reach here (offscreen); they drive
    // addRecipientPaths — the exact function this handler feeds.
    const QStringList files = QFileDialog::getOpenFileNames(
        this, tr("Select Recipient Certificates"), QString(),
        tr("Certificates (*.cer *.crt *.pem *.p10 *.p12);;All files (*)"));
    if (!files.isEmpty())
        addRecipientPaths(files);
}

void RecipientPickerDialog::removeSelected()
{
    const QList<QListWidgetItem *> selected = m_list->selectedItems();
    for (QListWidgetItem *item : selected) {
        const int row = m_list->row(item);
        if (row >= 0 && row < m_certPaths.size())
            m_certPaths.removeAt(row);
        delete item;
    }
    emit recipientsChanged();
}

bool RecipientPickerDialog::isAcceptEnabled() const
{
    const auto *box = findChild<QDialogButtonBox *>(
        QStringLiteral("recipientButtonBox"));
    return box && box->button(QDialogButtonBox::Ok)
               && box->button(QDialogButtonBox::Ok)->isEnabled();
}

void RecipientPickerDialog::updateAcceptEnabled()
{
    auto *box = findChild<QDialogButtonBox *>(QStringLiteral("recipientButtonBox"));
    if (box)
        box->button(QDialogButtonBox::Ok)->setEnabled(!m_certPaths.isEmpty());
}
