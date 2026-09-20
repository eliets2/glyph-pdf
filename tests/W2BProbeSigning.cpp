// SPDX-License-Identifier: MIT
// W2BProbeSigning.cpp — SWEEP-W2B INDEPENDENT verifier probe
//                     (W1-02, W1-03, F3, F5, FZ-3, F2).
//
// Written by the W2B guarantee-verification-engine against the SAVED tip
// (feat/sweep-w2-verify-b @ 2d29a16). Independent of the fix lane's repros:
// different hostile inputs, different fixtures, own expected literals.
// Asserts the FIXED contracts:
//
//   W1-02: fromJson refuses duplicate fieldName bindings while any aliased
//     entry is unsigned (SchemaInvalid, detail names both entries); a record
//     whose aliased entries ALL claim completion parses as history but
//     verifyAgainstDocument flags the alias as a warning and reports
//     consistent=false — never a silent false-success.
//   W1-03: precheck consults the engine's GLOBAL one-unsigned-field
//     precondition and refuses an UNMANAGED unsigned field
//     (StepRefusal::ForeignUnsignedField) BEFORE any mutation — the document
//     stays byte-identical across a refused precheck.
//   F3: a sidecar-pre-seeded reconfirmedSha256 never gates; the mutation gate
//     honors ONLY FillStepInput::userReconfirmedSha256 (out-of-band consent).
//   F5: a signature-field anchor outside the page MediaBox is REFUSED with an
//     honest error; the document gains no field.
//   FZ-3: the handshake verifies the magic VALUE ("glyphpdf-signrequest": 1);
//     999 / 1.5 / true / null / "one" / trailing-duplicate-2 all refuse
//     (MissingMagic, detail names found vs required).
//   F2: attainedLevelLabel floors to B-B exactly when a timestamp was
//     ATTEMPTED and the token did not parse; a validated token attains B-T;
//     an absent attempt never floors (pre-sign previews stay B-T). The
//     positive/negative TS_RESP fixture below is a REAL RFC 3161 response
//     (openssl ts, verified) / openssl-invalid garbage, grounding both
//     branches of the gate the fix installed.
//
// NC bases (docs/audit/SWEEP-W2B-VERIFY-2026-09-20.md):
//   W1-02 → 95a5044^   W1-03 → 8726cb9^   F3 → ee6ba95^
//   F5  → 2e4146d^     FZ-3  → 824532e^   F2 (label gate) → 7a43c3c^

#include <QtTest/QtTest>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <memory>

#include "core/SigningRequestModel.h"
#include "core/SigningRequestRunner.h"
#include "core/interfaces/ISignatureManager.h"
#include "engines/SignatureFieldCreator.h"
#include "engines/SignatureManager.h"
#include "shell/controllers/SecurityController.h"

#include <podofo/podofo.h>

#include <openssl/asn1.h>
#include <openssl/ts.h>

#ifdef SOURCE_DIR
static const QString kFixtureDir = QStringLiteral(SOURCE_DIR "/tests/fixtures/signing");
#else
static const QString kFixtureDir = QStringLiteral("tests/fixtures/signing");
#endif
static const QString kInputPdf = kFixtureDir + "/test_input.pdf";

#define REQUIRE_FIXTURE() \
    do { \
        if (!QFileInfo::exists(kInputPdf)) \
            QSKIP("tests/fixtures/signing/test_input.pdf missing."); \
    } while (0)

namespace {

QString sha256OfFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    return QCryptographicHash::hash(f.readAll(), QCryptographicHash::Sha256).toHex();
}

SigningRequestModel::Signer makeSigner(const QString &name, const QString &field,
                                       bool created)
{
    SigningRequestModel::Signer s;
    s.name = name;
    s.fieldName = field;
    s.anchorPage = 0;
    s.anchorRect = QRectF(60.0, 60.0, 150.0, 50.0); // inside any test page
    s.createdField = created;
    return s;
}

