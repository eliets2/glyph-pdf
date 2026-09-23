// SPDX-License-Identifier: Apache-2.0
// SEP13 lead 1 — a failed B-T timestamp is silently downgraded to B-B but the
// signDocument outcome is still plain Success.
//
// PARITY-GLM-REVIEW-2026-09-13 lead (SignatureManager.cpp, BtPdfSigner::
// ComputeSignature): when the requested PAdES level is >= B_T and a TSA URL is
// configured, an empty/failed timestamp fetch only produces
//   qWarning() << "B-T: TSA returned empty token — signature downgrades to B-B";
// No flag is recorded (Private tracks only dssMissing / docTimestampMissing),
// overallOk is untouched, so d->lastOutcome becomes SignOutcome::Success —
// whose interface contract reads "Fully successful at the requested PAdES
// level". The user believes they produced a timestamped signature.
//
// Probe: sign the fixture input with B_T and an UNREACHABLE https TSA (dead
// port on 127.0.0.1 → httpPost fails fast → empty token → the silent
// downgrade path). The correct contract (outcome must not claim full Success
// with no degradation detail) FAILS on candidate 83be3c2.
//
// NOTE: httpPost delivers via a queued invocation to qApp and blocks on a
// 20 s semaphore; inside a QtTest slot the GUI thread's event loop is blocked,
// so the fetch times out — which lands in exactly the same empty-token
// downgrade path being confirmed (a slower but equivalent TSA failure).
#include <QtTest/QtTest>
#include <QFile>
#include <QTemporaryDir>

#include "engines/SignatureManager.h"

// SignatureManager / SignOutcome / PAdESLevel live in the GLOBAL namespace.

namespace {

// Same fixtures TestSignatureRealCrypto uses (repo-committed).
const QString kFixtureDir = QStringLiteral(SOURCE_DIR) + "/tests/fixtures/signing";
const QString kInputPdf = kFixtureDir + "/test_input.pdf";
const QString kP12Path  = kFixtureDir + "/test_signer.p12";
const QString kP12Pass  = QStringLiteral("test");

// TSA that cannot be reached: reserved discard port on loopback → immediate
// connection-refused (or the harness semaphore timeout — same empty result).
const QString kDeadTsa = QStringLiteral("https://127.0.0.1:9/tsa");

} // namespace

class TestSep13LeadBtDowngrade : public QObject {
    Q_OBJECT

private slots:
    void init() {
        if (!QFileInfo::exists(kP12Path) || !QFileInfo::exists(kInputPdf))
            QSKIP("signing fixtures not present (tests/fixtures/signing)");
    }

    // Control pin (must PASS): B_T with NO TSA configured signs and reports
    // Success — pins the harness.
    void btWithoutTsaReportsSuccess() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        SignatureManager mgr;
        mgr.setSignatureLevel(PAdESLevel::B_T);
        // no setTsaUrl → the B-T enhancement is not requested
        const QString out = tmp.filePath("ctl_signed.pdf");
        const SignOutcome outcome = mgr.signDocument(kInputPdf, out, kP12Path,
                                                     kP12Pass, "ctl", "");
        QVERIFY2(outcome == SignOutcome::Success, "control: plain B_T sign must succeed");
    }

    // LEAD 1 CONFIRMATION (expected FAILURE on the candidate): a B_T request
    // whose TSA is unreachable must NOT be reported as an unconditional
    // Success with zero degradation detail.
    void failedTimestampMustNotReportPlainSuccess() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        SignatureManager mgr;
        mgr.setSignatureLevel(PAdESLevel::B_T);
        mgr.setTsaUrl(kDeadTsa);
        const QString out = tmp.filePath("deadtsa_signed.pdf");
        const SignOutcome outcome = mgr.signDocument(kInputPdf, out, kP12Path,
                                                     kP12Pass, "lead1", "");
        const SignatureOutcomeDetail detail = mgr.lastSignOutcomeDetail();
        qInfo() << "dead-TSA outcome:" << int(outcome)
                << "dssMissing =" << detail.dssMissing
                << "docTimestampMissing =" << detail.docTimestampMissing;

        QVERIFY2(QFile::exists(out), "the signed file itself must exist (B-B bytes were written)");

        // CORRECT contract: either the outcome is not a plain Success, or the
        // outcome detail records the B-T→B-B degradation. Candidate behavior:
        // plain Success, both detail flags false — the downgrade is invisible.
        QVERIFY2(outcome != SignOutcome::Success
                     || detail.dssMissing || detail.docTimestampMissing,
                 "SEP13 lead 1 CONFIRMED: B_T signing with an unreachable TSA was "
                 "reported as plain SignOutcome::Success with no degradation "
                 "detail — the silent B-B downgrade is invisible to the caller/UI");
    }
};

#include "TestSep13LeadBtDowngrade.moc"
QTEST_MAIN(TestSep13LeadBtDowngrade)
