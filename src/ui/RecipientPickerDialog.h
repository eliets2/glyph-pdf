// SPDX-License-Identifier: Apache-2.0
#ifndef RECIPIENTPICKERDIALOG_H
#define RECIPIENTPICKERDIALOG_H

#include <QDialog>
#include <QStringList>

class QLabel;
class QListWidget;
class QPushButton;

struct RecipientCertInfo;

// N17 (backlog row 33): certificate-encryption recipient picker — the UI over
// the EXISTING engine seam `IEncryptor::encryptWithCertificate(input, output,
// certPaths)`. The user picks one or more X.509 recipient certificates
// (.cer/.pem, DER or PEM — the engine accepts both); each accepted cert is
// parsed HERE so the list can show the subject and a SHA-256 fingerprint from
// the certificate itself, not just a file name.
//
// Honesty contracts pinned by TestCertEncryptPicker:
//   - OK stays disabled until at least one VALID recipient is present (an
//     empty recipient list must never reach the engine).
//   - An unreadable / non-certificate file is refused WITH a visible reason
//     naming the file (recipientErrorLabel) — never silently dropped.
//   - The ER-3 disclosure (recipientDisclosureLabel) explains the
//     session-key/recipient relationship: every recipient gets its own CMS
//     envelope, all wrapped around the ONE session key; re-encrypting a
//     multi-recipient document changes that session key and locks out
//     recipients that are not re-specified.
// The dialog holds NO engine and performs NO encryption — the calling
// controller drives `encryptWithCertificate` with `recipientCertPaths()`.
class RecipientPickerDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RecipientPickerDialog(QWidget *parent = nullptr);
    ~RecipientPickerDialog() override;

    // The seam the Browse button feeds — and the test seam that stands in for
    // the undrivable native multi-select dialog. Every path is parsed as an
    // X.509 certificate (DER first, then PEM, exactly the engine's order);
    // valid ones join the list, invalid ones are refused with a visible
    // reason. Returns true when AT LEAST ONE path was accepted.
    bool addRecipientPaths(const QStringList &paths);

    // Removes every recipient (Remove-selection button uses the same gate).
    void clearRecipients();

    int recipientCount() const { return m_certPaths.size(); }
    QStringList recipientCertPaths() const { return m_certPaths; }
    bool isAcceptEnabled() const;

signals:
    void recipientsChanged();

private slots:
    void browseCertificates();
    void removeSelected();
    void updateAcceptEnabled();

private:
    QListWidget *m_list;
    QLabel *m_disclosureLabel;   // ER-3 session-key disclosure (always shown)
    QLabel *m_errorLabel;        // visible refusal reason for the last rejection
    QPushButton *m_addButton;
    QPushButton *m_removeButton;
    QPushButton *m_clearButton;
    QStringList m_certPaths;     // accepted recipient certificate files, in add order
};

// Parsed recipient certificate — subject CN/RFC2253 name + SHA-256 fingerprint.
// Declared here (defined in the .cpp, OpenSSL-backed) so the list rows can be
// built from the certificate identity and unit tests can pin that identity.
struct RecipientCertInfo {
    QString filePath;
    QString subject;        // RFC2253 one-line subject (falls back to CN)
    QString fingerprintHex; // SHA-256 over the DER encoding, uppercase hex

    bool isValid() const { return !fingerprintHex.isEmpty(); }

    // Parse `path` as an X.509 certificate (DER, then PEM — the same order
    // the engine's encryptWithCertificate uses). Invalid/missing file → an
    // info whose isValid() is false.
    static RecipientCertInfo fromFile(const QString &path);
};

#endif // RECIPIENTPICKERDIALOG_H
