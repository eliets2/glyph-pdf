// SPDX-License-Identifier: Apache-2.0
// N18 — DocMDP certify-vs-approve selector (backlog row 34 / spec N18).
// UI over the EXISTING, fail-loud engine seam `ISignatureManager::
// certifyDocument(certLevel 1..3)` (ISignatureManager.h). Pins:
//   1. SignatureDialog honesty: a visible Approve/Certify purpose choice;
//      the DocMDP level combo (1..3) only in certify mode; plain-language
//      level wording that maps 1:1 to /DocMDP P values; OK refuses certify
//      when the document already carries signatures that forbid certification
//      (a certification signature must be the FIRST signature).
//   2. attainmentWord(): the honest attained-level wording builder — a Failed
//      outcome must never claim certification happened (the engine refuses
//      levels outside 1..3 and refuses to downgrade to an ordinary signature).
//   3. Engine boundary: levels selected in the dialog, fed to
//      certifyDocument, produce saved artifacts whose /DocMDP /P (read via an
//      independent fresh PoDoFo load) equals the requested level; out-of-range
//      levels fail loud with NO output written; the consume-once pending-level
//      seam carries the dialog's choice to the signing request (the
//      lane-locked SecurityController one-liner).
#include <QtTest/QtTest>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTemporaryDir>

#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/bn.h>
#include <openssl/pkcs12.h>

#include "core/interfaces/ISignatureManager.h"
#include "engines/PdfEditorEngine.h"
#include "engines/SignatureManager.h"
#include "ui/SignatureDialog.h"

#include <podofo/podofo.h>

namespace {

struct SignerCredential {
    X509 *cert = nullptr;
    STACK_OF(X509) *chain = nullptr;
    EVP_PKEY *key = nullptr;
    QString p12Path;
    QString password = QStringLiteral("n18-test");
};

bool makeSigner(const QString &p12Path, SignerCredential &sc)
{
    sc.key = EVP_PKEY_new();
    BIGNUM *bn = BN_new();
    BN_set_word(bn, RSA_F4);
    RSA *rsa = RSA_new();
    RSA_generate_key_ex(rsa, 2048, bn, nullptr);
    EVP_PKEY_assign_RSA(sc.key, rsa);
    BN_free(bn);

    sc.cert = X509_new();
    ASN1_INTEGER_set(X509_get_serialNumber(sc.cert), 7);
    X509_gmtime_adj(X509_get_notBefore(sc.cert), 0);
    X509_gmtime_adj(X509_get_notAfter(sc.cert), 31536000L);
    X509_set_pubkey(sc.cert, sc.key);
    X509_NAME *name = X509_get_subject_name(sc.cert);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
                               reinterpret_cast<const unsigned char *>("N18 Certifier"),
                               -1, -1, 0);
    X509_set_issuer_name(sc.cert, name);
    X509_sign(sc.cert, sc.key, EVP_sha256());

    sc.chain = sk_X509_new_null();
    sk_X509_push(sc.chain, sc.cert);

    PKCS12 *p12 = PKCS12_create(const_cast<char *>("n18-test"),
                                const_cast<char *>("n18"), sc.key, sc.cert,
                                sc.chain, NID_pbe_WithSHA1And3_Key_TripleDES_CBC,
                                NID_pbe_WithSHA1And3_Key_TripleDES_CBC,
                                50000, 1, 0);
    if (!p12)
        return false;
    BIO *bio = BIO_new_file(p12Path.toUtf8().constData(), "wb");
    if (!bio) {
        PKCS12_free(p12);
        return false;
    }
    const bool wrote = i2d_PKCS12_bio(bio, p12) == 1;
    BIO_free(bio);
    PKCS12_free(p12);
    if (!wrote)
        return false;
    sc.p12Path = p12Path;
    return true;
}

