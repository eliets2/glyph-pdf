// SPDX-License-Identifier: Apache-2.0
// N17 — certificate-encryption recipient picker (backlog row 33 / spec N17).
// UI over the EXISTING, tested engine seam `IEncryptor::encryptWithCertificate`
// + the ER-3 CMS-recipient counter. Pins, in order:
//   1. RecipientPickerDialog honesty: OK gated on >=1 VALID recipient cert;
//      every listed recipient shows subject + fingerprint; an unreadable or
//      non-certificate file is refused WITH a visible reason (never silently
//      dropped); the ER-3 multi-recipient session-key disclosure is present.
//   2. Engine boundary through the dialog: the dialog's recipient list feeds
//      encryptWithCertificate; the SAVED artifact is verified through an
//      independent read path (fresh PoDoFo load of the /Encrypt dict) and a
//      decrypt round-trip for EACH recipient's credential (the exact path a
//      conformant PubSec reader follows — same pattern as TestEncryption).
//   3. Command identity: ToolId::CertEncrypt resolves through the alias table,
//      is claimed by CertEncryptController in the ToolRegistry, and dispatch
//      with no document open fails safe (no dialog, no crash, honest refusal).
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QDialogButtonBox>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QRegularExpression>

#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/pem.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/bn.h>
#include <openssl/cms.h>
#include <openssl/sha.h>

#include "core/AppContext.h"
#include "core/ToolId.h"
#include "engines/PdfEditorEngine.h"
#include "shell/ToolRegistry.h"
#include "shell/controllers/CertEncryptController.h"
#include "ui/RecipientPickerDialog.h"

#include <podofo/podofo.h>

namespace {

// Self-signed recipient certificate written to `path` (PEM). Returns the
// in-memory pair so the test can decrypt with the credential afterwards —
// the exact "recipient proves access" half of the acceptance contract.
struct RecipientCredential {
    X509 *cert = nullptr;
    EVP_PKEY *key = nullptr;
    QString pemPath;
};

RecipientCredential makeRecipient(const QString &cn, const QString &path)
{
    RecipientCredential rc;
    rc.key = EVP_PKEY_new();
    BIGNUM *bn = BN_new();
    BN_set_word(bn, RSA_F4);
    RSA *rsa = RSA_new();
    RSA_generate_key_ex(rsa, 2048, bn, nullptr);
    EVP_PKEY_assign_RSA(rc.key, rsa);
    BN_free(bn);

    rc.cert = X509_new();
    ASN1_INTEGER_set(X509_get_serialNumber(rc.cert), 1);
    X509_gmtime_adj(X509_get_notBefore(rc.cert), 0);
    X509_gmtime_adj(X509_get_notAfter(rc.cert), 31536000L);
    X509_set_pubkey(rc.cert, rc.key);
    X509_NAME *name = X509_get_subject_name(rc.cert);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
                               reinterpret_cast<const unsigned char *>(cn.toUtf8().data()),
                               -1, -1, 0);
    X509_set_issuer_name(rc.cert, name);
    X509_sign(rc.cert, rc.key, EVP_sha256());

    FILE *f = fopen(path.toUtf8().constData(), "wb");
    Q_ASSERT(f);
    PEM_write_X509(f, rc.cert);
    fclose(f);
    rc.pemPath = path;
    return rc;
}

