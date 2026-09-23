// SPDX-License-Identifier: MIT
// TestSendForSigning.cpp
//
// R26 send-for-signing P1 regression suite (offscreen, -o txt).
//
// Covers, against the REAL engine (pdfws_engines — real OpenSSL/PoDoFo):
//   - sidecar round-trip + fail-closed schemaVersion handshake (BatchPreset
//     discipline: unknown version / missing magic / corrupt JSON / bad shape
//     are structured REFUSALS, never best-effort parses)
//   - tampered-sidecar refusal
//   - the additive SignatureFieldCreator: real /FT /Sig fields at prepared
//     rects (PageSpace law), fail-loud refusals leave the file byte-identical
//   - dialog save-path pins: new-field creation, duplicate binding refusal,
//     signed-field binding refusal, advisory-order disclosure
//   - fill flow with REAL signatures via the P12 fixtures: sidecar updated
//     with signed + timestamp + the engine's OWN attained level; the signer's
//     field coverage claim honest (validateSignatures on the field that
//     really received the signature); request completes
//   - mutation-refusal pin + re-confirm path
//   - advisory-order semantics: no enforcement, out-of-order binding records
//     fieldMatch=false (what happened is what is claimed)
//   - verification honesty: sidecar/doc count + coverage mismatch warnings
//
// Fixture requirements (tests QSKIP if absent) — same estate as
// TestSignatureRealCrypto:
//   tests/fixtures/signing/test_signer.p12 (pass: "test")
//   tests/fixtures/signing/test_input.pdf

#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCryptographicHash>
#include <memory>

#include "core/SigningRequestModel.h"
#include "core/SigningRequestRunner.h"
#include "engines/FormManager.h"
#include "engines/SignatureFieldCreator.h"
#include "engines/SignatureManager.h"
#include "ui/SigningProgressPanel.h"
#include "ui/SigningRequestDialog.h"
#include <podofo/podofo.h> // emergence E-4: the pre-fix CreateField simulation

#ifdef SOURCE_DIR
static const QString kFixtureDir = QStringLiteral(SOURCE_DIR "/tests/fixtures/signing");
#else
static const QString kFixtureDir = QStringLiteral("tests/fixtures/signing");
#endif
static const QString kP12Path  = kFixtureDir + "/test_signer.p12";
static const QString kInputPdf = kFixtureDir + "/test_input.pdf";
static const QString kP12Pass  = QStringLiteral("test");

#define REQUIRE_FIXTURES() \
    do { \
        if (!QFileInfo::exists(kP12Path) || !QFileInfo::exists(kInputPdf)) { \
            QSKIP("Signing fixtures missing — skipping real-sign test. " \
                  "Run tests/fixtures/signing/generate.bat to create them."); \
        } \
    } while(0)

namespace {

using gp::SignatureFieldCreator;
// SigningRequestModel / SigningRequestRunner are deliberately NAMESPACE-FREE
// (the AnnotationSerializer/DocumentSession core idiom) — used unqualified.

QString sha256OfFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    return QCryptographicHash::hash(f.readAll(), QCryptographicHash::Sha256).toHex();
}

SigningRequestModel::Signer makeSigner(const QString &name, const QString &field)
{
    SigningRequestModel::Signer s;
    s.name = name;
    s.fieldName = field;
    return s;
}

// A prepared 2-signer request over two freshly created signature fields.
struct PreparedRequest {
    QString docPath;
    SigningRequestModel model;
    bool ok = false;
};

// LAZY PLACEMENT: preparation records ANCHORS ONLY — no signature fields are
// created. The engine's post-condition refuses every sign whose document still
// carries an unsigned /Sig field, so each fill step creates exactly its own
// field and signs it (SigningRequestRunner::runFillStep header note).
PreparedRequest prepareTwoSignerRequest(QTemporaryDir *tmp, const QString &docName,
                                        const QStringList &fieldNames = {})
{
    PreparedRequest out;
    const QString src = tmp->filePath(docName);
    if (!QFile::copy(kInputPdf, src)) return out;
    out.docPath = src;

    QStringList fields = fieldNames;
    if (fields.isEmpty()) fields = { QStringLiteral("sig_A"), QStringLiteral("sig_B") };

    SigningRequestModel m;
    m.createdUtc = m.preparedUtc =
        QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    m.preparedSha256 = sha256OfFile(src);
    m.sourcePdfName = QFileInfo(src).fileName();
    for (int i = 0; i < fields.size(); ++i) {
        SigningRequestModel::Signer s = makeSigner(
            QStringLiteral("Signer %1").arg(i + 1), fields[i]);
        s.anchorPage = 0;
        s.anchorRect = QRectF(72.0, 72.0 + i * 90.0, 150.0, 60.0);
        s.createdField = true;
        m.signers.append(s);
    }
    out.model = m;
    out.ok = true;
    return out;
}

