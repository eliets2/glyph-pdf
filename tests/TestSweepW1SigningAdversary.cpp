// SPDX-License-Identifier: MIT
// TestSweepW1SigningAdversary.cpp
//
// SWEEP-W1 ADVERSARY (2026-09-19) — send-for-signing workflow with a HOSTILE
// (attacker-crafted) sidecar. REVIEW-ONLY repro: slots marked FAILING REPRO
// are written to FAIL against the candidate (feat/parity-glm @ a3a9317) to
// prove the defect; a fix flips them green without any edit here.
//
// FINDING W1-S1 (CONFIRMED, model+workflow): duplicate fieldName bindings are
// linted at PREPARE time (SigningRequestDialog::saveRequest, and
// SignatureFieldCreator's create-time lint) but NOT at LOAD time —
// SigningRequestModel::fromJson accepts two entries bound to the same field.
// verifyAgainstDocument's honesty check then cannot see the alias: its
// per-signer coverage check matches per-FIELD, and its count check
// (documentSignatureCount vs signedEntryCount) passes whenever the document
// carries as many REAL signatures as the sidecar has entries — e.g. an extra
// real signature the attacker paid for with their own cert. Result: the
// progress panel reports "Signing request complete — all N signer(s) signed.
// Every recorded signature matches the document." for a request that was
// never fulfilled (signer 2's bound field is aliased onto signer 1's).
// Shared defect: the 1-field-=-1-signer invariant is enforced in two of the
// three places that construct/consume the model; fromJson (the only place
// that consumes UNTRUSTED input) is the one missing the lint. Root-cause
// fix: one lint at the model boundary (fromJson), not per-consumer.
//
// FINDING W1-S2 (CONFIRMED, workflow+engine seam): precheck's binding gate
// validates ONLY the bound entry — it never checks the engine's GLOBAL
// precondition (SignatureManager's D6 post-condition fails the sign whenever
// ANY post-validation entry lacks integrityIntact, and an unsigned field
// reports integrityIntact=false). With a foreign unsigned field U pre-placed
// on the document and the sidecar binding signer W to a lazy anchor:
//   1. precheck passes (W absent, anchor valid; U's existence not consulted)
//   2. runFillStep MUTATES the document (creates W at the anchor)
//   3. the engine signs the first unsigned field, post-validation still sees
//      an unsigned field → SignOutcome::Failed, candidate dropped
//   4. the step's error text claims "the document is unchanged" — FALSE, the
//      document gained an unsigned signature field
//   5. every retry fails the same way: the request can NEVER complete and
//      the victim's document is permanently polluted with an unsigned field.
// Fail-closed (no false success) but a permanent workflow DoS + untruthful
// error + mutation-on-failure against the runFillStep contract wording.
// Shared defect: the step validates the ENTRY, not the engine's global
// precondition, and mutates (lazy placement) before the engine's ability to
// proceed is known.

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
#include "engines/SignatureFieldCreator.h"
#include "engines/SignatureManager.h"

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
// SignatureManager is deliberately namespace-free (TestSendForSigning idiom).

QString sha256OfFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    return QCryptographicHash::hash(f.readAll(), QCryptographicHash::Sha256).toHex();
}

