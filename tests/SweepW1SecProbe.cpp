// SPDX-License-Identifier: MIT
// SweepW1SecProbe.cpp — W1 security-sweep FAILING repros (REVIEW-ONLY lane).
//
// Every QVERIFY below asserts the DESIRED (documented/claimed) security
// behavior; a FAIL is the repro of a finding in
// docs/audit/SWEEP-W1-SECURITY-2026-09-20.md. This file is a probe: it never
// ships, never patches, and lives under .context/ only.
//
// Findings exercised here:
//   S1  reconfirmGateSkipsUserOnPreseededSidecar — the sidecar's
//        reconfirmedSha256 is trusted by SigningRequestRunner::precheck even
//        though anyone with write access to the (unsigned) sidecar can set it;
//        the documented "unless the user explicitly re-confirmed the change"
//        (SigningRequestModel.h) never consults the user in that case.
//   S2  sidecarAnchorNotClampedToPage — SignatureFieldCreator accepts a
//        sidecar-supplied anchor rect arbitrarily far outside the page
//        MediaBox, so a crafted <file>.signrequest.json lazily places an
//        INVISIBLE signature field and the fill step signs it silently.
//   S3  attainedLevelLabelsGarbageTimestampAsBt — SignatureManager sets
//        timestampMissing=false the moment the TSA response is NON-EMPTY
//        (SignatureManager.cpp ~:1421), never parsing it as a TS_RESP/TimeStampToken;
//        attainedLevelLabel then reports the requested level (B-T) as ATTAINED
//        for arbitrary garbage bytes (any HTTP-200 HTML error page from a
//        misconfigured or hostile TSA).
//   S4  (removed) the F1 policy-provenance demonstration slot — design-limitation demos stay out of the permanent suite; the W1-05 honesty pins in TestPolicyController/TestSupportBundle carry the disclosure contract
//        default location is parsed and enforced with NO provenance/ACL
//        verification; on default Windows ACLs a non-admin can pre-create
//        %PROGRAMDATA%\GlyphPDF\policy.json (squatter) and its signing/tsaUrl
//        flows into every sign/certify/timestamp dispatch.
//   S5  networkTouchpointMissingPolicyEnabledTsa — NetworkTouchpoints derives
//        the TSA touchpoint's enabled state from RAW QSettings while
//        enforcement uses the policy-effective value, so the "enabled = will
//        fire under the CURRENT settings" honesty contract
//        (NetworkTouchpoints.h) breaks whenever policy manages the key.
//
// Run: offscreen, serial, -o txt (repo idiom). FU-2: each slot uses its own
// unique-pattern QTemporaryDir and never leaks files outside it.

#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QCryptographicHash>
#include <QSettings>
#include <memory>

#include "core/SigningRequestModel.h"
#include "core/SigningRequestRunner.h"
#include "core/PolicyController.h"
#include "core/NetworkTouchpoints.h"
#include "core/interfaces/ISignatureManager.h"
#include "engines/SignatureManager.h"
#include "engines/SignatureFieldCreator.h"
#include <podofo/podofo.h>
#include "shell/controllers/SecurityController.h"

#ifdef SOURCE_DIR
static const QString kFixtureDir = QStringLiteral(SOURCE_DIR "/tests/fixtures/signing");
#else
static const QString kFixtureDir = QStringLiteral("tests/fixtures/signing");
#endif
static const QString kInputPdf = kFixtureDir + "/test_input.pdf";

#define REQUIRE_FIXTURE() \
    do { \
        if (!QFileInfo::exists(kInputPdf)) \
            QSKIP("tests/fixtures/signing/test_input.pdf missing — probe cannot run."); \
    } while (0)

namespace {

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
    s.anchorPage = 0;
    s.anchorRect = QRectF(72.0, 72.0, 150.0, 60.0);
    s.createdField = true;
    return s;
}

} // namespace

class SweepW1SecProbe : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<QTemporaryDir> newTmp(const char *tag)
    {
        // FU-2 temp hygiene: unique pattern per probe; retry once on collision.
        for (int attempt = 0; attempt < 2; ++attempt) {
            auto d = std::make_unique<QTemporaryDir>(
                QDir::tempPath() + QStringLiteral("/sweepw1sec-%1-XXXXXX").arg(tag));
            if (d->isValid()) return d;
        }
        return nullptr;
    }