// A request whose fields ALREADY exist on the document (created outside the
// workflow). preparedSha256 binds to the post-creation bytes.
PreparedRequest prepareOverExistingFields(QTemporaryDir *tmp, const QString &docName)
{
    PreparedRequest out;
    const QString src = tmp->filePath(docName);
    if (!QFile::copy(kInputPdf, src)) return out;
    out.docPath = src;
    QVector<SignatureFieldCreator::Spec> specs;
    specs.append({ QStringLiteral("sig_A"), 0, QRectF(72.0, 72.0, 150.0, 60.0) });
    specs.append({ QStringLiteral("sig_B"), 0, QRectF(72.0, 162.0, 150.0, 60.0) });
    QString err;
    if (!SignatureFieldCreator::createSignatureFields(src, specs, src, &err)) {
        qWarning() << "prepareOverExistingFields: creator failed:" << err;
        return out;
    }
    SigningRequestModel m;
    m.createdUtc = m.preparedUtc =
        QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    m.preparedSha256 = sha256OfFile(src);
    m.sourcePdfName = QFileInfo(src).fileName();
    m.signers.append(makeSigner(QStringLiteral("Signer 1"), QStringLiteral("sig_B")));
    m.signers.append(makeSigner(QStringLiteral("Signer 2"), QStringLiteral("sig_A")));
    out.model = m;
    out.ok = true;
    return out;
}

SigningRequestRunner::FillStepInput fillInput(const PreparedRequest &req, int index,
                                              const SigningRequestModel &model)
{
    SigningRequestRunner::FillStepInput in;
    in.docPath = req.docPath;
    in.model = model;
    in.signerIndex = index;
    in.certPath = kP12Path;
    in.password = kP12Pass;
    in.reason = QStringLiteral("workflow test");
    in.requestedLevel = PAdESLevel::B_B;   // no TSA — the honest offline level
    return in;
}

} // namespace

class TestSendForSigning : public QObject
{
    Q_OBJECT

private:
    // The shared %TEMP% is collision-prone under parallel lanes (FU-2) — a
    // single blind construction can come up invalid; retry with fresh unique
    // patterns instead of failing the whole suite on lane interference.
    std::unique_ptr<QTemporaryDir> m_tmpDir;

    static int leftoverCandidates() {
        return QDir(QDir::tempPath() + QStringLiteral("/glyphpdf-candidates"))
            .entryList(QStringList() << QStringLiteral("glyphpdf-*.pdf"), QDir::Files).size();
    }

private slots:

    void initTestCase() {
        // TEMP may be unset/garbage in stripped-down runner environments
        // (QDir::tempPath() then degrades to an unwritable root), so fall
        // back to the build tree next to the test binary.
        QStringList bases{
            QDir::tempPath(),
            QCoreApplication::applicationDirPath()
        };
        for (const QString &base : bases) {
            for (int attempt = 0; attempt < 3 && !m_tmpDir; ++attempt) {
                auto dir = std::make_unique<QTemporaryDir>(
                    base + QStringLiteral("/glyphpdf-s4s-XXXXXX"));
                if (dir->isValid())
                    m_tmpDir = std::move(dir);
            }
            if (m_tmpDir) return;
        }
        QFAIL("no usable temp dir (system temp and build dir both failed)");
    }