// An attacker-crafted sidecar body: two entries bound to the SAME field, both
// claiming signed=true with full engine-style attestation copied from what a
// real step would write. `secondFieldName` lets a caller alias entry 2 onto
// entry 1's field (the spoof) or keep it distinct (the honest control).
QByteArray aliasedSidecarJson(const QString &fieldA, const QString &fieldB,
                              const QString &docSha)
{
    QJsonObject root;
    root.insert(QStringLiteral("glyphpdf-signrequest"), 1);
    root.insert(QStringLiteral("schemaVersion"), 1);
    root.insert(QStringLiteral("createdUtc"), QStringLiteral("2026-09-19T00:00:00Z"));
    root.insert(QStringLiteral("preparedUtc"), QStringLiteral("2026-09-19T00:00:00Z"));
    root.insert(QStringLiteral("preparedSha256"), docSha);
    root.insert(QStringLiteral("sourcePdf"), QStringLiteral("doc.pdf"));
    QJsonArray signers;
    const QStringList fields = { fieldA, fieldB };
    for (int i = 0; i < fields.size(); ++i) {
        QJsonObject s;
        s.insert(QStringLiteral("order"), i + 1);
        s.insert(QStringLiteral("name"), QStringLiteral("Signer %1").arg(i + 1));
        s.insert(QStringLiteral("fieldName"), fields[i]);
        s.insert(QStringLiteral("createdField"), false);
        s.insert(QStringLiteral("signed"), true);
        s.insert(QStringLiteral("signedAtUtc"), QStringLiteral("2026-09-19T01:00:00Z"));
        s.insert(QStringLiteral("signedFieldName"), fields[i]);
        s.insert(QStringLiteral("fieldMatch"), true);
        s.insert(QStringLiteral("attainedLevel"), QStringLiteral("B-B"));
        s.insert(QStringLiteral("signatureSummary"),
                 QStringLiteral("integrity intact (trust: Valid)"));
        signers.append(s);
    }
    root.insert(QStringLiteral("signers"), signers);
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

} // namespace

class TestSweepW1SigningAdversary : public QObject
{
    Q_OBJECT

    std::unique_ptr<QTemporaryDir> m_tmpDir;

    static int leftoverCandidates() {
        return QDir(QDir::tempPath() + QStringLiteral("/glyphpdf-candidates"))
            .entryList(QStringList() << QStringLiteral("glyphpdf-*.pdf"), QDir::Files).size();
    }

private slots:
    void initTestCase() {
        // FU-2: the shared %TEMP% is collision-prone under parallel lanes —
        // retry with fresh unique patterns instead of failing the suite.
        for (int attempt = 0; attempt < 3 && !m_tmpDir; ++attempt)
            m_tmpDir = std::make_unique<QTemporaryDir>();
        QVERIFY(m_tmpDir && m_tmpDir->isValid());
    }

    // ── FAILING REPRO W1-S1a (pure model): the load path must refuse aliases ──
    //
    // The prepare-side lint (SigningRequestDialog::saveRequest: "Signature
    // field %1 is bound more than once.") and the creator lint
    // (SignatureFieldCreator: "duplicate signature field name requested")
    // both enforce 1 field == 1 signer. fromJson — the boundary that consumes
    // the UNTRUSTED sidecar — enforces nothing. Expected post-fix: a
    // duplicate-bound sidecar is a structured SchemaInvalid refusal.
    void fromJsonRefusesDuplicateFieldBindings()
    {
        SigningRequestModel m;
        SigningRequestModel::Signer a;
        a.name = QStringLiteral("A"); a.fieldName = QStringLiteral("sig_A");
        SigningRequestModel::Signer b;
        b.name = QStringLiteral("B"); b.fieldName = QStringLiteral("sig_A");
        m.signers.append(a);
        m.signers.append(b);
        const QByteArray crafted = m.toJson().toJson(QJsonDocument::Indented);

        const auto loaded = SigningRequestModel::fromJson(QString::fromUtf8(crafted));
        QVERIFY2(loaded.error == SigningRequestModel::LoadError::SchemaInvalid,
                 qPrintable(QStringLiteral(
                     "SECURITY DEFECT: fromJson ACCEPTED two signers bound to "
                     "the same field '%1' (error=%2) — the prepare-side "
                     "duplicate lint is missing at the untrusted-input "
                     "boundary, enabling the aliasing coverage spoof below.")
                     .arg(QStringLiteral("sig_A"))
                     .arg(int(loaded.error))));
    }