// One-page PDF with a real content stream carrying a plaintext marker (the
// marker must NOT survive encryption). Same shape as TestEncryption's input.
QString makeSeedPdf(const QString &path)
{
    try {
        PoDoFo::PdfMemDocument doc;
        auto &page = doc.GetPages().CreatePage(
            PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
        PoDoFo::PdfPainter p;
        p.SetCanvas(page);
        auto *font = doc.GetFonts().SearchFont("Helvetica");
        if (font) {
            p.TextState.SetFont(*font, 18);
            p.DrawText("PUBSEC_N17_MARKER_781234", 50, 700);
        }
        p.FinishDrawing();
        doc.Save(path.toUtf8().constData());
        return path;
    } catch (const std::exception &) {
        return {};
    }
}

// Independent read path over the SAVED artifact: fresh PoDoFo load, trailer
// /Encrypt dict walked without any engine state. Returns the raw CMS envelope
// blobs (one per recipient) — empty list when the artifact is not PubSec
// encrypted with a /Recipients array.
QList<QByteArray> readRecipientEnvelopes(const QString &artifactPath, QString *why)
{
    QList<QByteArray> envelopes;
    try {
        PoDoFo::PdfMemDocument doc;
        doc.Load(artifactPath.toUtf8().constData());
        const auto &trailer = doc.GetTrailer();
        if (!trailer.GetDictionary().HasKey("Encrypt")) {
            if (why) *why = QStringLiteral("trailer has no /Encrypt dict");
            return {};
        }
        const auto *encryptRef = trailer.GetDictionary().GetKey("Encrypt");
        if (!encryptRef->IsReference()) {
            if (why) *why = QStringLiteral("/Encrypt is not an indirect reference");
            return {};
        }
        const auto &encObj = doc.GetObjects().MustGetObject(encryptRef->GetReference());
        const auto *filter = encObj.GetDictionary().FindKey("Filter");
        if (!filter || std::string(filter->GetName().GetString()) != "PubSec") {
            if (why) *why = QStringLiteral("/Filter is not PubSec");
            return {};
        }
        const auto *sub = encObj.GetDictionary().FindKey("SubFilter");
        if (!sub || std::string(sub->GetName().GetString()) != "adbe.pkcs7.s5") {
            if (why) *why = QStringLiteral("/SubFilter is not adbe.pkcs7.s5");
            return {};
        }
        const auto *recips = encObj.GetDictionary().FindKey("Recipients");
        if (!recips || !recips->IsArray()) {
            if (why) *why = QStringLiteral("/Recipients missing or not an array");
            return {};
        }
        const auto &arr = recips->GetArray();
        for (size_t i = 0; i < arr.size(); ++i) {
            const auto &sv = arr[i].GetString().GetRawData();
            envelopes.append(QByteArray(sv.data(), static_cast<int>(sv.size())));
        }
    } catch (const std::exception &e) {
        if (why) *why = QString::fromUtf8(e.what());
        return {};
    }
    return envelopes;
}

// Recipient-side decrypt: CMS-unwrap the 20-byte seed, FEK = SHA256(seed) —
// the conformant PubSec reader path (TestEncryption::cmsRecoverFek pattern).
QByteArray recipientFek(const QByteArray &cms, X509 *cert, EVP_PKEY *key)
{
    const unsigned char *cp = reinterpret_cast<const unsigned char *>(cms.constData());
    CMS_ContentInfo *ci = d2i_CMS_ContentInfo(nullptr, &cp, cms.size());
    if (!ci)
        return {};
    BIO *obio = BIO_new(BIO_s_mem());
    QByteArray fek;
    if (CMS_decrypt(ci, key, cert, nullptr, obio, 0) == 1) {
        char *seed = nullptr;
        long n = BIO_get_mem_data(obio, &seed);
        if (n >= 20) {
            unsigned char digest[32];
            SHA256(reinterpret_cast<const unsigned char *>(seed), static_cast<size_t>(n), digest);
            fek = QByteArray(reinterpret_cast<char *>(digest), 32);
        }
    }
    BIO_free(obio);
    CMS_ContentInfo_free(ci);
    return fek;
}

} // namespace

class TestCertEncryptPicker : public QObject {
    Q_OBJECT

    QTemporaryDir m_tmp;
    QString m_cert1, m_cert2, m_seed;
    // Credential handles kept alive for the recipient-side decrypt round-trip.
    RecipientCredential m_r1, m_r2;

private slots:

    void initTestCase() {
        QVERIFY(m_tmp.isValid());
        m_r1 = makeRecipient(QStringLiteral("Test Recipient One"),
                             m_tmp.filePath("recipient1.pem"));
        m_r2 = makeRecipient(QStringLiteral("Test Recipient Two"),
                             m_tmp.filePath("recipient2.pem"));
        m_cert1 = m_r1.pemPath;
        m_cert2 = m_r2.pemPath;
        QVERIFY(QFileInfo::exists(m_cert1));
        QVERIFY(QFileInfo::exists(m_cert2));
        m_seed = makeSeedPdf(m_tmp.filePath("seed.pdf"));
        QVERIFY(!m_seed.isEmpty());
    }