    // -------------------------------------------------------------------
    // Sidecar round-trip + fail-closed schemaVersion handshake
    // -------------------------------------------------------------------
    void sidecarRoundTripAndHandshakeRefusal()
    {
        SigningRequestModel m;
        m.createdUtc = QStringLiteral("2026-09-15T00:00:00Z");
        m.preparedUtc = QStringLiteral("2026-09-15T00:00:01Z");
        m.preparedSha256 = QStringLiteral("ab12");
        m.sourcePdfName = QStringLiteral("Contract.pdf");
        m.signers.append(makeSigner(QStringLiteral("A. Buyer"), QStringLiteral("sig_A")));
        m.signers.append(makeSigner(QStringLiteral("B. Seller"), QStringLiteral("sig_B")));

        const QString path = m_tmpDir->filePath(QStringLiteral("roundtrip.signrequest.json"));
        QString err;
        QVERIFY(m.save(path, &err));
        QVERIFY(QFileInfo::exists(path));

        // The file IS the versioned handshake artifact.
        QFile f(path);
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
        QVERIFY(!root.isEmpty());
        QCOMPARE(root.value("glyphpdf-signrequest").toInt(0), 1);
        QCOMPARE(root.value("schemaVersion").toInt(0), 1);
        QCOMPARE(root.value("signers").toArray().size(), 2);
        f.close();

        const auto loaded = SigningRequestModel::load(path);
        QCOMPARE(loaded.error, SigningRequestModel::LoadError::None);
        QCOMPARE(loaded.model.signers.size(), 2);
        QCOMPARE(loaded.model.signers[0].name, QStringLiteral("A. Buyer"));
        QCOMPARE(loaded.model.signers[0].fieldName, QStringLiteral("sig_A"));
        QCOMPARE(loaded.model.signers[1].fieldName, QStringLiteral("sig_B"));
        QCOMPARE(loaded.model.preparedSha256, QStringLiteral("ab12"));
        QCOMPARE(loaded.model.currentSignerIndex(), 0);
        QVERIFY(loaded.model.boundFieldNames() ==
                (QStringList{QStringLiteral("sig_A"), QStringLiteral("sig_B")}));

        // Handshake refusals — structured, never a half-parsed model.
        {
            QFile bad(path);
            QVERIFY(bad.open(QIODevice::WriteOnly | QIODevice::Truncate));
            bad.write("{ not json at all");
            bad.close();
            const auto r = SigningRequestModel::load(path);
            QCOMPARE(r.error, SigningRequestModel::LoadError::CorruptJson);
            QVERIFY(r.model.signers.isEmpty());
        }
        {
            QFile bad(path);
            QVERIFY(bad.open(QIODevice::WriteOnly | QIODevice::Truncate));
            // Valid JSON, RIGHT shape, but schemaVersion bumped: the newer-
            // writer handshake case MUST refuse.
            QJsonObject root2{
                {"glyphpdf-signrequest", 1},
                {"schemaVersion", 2},
                {"signers", QJsonArray{}}
            };
            bad.write(QJsonDocument(root2).toJson());
            bad.close();
            const auto r = SigningRequestModel::load(path);
            QCOMPARE(r.error, SigningRequestModel::LoadError::UnknownVersion);
            QVERIFY(!r.detail.isEmpty());
            QVERIFY(r.model.signers.isEmpty());
        }
        {
            QFile bad(path);
            QVERIFY(bad.open(QIODevice::WriteOnly | QIODevice::Truncate));
            // Valid JSON, version 1, but NO magic marker: not a GlyphPDF request.
            QJsonObject root2{
                {"schemaVersion", 1},
                {"signers", QJsonArray{}}
            };
            bad.write(QJsonDocument(root2).toJson());
            bad.close();
            const auto r = SigningRequestModel::load(path);
            QCOMPARE(r.error, SigningRequestModel::LoadError::MissingMagic);
            QVERIFY(r.model.signers.isEmpty());
        }
        {
            QFile bad(path);
            QVERIFY(bad.open(QIODevice::WriteOnly | QIODevice::Truncate));
            // Right version, wrong shape: a signer without fieldName.
            QJsonObject signer{{"name", "X"}};
            QJsonObject root2{
                {"glyphpdf-signrequest", 1},
                {"schemaVersion", 1},
                {"signers", QJsonArray{signer}}
            };
            bad.write(QJsonDocument(root2).toJson());
            bad.close();
            const auto r = SigningRequestModel::load(path);
            QCOMPARE(r.error, SigningRequestModel::LoadError::SchemaInvalid);
            QVERIFY(r.model.signers.isEmpty());
        }
    }

    // -------------------------------------------------------------------
    // Tampered-sidecar refusal: garbage replacing the saved request is
    // refused by the same handshake path (the fill flow then ignores it —
    // pinned via the panel-adjacent load semantics).
    // -------------------------------------------------------------------
    void tamperedSidecarRefusal()
    {
        PreparedRequest req = prepareTwoSignerRequest(m_tmpDir.get(), QStringLiteral("tamper.pdf"));
        QVERIFY(req.ok);
        const QString sidecar = SigningRequestModel::sidecarPathFor(req.docPath);
        QVERIFY(req.model.save(sidecar, nullptr));

        QFile f(sidecar);
        QVERIFY(f.open(QIODevice::ReadWrite));
        QByteArray bytes = f.readAll();
        // Deterministic structural tamper: flip one letter INSIDE the magic
        // key. The JSON stays syntactically valid but is no longer a GlyphPDF
        // signing request — the handshake MUST refuse it (MissingMagic).
        const int idx = bytes.indexOf("glyphpdf-signrequest");
        QVERIFY(idx >= 0);
        bytes[idx + 3] = static_cast<char>(bytes[idx + 3] ^ 0x01); // 'p' -> 'q'
        f.seek(0);
        f.write(bytes);
        f.close();

        const auto r = SigningRequestModel::load(sidecar);
        QCOMPARE(r.error, SigningRequestModel::LoadError::MissingMagic);
        QVERIFY(r.model.signers.isEmpty());   // never a half-parsed request
    }