private slots:
    // ── S1 ──────────────────────────────────────────────────────────────────
    // The mutation gate must refuse changed bytes UNLESS THE USER re-confirmed
    // (SigningRequestModel.h honesty rule 1). The user is only consulted in
    // SendForSigningController when precheck returns DocumentChanged — so a
    // sidecar whose reconfirmedSha256 was pre-seeded by anyone else skips the
    // user entirely. DESIRED: precheck returns DocumentChanged here (it cannot
    // know who wrote reconfirmedSha256). ACTUAL: StepRefusal::None.
    void reconfirmGateSkipsUserOnPreseededSidecar()
    {
        REQUIRE_FIXTURE();
        auto tmp = newTmp("s1");
        QVERIFY(tmp);
        const QString doc = tmp->filePath(QStringLiteral("doc.pdf"));
        QVERIFY(QFile::copy(kInputPdf, doc));

        SigningRequestModel m;
        m.createdUtc = m.preparedUtc =
            QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
        m.preparedSha256 = sha256OfFile(doc);   // binds the ORIGINAL bytes
        m.signers.append(makeSigner(QStringLiteral("Signer 1"), QStringLiteral("sig_A")));

        // Attacker mutates the document (any writer with file access)…
        QVector<gp::SignatureFieldCreator::Spec> specs;
        specs.append({ QStringLiteral("Mutant"), 0, QRectF(72.0, 400.0, 150.0, 60.0) });
        QString createErr;
        QVERIFY(gp::SignatureFieldCreator::createSignatureFields(doc, specs, doc, &createErr));
        const QString mutatedHash = sha256OfFile(doc);
        QVERIFY(mutatedHash != m.preparedSha256);

        // …and pre-seeds reconfirmedSha256 in the UNSIGNED sidecar. No user
        // dialog has run; the field is indistinguishable from an app-written
        // one (SendForSigningController::runSignStep writes the same field).
        m.reconfirmedSha256 = mutatedHash;

        SignatureManager mgr;
        SigningRequestRunner::FillStepInput in;
        in.docPath = doc;
        in.model = m;
        in.signerIndex = 0;
        in.requestedLevel = PAdESLevel::B_B;

        const auto pre = SigningRequestRunner::precheck(mgr, in);
        // DESIRED (the documented honesty rule): refuse — the app cannot know
        // the "re-confirm" came from the user.
        QCOMPARE(int(pre.code), int(SigningRequestRunner::StepRefusal::DocumentChanged));
        // Actual (pre-fix): pre.code == None → the fill flow proceeds straight
        // to the SignatureDialog with NO DocumentChanged disclosure, and the
        // user signs the mutated bytes believing they are the prepared ones.
    }

    // ── S2 ──────────────────────────────────────────────────────────────────
    // A sidecar anchor must never place the signature field outside the page
    // (an invisible field the signer cannot see but does sign). DESIRED: the
    // creator refuses (or clamps) an off-page anchor. ACTUAL: it succeeds and
    // the widget /Rect ends up fully outside the MediaBox.
    void sidecarAnchorNotClampedToPage()
    {
        REQUIRE_FIXTURE();
        auto tmp = newTmp("s2");
        QVERIFY(tmp);
        const QString doc = tmp->filePath(QStringLiteral("offpage.pdf"));
        QVERIFY(QFile::copy(kInputPdf, doc));

        QVector<gp::SignatureFieldCreator::Spec> specs;
        // The exact data a crafted <file>.signrequest.json can carry:
        // rectFromJson only demands x,y >= 0 and w,h > 0 (no page-bounds check),
        // and precheck only demands anchorRect.isValid().
        specs.append({ QStringLiteral("OffPage"), 0,
                       QRectF(200000.0, 200000.0, 200.0, 100.0) });
        QString err;
        const bool created = gp::SignatureFieldCreator::createSignatureFields(doc, specs, doc, &err);
        // DESIRED: refuse an anchor that cannot be visible on the page.
        QVERIFY2(!created, "createSignatureFields accepted an off-page anchor — "
                           "an invisible signature field can be planted via the sidecar");
        if (!created) return;

        // Demonstrate the consequence if the refusal above is ever relaxed:
        // the widget rect lies fully outside the page MediaBox.
        PoDoFo::PdfMemDocument doc2;
        doc2.Load(doc.toUtf8().constData());
        auto &page = doc2.GetPages().GetPageAt(0);
        const PoDoFo::Rect media = page.GetMediaBox();
        bool outside = false;
        for (PoDoFo::PdfField *field : doc2.GetFieldsIterator()) {
            if (QString::fromStdString(field->GetFullName()) != QStringLiteral("OffPage"))
                continue;
            auto *sig = static_cast<PoDoFo::PdfSignature *>(field);
            if (auto *widget = sig->GetWidget()) {
                const PoDoFo::Rect r = widget->GetRect();
                outside = r.X > media.Width || r.Y > media.Height ||
                          r.X + r.Width > media.Width || r.Y + r.Height > media.Height;
            }
            break;
        }
        QVERIFY(outside); // the planted field is genuinely invisible
    }

    // ── S3 ──────────────────────────────────────────────────────────────────
    // The attained-level label must not claim B-T (or above) from a token that
    // was never validated. SignatureManager.cpp clears timestampMissing=false
    // as soon as fetchTimestampToken returns ANY non-empty bytes (no
    // d2i_TS_RESP / CMS parse of the token), so an HTML "200 OK" error page
    // from a misconfigured or hostile TSA is embedded verbatim and the UI
    // reports "PAdES B-T attained". DESIRED: the label for a never-validated
    // token stays at the honest floor (B-B). ACTUAL: "B-T".
    void attainedLevelLabelsGarbageTimestampAsBt()
    {
        // This is exactly the SignatureOutcomeDetail the embed produces for a
        // garbage (non-empty, unparseable) TSA response: timestampMissing=false.
        SignatureOutcomeDetail garbageToken;
        garbageToken.timestampMissing = false;
        // F2 follow-up (three-state contract): a real embed ATTEMPTS the fetch
        // and records that the token failed to parse — absence of an attempt
        // (default false) must never floor, or every pre-sign B-T preview
        // would read "B-B".
        garbageToken.timestampAttempted = true;
        garbageToken.timestampTokenValid = false;

        const QString label = gp::SecurityController::attainedLevelLabel(
            PAdESLevel::B_T, garbageToken);
        // DESIRED: presence of bytes is NOT attainment — refuse to claim B-T.
        QCOMPARE(label, QStringLiteral("B-B"));
        // Actual (pre-fix): returns "B-T" — an overclaim the failure wording
        // then repeats ("the signature is B-T, not B-B").
    }

    // ── S4 ──────────────────────────────────────────────────────────────────
    // The machine policy is documented as admin-controlled "by ACL assumption"
    // (PolicyController.h), but load() verifies NO provenance: on default
    // Windows ACLs any interactive user can pre-create the default
    // %PROGRAMDATA%\GlyphPDF\policy.json before an admin ever does, and every
    // value it carries flows into the signing dispatch. Here the GLYPHPDF_
    // POLICY_PATH seam stands in for the squatter file (same parse/enforce
    // path, no ACL check in either). DESIRED: an unverified policy must not
    // drive signing. ACTUAL: it does.
    // ── S5 ──────────────────────────────────────────────────────────────────
    // The Network page + support bundle claim "enabled = the touchpoint WILL
    // fire under the CURRENT settings" (NetworkTouchpoints.h). Enforcement is
    // policy-effective, but enumerate() reads RAW QSettings — so with a
    // managed signing/tsaUrl and an empty user setting the page shows the TSA
    // touchpoint as DISABLED while every sign dispatch WILL fetch it. DESIRED:
    // the enumerated state matches enforcement. ACTUAL: disabled.
    void networkTouchpointMissingPolicyEnabledTsa()
    {
        auto tmp = newTmp("s5");
        QVERIFY(tmp);
        const QString squatterPolicy = tmp->filePath(QStringLiteral("policy.json"));
        QJsonObject settings;
        settings.insert(QStringLiteral("signing/tsaUrl"),
                        QStringLiteral("https://tsa.corp.example/rfc3161"));
        QJsonObject root;
        root.insert(QStringLiteral("schemaVersion"), 1);
        root.insert(QStringLiteral("settings"), settings);
        QFile f(squatterPolicy);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QJsonDocument(root).toJson());
        f.close();

        qputenv("GLYPHPDF_POLICY_PATH", squatterPolicy.toUtf8());
        auto &policy = gp::PolicyController::instance();
        policy.resetForTesting();
        policy.ensureLoaded();
        QCOMPARE(policy.state(), gp::PolicyController::State::Loaded);

        const QString ini = tmp->filePath(QStringLiteral("user.ini"));
        QSettings user(ini, QSettings::IniFormat);
        QVERIFY(!user.contains(QStringLiteral("signing/tsaUrl"))); // user: none

        const auto tps = gp::NetworkTouchpoints::enumerate(user);
        qputenv("GLYPHPDF_POLICY_PATH", QByteArray());
        policy.resetForTesting();

        const gp::NetworkTouchpoint *tsa = nullptr;
        for (const auto &tp : tps)
            if (tp.id == QStringLiteral("tsa")) { tsa = &tp; break; }
        QVERIFY(tsa);
        // DESIRED: the touchpoint is enabled — readSigningConfig WILL dispatch
        // to the policy TSA on the next signing above B-B / timestamp.
        QVERIFY2(tsa->enabled,
                 "TSA touchpoint shown DISABLED while the enforced policy URL "
                 "makes it fire — the Network page / support bundle misreport "
                 "the machine's true network behavior under policy");
        // Actual (pre-fix): enabled == false ("Disabled: no TSA URL configured").
    }
};

QTEST_MAIN(SweepW1SecProbe)
#include "SweepW1SecProbe.moc"