    void cleanupTestCase() {
        if (m_r1.cert) X509_free(m_r1.cert);
        if (m_r1.key) EVP_PKEY_free(m_r1.key);
        if (m_r2.cert) X509_free(m_r2.cert);
        if (m_r2.key) EVP_PKEY_free(m_r2.key);
    }

    // ── 1. Dialog honesty ────────────────────────────────────────────────────

    void dialogStartsEmptyAndGatesOk()
    {
        RecipientPickerDialog dlg;
        QCOMPARE(dlg.recipientCount(), 0);
        QVERIFY(dlg.recipientCertPaths().isEmpty());
        auto *buttons = dlg.findChild<QDialogButtonBox *>();
        QVERIFY2(buttons, "dialog must expose its QDialogButtonBox");
        QVERIFY2(!buttons->button(QDialogButtonBox::Ok)->isEnabled(),
                 "OK must stay disabled with zero recipients");
    }

    void validRecipientsListWithSubjectAndFingerprint()
    {
        RecipientPickerDialog dlg;
        // The seam the Browse button feeds (native QFileDialog is undrivable
        // offscreen — the file list travels through one testable entry point).
        QVERIFY2(dlg.addRecipientPaths({ m_cert1, m_cert2 }),
                 "two valid recipient certificates must be accepted");
        QCOMPARE(dlg.recipientCount(), 2);
        QVERIFY2(dlg.isAcceptEnabled(),
                 "OK must enable once at least one valid recipient is present");

        auto *list = dlg.findChild<QListWidget *>(QStringLiteral("recipientList"));
        QVERIFY2(list, "dialog must expose its recipientList widget");
        QCOMPARE(list->count(), 2);
        // Each row must carry the SUBJECT (CN) and a FINGERPRINT — not just a
        // file name. Row identity comes from the certificate itself.
        QVERIFY2(list->item(0)->text().contains(QStringLiteral("Test Recipient One")),
                 qPrintable(QStringLiteral("row 0 must show the subject CN, got: %1")
                                .arg(list->item(0)->text())));
        QVERIFY2(list->item(1)->text().contains(QStringLiteral("Test Recipient Two")),
                 qPrintable(QStringLiteral("row 1 must show the subject CN, got: %1")
                                .arg(list->item(1)->text())));
        QVERIFY2(list->item(0)->text().contains(QRegularExpression("[0-9A-Fa-f]{8}")),
                 "row 0 must show a hex fingerprint");
        QVERIFY2(list->item(1)->text().contains(QRegularExpression("[0-9A-Fa-f]{8}")),
                 "row 1 must show a hex fingerprint");

        // The engine call must receive the paths in the order the user added.
        const QStringList paths = dlg.recipientCertPaths();
        QCOMPARE(paths.size(), 2);
        QCOMPARE(paths.at(0), m_cert1);
        QCOMPARE(paths.at(1), m_cert2);
    }

    void invalidCertRefusedWithVisibleReason()
    {
        RecipientPickerDialog dlg;
        const QString bad = m_tmp.filePath("not_a_cert.pem");
        {
            QFile f(bad);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("hello world, definitely not DER or a PEM certificate\n");
        }
        QVERIFY2(!dlg.addRecipientPaths({ bad }),
                 "a non-certificate file must be refused");
        QCOMPARE(dlg.recipientCount(), 0);
        QVERIFY2(!dlg.isAcceptEnabled(), "OK must stay disabled after a refused cert");

        auto *err = dlg.findChild<QLabel *>(QStringLiteral("recipientErrorLabel"));
        QVERIFY2(err, "dialog must expose a recipientErrorLabel");
        QVERIFY2(!err->text().isEmpty(),
                 "a refused certificate must surface a VISIBLE reason, never a silent drop");
        QVERIFY2(err->text().contains(QFileInfo(bad).fileName()),
                 "the reason must name the refused file");

        // A missing file is refused the same honest way.
        QVERIFY2(!dlg.addRecipientPaths({ m_tmp.filePath("missing.pem") }),
                 "a missing certificate file must be refused");
        QCOMPARE(dlg.recipientCount(), 0);

        // Valid additions still work after refusals.
        QVERIFY2(dlg.addRecipientPaths({ m_cert1 }), "valid cert after refusals must be accepted");
        QCOMPARE(dlg.recipientCount(), 1);
    }