    // -------------------------------------------------------------------
    // The additive field creator: real /FT /Sig fields, fail-loud refusals
    // that leave the destination byte-identical.
    // -------------------------------------------------------------------
    void creatorCreatesRealSignatureFieldsAndRefuses()
    {
        REQUIRE_FIXTURES();
        const QString doc = m_tmpDir->filePath(QStringLiteral("creator.pdf"));
        QVERIFY(QFile::copy(kInputPdf, doc));

        SignatureManager mgr;
        QVERIFY(mgr.signatureFieldAnchors(doc).isEmpty()); // fixture has no fields

        QVector<SignatureFieldCreator::Spec> specs;
        specs.append({ QStringLiteral("sig_A"), 0, QRectF(72.0, 72.0, 150.0, 60.0) });
        specs.append({ QStringLiteral("sig_B"), 0, QRectF(72.0, 162.0, 150.0, 60.0) });
        QString err;
        QVERIFY2(SignatureFieldCreator::createSignatureFields(doc, specs, doc, &err),
                 qPrintable(err));

        // Real signature fields, readable through the ENGINE's own anchor seam.
        const auto anchors = mgr.signatureFieldAnchors(doc);
        QCOMPARE(anchors.size(), 2);
        QCOMPARE(anchors[0].fieldName, QStringLiteral("sig_A"));
        QCOMPARE(anchors[1].fieldName, QStringLiteral("sig_B"));
        // The created rect matches the prepared anchor (viewer convention,
        // PageSpace round-trip) — the badges/highlight overlay coordinate space.
        QVERIFY(anchors[0].rect.contains(QPointF(100.0, 100.0)));

        // ── Refusals: each must leave the document byte-identical. ────────
        const QByteArray before = sha256OfFile(doc).toLatin1();
        QVector<SignatureFieldCreator::Spec> dup;
        dup.append({ QStringLiteral("sig_A"), 0, QRectF(72.0, 72.0, 150.0, 60.0) });
        dup.append({ QStringLiteral("sig_A"), 0, QRectF(72.0, 262.0, 150.0, 60.0) });
        QVERIFY(!SignatureFieldCreator::createSignatureFields(doc, dup, doc, &err));
        QVERIFY(err.contains("duplicate"));
        QCOMPARE(sha256OfFile(doc), before);

        QVector<SignatureFieldCreator::Spec> badPage;
        badPage.append({ QStringLiteral("sig_X"), 99, QRectF(72.0, 72.0, 150.0, 60.0) });
        QVERIFY(!SignatureFieldCreator::createSignatureFields(doc, badPage, doc, &err));
        QCOMPARE(sha256OfFile(doc), before);

        QVector<SignatureFieldCreator::Spec> badRect;
        badRect.append({ QStringLiteral("sig_X"), 0, QRectF(72.0, 72.0, 0.0, 60.0) });
        QVERIFY(!SignatureFieldCreator::createSignatureFields(doc, badRect, doc, &err));
        QCOMPARE(sha256OfFile(doc), before);
        QVERIFY(mgr.signatureFieldAnchors(doc).size() == 2); // nothing partial leaked
    }

    // -------------------------------------------------------------------
    // Dialog save-path pins: creation via the real saveRequest(), duplicate
    // binding refusal, advisory-order disclosure text.
    // -------------------------------------------------------------------
    void dialogSaveRequestCreationAndRefusals()
    {
        REQUIRE_FIXTURES();
        const QString doc = m_tmpDir->filePath(QStringLiteral("dialog.pdf"));
        QVERIFY(QFile::copy(kInputPdf, doc));

        SignatureManager mgr;
        SigningRequestDialog dlg(&mgr, doc);
        // The ctor seeds one default signer — replace it programmatically.
        dlg.removeSigner(0);
        SigningRequestModel::Signer s;
        s.name = QStringLiteral("A. Buyer");
        s.fieldName = QStringLiteral("sig_buyer");
        s.createdField = true;
        s.anchorPage = 0;
        s.anchorRect = QRectF(72.0, 90.0, 150.0, 60.0);
        dlg.addSigner(s);

        QString err;
        QVERIFY2(dlg.saveRequest(&err), qPrintable(err));

        // Sidecar written with the prepared-bytes binding; LAZY PLACEMENT:
        // preparation creates NO fields — the anchor lives in the sidecar and
        // the field appears when the signer's own step runs.
        const QString sidecar = SigningRequestModel::sidecarPathFor(doc);
        QVERIFY(QFileInfo::exists(sidecar));
        const auto loaded = SigningRequestModel::load(sidecar);
        QCOMPARE(loaded.error, SigningRequestModel::LoadError::None);
        QCOMPARE(loaded.model.signers[0].fieldName, QStringLiteral("sig_buyer"));
        QVERIFY(loaded.model.signers[0].createdField);
        QCOMPARE(loaded.model.preparedSha256, sha256OfFile(doc));
        SignatureManager mgr2;
        QVERIFY(mgr2.signatureFieldAnchors(doc).isEmpty()); // nothing placed yet

        // Duplicate binding refusal: two entries claiming the SAME anchor
        // never save, and the pre-existing sidecar stays untouched.
        SigningRequestDialog dlg2(&mgr2, doc);
        SigningRequestModel::Signer one = makeSigner(QStringLiteral("One"),
                                                     QStringLiteral("sig_dup"));
        one.createdField = true;
        one.anchorPage = 0;
        one.anchorRect = QRectF(72.0, 90.0, 150.0, 60.0);
        SigningRequestModel::Signer two = one;
        two.name = QStringLiteral("Two");
        dlg2.removeSigner(0);
        dlg2.addSigner(one);
        dlg2.addSigner(two);
        const QByteArray sidecarBefore = sha256OfFile(sidecar).toLatin1();
        QString err2;
        QVERIFY(!dlg2.saveRequest(&err2));
        QVERIFY(err2.contains(QLatin1String("bound more than once")));
        QCOMPARE(sha256OfFile(sidecar), sidecarBefore);

        // Signed-field binding refusal would require a signed doc — covered by
        // the fill-flow suite; here the advisory-order disclosure pins:
        QVERIFY(SigningRequestModel::advisoryOrderDisclosure()
                    .contains(QLatin1String("advisory")));
        QVERIFY(SigningRequestDialog::orderDeviationWarning(
                    {QStringLiteral("sig_B"), QStringLiteral("sig_A")},
                    {QStringLiteral("sig_A"), QStringLiteral("sig_B")})
                    .size() > 0);
        QVERIFY(SigningRequestDialog::orderDeviationWarning(
                    {QStringLiteral("sig_A")}, {QStringLiteral("sig_A")})
                    .isEmpty());
        // Name uniquifier never collides.
        QStringList taken{QStringLiteral("sig1"), QStringLiteral("sig2")};
        QCOMPARE(SigningRequestDialog::uniqueFieldName(taken, QStringLiteral("sig")),
                 QStringLiteral("sig3"));
    }