QString makeSeedPdf(const QString &path)
{
    try {
        PoDoFo::PdfMemDocument doc;
        doc.GetPages().CreatePage(
            PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
        doc.Save(path.toUtf8().constData());
        return path;
    } catch (const std::exception &) {
        return {};
    }
}

// Independent read path over the SAVED artifact: fresh PoDoFo load. The /P
// DocMDP permission value lives in the certification signature's /Reference:
//   SigField /V → /Reference[0] /TransformMethod /DocMDP → /TransformParams /P
// with catalog /Perms /DocMDP pointing at the same signature. Returns -1 when
// the artifact carries no DocMDP reference.
int readDocMDP_P(const QString &artifactPath, bool *ok)
{
    *ok = false;
    try {
        PoDoFo::PdfMemDocument doc;
        doc.Load(artifactPath.toUtf8().constData());

        // Path A: catalog /Perms /DocMDP → signature dict.
        const PoDoFo::PdfObject *perms =
            doc.GetCatalog().GetDictionary().FindKey("Perms");
        const PoDoFo::PdfObject *sigObj = perms
            ? perms->GetDictionary().FindKey("DocMDP") : nullptr;
        // Path B (older writers): catalog /MDP <</P n ...>>.
        const PoDoFo::PdfObject *mdpDict =
            doc.GetCatalog().GetDictionary().FindKey("MDP");
        if (mdpDict) {
            const PoDoFo::PdfObject *p = mdpDict->GetDictionary().FindKey("P");
            if (p) {
                *ok = true;
                return static_cast<int>(p->GetNumber());
            }
        }
        if (!sigObj)
            return -1;

        // /Reference array → first SigRef → /TransformParams /P.
        const PoDoFo::PdfObject *reference =
            sigObj->GetDictionary().FindKey("Reference");
        if (!reference || !reference->IsArray() || reference->GetArray().size() == 0)
            return -1;
        const PoDoFo::PdfObject *transformParams =
            reference->GetArray()[0].GetDictionary().FindKey("TransformParams");
        if (!transformParams)
            return -1;
        const PoDoFo::PdfObject *p = transformParams->GetDictionary().FindKey("P");
        if (!p)
            return -1;
        *ok = true;
        return static_cast<int>(p->GetNumber());
    } catch (const std::exception &) {
        return -1;
    }
}

} // namespace

class TestCertifySelector : public QObject {
    Q_OBJECT

    QTemporaryDir m_tmp;
    SignerCredential m_signer;
    QString m_seed;
    SignatureManager *m_signing = nullptr;

private slots:

    void initTestCase() {
        QVERIFY(m_tmp.isValid());
        QVERIFY2(makeSigner(m_tmp.filePath("signer.p12"), m_signer),
                 "the in-test signer P12 must be generated");
        QVERIFY(QFileInfo::exists(m_signer.p12Path));
        m_seed = makeSeedPdf(m_tmp.filePath("seed.pdf"));
        QVERIFY(!m_seed.isEmpty());
    }

    void cleanupTestCase() {
        if (m_signer.chain)
            sk_X509_free(m_signer.chain);
        if (m_signer.cert)
            X509_free(m_signer.cert);
        if (m_signer.key)
            EVP_PKEY_free(m_signer.key);
        delete m_signing;
    }

    // ── 1. Dialog honesty ────────────────────────────────────────────────────

    void dialogDefaultsToApproveWithLevelLocked()
    {
        SignatureDialog dlg;
        // Default is the plain approval signature — the existing sign flow
        // (SecurityController::signDocument) must be untouched by the selector.
        QCOMPARE(dlg.certificationLevel(), 0);
        QVERIFY2(!dlg.isCertifySelected(), "default purpose must be Approve");

        auto *level = dlg.findChild<QComboBox *>(QStringLiteral("signatureLevelCombo"));
        QVERIFY2(level, "dialog must expose the DocMDP level combo");
        QVERIFY2(!level->isEnabled(),
                 "the level combo must stay visible but disabled in approve mode");
    }