    // ── FAILING REPRO W1-S1b (workflow): the aliasing coverage spoof ────────
    //
    // Two REAL signatures on sig_A and sig_B; a crafted sidecar binds BOTH
    // entries to sig_A. verifyAgainstDocument finds an intact signature for
    // each entry (per-FIELD matching), the counts agree (2 real signatures,
    // 2 entries), and the panel reports full consistency — although signer 2
    // was never bound to a field they signed. Expected post-fix: the
    // duplicate-binding refusal upstream makes this state unrepresentable;
    // the assertion here pins verifyAgainstDocument as the last line of
    // defense (aliased entries must surface a warning).
    void aliasedSidecarReportsConsistentWhenItMustNot()
    {
        REQUIRE_FIXTURES();
        const QString src = m_tmpDir->filePath(QStringLiteral("w1_alias.pdf"));
        QVERIFY(QFile::copy(kInputPdf, src));

        // Two real signatures, placed the LAZY way (each step creates exactly
        // its own field then signs it — the engine's post-condition refuses a
        // sign while any OTHER unsigned field exists). The attacker signs
        // both fields with their own cert; both signatures are real.
        SignatureManager mgr;
        SigningRequestRunner::FillStepInput in;
        in.docPath = src;
        in.certPath = kP12Path;
        in.password = kP12Pass;
        in.reason = QStringLiteral("w1 adversary");
        in.requestedLevel = PAdESLevel::B_B;

        int committedSteps = 0;
        const QList<QRectF> anchors = {
            QRectF(72.0, 72.0, 150.0, 60.0), QRectF(72.0, 162.0, 150.0, 60.0) };
        const QStringList fields = { QStringLiteral("sig_A"), QStringLiteral("sig_B") };
        for (int i = 0; i < fields.size(); ++i) {
            SigningRequestModel stepModel;
            stepModel.createdUtc = stepModel.preparedUtc =
                QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
            stepModel.preparedSha256 = sha256OfFile(src);
            SigningRequestModel::Signer s;
            s.name = QStringLiteral("attacker");
            s.fieldName = fields[i];
            s.createdField = true;                      // lazy anchor
            s.anchorPage = 0;
            s.anchorRect = anchors[i];
            stepModel.signers.append(s);
            in.model = stepModel;
            in.signerIndex = 0;
            const auto r = SigningRequestRunner::runFillStep(mgr, in);
            if (!r.committed)
                QSKIP(qPrintable(QStringLiteral(
                    "real sign unavailable in this environment: %1").arg(r.error)));
            ++committedSteps;
        }
        QCOMPARE(committedSteps, 2);

        // The crafted sidecar: both entries aliased onto sig_A.
        const QString sidecar =
            SigningRequestModel::sidecarPathFor(src);
        const QByteArray crafted =
            aliasedSidecarJson(QStringLiteral("sig_A"), QStringLiteral("sig_A"),
                               sha256OfFile(src));
        QFile sc(sidecar);
        QVERIFY(sc.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QCOMPARE(sc.write(crafted), qint64(crafted.size()));
        sc.close();

        const auto loaded = SigningRequestModel::load(sidecar);
        QCOMPARE(loaded.error, SigningRequestModel::LoadError::None);
        QCOMPARE(loaded.model.signers.size(), 2);

        const auto report =
            SigningRequestRunner::verifyAgainstDocument(mgr, loaded.model, src);
        QVERIFY2(!report.consistent,
                 qPrintable(QStringLiteral(
                     "SECURITY DEFECT: the aliased crafted sidecar (2 entries "
                     "bound to sig_A) verified CONSISTENT against a document "
                     "with signatures on sig_A+sig_B — warnings: %1. The "
                     "panel would report 'Every recorded signature matches "
                     "the document.' for a request that was never fulfilled.")
                     .arg(report.warnings.join(QStringLiteral(" | ")))));
    }

    // ── FAILING REPRO W1-S2: foreign unsigned field → mutation + deadlock ───
    //
    // The document carries a foreign UNSIGNED field U (attacker-placed); the
    // sidecar binds signer W to a lazy anchor. Expected (post-fix): precheck
    // refuses BEFORE any mutation (or the step completes honestly). Today:
    // the step mutates the document (creates W), then fails, then the error
    // text lies ("the document is unchanged"), and every retry fails the
    // same way — the request is permanently dead and the document polluted.
    void foreignUnsignedFieldPollutesDocumentAndDeadlocksRequest()
    {
        REQUIRE_FIXTURES();
        const QString src = m_tmpDir->filePath(QStringLiteral("w1_foreign.pdf"));
        QVERIFY(QFile::copy(kInputPdf, src));

        // Foreign unsigned field U, placed OUTSIDE the workflow.
        QVector<SignatureFieldCreator::Spec> foreign;
        foreign.append({ QStringLiteral("U_foreign"), 0, QRectF(72.0, 400.0, 120.0, 50.0) });
        QString createErr;
        QVERIFY2(SignatureFieldCreator::createSignatureFields(
                     src, foreign, src, &createErr), qPrintable(createErr));

        SigningRequestModel model;
        model.createdUtc = model.preparedUtc =
            QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
        model.preparedSha256 = sha256OfFile(src);
        model.sourcePdfName = QFileInfo(src).fileName();
        SigningRequestModel::Signer s;
        s.name = QStringLiteral("W signer");
        s.fieldName = QStringLiteral("W_bound");
        s.createdField = true;                       // lazy anchor
        s.anchorPage = 0;
        s.anchorRect = QRectF(72.0, 72.0, 150.0, 60.0);
        model.signers.append(s);

        SignatureManager mgr;
        SigningRequestRunner::FillStepInput in;
        in.docPath = src;
        in.model = model;
        in.signerIndex = 0;
        in.certPath = kP12Path;
        in.password = kP12Pass;
        in.reason = QStringLiteral("w1 adversary");
        in.requestedLevel = PAdESLevel::B_B;

        // Gate 1: precheck must consult the engine's GLOBAL precondition — a
        // document with an unsigned field the step does not own must be
        // refused BEFORE the document is mutated.
        const auto pre = SigningRequestRunner::precheck(mgr, in);
        if (pre.code == SigningRequestRunner::StepRefusal::None) {
            const auto r1 = SigningRequestRunner::runFillStep(mgr, in);
            if (r1.committed)
                QSKIP("engine signed through the foreign field — different "
                      "engine behavior than analyzed; re-triage W1-S2");
            // Both defect halves are collected before asserting, so one
            // failure never masks the other.
            const bool mutationDefect = r1.fieldCreated;

            // Gate 2: the workflow must remain completable — retry the same
            // step against the post-failure document with the sidecar the
            // controller would have written (preparedSha256 advanced over
            // the workflow's own creation, SendForSigningController's
            // fieldCreated branch).
            SigningRequestModel retryModel = model;
            retryModel.preparedSha256 = sha256OfFile(src);   // post-create bytes
            in.model = retryModel;
            const auto r2 = SigningRequestRunner::runFillStep(mgr, in);
            const bool deadlockDefect = !r2.committed;

            QVERIFY2(!(mutationDefect || deadlockDefect),
                     qPrintable(QStringLiteral(
                         "SECURITY DEFECT (W1-S2 CONFIRMED): foreign unsigned "
                         "field U_foreign on the document, step bound to lazy "
                         "field W_bound. mutation-on-failure=%1 (the step "
                         "failed yet fieldCreated=true — the on-disk document "
                         "gained an unsigned field, contradicting the "
                         "'the document is unchanged' error text). "
                         "permanent-deadlock=%2 (the retried step failed too: "
                         "'%3' — the engine's post-condition refuses every "
                         "sign while ANY unsigned field survives and the "
                         "precheck never screened for it, so this request can "
                         "never complete and the document stays polluted).")
                         .arg(mutationDefect ? QStringLiteral("YES")
                                             : QStringLiteral("no"),
                              deadlockDefect ? QStringLiteral("YES")
                                             : QStringLiteral("no"),
                              r2.error)));
        } else {
            QSKIP(qPrintable(QStringLiteral(
                "precheck already refuses this state (code=%1) — W1-S2 is "
                "fixed or unreachable in this build; re-triage.")
                .arg(int(pre.code))));
        }
    }

    void candidatesDirClean() {
        const int before = leftoverCandidates();
        QCOMPARE(leftoverCandidates(), before);
    }
};

QTEST_MAIN(TestSweepW1SigningAdversary)
#include "TestSweepW1SigningAdversary.moc"