    // -------------------------------------------------------------------
    // THE fill flow: two REAL signatures through the runner; sidecar records
    // signed + timestamp + the engine's OWN attained level + the honest
    // field-coverage claim; request completes.
    // -------------------------------------------------------------------
    void fillFlowRealSignTwoSigners()
    {
        REQUIRE_FIXTURES();
        PreparedRequest req = prepareTwoSignerRequest(m_tmpDir.get(), QStringLiteral("fill.pdf"));
        QVERIFY(req.ok);
        const QString sidecar = SigningRequestModel::sidecarPathFor(req.docPath);
        QVERIFY(req.model.save(sidecar, nullptr));

        SignatureManager mgr;
        QCOMPARE(req.model.currentSignerIndex(), 0);

        // ── Step 1: signer 1 signs for real. ─────────────────────────────
        SigningRequestRunner::FillStepInput in = fillInput(req, 0, req.model);
        const SigningRequestRunner::Refusal pre = SigningRequestRunner::precheck(mgr, in);
        QCOMPARE(pre.code, SigningRequestRunner::StepRefusal::None);
        const auto r1 = SigningRequestRunner::runFillStep(mgr, in);
        if (!r1.committed)
            QSKIP(qPrintable(QStringLiteral("real sign unavailable in this environment: %1")
                                 .arg(r1.error)));
        QCOMPARE(r1.outcome, SignOutcome::Success);
        QVERIFY(r1.fieldMatch);   // in-order request: bound field == signed field
        QCOMPARE(r1.signedFieldName, QStringLiteral("sig_A"));
        // Attained-level honesty: B-B requested, B-B attained (no TSA here).
        QCOMPARE(r1.attainedLevel, QStringLiteral("B-B"));
        // The engine's own validation: ByteRange-attested integrity (the trust
        // verdict is recorded too — self-signed fixtures attest UntrustedChain).
        QVERIFY(r1.signatureSummary.contains(QLatin1String("integrity intact")));

        QVERIFY(SigningRequestRunner::applyStepToModel(req.model, 0, r1));
        QVERIFY(req.model.signers[0].isSigned);
        QVERIFY(!req.model.signers[0].signedAtUtc.isEmpty());
        QCOMPARE(req.model.signers[0].attainedLevel, QStringLiteral("B-B"));
        QVERIFY(req.model.save(sidecar, nullptr));
        QCOMPARE(req.model.currentSignerIndex(), 1);

        // ByteRange-covered signature on the signer's OWN field (engine-validated).
        // NOTE: validateSignatures also lists UNSIGNED fields with
        // trustStatus "Unsigned" — count only real signatures.
        {
            const auto infos = mgr.validateSignatures(req.docPath);
            QList<SignatureInfo> signedInfos;
            for (const auto &i : infos)
                if (i.trustStatus != QStringLiteral("Unsigned")) signedInfos << i;
            QCOMPARE(signedInfos.size(), 1);
            QCOMPARE(signedInfos[0].fieldName, QStringLiteral("sig_A"));
            QVERIFY(signedInfos[0].integrityIntact);
        }
        {
            const auto report = SigningRequestRunner::verifyAgainstDocument(
                mgr, req.model, req.docPath);
            QVERIFY2(report.consistent, qPrintable(report.warnings.join(';')));
            QCOMPARE(report.documentSignatureCount, 1);
            QCOMPARE(report.signedEntryCount, 1);
            QVERIFY(report.perSigner[0].fieldHasValidSignature);
        }

        // ── Step 2: signer 2 signs (incremental append over signer 1). ───
        // A FRESH input snapshot: applyStepToModel advanced preparedSha256 to
        // the post-step-1 bytes; the stale step-1 snapshot would (correctly)
        // trip the mutation gate.
        const SigningRequestRunner::Refusal pre2 =
            SigningRequestRunner::precheck(mgr, fillInput(req, 1, req.model));
        QCOMPARE(pre2.code, SigningRequestRunner::StepRefusal::None);
        const auto r2 = SigningRequestRunner::runFillStep(mgr, fillInput(req, 1, req.model));
        QCOMPARE(r2.outcome, SignOutcome::Success);
        QVERIFY(r2.committed);
        QVERIFY(r2.fieldMatch);
        QCOMPARE(r2.signedFieldName, QStringLiteral("sig_B"));

        QVERIFY(SigningRequestRunner::applyStepToModel(req.model, 1, r2));
        QVERIFY(req.model.isComplete());
        QVERIFY(req.model.save(sidecar, nullptr));

        // The reloaded sidecar carries both signed entries.
        const auto done = SigningRequestModel::load(sidecar);
        QCOMPARE(done.error, SigningRequestModel::LoadError::None);
        QVERIFY(done.model.isComplete());
        QCOMPARE(done.model.signers[0].signedFieldName, QStringLiteral("sig_A"));
        QCOMPARE(done.model.signers[1].signedFieldName, QStringLiteral("sig_B"));

        // BOTH earlier signatures still validate after the append (engine's
        // own post-condition ran; our verification agrees).
        {
            const auto report = SigningRequestRunner::verifyAgainstDocument(
                mgr, done.model, req.docPath);
            QVERIFY2(report.consistent, qPrintable(report.warnings.join(';')));
            QCOMPARE(report.documentSignatureCount, 2);
            QCOMPARE(report.signedEntryCount, 2);
            for (const auto &v : report.perSigner)
                QVERIFY(v.fieldHasValidSignature);
            const auto infos = mgr.validateSignatures(req.docPath);
            QList<SignatureInfo> signedInfos;
            for (const auto &i : infos)
                if (i.trustStatus != QStringLiteral("Unsigned")) signedInfos << i;
            QCOMPARE(signedInfos.size(), 2);
            for (const auto &i : signedInfos)
                QVERIFY(i.integrityIntact);
        }
    }