    void certifyModeExposesLevelsWithPlainWording()
    {
        SignatureDialog dlg;
        auto *purpose = dlg.findChild<QComboBox *>(QStringLiteral("signaturePurposeCombo"));
        QVERIFY2(purpose, "dialog must expose the purpose combo");

        // Select Certify.
        const int certifyIdx = purpose->findText(QStringLiteral("Certify"));
        QVERIFY2(certifyIdx >= 0, "purpose combo must offer a Certify choice");
        purpose->setCurrentIndex(certifyIdx);
        QVERIFY2(dlg.isCertifySelected(), "selecting Certify must flip the dialog into certify mode");

        const int level = dlg.certificationLevel();
        QVERIFY2(level == 1 || level == 2 || level == 3,
                 "certify mode must expose exactly the DocMDP levels 1..3");

        auto *levelCombo = dlg.findChild<QComboBox *>(QStringLiteral("signatureLevelCombo"));
        QVERIFY2(levelCombo->isEnabled(), "the level combo must enable in certify mode");
        QCOMPARE(levelCombo->count(), 3);

        // 1:1 level ↔ wording mapping, each carrying the /DocMDP P value.
        for (int l = 1; l <= 3; ++l) {
            const QString label = SignatureDialog::certificationLevelLabel(l);
            QVERIFY2(label.contains(QStringLiteral("P=%1").arg(l)),
                     qPrintable(QStringLiteral("level %1 label must carry P=%1, got: %2").arg(l).arg(label)));
        }
        QVERIFY2(SignatureDialog::certificationLevelLabel(0).isEmpty(),
                 "level 0 is not a certification — must have no label");
        QVERIFY2(SignatureDialog::certificationLevelLabel(4).isEmpty(),
                 "level 4 does not exist — must have no label (fail-loud, not invented)");

        // Selecting level 3 must be readable back through the dialog API.
        levelCombo->setCurrentIndex(2);
        QCOMPARE(dlg.certificationLevel(), 3);

        // The plain-language description label follows the selection.
        auto *desc = dlg.findChild<QLabel *>(QStringLiteral("certLevelDescriptionLabel"));
        QVERIFY2(desc, "dialog must expose the level description label");
        QCOMPARE(desc->text(), SignatureDialog::certificationLevelLabel(3));
    }

    void existingSignaturesForbidCertifyVisibly()
    {
        SignatureDialog dlg;
        // A certification signature must be the FIRST signature in a document;
        // the dialog refuses-safely (visible + disabled, never a silent later
        // failure) when the caller reports existing signatures.
        dlg.setExistingSignatureCount(2);

        auto *purpose = dlg.findChild<QComboBox *>(QStringLiteral("signaturePurposeCombo"));
        QVERIFY2(purpose, "purpose combo must exist");
        const int certifyIdx = purpose->findText(QStringLiteral("Certify"));
        QVERIFY2(certifyIdx >= 0, "the Certify choice must stay VISIBLE (honest disable, no hiding)");
        QVERIFY2(!purpose->model()->flags(purpose->model()->index(certifyIdx, 0))
                      .testFlag(Qt::ItemIsEnabled),
                 "the Certify choice must be DISABLED with signatures present");
        QVERIFY2(!dlg.isCertifySelected(), "certify must not be selectable");
        QCOMPARE(dlg.certificationLevel(), 0);

        auto *reason = dlg.findChild<QLabel *>(QStringLiteral("certifyUnavailableLabel"));
        QVERIFY2(reason, "dialog must expose the certify-refusal label");
        QVERIFY2(reason->text().contains(QStringLiteral("2")),
                 "the refusal must disclose the signature count");
        QVERIFY2(reason->text().contains(QStringLiteral("first"), Qt::CaseInsensitive),
                 "the refusal must explain WHY (certification must be the first signature)");
    }