// An all-JSON sidecar with the given signer entries (raw JSON, NOT through
// the model — the bytes are the attacker's).
QByteArray sidecarJson(const QList<QPair<QString, QString>>& fieldAndName,
                       const QString& preparedSha, bool claimSigned,
                       const QString& reconfirmedSha = QString())
{
    QJsonObject root;
    root.insert(QStringLiteral("glyphpdf-signrequest"), 1);
    root.insert(QStringLiteral("schemaVersion"), 1);
    root.insert(QStringLiteral("createdUtc"), QStringLiteral("2026-09-20T00:00:00Z"));
    root.insert(QStringLiteral("preparedUtc"), QStringLiteral("2026-09-20T00:00:00Z"));
    root.insert(QStringLiteral("preparedSha256"), preparedSha);
    root.insert(QStringLiteral("sourcePdf"), QStringLiteral("doc.pdf"));
    if (!reconfirmedSha.isEmpty())
        root.insert(QStringLiteral("reconfirmedSha256"), reconfirmedSha);
    QJsonArray signers;
    for (int i = 0; i < fieldAndName.size(); ++i) {
        QJsonObject s;
        s.insert(QStringLiteral("order"), i + 1);
        s.insert(QStringLiteral("name"), fieldAndName[i].second);
        s.insert(QStringLiteral("fieldName"), fieldAndName[i].first);
        s.insert(QStringLiteral("createdField"), false);
        s.insert(QStringLiteral("signed"), claimSigned);
        s.insert(QStringLiteral("signedAtUtc"), QStringLiteral("2026-09-20T01:00:00Z"));
        s.insert(QStringLiteral("signedFieldName"), fieldAndName[i].first);
        s.insert(QStringLiteral("fieldMatch"), true);
        s.insert(QStringLiteral("attainedLevel"), QStringLiteral("B-B"));
        s.insert(QStringLiteral("signatureSummary"), QStringLiteral("integrity intact"));
        signers.append(s);
    }
    root.insert(QStringLiteral("signers"), signers);
    return QJsonDocument(root).toJson();
}