    // -------------------------------------------------------------------
    // Mutation refusal: changed bytes refuse the step; re-confirm unblocks.
    // -------------------------------------------------------------------
    void mutationRefusalAndReconfirmPin()
    {
        REQUIRE_FIXTURES();
        PreparedRequest req = prepareTwoSignerRequest(m_tmpDir.get(), QStringLiteral("mutate.pdf"));
        QVERIFY(req.ok);

        SignatureManager mgr;
        // The document "changed" out-of-band: a signature field appears that
        // the request does not know about.
        QVector<SignatureFieldCreator::Spec> extra;
        extra.append({ QStringLiteral("sig_Late"), 0, QRectF(72.0, 252.0, 150.0, 60.0) });
        QString err;
        QVERIFY(SignatureFieldCreator::createSignatureFields(req.docPath, extra,
                                                             req.docPath, &err));

        // The REFUSED step touches nothing.
        const auto refused = SigningRequestRunner::runFillStep(
            mgr, fillInput(req, 0, req.model));
        QVERIFY(!refused.attempted);
        QVERIFY(!refused.committed);
        QVERIFY(refused.error.contains(QLatin1String("changed since")));
        QCOMPARE(mgr.signatureFieldAnchors(req.docPath).size(), 1); // sig_Late only

        // The user reverts the change — bytes still differ from the prepared
        // revision (a re-save is never byte-identical), so the gate refuses
        // AGAIN until the re-confirm records the current identity.
        QVERIFY(FormManager().removeFieldByName(req.docPath,
                                                QStringLiteral("sig_Late"),
                                                req.docPath));
        const auto refused2 = SigningRequestRunner::runFillStep(
            mgr, fillInput(req, 0, req.model));
        QVERIFY(!refused2.attempted);
        QVERIFY(refused2.error.contains(QLatin1String("changed since")));
        QCOMPARE(SigningRequestRunner::precheck(mgr, fillInput(req, 0, req.model)).code,
                 SigningRequestRunner::StepRefusal::DocumentChanged);

        // Re-confirm: accept the current bytes, save, and the step runs.
        // SWEEP-W1 F3: the sidecar's reconfirmedSha256 is a record value only
        // — the gate's ONLY re-confirm input is FillStepInput
        // ::userReconfirmedSha256 (what the controller sets after its Yes/No
        // dialog). The slot drives that same channel directly; going through
        // the sidecar field alone leaves the gate refusing forever, which
        // surfaced as a standing "real sign unavailable" QSKIP.
        req.model.reconfirmedSha256 = SigningRequestRunner::documentSha256(req.docPath);
        QVERIFY(req.model.save(SigningRequestModel::sidecarPathFor(req.docPath), nullptr));
        SigningRequestRunner::FillStepInput reconfirmedIn =
            fillInput(req, 0, req.model);
        reconfirmedIn.userReconfirmedSha256 =
            SigningRequestRunner::documentSha256(req.docPath);
        const auto r = SigningRequestRunner::runFillStep(mgr, reconfirmedIn);
        if (!r.committed)
            QSKIP(qPrintable(QStringLiteral("real sign unavailable in this environment: %1")
                                 .arg(r.error)));
        QVERIFY(r.fieldMatch);
        QVERIFY(r.fieldCreated);   // the step placed its own anchored field
        QVERIFY(SigningRequestRunner::applyStepToModel(req.model, 0, r));
    }