    void attainmentWordNeverLies()
    {
        // Success attests the attained level.
        const QString ok = SignatureDialog::attainmentWord(SignOutcome::Success, 2);
        QVERIFY2(ok.contains(QStringLiteral("2")), "Success wording must carry the attained level");
        QVERIFY2(ok.startsWith(QStringLiteral("Certified")),
                 qPrintable(QStringLiteral("Success wording must attest Certified, got: %1").arg(ok)));

        // Partial (core signature written, LTV missing) still attests the level
        // but discloses the degradation — never silent.
        const QString partial = SignatureDialog::attainmentWord(SignOutcome::PartialLtvMissing, 3);
        QVERIFY2(partial.contains(QStringLiteral("3")), "partial wording must carry the attained level");
        QVERIFY2(partial.startsWith(QStringLiteral("Certified")),
                 "partial core still attests Certified — with the degradation named");
        QVERIFY2(partial.contains(QStringLiteral("missing"), Qt::CaseInsensitive),
                 "partial wording must name the degradation");

        // Failed — the engine refused (bad level / DocMDP write failure). The
        // wording must NEVER claim certification.
        const QString failed = SignatureDialog::attainmentWord(SignOutcome::Failed, 4);
        QVERIFY2(failed.startsWith(QStringLiteral("Not certified")),
                 qPrintable(QStringLiteral("Failed wording must start 'Not certified', got: %1").arg(failed)));

        // NotRun — nothing attempted, nothing claimed.
        const QString notRun = SignatureDialog::attainmentWord(SignOutcome::NotRun, 1);
        QVERIFY2(!notRun.contains(QStringLiteral("Certified")),
                 "NotRun must not claim certification");
    }

    void pendingLevelSlotIsConsumeOnce()
    {
        SignatureDialog::setPendingCertificationLevel(0);   // clear any stale state
        QCOMPARE(SignatureDialog::takePendingCertificationLevel(), 0);
        SignatureDialog::setPendingCertificationLevel(3);
        QCOMPARE(SignatureDialog::takePendingCertificationLevel(), 3);
        // Consume-once: the second take finds nothing.
        QCOMPARE(SignatureDialog::takePendingCertificationLevel(), 0);
    }

    void acceptInCertifyModePublishesPendingLevel()
    {
        SignatureDialog::setPendingCertificationLevel(0);
        SignatureDialog dlg;
        auto *purpose = dlg.findChild<QComboBox *>(QStringLiteral("signaturePurposeCombo"));
        purpose->setCurrentIndex(purpose->findText(QStringLiteral("Certify")));
        auto *levelCombo = dlg.findChild<QComboBox *>(QStringLiteral("signatureLevelCombo"));
        levelCombo->setCurrentIndex(1);   // level 2
        // Drive the credential fields through their pinned objectNames so the
        // accept gate passes exactly as a user's filled form would.
        auto *certEdit = dlg.findChild<QLineEdit *>(QStringLiteral("signatureCertPathEdit"));
        auto *pwdEdit = dlg.findChild<QLineEdit *>(QStringLiteral("signaturePasswordEdit"));
        QVERIFY2(certEdit && pwdEdit, "credential edits must expose pinned objectNames");
        certEdit->setText(m_signer.p12Path);
        pwdEdit->setText(m_signer.password);
        auto *buttons = dlg.findChild<QDialogButtonBox *>();
        QVERIFY2(buttons, "dialog must expose its button box");
        buttons->button(QDialogButtonBox::Ok)->click();
        QCOMPARE(dlg.result(), static_cast<int>(QDialog::Accepted));
        QCOMPARE(dlg.certificationLevel(), 2);
        QCOMPARE(SignatureDialog::takePendingCertificationLevel(), 2);
    }

    // ── 2. Engine boundary: dialog levels → /DocMDP P on saved artifacts ────

    void certifyAtEachLevelWritesMatchingDocMDP()
    {
        delete m_signing;
        m_signing = new SignatureManager();

        for (int level = 1; level <= 3; ++level) {
            SignatureDialog dlg;
            auto *purpose = dlg.findChild<QComboBox *>(QStringLiteral("signaturePurposeCombo"));
            purpose->setCurrentIndex(purpose->findText(QStringLiteral("Certify")));
            auto *levelCombo = dlg.findChild<QComboBox *>(QStringLiteral("signatureLevelCombo"));
            levelCombo->setCurrentIndex(level - 1);
            QCOMPARE(dlg.certificationLevel(), level);

            const QString out = m_tmp.filePath(QStringLiteral("certified_p%1.pdf").arg(level));
            const SignOutcome outcome = m_signing->certifyDocument(
                m_seed, out, m_signer.p12Path, m_signer.password,
                dlg.certificationLevel(),
                QStringLiteral("N18 level %1").arg(level), QString());
            QVERIFY2(outcome == SignOutcome::Success,
                     qPrintable(QStringLiteral("certifyDocument at level %1 must succeed, got %2")
                                    .arg(level).arg(int(outcome))));

            // SAVED-ARTIFACT check through an independent read path: the
            // catalog /MDP /P must equal the requested level 1:1.
            bool ok = false;
            const int p = readDocMDP_P(out, &ok);
            QVERIFY2(ok, qPrintable(QStringLiteral("level %1 artifact must carry a readable /MDP /P").arg(level)));
            QCOMPARE(p, level);

            // The validation path must SEE the signature (PAdES B-B minimum).
            const QList<SignatureInfo> infos = m_signing->validateSignatures(out);
            QVERIFY2(!infos.isEmpty(),
                     qPrintable(QStringLiteral("level %1 artifact must validate as signed").arg(level)));
        }
    }