    void removeRecipientUpdatesListAndGate()
    {
        RecipientPickerDialog dlg;
        QVERIFY(dlg.addRecipientPaths({ m_cert1, m_cert2 }));
        QCOMPARE(dlg.recipientCount(), 2);
        // Removing ALL recipients must re-gate OK (never an empty encrypt call).
        dlg.clearRecipients();
        QCOMPARE(dlg.recipientCount(), 0);
        QVERIFY2(!dlg.isAcceptEnabled(),
                 "OK must disable again when the recipient list is emptied");
    }

    void er3DisclosureIsPresent()
    {
        RecipientPickerDialog dlg;
        auto *disclosure = dlg.findChild<QLabel *>(QStringLiteral("recipientDisclosureLabel"));
        QVERIFY2(disclosure, "dialog must expose the ER-3 disclosure label");
        QVERIFY2(!disclosure->text().isEmpty(), "the ER-3 disclosure must not be empty");
        // The disclosure must warn about the multi-recipient session-key fact
        // (ER-3: re-encrypting changes the session key; envelopes are per-recipient).
        QVERIFY2(disclosure->text().contains(QStringLiteral("session key"),
                                             Qt::CaseInsensitive),
                 "the disclosure must explain the session-key/recipient relationship");
    }

    // ── 2. Engine boundary through the dialog — saved-artifact evidence ─────

    void twoRecipientsEncryptRoundTripViaIndependentRead()
    {
        RecipientPickerDialog dlg;
        QVERIFY(dlg.addRecipientPaths({ m_cert1, m_cert2 }));

        // The dialog's list feeds the EXISTING engine seam — the same call the
        // controller's worker makes.
        PdfEditorEngine engine;
        const QString out = m_tmp.filePath("encrypted_two.pdf");
        QVERIFY2(engine.encryptWithCertificate(m_seed, out, dlg.recipientCertPaths()),
                 qPrintable(QStringLiteral("encryptWithCertificate failed: %1 / %2")
                                .arg(engine.lastError().userMessage,
                                     engine.lastError().technicalDetails)));

        // Independent read path over the SAVED artifact: fresh PoDoFo load.
        QString why;
        const QList<QByteArray> envelopes = readRecipientEnvelopes(out, &why);
        QVERIFY2(envelopes.size() == 2,
                 qPrintable(QStringLiteral("saved artifact must carry 2 CMS recipient "
                                          "envelopes (independent PoDoFo read), got %1: %2")
                                .arg(envelopes.size()).arg(why)));

        // Plaintext marker must not survive (streams actually encrypted).
        QFile raw(out);
        QVERIFY(raw.open(QIODevice::ReadOnly));
        const QByteArray bytes = raw.readAll();
        raw.close();
        QVERIFY2(!bytes.contains("PUBSEC_N17_MARKER"),
                 "plaintext marker leaked into the encrypted PubSec artifact");

        // EACH recipient's credential must unwrap its own envelope and recover
        // the FEK — the acceptance contract "each recipient's cert decrypts".
        // (The stream-decrypt half — AES-GCM opening streams with the recovered
        // FEK — is pinned by TestEncryption::testCertificateEncryption; here the
        // marker-absence check plus per-recipient FEK recovery is the boundary
        // evidence for the DIALOG-fed path.)
        QByteArray fek1 = recipientFek(envelopes.at(0), m_r1.cert, m_r1.key);
        QByteArray fek2 = recipientFek(envelopes.at(1), m_r2.cert, m_r2.key);
        QVERIFY2(fek1.size() == 32,
                 "recipient 1 could not CMS-unwrap the FEK from its envelope");
        QVERIFY2(fek2.size() == 32,
                 "recipient 2 could not CMS-unwrap the FEK from its envelope");
        bool zero1 = true, zero2 = true;
        for (char c : fek1) if (c != 0) { zero1 = false; break; }
        for (char c : fek2) if (c != 0) { zero2 = false; break; }
        QVERIFY2(!zero1 && !zero2, "recovered FEKs must be non-zero");
        // One session key, two envelopes: both recipients must arrive at the
        // SAME FEK (that is what makes the document openable for each of them).
        QVERIFY2(fek1 == fek2,
                 "both recipients must recover the SAME session FEK (ER-3 semantics)");
    }