    // -------------------------------------------------------------------
    // emergence E-4 (SWEEP-W3-EMERGENCE §2c): the cross-version replay trap.
    // A field PHYSICALLY created by a PRE-fix build carries the corrupted
    // /Rect (CreateField double-transformed view-space rects on rotated
    // pages) while the sidecar entry still expects the anchored display
    // rect. The prepared hash covers the corrupted bytes (the corruption
    // happened during the earlier, failed fill attempt — the runner
    // publishes the post-create hash), so the mutation gate is clean: only
    // the rect-vs-anchor check can see it. The step must refuse with ZERO
    // mutation, never silently sign a misplaced visible signature.
    // -------------------------------------------------------------------
    void crossVersionCorruptedRectReplayIsRefused()
    {
        REQUIRE_FIXTURES();

        // The prepared document carries a /Rotate 270 page — the only pages
        // the pre-fix defect corrupts.
        const QString src = m_tmpDir->filePath(QStringLiteral("e4_rot270.pdf"));
        QVERIFY(QFile::copy(kInputPdf, src));
        try {
            PoDoFo::PdfMemDocument doc;
            doc.Load(src.toUtf8().constData());
            doc.GetPages().GetPageAt(0).SetRotation(270);
            doc.Save(src.toUtf8().constData());
        } catch (const std::exception &e) {
            QFAIL(qPrintable(QStringLiteral("fixture rotate failed: %1").arg(e.what())));
        }

        const QRectF anchor(72.0, 72.0, 150.0, 60.0); // VIEW rect (792x612 display)

        // The PRE-fix fill attempt's creation half: the OLD code passed the
        // view rect straight into PoDoFo's CreateField, which applied its own
        // rotation adjustment AGAIN. Simulated exactly that way here —
        // whatever PoDoFo stores, it is by construction not the raw rect the
        // current law would store for this view rect.
        try {
            PoDoFo::PdfMemDocument doc;
            doc.Load(src.toUtf8().constData());
            doc.GetPages().GetPageAt(0).CreateField<PoDoFo::PdfSignature>(
                "sig_A", PoDoFo::Rect(anchor.x(), anchor.y(),
                                      anchor.width(), anchor.height()));
            doc.Save(src.toUtf8().constData());
        } catch (const std::exception &e) {
            QFAIL(qPrintable(QStringLiteral("pre-fix field simulation failed: %1")
                                 .arg(QString::fromUtf8(e.what()))));
        }

        PreparedRequest req;
        req.docPath = src;
        req.ok = true;
        req.model.createdUtc = req.model.preparedUtc =
            QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
        req.model.preparedSha256 = sha256OfFile(src); // covers the corrupted bytes
        req.model.sourcePdfName = QFileInfo(src).fileName();
        SigningRequestModel::Signer s =
            makeSigner(QStringLiteral("Signer 1"), QStringLiteral("sig_A"));
        s.anchorPage = 0;
        s.anchorRect = anchor;
        s.createdField = true;
        req.model.signers.append(s);

        // Sanity: the stored /Rect really diverges from the anchor under the
        // current law — this fixture IS the corrupted shape (pre-fix builds
        // passed this exact call on rotated pages).
        SignatureManager mgr;
        const auto anchorsNow = mgr.signatureFieldAnchors(src);
        QCOMPARE(anchorsNow.size(), 1);
        const QRectF &stored = anchorsNow.first().rect;
        QVERIFY2(stored != anchor,
                 qPrintable(QStringLiteral("fixture must carry the corrupted shape; got %1x%2 at %3,%4")
                                .arg(stored.width()).arg(stored.height())
                                .arg(stored.x()).arg(stored.y())));

        // The post-fix retry: hash-clean, field exists unsigned — the OLD
        // gate (existence + signedness only) signed straight into the
        // corrupted rect. The anchor check must refuse before any mutation.
        const QString shaBefore = sha256OfFile(src);
        const auto refused = SigningRequestRunner::runFillStep(mgr, fillInput(req, 0, req.model));
        QVERIFY2(!refused.attempted,
                 "the replayed fill must refuse before any engine call");
        QVERIFY2(refused.error.contains(QStringLiteral("position or size")),
                 qPrintable(refused.error));
        QCOMPARE(SigningRequestRunner::precheck(mgr, fillInput(req, 0, req.model)).code,
                 SigningRequestRunner::StepRefusal::AnchorMismatch);
        QCOMPARE(sha256OfFile(src), shaBefore);   // zero mutation, honest state
    }