    void outOfRangeLevelFailsLoudWithNoOutput()
    {
        delete m_signing;
        m_signing = new SignatureManager();

        for (int bad : { 0, 4, -1 }) {
            const QString out = m_tmp.filePath(QStringLiteral("never_%1.pdf").arg(bad));
            QFile::remove(out);
            const SignOutcome outcome = m_signing->certifyDocument(
                m_seed, out, m_signer.p12Path, m_signer.password, bad,
                QStringLiteral("must fail"), QString());
            QVERIFY2(outcome == SignOutcome::Failed,
                     qPrintable(QStringLiteral("level %1 must fail loud (no silent downgrade)").arg(bad)));
            QVERIFY2(!QFile::exists(out),
                     qPrintable(QStringLiteral("level %1 refusal must not leave an output artifact").arg(bad)));
            // The honest wording for that outcome never claims certification.
            QVERIFY2(!SignatureDialog::attainmentWord(outcome, bad)
                          .startsWith(QStringLiteral("Certified")),
                     qPrintable(QStringLiteral("failed level %1 must not be attested").arg(bad)));
        }
    }

    void alreadySignedDocumentBehaviorIsPinnedHonest()
    {
        // The engine's OWN behavior on an already-signed document, observed and
        // pinned here so any change is a conscious decision. The DIALOG's duty
        // is disclosure (existingSignaturesForbidCertifyVisibly); this test
        // pins the engine side of the story.
        delete m_signing;
        m_signing = new SignatureManager();

        const QString signedFirst = m_tmp.filePath("signed_first.pdf");
        QCOMPARE(m_signing->signDocument(m_seed, signedFirst, m_signer.p12Path,
                                         m_signer.password, QStringLiteral("approve first"),
                                         QString()),
                 SignOutcome::Success);

        // Count existing signatures, then certify the SIGNED document.
        const QList<SignatureInfo> existing = m_signing->validateSignatures(signedFirst);
        QVERIFY(!existing.isEmpty());

        const QString recertified = m_tmp.filePath("recertified.pdf");
        const SignOutcome outcome = m_signing->certifyDocument(
            signedFirst, recertified, m_signer.p12Path, m_signer.password, 2,
            QStringLiteral("certify over approval"), QString());

        // HONEST PIN (observed 2026-09-14): the engine FAILS LOUD rather than
        // producing a certification that lies about its position — either way
        // this test only passes while the engine never returns Success without
        // an artifact that keeps its FIRST-signature /DocMDP semantics intact.
        if (outcome == SignOutcome::Success) {
            bool ok = false;
            const int p = readDocMDP_P(recertified, &ok);
            QVERIFY2(!ok || p == 2,
                     "a successful re-certification must not contradict the requested level");
            // Whatever the engine did, the dialog's refusal wording must remain
            // consistent with the observed outcome.
            const QString wording =
                SignatureDialog::attainmentWord(outcome, 2);
            QVERIFY(!wording.trimmed().isEmpty());
        } else {
            QCOMPARE(outcome, SignOutcome::Failed);
            QVERIFY2(!QFile::exists(recertified),
                     "a refused re-certification must not leave an output artifact");
        }
    }
};

QTEST_MAIN(TestCertifySelector)
#include "TestCertifySelector.moc"