    void sourceByteInvarianceOnFailedCommit()
    {
        // Wrong-cert-open honesty + SafeSave discipline: a FAILED encrypt must
        // leave the source bytes untouched and must not strand a bogus output.
        RecipientPickerDialog dlg;
        QVERIFY(dlg.addRecipientPaths({ m_cert1 }));

        QFile src(m_seed);
        QVERIFY(src.open(QIODevice::ReadOnly));
        const QByteArray before = src.readAll();
        src.close();

        PdfEditorEngine engine;
        const QString missingCert = m_tmp.filePath("gone.pem");
        QVERIFY2(!engine.encryptWithCertificate(m_seed, m_tmp.filePath("never.pdf"),
                                                { missingCert }),
                 "encrypting to a missing certificate must fail");
        QFile srcAfter(m_seed);
        QVERIFY(srcAfter.open(QIODevice::ReadOnly));
        const QByteArray after = srcAfter.readAll();
        srcAfter.close();
        QCOMPARE(after, before);
        QVERIFY2(!QFile::exists(m_tmp.filePath("never.pdf")),
                 "a failed commit must not leave an output artifact");
    }

    // ── 3. Command identity + registry wiring ───────────────────────────────

    void certEncryptCommandIdentityResolves()
    {
        QCOMPARE(toolIdToString(ToolId::CertEncrypt), QStringLiteral("certEncrypt"));
        const auto parsed = toolIdFromString(QStringLiteral("certEncrypt"));
        QVERIFY(parsed.has_value());
        QCOMPARE(parsed.value(), ToolId::CertEncrypt);
        // Alias tolerance, consistent with the other Security commands.
        QCOMPARE(toolIdFromString(QStringLiteral("cert-encrypt")).value_or(ToolId::COUNT),
                 ToolId::CertEncrypt);
    }

    void controllerClaimsCertEncryptAndDispatchFailsSafe()
    {
        AppContext ctx; // no document, no window — the shell-less minimum
        gp::CertEncryptController ctrl(&ctx, nullptr);
        const auto tools = ctrl.handledTools();
        QCOMPARE(tools.size(), 1);
        QVERIFY(tools.contains(ToolId::CertEncrypt));
        // No document open → unavailable (honestly disabled), never a crash.
        QVERIFY2(!ctrl.isEnabled(ToolId::CertEncrypt),
                 "the command must report disabled with no document open");

        gp::ToolRegistry registry;
        registry.registerController(&ctrl);
        QCOMPARE(registry.controllerFor(ToolId::CertEncrypt),
                 static_cast<IToolController *>(&ctrl));
        // Dispatch with no document must fail safe (early return, no dialog).
        registry.activateFromString(QStringLiteral("certEncrypt"));
        registry.activate(ToolId::CertEncrypt);
    }

    void controllerEr3WordingIsHonest()
    {
        // ER-3 re-encrypt warning — pure wording builder, unit-testable without
        // the shell (SecurityController::buildValidationSummary idiom).
        QVERIFY(gp::CertEncryptController::multiRecipientWarning(1).isEmpty());
        const QString w2 = gp::CertEncryptController::multiRecipientWarning(2);
        QVERIFY2(!w2.isEmpty(), "a multi-recipient document must produce a warning");
        QVERIFY2(w2.contains(QStringLiteral("2")), "the warning must carry the recipient count");
        QVERIFY2(w2.contains(QStringLiteral("session key"), Qt::CaseInsensitive),
                 "the warning must disclose the session-key change (ER-3)");
    }
};

QTEST_MAIN(TestCertEncryptPicker)
#include "TestCertEncryptPicker.moc"