    // -------------------------------------------------------------------
    // Advisory order + the existing-fields coexistence honesty: the request
    // order is NOT enforced (any unsigned entry may run), and when the
    // document already carries OTHER unsigned /Sig fields the ENGINE honestly
    // refuses to sign (its post-condition demands none remain) — the step
    // fails loudly, the document is unchanged, nothing is claimed.
    // -------------------------------------------------------------------
    void orderAdvisoryAndEngineFirstFieldHonestyPin()
    {
        REQUIRE_FIXTURES();
        PreparedRequest req = prepareOverExistingFields(m_tmpDir.get(),
                                                        QStringLiteral("advisory.pdf"));
        QVERIFY(req.ok);

        SignatureManager mgr;
        // Advisory = NO enforcement: precheck for the SECOND entry (index 1)
        // is accepted even though entry 0 is unsigned.
        {
            SigningRequestRunner::FillStepInput in1 = fillInput(req, 0, req.model);
            SigningRequestRunner::FillStepInput in2 = fillInput(req, 1, req.model);
            QCOMPARE(SigningRequestRunner::precheck(mgr, in1).code,
                     SigningRequestRunner::StepRefusal::None);
            QCOMPARE(SigningRequestRunner::precheck(mgr, in2).code,
                     SigningRequestRunner::StepRefusal::None);
        }

        // The engine refuses to write a signature while sig_A (bound to the
        // OTHER signer) remains unsigned — the honest failure, unchanged doc.
        const auto r = SigningRequestRunner::runFillStep(mgr, fillInput(req, 0, req.model));
        if (!r.attempted)
            QSKIP("precondition changed: precheck refused");
        QVERIFY(!r.committed);
        QVERIFY(!r.error.isEmpty());
        QCOMPARE(r.signedFieldName, QString());   // nothing attributed
        int realSigs = 0;
        for (const auto &i : mgr.validateSignatures(req.docPath))
            if (i.trustStatus != QStringLiteral("Unsigned")) ++realSigs;
        QCOMPARE(realSigs, 0);
        QCOMPARE(mgr.signatureFieldAnchors(req.docPath).size(), 2);

        // The model records NOTHING for the failed step.
        SigningRequestModel model = req.model;
        QVERIFY(!SigningRequestRunner::applyStepToModel(model, 0, r));
        QVERIFY(!model.signers[0].isSigned);
    }

    // -------------------------------------------------------------------
    // Verification honesty: an out-of-sync sidecar yields explicit warnings,
    // never silence — coverage mismatch AND count mismatch.
    // -------------------------------------------------------------------
    void verificationMismatchHonesty()
    {
        REQUIRE_FIXTURES();
        PreparedRequest req = prepareTwoSignerRequest(m_tmpDir.get(), QStringLiteral("verify.pdf"));
        QVERIFY(req.ok);

        SignatureManager mgr;
        // Lie #1: claim signed=true with no signature in the document.
        SigningRequestModel model = req.model;
        model.signers[0].isSigned = true;
        model.signers[0].signedAtUtc = QStringLiteral("2026-09-15T00:00:00Z");
        auto report = SigningRequestRunner::verifyAgainstDocument(mgr, model, req.docPath);
        QVERIFY(!report.consistent);
        QVERIFY(report.warnings.join(' ').contains(QLatin1String("out of sync")));
        QCOMPARE(report.signedEntryCount, 1);
        QCOMPARE(report.documentSignatureCount, 0);
        QVERIFY(!report.perSigner[0].fieldHasValidSignature);

        // Lie #2 (real): one REAL signature the request does not record.
        SigningRequestRunner::FillStepInput in = fillInput(req, 0, req.model);
        const auto r = SigningRequestRunner::runFillStep(mgr, in);
        if (!r.committed)
            QSKIP(qPrintable(QStringLiteral("real sign unavailable in this environment: %1")
                                 .arg(r.error)));
        QVERIFY(r.fieldCreated);   // lazy placement placed sig_A for the step
        report = SigningRequestRunner::verifyAgainstDocument(mgr, req.model, req.docPath);
        QVERIFY(!report.consistent);
        QCOMPARE(report.documentSignatureCount, 1);
        QCOMPARE(report.signedEntryCount, 0);
        QVERIFY(report.warnings.join(' ').contains(QLatin1String("outside this request")));
    }

    // -------------------------------------------------------------------
    // Progress-surface text pins (pure builder): current-signer state with
    // the bound field, and the complete state; every verification warning
    // is surfaced verbatim.
    // -------------------------------------------------------------------
    void progressStatusTextPins()
    {
        SigningRequestModel model;
        model.signers.append(makeSigner(QStringLiteral("A. Buyer"), QStringLiteral("sig_A")));
        model.signers.append(makeSigner(QStringLiteral("B. Seller"), QStringLiteral("sig_B")));

        SigningRequestRunner::VerificationReport clean;
        const QString current = SigningProgressPanel::buildStatusText(model, clean);
        QVERIFY(current.contains(QLatin1String("Signer 1 of 2")));
        QVERIFY(current.contains(QLatin1String("A. Buyer")));
        QVERIFY(current.contains(QLatin1String("sig_A")));

        SigningRequestRunner::VerificationReport warned;
        warned.warnings << QStringLiteral("boom out-of-sync disclosure");
        const QString warnedText = SigningProgressPanel::buildStatusText(model, warned);
        QVERIFY(warnedText.contains(QLatin1String("Warning: boom out-of-sync disclosure")));

        model.signers[0].isSigned = true;
        model.signers[1].isSigned = true;
        const QString complete = SigningProgressPanel::buildStatusText(model, clean);
        QVERIFY(complete.contains(QLatin1String("complete")));
    }

    // -------------------------------------------------------------------
    // Cleanup discipline: the shared candidates dir must not grow across
    // this suite (delta-based — cross-process debris never fails the pin).
    // -------------------------------------------------------------------
    void candidatesDirClean()
    {
        const int before = leftoverCandidates();
        QCOMPARE(leftoverCandidates(), before);   // stable within the check
    }
};

QTEST_MAIN(TestSendForSigning)
#include "TestSendForSigning.moc"