// Real RFC 3161 TS_RESP (DER), generated 2026-09-20 with `openssl ts -reply`
// (TSA cert with critical timeStamping EKU); `openssl ts -verify` OK against
// its signer. .context/sweep-w2b-evidence/resp.{ts,hex}.
const unsigned char kRealTsRespDer[] = {
// TSRESP_HEX_START
0x30,0x82,0x02,0xb1,0x30,0x03,0x02,0x01,0x00,0x30,0x82,0x02,
0xa8,0x06,0x09,0x2a,0x86,0x48,0x86,0xf7,0x0d,0x01,0x07,0x02,
0xa0,0x82,0x02,0x99,0x30,0x82,0x02,0x95,0x02,0x01,0x03,0x31,
0x0f,0x30,0x0d,0x06,0x09,0x60,0x86,0x48,0x01,0x65,0x03,0x04,
0x02,0x01,0x05,0x00,0x30,0x81,0x8a,0x06,0x0b,0x2a,0x86,0x48,
0x86,0xf7,0x0d,0x01,0x09,0x10,0x01,0x04,0xa0,0x7b,0x04,0x79,
0x30,0x77,0x02,0x01,0x01,0x06,0x04,0x2a,0x03,0x04,0x01,0x30,
0x31,0x30,0x0d,0x06,0x09,0x60,0x86,0x48,0x01,0x65,0x03,0x04,
0x02,0x01,0x05,0x00,0x04,0x20,0x18,0x31,0x45,0xe8,0x36,0xff,
0x4d,0xdf,0x03,0xde,0x4c,0xc2,0xcf,0xc9,0x94,0xb2,0xad,0xe8,
0x7c,0xa8,0xc9,0x1e,0x75,0x24,0x9f,0x6c,0x3c,0x97,0x64,0x65,
0x1a,0xf9,0x02,0x01,0x01,0x18,0x0f,0x32,0x30,0x32,0x36,0x30,
0x39,0x32,0x30,0x31,0x33,0x35,0x32,0x30,0x35,0x5a,0x30,0x03,
0x02,0x01,0x01,0x02,0x08,0x13,0xa2,0xfd,0x2d,0x56,0xde,0x92,
0x6c,0xa0,0x16,0xa4,0x14,0x30,0x12,0x31,0x10,0x30,0x0e,0x06,
0x03,0x55,0x04,0x03,0x0c,0x07,0x57,0x32,0x42,0x20,0x54,0x53,
0x41,0x31,0x82,0x01,0xf0,0x30,0x82,0x01,0xec,0x02,0x01,0x01,
0x30,0x2a,0x30,0x12,0x31,0x10,0x30,0x0e,0x06,0x03,0x55,0x04,
0x03,0x0c,0x07,0x57,0x32,0x42,0x20,0x54,0x53,0x41,0x02,0x14,
0x6c,0x17,0x13,0xc5,0x50,0x84,0x6b,0x70,0x2b,0xa4,0x60,0x12,
0x9e,0x8a,0xc5,0xd5,0x57,0xd5,0xc0,0x7c,0x30,0x0d,0x06,0x09,
0x60,0x86,0x48,0x01,0x65,0x03,0x04,0x02,0x01,0x05,0x00,0xa0,
0x81,0x98,0x30,0x1a,0x06,0x09,0x2a,0x86,0x48,0x86,0xf7,0x0d,
0x01,0x09,0x03,0x31,0x0d,0x06,0x0b,0x2a,0x86,0x48,0x86,0xf7,
0x0d,0x01,0x09,0x10,0x01,0x04,0x30,0x1c,0x06,0x09,0x2a,0x86,
0x48,0x86,0xf7,0x0d,0x01,0x09,0x05,0x31,0x0f,0x17,0x0d,0x32,
0x36,0x30,0x39,0x32,0x30,0x31,0x33,0x35,0x32,0x30,0x35,0x5a,
0x30,0x2b,0x06,0x0b,0x2a,0x86,0x48,0x86,0xf7,0x0d,0x01,0x09,
0x10,0x02,0x0c,0x31,0x1c,0x30,0x1a,0x30,0x18,0x30,0x16,0x04,
0x14,0xe5,0xed,0xba,0x88,0x68,0xc2,0x1e,0x63,0x73,0x9d,0xf6,
0xf6,0xd9,0x03,0x83,0x63,0xa5,0xbb,0x7c,0xd8,0x30,0x2f,0x06,
0x09,0x2a,0x86,0x48,0x86,0xf7,0x0d,0x01,0x09,0x04,0x31,0x22,
0x04,0x20,0x15,0xeb,0xc2,0x92,0xa6,0xf4,0x43,0xff,0x03,0x54,
0x20,0x38,0x33,0x02,0x48,0xc4,0x10,0xa5,0xb1,0xa2,0x0a,0x91,
0x95,0x3c,0xa6,0x43,0xbf,0xcf,0x0d,0xd3,0x6a,0xb8,0x30,0x0d,
0x06,0x09,0x2a,0x86,0x48,0x86,0xf7,0x0d,0x01,0x01,0x01,0x05,
0x00,0x04,0x82,0x01,0x00,0x5c,0x93,0x86,0xf4,0x9c,0xe6,0xd1,
0xfb,0x7c,0xe8,0x1c,0x7d,0x9e,0xb7,0x33,0x76,0x2f,0x91,0x87,
0x7b,0x2a,0x7b,0xaa,0xc2,0xcd,0x16,0xb3,0xf1,0xe9,0xcf,0xaf,
0x86,0x67,0xdc,0xd1,0x8e,0x3d,0x39,0x5f,0x6e,0x03,0x20,0xcc,
0x1a,0x9b,0x59,0x48,0xad,0x06,0x4b,0x6b,0xce,0x03,0x28,0x4e,
0x95,0x65,0xe8,0x51,0x87,0x3c,0x90,0xa9,0xd6,0xc5,0x26,0xb9,
0x50,0x9a,0x06,0x2f,0x07,0x18,0x52,0xe5,0xa0,0x53,0xc4,0xe5,
0x03,0xfc,0x39,0x50,0x18,0x92,0x4a,0xa2,0x15,0x8a,0x2f,0x9f,
0x3d,0x9f,0xf5,0x85,0xa6,0x9d,0x05,0xd7,0x59,0x24,0x24,0xea,
0xbe,0xaf,0xd7,0x36,0x8b,0x3d,0x24,0x99,0x99,0xf6,0x35,0x5e,
0x75,0x70,0x76,0x56,0x3d,0x4a,0x9f,0xc8,0xec,0xe6,0xff,0xbd,
0xa6,0x18,0x02,0xaa,0x33,0x2e,0x1d,0x56,0xaa,0x71,0x0d,0x28,
0x7a,0x0d,0x9c,0x84,0x0a,0x2b,0x1d,0x80,0x1b,0x3e,0x44,0xe5,
0x2f,0xc3,0x31,0x2b,0xfe,0x37,0xc9,0xfd,0x8e,0x43,0x10,0x68,
0xf5,0xe6,0x47,0xfb,0x96,0xd6,0xcd,0x3b,0xe1,0x0e,0x97,0xbc,
0x1f,0x59,0x12,0xf8,0x0f,0x9a,0xae,0x0d,0x4b,0x7a,0x8e,0xbf,
0xbe,0x0f,0x81,0xeb,0x2f,0x0e,0x4d,0xee,0x35,0x23,0xcf,0x82,
0xa9,0x2d,0xb0,0x5a,0x93,0x52,0xf6,0x65,0x5f,0xdb,0x45,0xb7,
0xdc,0x4c,0x84,0x79,0x2a,0x61,0xe3,0x27,0xd0,0x86,0xb3,0x8a,
0x36,0xbf,0xc2,0xf2,0x61,0xaf,0x12,0xbf,0xa6,0xf5,0x57,0x3f,
0x84,0x44,0x8d,0xf4,0x64,0x5e,0x87,0x0f,0x11,0xbf,0x85,0x08,
0x01,0xa6,0xd9,0x5b,0x23,0xe6,0xf5,0xc7,0x29,
// TSRESP_HEX_END
};

} // namespace

class W2BProbeSigning : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<QTemporaryDir> newTmp(const char *tag)
    {
        for (int attempt = 0; attempt < 2; ++attempt) {
            auto d = std::make_unique<QTemporaryDir>(
                QDir::tempPath() + QStringLiteral("/w2bprobe-%1-XXXXXX").arg(tag));
            if (d->isValid()) return d;
        }
        return nullptr;
    }

private slots:
    // ── W1-02: the alias lint at the untrusted boundary ─────────────────────
    void fromJsonRefusesAliasedUnsignedBindings()
    {
        // Whitespace must not launder the alias either (the lint trims).
        const auto body = sidecarJson(
            { { QStringLiteral("sig_A"), QStringLiteral("Signer 1") },
              { QStringLiteral(" sig_A "), QStringLiteral("Signer 2") } },
            QStringLiteral("deadbeef"), /*claimSigned*/ false);
        const auto r = SigningRequestModel::fromJson(QString::fromUtf8(body));
        QVERIFY2(r.error == SigningRequestModel::LoadError::SchemaInvalid,
                 qPrintable(QStringLiteral(
                     "W1-02 REGRESSION: fromJson ACCEPTED two unsigned signers "
                     "bound to the same field (error=%1) — the alias spoof is "
                     "back at the untrusted boundary").arg(int(r.error))));
        QVERIFY2(r.detail.contains(QStringLiteral("bound more than once")),
                 qPrintable(QStringLiteral("refusal detail: %1").arg(r.detail)));
    }

    void fromJsonAcceptsDistinctBindingsControl()
    {
        const auto body = sidecarJson(
            { { QStringLiteral("sig_A"), QStringLiteral("Signer 1") },
              { QStringLiteral("sig_B"), QStringLiteral("Signer 2") } },
            QStringLiteral("deadbeef"), false);
        const auto r = SigningRequestModel::fromJson(QString::fromUtf8(body));
        QCOMPARE(r.error, SigningRequestModel::LoadError::None);
        QCOMPARE(r.model.signers.size(), 2);
    }

    void verifyFlagsAliasedCompletedHistory()
    {
        REQUIRE_FIXTURE();
        // An aliased record where BOTH entries claim completion parses as
        // history (the parse lint scopes to unsigned aliases) — its truth is
        // audited: verifyAgainstDocument must warn and refuse "consistent".
        const auto body = sidecarJson(
            { { QStringLiteral("sig_A"), QStringLiteral("Signer 1") },
              { QStringLiteral("sig_A"), QStringLiteral("Signer 2") } },
            sha256OfFile(kInputPdf), /*claimSigned*/ true);
        const auto r = SigningRequestModel::fromJson(QString::fromUtf8(body));
        QCOMPARE(r.error, SigningRequestModel::LoadError::None);

        SignatureManager mgr;
        const auto rep = SigningRequestRunner::verifyAgainstDocument(
            mgr, r.model, kInputPdf);
        QVERIFY2(!rep.consistent,
                 "W1-02 REGRESSION: an aliased all-signed sidecar verified "
                 "CONSISTENT — the false-success spoof is back");
        bool aliasWarning = false;
        for (const QString& w : rep.warnings)
            if (w.contains(QStringLiteral("same field"))) aliasWarning = true;
        QVERIFY2(aliasWarning,
                 qPrintable(QStringLiteral(
                     "expected the alias audit warning, got %1 warning(s): %2")
                     .arg(rep.warnings.size())
                     .arg(rep.warnings.join(QStringLiteral(" | ")))));
    }

    // ── W1-03: global one-unsigned-field precondition, before mutation ─────
    void precheckRefusesForeignUnsignedFieldBeforeMutation()
    {
        REQUIRE_FIXTURE();
        auto tmp = newTmp("w103");
        QVERIFY(tmp);
        const QString doc = tmp->filePath(QStringLiteral("doc.pdf"));
        QVERIFY(QFile::copy(kInputPdf, doc));

        // A foreign UNSIGNED field no request entry binds (attacker-placed or
        // stale). Placed via the same creator the fill flow uses.
        QVector<gp::SignatureFieldCreator::Spec> foreign;
        foreign.append({ QStringLiteral("U_foreign"), 0, QRectF(72.0, 400.0, 150.0, 60.0) });
        QString ferr;
        QVERIFY(gp::SignatureFieldCreator::createSignatureFields(doc, foreign, doc, &ferr));
        const QString shaWithForeign = sha256OfFile(doc);

        SigningRequestModel m;
        m.createdUtc = m.preparedUtc = QStringLiteral("2026-09-20T00:00:00Z");
        m.preparedSha256 = shaWithForeign;              // honest: binds CURRENT bytes
        // createdField=true == the lazy-anchor semantics (the field does not
        // exist yet; the step creates it at the anchor) — same as the
        // committed repro. With createdField=false the BINDING gate would
        // refuse MissingField before the W1-03 global gate ever runs.
        m.signers.append(makeSigner(QStringLiteral("Signer W"),
                                    QStringLiteral("W_lazy"), true));

        SignatureManager mgr;
        SigningRequestRunner::FillStepInput in;
        in.docPath = doc;
        in.model = m;
        in.signerIndex = 0;
        in.requestedLevel = PAdESLevel::B_B;

        const auto pre = SigningRequestRunner::precheck(mgr, in);
        QVERIFY2(pre.code == SigningRequestRunner::StepRefusal::ForeignUnsignedField,
                 qPrintable(QStringLiteral(
                     "W1-03 REGRESSION: precheck returned %1 (expected "
                     "ForeignUnsignedField) — the step would mutate the "
                     "document and deadlock forever").arg(int(pre.code))));
        QVERIFY2(pre.message.contains(QStringLiteral("U_foreign")),
                 qPrintable(QStringLiteral("refusal must name the offender: %1").arg(pre.message)));
        // Pure-read contract: the refused precheck changed NOTHING.
        QCOMPARE(sha256OfFile(doc), shaWithForeign);
    }

    void precheckAcceptsManagedForeignField()
    {
        REQUIRE_FIXTURE();
        auto tmp = newTmp("w103c");
        QVERIFY(tmp);
        const QString doc = tmp->filePath(QStringLiteral("doc.pdf"));
        QVERIFY(QFile::copy(kInputPdf, doc));
        // The unsigned field is bound by ANOTHER entry of the SAME request —
        // the request's own business; the gate must NOT refuse it.
        QVector<gp::SignatureFieldCreator::Spec> own;
        own.append({ QStringLiteral("W2_bound"), 0, QRectF(72.0, 400.0, 150.0, 60.0) });
        QString ferr;
        QVERIFY(gp::SignatureFieldCreator::createSignatureFields(doc, own, doc, &ferr));

        SigningRequestModel m;
        m.createdUtc = m.preparedUtc = QStringLiteral("2026-09-20T00:00:00Z");
        m.preparedSha256 = sha256OfFile(doc);
        m.signers.append(makeSigner(QStringLiteral("S1"), QStringLiteral("W2_bound"), false));

        SignatureManager mgr;
        SigningRequestRunner::FillStepInput in;
        in.docPath = doc; in.model = m; in.signerIndex = 0;
        in.requestedLevel = PAdESLevel::B_B;
        const auto pre = SigningRequestRunner::precheck(mgr, in);
        QVERIFY2(pre.code != SigningRequestRunner::StepRefusal::ForeignUnsignedField,
                 "the global gate must not refuse a field the request itself binds");
    }

    // ── F3: sidecar-pre-seeded reconfirm can never skip the user ───────────
    void preseededSidecarReconfirmIsNotGateInput()
    {
        REQUIRE_FIXTURE();
        auto tmp = newTmp("f3");
        QVERIFY(tmp);
        const QString doc = tmp->filePath(QStringLiteral("doc.pdf"));
        QVERIFY(QFile::copy(kInputPdf, doc));

        const QString preparedSha = sha256OfFile(doc);
        // Attacker mutates the bytes…
        QVector<gp::SignatureFieldCreator::Spec> specs;
        specs.append({ QStringLiteral("Mutant"), 0, QRectF(72.0, 410.0, 140.0, 55.0) });
        QString cerr;
        QVERIFY(gp::SignatureFieldCreator::createSignatureFields(doc, specs, doc, &cerr));
        const QString mutatedSha = sha256OfFile(doc);
        QVERIFY(mutatedSha != preparedSha);

        // …and pre-seeds reconfirmedSha256 in the unsigned sidecar.
        const auto body = sidecarJson(
            { { QStringLiteral("sig_A"), QStringLiteral("Signer 1") } },
            preparedSha, false, /*reconfirmedSha*/ mutatedSha);
        const auto r = SigningRequestModel::fromJson(QString::fromUtf8(body));
        QCOMPARE(r.error, SigningRequestModel::LoadError::None);
        QCOMPARE(r.model.reconfirmedSha256, mutatedSha);   // the record is there…

        SignatureManager mgr;
        SigningRequestRunner::FillStepInput in;
        in.docPath = doc; in.model = r.model; in.signerIndex = 0;
        in.requestedLevel = PAdESLevel::B_B;
        // …but NEVER gates: no user dialog ran, so the step must refuse.
        const auto pre = SigningRequestRunner::precheck(mgr, in);
        QVERIFY2(pre.code == SigningRequestRunner::StepRefusal::DocumentChanged,
                 qPrintable(QStringLiteral(
                     "F3 REGRESSION: a sidecar-written reconfirmedSha256 let "
                     "precheck return %1 — the user was skipped and the "
                     "mutated bytes would be signed").arg(int(pre.code))));

        // Control: the OUT-OF-BAND channel (controller after its Yes/No
        // dialog) IS honored — the gate opens with user consent.
        SigningRequestRunner::FillStepInput in2 = in;
        in2.userReconfirmedSha256 = mutatedSha;
        const auto pre2 = SigningRequestRunner::precheck(mgr, in2);
        QVERIFY2(pre2.code != SigningRequestRunner::StepRefusal::DocumentChanged,
                 "the out-of-band user re-confirm must keep authorizing the step");
    }

    // ── F5: off-page anchors refused, honest error, no partial write ───────
    void offPageAnchorRefusedAndNothingPlaced()
    {
        REQUIRE_FIXTURE();
        auto tmp = newTmp("f5");
        QVERIFY(tmp);
        const QString doc = tmp->filePath(QStringLiteral("offpage.pdf"));
        QVERIFY(QFile::copy(kInputPdf, doc));

        QVector<gp::SignatureFieldCreator::Spec> specs;
        specs.append({ QStringLiteral("Invisible"), 0, QRectF(200000.0, 200000.0, 200.0, 100.0) });
        QString err;
        const bool created = gp::SignatureFieldCreator::createSignatureFields(doc, specs, doc, &err);
        QVERIFY2(!created,
                 "F5 REGRESSION: createSignatureFields accepted an off-page "
                 "anchor — an invisible field the signer can never see");
        QVERIFY2(err.contains(QStringLiteral("outside page")),
                 qPrintable(QStringLiteral("error must be honest: %1").arg(err)));

        // Independent read: the document gained NO field (no partial write).
        try {
            PoDoFo::PdfMemDocument d;
            d.Load(doc.toUtf8().constData());
            bool found = false;
            for (PoDoFo::PdfField* f : d.GetFieldsIterator())
                if (QString::fromStdString(f->GetFullName()) == QStringLiteral("Invisible"))
                    found = true;
            QVERIFY2(!found, "the refused anchor must not leave the field behind");
        } catch (const std::exception& e) {
            QFAIL(qPrintable(QString("reopen failed: %1").arg(e.what())));
        }

        // Control: an on-page anchor still places the field.
        QVector<gp::SignatureFieldCreator::Spec> ok;
        ok.append({ QStringLiteral("Visible"), 0, QRectF(72.0, 72.0, 150.0, 50.0) });
        QString err2;
        QVERIFY2(gp::SignatureFieldCreator::createSignatureFields(doc, ok, doc, &err2),
                 qPrintable(QStringLiteral("on-page anchor refused?! %1").arg(err2)));
    }

    // ── FZ-3: the magic VALUE is verified, not just the key ────────────────
    void magicValueIsVerifiedNotJustKey()
    {
        QList<QJsonValue> wrong = {
            QJsonValue(999.0), QJsonValue(1.5), QJsonValue(true),
            QJsonValue(), QJsonValue(QStringLiteral("one")),
        };
        for (const QJsonValue& v : wrong) {
            QJsonObject root;
            root.insert(QStringLiteral("glyphpdf-signrequest"), v);
            root.insert(QStringLiteral("schemaVersion"), 1);
            root.insert(QStringLiteral("preparedSha256"), QStringLiteral("x"));
            QJsonArray signers;
            root.insert(QStringLiteral("signers"), signers);
            const auto r = SigningRequestModel::fromJson(
                QString::fromUtf8(QJsonDocument(root).toJson()));
            QVERIFY2(r.error == SigningRequestModel::LoadError::MissingMagic,
                     qPrintable(QStringLiteral(
                         "FZ-3 REGRESSION: magic %1 decoded as a valid request "
                         "(error=%2) — the handshake no longer implements its "
                         "documented contract")
                         .arg(v.toVariant().toString(), int(r.error))));
            QVERIFY2(r.detail.contains(QStringLiteral("magic")),
                     qPrintable(QStringLiteral("detail should name the magic: %1").arg(r.detail)));
        }

        // The trailing-duplicate-key mutant (Qt keeps the LAST occurrence).
        const QByteArray dup =
            "{\n  \"glyphpdf-signrequest\": 1,\n  \"schemaVersion\": 1,\n"
            "  \"preparedSha256\": \"x\",\n  \"signers\": [],\n"
            "  \"glyphpdf-signrequest\": 2\n}\n";
        const auto r = SigningRequestModel::fromJson(QString::fromUtf8(dup));
        QVERIFY2(r.error == SigningRequestModel::LoadError::MissingMagic,
                 "FZ-3 REGRESSION: a duplicated magic with a doctored trailing "
                 "value sailed through — Qt's last-key-wins must hit the gate");

        // Control: the real pair decodes.
        const auto ok = SigningRequestModel::fromJson(QString::fromUtf8(
            sidecarJson({ { QStringLiteral("sig_A"), QStringLiteral("S") } },
                        QStringLiteral("x"), false)));
        QCOMPARE(ok.error, SigningRequestModel::LoadError::None);
    }

    // ── F2: the B-T+ gate is three-state ────────────────────────────────────
    void garbageTokenFloorsToBB()
    {
        SignatureOutcomeDetail garbage;
        garbage.timestampMissing = false;
        garbage.timestampAttempted = true;
        garbage.timestampTokenValid = false;
        QCOMPARE(gp::SecurityController::attainedLevelLabel(PAdESLevel::B_T, garbage),
                 QStringLiteral("B-B"));
    }

    void validatedTokenAttainsBT()
    {
        // The POSITIVE control (kills the always-floor false-pass): a token
        // that parsed as an RFC 3161 TS_RESP attains B-T. The fixture below
        // IS parseable — proven in the next slot.
        SignatureOutcomeDetail valid;
        valid.timestampMissing = false;
        valid.timestampAttempted = true;
        valid.timestampTokenValid = true;
        QCOMPARE(gp::SecurityController::attainedLevelLabel(PAdESLevel::B_T, valid),
                 QStringLiteral("B-T"));
        QCOMPARE(gp::SecurityController::attainedLevelLabel(PAdESLevel::B_B, valid),
                 QStringLiteral("B-B")); // label never exceeds the request
    }

    void absentAttemptDoesNotFloorPreviews()
    {
        // No timestamp configured this attempt: a B_T PREVIEW must read B-T
        // (the 2d29a16 refinement — the first F2 fix floored this case too).
        SignatureOutcomeDetail noAttempt;
        noAttempt.timestampMissing = false;
        noAttempt.timestampAttempted = false;
        noAttempt.timestampTokenValid = false;
        QCOMPARE(gp::SecurityController::attainedLevelLabel(PAdESLevel::B_T, noAttempt),
                 QStringLiteral("B-T"));
    }

    void unreachableTsaAttemptStillFloors()
    {
        SignatureOutcomeDetail unreachable;
        unreachable.timestampMissing = true;   // fetch failed / empty token
        unreachable.timestampAttempted = true;
        unreachable.timestampTokenValid = false;
        QCOMPARE(gp::SecurityController::attainedLevelLabel(PAdESLevel::B_T, unreachable),
                 QStringLiteral("B-B"));
    }

    // Grounds the two F2 fixture branches in the REAL OpenSSL decision the
    // engine uses at embed time (d2i_TS_RESP): the token fixture parses; the
    // garbage fixture does not.
    void tsRespFixtureGroundTruth()
    {
        const unsigned char* p = kRealTsRespDer;
        TS_RESP* resp = d2i_TS_RESP(nullptr, &p, (long)sizeof(kRealTsRespDer));
        QVERIFY2(resp != nullptr, "the B-T positive-control fixture must be a "
                                  "genuinely parseable RFC 3161 TS_RESP");
        if (resp) {
            TS_STATUS_INFO* st = TS_RESP_get_status_info(resp);
            // status 0 == granted (RFC 3161 PKIStatus)
            QCOMPARE(ASN1_INTEGER_get(TS_STATUS_INFO_get0_status(st)), 0);
            TS_RESP_free(resp);
        }
        // The garbage the committed probe models: ANY HTTP-200 body…
        const QByteArray garbage = "<html><body>503 Service Unavailable</body></html>";
        const unsigned char* g = reinterpret_cast<const unsigned char*>(garbage.constData());
        TS_RESP* bad = d2i_TS_RESP(nullptr, &g, garbage.size());
        QVERIFY2(bad == nullptr, "garbage must NOT parse as TS_RESP");
        TS_RESP_free(bad);
    }
};

QTEST_MAIN(W2BProbeSigning)
#include "W2BProbeSigning.moc"
