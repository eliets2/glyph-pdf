// SPDX-License-Identifier: Apache-2.0
// TestGsdW2Probe — the GSD-verifier W2 pass's OWN independent probes
// (docs/audit/SWEEP-W2-GSD-2026-09-21.md). One slot per queued fix row;
// every slot uses ITS OWN fixture values, harness shape, or entry path —
// deliberately different from the committed pins each row ships with — so a
// pass here is a second, independent confirmation, not a replay.
//
// Rows probed (all implemented-awaiting-review at branch time):
//   EM-1  645f4994  read-only Form Builder bypass (boundary + policy)
//   EM-2  3325ea19  policy-blocked batch OCR honest failure (fra, both legs)
//   EM-3  ee2cf661  signature badge anchors on all rotations (off-center rect)
//   EM-4  cee2777c  cross-version rect replay refusal (rot 90 variant)
//   EM-5  9463f6c0  policy-literate TSA refusal (B-LT level variant)
//   EM-6  c1552c26  SafeSave destination identity (same-size + kept-mtime)
//   E-2   ed04426  sanitize trailer UAF (in-place data-absence mechanism)
//   ri    e620757b  runIntersects glyph band (own two-line geometry)
//   F2b-D1 72069bd8 BatchPresetStore root mkpath (deep/blocked/recreated)
//   F2-3s 2d29a16  timestampAttempted three-state label truth table

#include <QtTest/QtTest>
#include <QAtomicInt>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QTemporaryDir>
#include <QUndoStack>

#include "core/AppContext.h"
#include "core/PolicyController.h"
#include "core/interfaces/IOcrEngine.h"
#include "core/interfaces/IPdfEditorEngine.h"
#include "core/interfaces/ISignatureManager.h"
#include "core/SigningRequestModel.h"
#include "core/SigningRequestRunner.h"
#include "core/BatchPreset.h"
#include "commands/EditFormFieldCommand.h"
#include "engines/DocumentSession.h"
#include "engines/FormManager.h"
#include "engines/PdfEditorEngine.h"
#include "engines/RedactOperation.h"
#include "engines/SafeSave.h"
#include "engines/SignatureFieldCreator.h"
#include "engines/SignatureManager.h"
#include "engines/pdfium/PdfiumBackend.h"
#include "modes/BatchMode.h"
#include "shell/EditPolicy.h"
#include "shell/controllers/SecurityController.h"
#include <podofo/podofo.h>

using gp::PolicyController;
using gp::SecurityController;

namespace {

constexpr const char* kFixtureDir = "tests/fixtures/signing";
const QString kP12Path = QString(kFixtureDir) + "/test_signer.p12";
const QString kP12Pass = QStringLiteral("test");
const QString kInputPdf = QString(kFixtureDir) + "/test_input.pdf";

QByteArray sha256File(const QString& p)
{
    QFile f(p);
    if (!f.open(QIODevice::ReadOnly)) return {};
    return QCryptographicHash::hash(f.readAll(), QCryptographicHash::Sha256).toHex();
}

QByteArray fileBytes(const QString& p)
{
    QFile f(p);
    if (!f.open(QIODevice::ReadOnly)) return {};
    return f.readAll();
}

// Minimal hand-written one-page PDF (byte-accurate xref) — my own builder.
// `lines` = (text, baseline y) pairs; empty list = image-only semantics.
QString writeLinePdf(const QString& p, int rotation,
                     const QList<QPair<QString, double>>& lines,
                     const QByteArray& mediaBox = "[0 0 612 792]")
{
    QByteArray pdf = "%PDF-1.4\n";
    QList<qint64> offsets;
    const auto mark = [&offsets, &pdf]() { offsets.append(pdf.size()); };
    const auto obj = [&pdf, &mark](int n, const QByteArray& body) {
        mark();
        pdf += QByteArray::number(n) + " 0 obj\n" + body + "\nendobj\n";
    };
    QByteArray content;
    for (const auto& l : lines)
        content += QByteArray("BT /F1 12 Tf 72 ") + QByteArray::number(l.second)
                 + " Td (" + l.first.toUtf8() + ") Tj ET\n";
    obj(1, "<</Type/Catalog/Pages 2 0 R>>");
    obj(2, "<</Type/Pages/Kids[3 0 R]/Count 1>>");
    obj(3, "<</Type/Page/Parent 2 0 R/MediaBox" + mediaBox + "/Rotate "
             + QByteArray::number(rotation) + "/Contents 4 0 R"
             + "/Resources<</Font<</F1 5 0 R>>>>>>");
    obj(4, "<</Length " + QByteArray::number(content.size()) + ">>stream\n"
             + content + "endstream");
    obj(5, "<</Type/Font/Subtype/Type1/BaseFont/Helvetica"
             "/Encoding/WinAnsiEncoding>>");
    const qint64 xref = pdf.size();
    pdf += QString("xref\n0 %1\n").arg(offsets.size() + 1).toLatin1();
    pdf += "0000000000 65535 f \n";
    for (qint64 o : offsets)
        pdf += QString("%1 00000 n \n").arg(o, 10, 10, QChar('0')).toLatin1();
    pdf += QString("trailer<</Size %1/Root 1 0 R>>\nstartxref\n%2\n%%EOF\n")
               .arg(offsets.size() + 1).arg(xref).toLatin1();
    QFile f(p);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return {};
    if (f.write(pdf) != pdf.size()) return {};
    return p;
}

// OCR stub mirroring the engine gate's refusal contract; records how many
// images reach processImage (the wrong-language trap must be unreachable).
struct GsdRefusedOcr final : public IOcrEngine {
    QAtomicInt calls{0};
    QString requestedLang;
    bool initialize(const QString& lang, const QString&) override
    {
        requestedLang = lang;
        return false; // the (stubbed) refused language-data download
    }
    QList<OcrResult> processImage(const QImage&) override
    {
        calls.fetchAndAddRelaxed(1);
        OcrResult r;
        r.text = QStringLiteral("ocrlayer");
        r.boundingBox = QRectF(10, 10, 80, 20);
        r.confidence = 95;
        return { r };
    }
    QString getRawText(const QImage&) override { return QStringLiteral("ocrlayer"); }
    bool isMockImplementation() const override { return true; }
};

struct GsdOcrHarness {
    QTemporaryDir tmp;
    AppContext ctx;
    gp::BatchMode bm;
    std::shared_ptr<GsdRefusedOcr> ocr;
    bool ok() const { return tmp.isValid(); }
    GsdOcrHarness()
    {
        ctx.pdfEditor = std::make_shared<PdfEditorEngine>();
        ocr = std::make_shared<GsdRefusedOcr>();
        ctx.ocr = ocr;
        bm.setAppContext(&ctx);
    }
};

bool writeManagedPolicyFile(const QString& p, const QJsonObject& settings)
{
    QFile pf(p);
    if (!pf.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    const bool written =
        pf.write(QJsonDocument(QJsonObject{
                       { QStringLiteral("schemaVersion"), 1 },
                       { QStringLiteral("settings"), settings } })
                     .toJson()) > 0;
    pf.close();
    return written && PolicyController::instance().load(p);
}

QString whyNotWithNoOutputClause(const gp::BatchMode& bm)
{
    for (int i = 0; i < bm.errorLogCount(); ++i) {
        const QString d = bm.errorDetailForTest(i);
        if (d.contains(QStringLiteral("no output was written"))) return d;
    }
    return {};
}

void drainBatch(gp::BatchMode& bm)
{
    int waited = 0;
    while (bm.isBatchRunning() && waited < 30000) {
        QTest::qWait(50);
        waited += 50;
    }
    // QueuedConnection accounting lag: drain until the counters settle.
    int settled = 0;
    while (settled < 5000
           && bm.successCount() + bm.failCount() + bm.skipCount() == 0) {
        QTest::qWait(50);
        settled += 50;
    }
}

} // namespace

class TestGsdW2Probe : public QObject {
    Q_OBJECT

    QTemporaryDir m_dir;

    QString path(const QString& name) const { return m_dir.filePath(name); }

private slots:
    void initTestCase()
    {
        // Isolate QSettings (the TestBatchOcrLanguage idiom): the probe must
        // neither read the user's real preferences nor clobber them.
        QCoreApplication::setOrganizationName(QStringLiteral("GlyphPDFTests"));
        QCoreApplication::setApplicationName(QStringLiteral("TestGsdW2Probe"));
        QVERIFY(m_dir.isValid());
        PolicyController::instance().resetForTesting();
        // W1-05 structural close: disclosed assume-trusted seam — this
        // probe's policy fixtures are standard-user-written (the untrusted-
        // owner gate itself is pinned in TestPolicyWiring).
        qputenv("GLYPHPDF_POLICY_ASSUME_TRUSTED", "1");
    }
    void cleanupTestCase()
    {
        PolicyController::instance().resetForTesting();
        qunsetenv("GLYPHPDF_POLICY_ASSUME_TRUSTED");
        QSettings().remove(QStringLiteral("ocr/allowNetworkDownload"));
        QSettings().remove(QStringLiteral("ocr/language"));
    }

    // ── EM-1 (645f4994): the persistence boundary is the gate ───────────────
    // Angle unlike the committed pins: a command that SUCCEEDED while the
    // session was writable must leave the file immutable once the session
    // locks over, and the ONE policy must be consultable directly.
    void em1_boundaryGateBlocksApplyAndPolicyIsTheOneTruth()
    {
        const QString pdf = writeLinePdf(path(QStringLiteral("em1.pdf")), 0,
                                         { { QStringLiteral("EM1 probe text"), 700.0 } });
        QVERIFY(!pdf.isEmpty());
        FormManager fm;
        QVERIFY(fm.addTextField(pdf, 0, QRectF(72, 150, 140, 30),
                                QStringLiteral("em1_field"), pdf));

        DocumentSession doc;
        doc.setPath(pdf);
        QUndoStack stack;

        // A tooltip edit applied while WRITABLE — the legitimate path.
        EditFormFieldProperties tipProps;
        tipProps.tooltip = QStringLiteral("pre-lock tooltip");
        stack.push(new EditFormFieldCommand(&fm, &doc,
                                            QStringLiteral("em1_field"), tipProps));
        QCOMPARE(stack.count(), 1);
        const QByteArray afterTip = sha256File(pdf);
        QVERIFY(!afterTip.isEmpty());

        // Lock-over. A fresh push is refused and leaves no undo entry.
        doc.setReadOnly(true);
        EditFormFieldProperties evil;
        evil.tooltip = QStringLiteral("must never persist");
        evil.defaultVal = QStringLiteral("must never persist");
        stack.push(new EditFormFieldCommand(&fm, &doc,
                                            QStringLiteral("em1_field"), evil));
        // The refused push adds NO entry (obsolete -> deleted): the stack
        // still holds exactly the pre-lock command.
        QCOMPARE(stack.count(), 1);
        QCOMPARE(stack.index(), 1);
        QCOMPARE(sha256File(pdf), afterTip);

        // The boundary reports the ONE policy's message, not a bare error.
        EditFormFieldCommand direct(&fm, &doc, QStringLiteral("em1_field"), evil);
        direct.redo();
        QVERIFY2(!direct.succeeded(), "read-only apply must fail at the boundary");
        QVERIFY2(direct.lastError().contains(QStringLiteral("read-only")),
                 qPrintable(QStringLiteral("boundary must carry the read-only "
                                          "disclosure, got: %1").arg(direct.lastError())));
        QCOMPARE(gp::EditPolicy::mutationBlocked(&doc), true);
        QVERIFY(!gp::EditPolicy::readOnlyMessage().isEmpty());

        // The file still loads and was never corrupted by the refused paths.
        PdfEditorEngine loader;
        QVERIFY(loader.loadDocumentForEditing(pdf));
    }

    // ── EM-2 (3325ea19): the USER-setting leg, French, zero engine feeds ────
    void em2_userSettingLegFailsHonestlyFrenchNoEngineFeeds()
    {
        // Leg 1: NO policy — the deciding half is the USER's disabled setting.
        QSettings().setValue(QStringLiteral("ocr/language"), QStringLiteral("FR"));
        QSettings().setValue(QStringLiteral("ocr/allowNetworkDownload"), false);
        PolicyController::instance().resetForTesting();

        GsdOcrHarness h1;
        QVERIFY(h1.ok());
        const QString scanned =
            writeLinePdf(h1.tmp.filePath(QStringLiteral("gsd_scan.pdf")), 0, {});
        QVERIFY(!scanned.isEmpty());
        QSettings langProbeSettings;
        qWarning() << "GSD-EM2 readback ocr/language ="
                   << langProbeSettings.value(QStringLiteral("ocr/language")).toString();
        h1.bm.addFilesForTest({ scanned });
        h1.bm.setOperationForTest(5); // OCR
        h1.bm.onRunBatch();
        drainBatch(h1.bm);

        QCOMPARE(h1.bm.failCount(), 1);
        QCOMPARE(h1.bm.successCount(), 0);
        QCOMPARE(h1.ocr->calls.loadRelaxed(), 0); // never fed: no wrong-language layer
        QCOMPARE(h1.ocr->requestedLang, QStringLiteral("fra"));
        QVERIFY2(!QFileInfo::exists(h1.tmp.filePath(QStringLiteral("gsd_scan_ocr.pdf"))),
                 "no _ocr.pdf may exist for a refused file");
        const QString detail = whyNotWithNoOutputClause(h1.bm);
        QVERIFY2(detail.contains(QStringLiteral("the OCR download setting")),
                 qPrintable(QStringLiteral("unmanaged refusal must name the user "
                                          "setting, got: %1").arg(detail)));
        QVERIFY2(detail.contains(QStringLiteral("fra")), qPrintable(detail));
        QVERIFY2(!detail.contains(QStringLiteral("machine policy")),
                 qPrintable(QStringLiteral("no policy is loaded — the whyNot must "
                                          "not claim one, got: %1").arg(detail)));

        // Leg 2 (fresh harness): user opted IN, MANAGED policy refuses —
        // the wording must flip to the policy for the same fra request.
        QSettings().setValue(QStringLiteral("ocr/allowNetworkDownload"), true);
        QVERIFY(writeManagedPolicyFile(
            h1.tmp.filePath(QStringLiteral("gsd-e2-policy.json")),
            QJsonObject{ { QStringLiteral("ocr/allowNetworkDownload"), false } }));

        GsdOcrHarness h2;
        QVERIFY(h2.ok());
        const QString scanned2 =
            writeLinePdf(h2.tmp.filePath(QStringLiteral("gsd_scan.pdf")), 0, {});
        h2.bm.addFilesForTest({ scanned2 });
        h2.bm.setOperationForTest(5);
        h2.bm.onRunBatch();
        drainBatch(h2.bm);
        QCOMPARE(h2.bm.failCount(), 1);
        QCOMPARE(h2.ocr->calls.loadRelaxed(), 0);
        const QString detail2 = whyNotWithNoOutputClause(h2.bm);
        QVERIFY2(detail2.contains(QStringLiteral("machine policy")),
                 qPrintable(QStringLiteral("managed refusal must name the policy, "
                                          "got: %1").arg(detail2)));
        QVERIFY2(detail2.contains(QStringLiteral("fra")), qPrintable(detail2));

        PolicyController::instance().resetForTesting();
        QSettings().remove(QStringLiteral("ocr/allowNetworkDownload"));
        QSettings().remove(QStringLiteral("ocr/language"));
    }

    // ── EM-3 (ee2cf661): badge anchors, MY off-center rect, all rotations ───
    void em3_badgeAnchorReadsBackTheDisplayedFieldEveryRotation()
    {
        // Off-center, asymmetric — no flip can cancel into it by accident.
        // Two law shapes per rotation: the origin-0 box AND an offset-origin
        // box (lower-left 100,50) — the legacy read-back dropped the MediaBox
        // lower-left origin, so the offset shape is what discriminates.
        const QRectF anchor(73.0, 411.0, 219.0, 33.0);
        const QList<QPair<QByteArray, const char*>> boxes = {
            { QByteArrayLiteral("[0 0 612 792]"), "o0" },
            { QByteArrayLiteral("[100 50 712 842]"), "o100x50" },
        };
        const auto close = [](const QRectF& a, const QRectF& b) {
            return std::fabs(a.x() - b.x()) < 0.01
                && std::fabs(a.y() - b.y()) < 0.01
                && std::fabs(a.width() - b.width()) < 0.01
                && std::fabs(a.height() - b.height()) < 0.01;
        };
        for (const auto& box : boxes) {
            for (const int rotation : { 0, 90, 180, 270 }) {
                const QString tag = QStringLiteral("%1-r%2").arg(box.second).arg(rotation);
                const QString src = writeLinePdf(path(QStringLiteral("em3-%1.pdf").arg(tag)),
                                                 rotation,
                                                 { { QStringLiteral("EM3 anchor probe"), 640.0 } },
                                                 box.first);
                QVERIFY(!src.isEmpty());
                const QString out = path(QStringLiteral("em3-out-%1.pdf").arg(tag));
                QVector<gp::SignatureFieldCreator::Spec> specs;
                gp::SignatureFieldCreator::Spec spec;
                spec.fieldName = QStringLiteral("GsdSig%1").arg(tag);
                spec.pageIndex = 0;
                spec.viewerRect = anchor;
                specs.append(spec);
                QString err;
                QVERIFY2(gp::SignatureFieldCreator::createSignatureFields(src, specs, out, &err),
                         qPrintable(QStringLiteral("%1 placement refused: %2").arg(tag, err)));

                SignatureManager mgr;
                const auto anchors = mgr.signatureFieldAnchors(out);
                const ISignatureManager::SignatureFieldAnchor* a = nullptr;
                for (const auto& cand : anchors)
                    if (cand.fieldName == spec.fieldName) { a = &cand; break; }
                QVERIFY2(a, qPrintable(QStringLiteral("%1: anchor missing").arg(tag)));
                QCOMPARE(a->pageIndex, 0);
                QVERIFY2(close(a->rect, anchor),
                         qPrintable(QStringLiteral("EM-3 %1: badge anchor %2,%3 %4x%5 "
                                                  "!= displayed field %6,%7 %8x%9")
                                        .arg(tag)
                                        .arg(a->rect.x()).arg(a->rect.y())
                                        .arg(a->rect.width()).arg(a->rect.height())
                                        .arg(anchor.x()).arg(anchor.y())
                                        .arg(anchor.width()).arg(anchor.height())));
            }
        }
    }

    // ── EM-4 (cee2777c): the replay trap on /Rotate 90 ──────────────────────
    void em4_rot90ReplayTrapRefusedAndProductionFieldNeverRefused()
    {
        if (!QFileInfo::exists(kP12Path) || !QFileInfo::exists(kInputPdf))
            QSKIP("Signing fixtures missing (tests/fixtures/signing).");

        // Trap leg: a pre-W2B-1 build stored view-space rects straight into
        // PoDoFo's CreateField (its own rotation adjustment applied AGAIN).
        // Simulated exactly that way on a /Rotate 90 page with MY anchors.
        const QRectF anchor(100.0, 200.0, 120.0, 40.0); // view rect (792x612 display)
        const QString src = path(QStringLiteral("em4_rot90.pdf"));
        QVERIFY(QFile::copy(kInputPdf, src));
        try {
            PoDoFo::PdfMemDocument doc;
            doc.Load(src.toUtf8().constData());
            doc.GetPages().GetPageAt(0).SetRotation(90);
            doc.Save(src.toUtf8().constData());
        } catch (const std::exception& e) {
            QFAIL(qPrintable(QStringLiteral("rotate fixture failed: %1").arg(e.what())));
        }
        try {
            PoDoFo::PdfMemDocument doc;
            doc.Load(src.toUtf8().constData());
            doc.GetPages().GetPageAt(0).CreateField<PoDoFo::PdfSignature>(
                "gsd_sig_a", PoDoFo::Rect(anchor.x(), anchor.y(),
                                          anchor.width(), anchor.height()));
            doc.Save(src.toUtf8().constData());
        } catch (const std::exception& e) {
            QFAIL(qPrintable(QStringLiteral("pre-fix field simulation failed: %1")
                                 .arg(QString::fromUtf8(e.what()))));
        }

        SigningRequestModel m;
        m.createdUtc = m.preparedUtc = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
        m.preparedSha256 = sha256File(src); // the post-create (corrupted) bytes
        m.sourcePdfName = QFileInfo(src).fileName();
        SigningRequestModel::Signer s;
        s.name = QStringLiteral("Gsd Signer");
        s.fieldName = QStringLiteral("gsd_sig_a");
        s.anchorPage = 0;
        s.anchorRect = anchor;
        s.createdField = true;
        m.signers.append(s);

        // Precondition: the stored rect really diverges from the anchor under
        // the current law (the fixture IS the corrupted shape).
        SignatureManager mgr;
        const auto anchorsNow = mgr.signatureFieldAnchors(src);
        QCOMPARE(anchorsNow.size(), 1);
        QVERIFY2(anchorsNow.first().rect != anchor,
                 "fixture must carry the corrupted rect for the trap to bite");

        SigningRequestRunner::FillStepInput in;
        in.docPath = src;
        in.model = m;
        in.signerIndex = 0;
        in.certPath = kP12Path;
        in.password = kP12Pass;
        in.reason = QStringLiteral("gsd probe");
        in.requestedLevel = PAdESLevel::B_B;

        const QByteArray shaBefore = sha256File(src);
        const auto refused = SigningRequestRunner::runFillStep(mgr, in);
        QVERIFY2(!refused.attempted,
                 "the rot-90 replayed fill must refuse before any engine call");
        QCOMPARE(SigningRequestRunner::precheck(mgr, in).code,
                 SigningRequestRunner::StepRefusal::AnchorMismatch);
        QVERIFY2(refused.error.contains(QStringLiteral("position or size")),
                 qPrintable(refused.error));
        QVERIFY2(refused.error.contains(QStringLiteral("older version")),
                 qPrintable(QStringLiteral("the refusal must name the cross-version "
                                          "cause, got: %1").arg(refused.error)));
        QCOMPARE(sha256File(src), shaBefore); // zero mutation

        // Control leg: a field created by the CURRENT production creator at
        // the same anchor must NEVER be refused (no false positive).
        const QString src2 = path(QStringLiteral("em4_control.pdf"));
        QVERIFY(QFile::copy(kInputPdf, src2));
        try {
            PoDoFo::PdfMemDocument doc;
            doc.Load(src2.toUtf8().constData());
            doc.GetPages().GetPageAt(0).SetRotation(90);
            doc.Save(src2.toUtf8().constData());
        } catch (const std::exception& e) {
            QFAIL(qPrintable(e.what()));
        }
        QVector<gp::SignatureFieldCreator::Spec> specs;
        gp::SignatureFieldCreator::Spec spec;
        spec.fieldName = QStringLiteral("gsd_sig_ctrl");
        spec.pageIndex = 0;
        spec.viewerRect = anchor;
        specs.append(spec);
        QString cerrStr;
        QVERIFY2(gp::SignatureFieldCreator::createSignatureFields(src2, specs, src2, &cerrStr),
                 qPrintable(cerrStr));
        SigningRequestModel m2;
        m2.createdUtc = m2.preparedUtc =
            QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
        m2.preparedSha256 = sha256File(src2);
        m2.sourcePdfName = QFileInfo(src2).fileName();
        SigningRequestModel::Signer s2;
        s2.name = QStringLiteral("Gsd Signer");
        s2.fieldName = QStringLiteral("gsd_sig_ctrl");
        s2.anchorPage = 0;
        s2.anchorRect = anchor;
        s2.createdField = true;
        m2.signers.append(s2);
        SigningRequestRunner::FillStepInput in2;
        in2.docPath = src2;
        in2.model = m2;
        in2.signerIndex = 0;
        in2.certPath = kP12Path;
        in2.password = kP12Pass;
        in2.reason = QStringLiteral("gsd probe control");
        in2.requestedLevel = PAdESLevel::B_B;
        QCOMPARE(SigningRequestRunner::precheck(mgr, in2).code,
                 SigningRequestRunner::StepRefusal::None);
    }

    // ── EM-5 (9463f6c0): the B-LT variant names the policy, no dead advice ──
    void em5_policyTsaRefusalAtBltLevelAndHonestUnmanagedControl()
    {
        // Managed-empty policy file; the effective TSA URL is empty.
        const QString policyPath = path(QStringLiteral("gsd-e5-policy.json"));
        QVERIFY(writeManagedPolicyFile(
            policyPath,
            QJsonObject{ { QStringLiteral("signing/tsaUrl"), QString() } }));

        const QString refusal = SecurityController::signingPreflightRefusal(
            PAdESLevel::B_LT, QString(), /*forTimestamp*/ false);
        QVERIFY2(!refusal.isEmpty(), "B-LT without an effective TSA URL must refuse");
        QVERIFY2(refusal.contains(QStringLiteral("managed by machine policy")),
                 qPrintable(refusal));
        QVERIFY2(refusal.contains(QStringLiteral("signing/tsaUrl")), qPrintable(refusal));
        QVERIFY2(refusal.contains(QStringLiteral("B-LT")),
                 qPrintable(QStringLiteral("the B>BB refusal must interpolate the "
                                          "requested level, got: %1").arg(refusal)));
        QVERIFY2(!refusal.contains(QStringLiteral("Set the TSA URL under Preferences")),
                 qPrintable(QStringLiteral("managed refusal must not point at the "
                                          "dead setter, got: %1").arg(refusal)));

        // Unmanaged: the SAME B-LT request keeps the actionable advice and
        // never claims a policy.
        PolicyController::instance().resetForTesting();
        const QString plain = SecurityController::signingPreflightRefusal(
            PAdESLevel::B_LT, QString(), false);
        QVERIFY2(!plain.isEmpty(), "the unmanaged refusal must still fire");
        QVERIFY2(plain.contains(QStringLiteral("Set the TSA URL under Preferences")),
                 qPrintable(plain));
        QVERIFY2(!plain.contains(QStringLiteral("machine policy")), qPrintable(plain));

        PolicyController::instance().resetForTesting();
    }

    // ── EM-6 (c1552c26): same-size + preserved-mtime stale writer ───────────
    void em6_sameSizeKeptMtimeStaleCommitRefused()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        // Equal-LENGTH markers (22 chars each) => same-size files, divergent
        // bytes — any size heuristic is defeated by construction.
        const QString dest = makeMarkedPdf(tmp.path(), QStringLiteral("e6-dest.pdf"),
                                           QStringLiteral("DEST-V1-ORIGINAL-BYTES"));
        QVERIFY(!dest.isEmpty());
        const QByteArray destSha = sha256File(dest);
        const qint64 destSize = QFileInfo(dest).size();
        const QDateTime destMtime = QFileInfo(dest).lastModified();

        // Instance B captures the identity of the CURRENT bytes.
        const auto identity = gp::SafeSave::captureDestinationIdentity(dest);
        QVERIFY(identity.valid);

        // Instance A replaces the destination with a SAME-SIZE different
        // document and restores the original mtime.
        const QString rival = makeMarkedPdf(tmp.path(), QStringLiteral("e6-rival.pdf"),
                                            QStringLiteral("RIVAL-WRITER-NEW-BYTES"));
        QVERIFY(!rival.isEmpty());
        QCOMPARE(QFileInfo(rival).size(), destSize);
        QVERIFY2(sha256File(rival) != destSha, "the rival must differ in bytes");
        QVERIFY(QFile::remove(dest));
        QVERIFY(QFile::copy(rival, dest));
        {
            QFile tf(dest);
            QVERIFY(tf.open(QIODevice::ReadWrite));
            QVERIFY2(tf.setFileTime(destMtime, QFile::FileModificationTime),
                     "mtime restore must succeed (a writable handle is required)");
            tf.close();
        }

        // B's stale candidate: a deterministic re-serialization of what B
        // still holds — the ORIGINAL V1 bytes.
        const QString stale = makeMarkedPdf(tmp.path(), QStringLiteral("e6-stale.pdf"),
                                            QStringLiteral("DEST-V1-ORIGINAL-BYTES"));
        QVERIFY(!stale.isEmpty());
        QCOMPARE(sha256File(stale), destSha); // genuinely the stale revision

        QString err;
        QVERIFY2(!gp::SafeSave::commitFileToDestination(
                     stale, dest, &err,
                     gp::SafeSave::CommitFaultForTesting::None, identity),
                 qPrintable(QStringLiteral("same-size stale commit must refuse: %1").arg(err)));
        QVERIFY2(err.contains(QStringLiteral("changed on disk")), qPrintable(err));
        QVERIFY2(sha256File(dest) == sha256File(rival),
                 "the rival writer's bytes must survive untouched");

        // A fresh identity (the CURRENT bytes) commits normally.
        const auto fresh = gp::SafeSave::captureDestinationIdentity(dest);
        QVERIFY(fresh.valid);
        QVERIFY2(gp::SafeSave::commitFileToDestination(stale, dest, &err,
                                                       gp::SafeSave::CommitFaultForTesting::None,
                                                       fresh),
                 qPrintable(err));

        // Vanished destination: a valid identity whose file is gone refuses.
        const auto ghost = gp::SafeSave::captureDestinationIdentity(dest);
        QVERIFY(ghost.valid);
        QVERIFY(QFile::remove(dest));
        QString err2;
        QVERIFY2(!gp::SafeSave::commitFileToDestination(stale, dest, &err2,
                                                        gp::SafeSave::CommitFaultForTesting::None,
                                                        ghost),
                 "a vanished destination must refuse under a valid identity");
        QVERIFY2(err2.contains(QStringLiteral("changed on disk")), qPrintable(err2));
    }

    // ── E-2 sanitize UAF (ed04426): the IN-PLACE mechanism, observed ────────
    void em_sanitizeTwiceInfoAndOutlinesScrubbedInPlace()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        // My own fixture: /Info metadata + a REAL /Outlines tree.
        const QString pdf = tmp.filePath(QStringLiteral("gsd-uaf.pdf"));
        {
            QFile f(pdf);
            QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
            QByteArray out = "%PDF-1.4\n";
            QList<qint64> offs;
            const auto add = [&](const QByteArray& body) {
                offs.append(out.size());
                out += QByteArray::number(offs.size()) + " 0 obj\n" + body + "\nendobj\n";
            };
            add("<</Type/Catalog/Pages 2 0 R/Outlines 6 0 R/PageMode/UseOutlines>>");
            add("<</Type/Pages/Kids[3 0 R]/Count 1>>");
            add("<</Type/Page/Parent 2 0 R/MediaBox[0 0 612 792]/Contents 4 0 R"
                "/Resources<</Font<</F1 5 0 R>>>>>>");
            const QByteArray stream =
                QByteArray("BT /F1 12 Tf 72 700 Td (GsdUafProbe visible text) Tj ET\n");
            add("<</Length " + QByteArray::number(stream.size()) + ">>stream\n"
                + stream + "endstream");
            add("<</Type/Font/Subtype/Type1/BaseFont/Helvetica>>");
            add("<</Type/Outlines/First 7 0 R/Last 7 0 R/Count 1>>");
            add("<</Title(GsdSecretBookmark)/Parent 6 0 R/Dest[3 0 R/Fit]>>");
            add("<</Title(GsdSecretTitle)/Author(GsdSecretAuthor)"
                "/Producer(GsdSecretProducer)>>");
            const qint64 xref = out.size();
            out += QString("xref\n0 %1\n").arg(offs.size() + 1).toLatin1();
            out += "0000000000 65535 f \n";
            for (qint64 o : offs)
                out += QString("%1 00000 n \n").arg(o, 10, 10, QChar('0')).toLatin1();
            out += QString("trailer<</Size %1/Root 1 0 R/Info 8 0 R>>\nstartxref\n%2\n%%EOF\n")
                       .arg(offs.size() + 1).arg(xref).toLatin1();
            f.write(out);
        }

        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(pdf));
        churnHeap();
        const QString out1 = tmp.filePath(QStringLiteral("gsd-uaf-1.pdf"));
        const QString out2 = tmp.filePath(QStringLiteral("gsd-uaf-2.pdf"));
        QVERIFY2(engine.sanitizeDocument(out1), "sanitize #1 must succeed");
        churnHeap();
        QVERIFY2(engine.sanitizeDocument(out2),
                 "sanitize #2 on the SAME loaded document must succeed (the soak "
                 "signature would SegFault here on the regression)");
        churnHeap();
        PdfMetadata meta;
        QVERIFY2(engine.getMetadata(meta), "metadata read on the live document must not crash");

        // The output: metadata GONE (data-absence), structure ALIVE (keys
        // kept — the in-place contract, distinguishable from key-removal),
        // pages intact, and a SECOND engine (PDFium) loads and extracts.
        PoDoFo::PdfMemDocument outDoc;
        outDoc.Load(out2.toUtf8().constData());
        auto& trailer = outDoc.GetTrailer().GetDictionary();
        auto* info = trailer.FindKey("Info");
        QVERIFY2(info, "the trailer must still REFERENCE /Info (in-place, not removed)");
        QVERIFY2(!info->GetDictionary().FindKey("Title")
                     && !info->GetDictionary().FindKey("Author")
                     && !info->GetDictionary().FindKey("Producer"),
                 "no user metadata may survive in /Info");
        auto* root = trailer.FindKey("Root");
        QVERIFY(root);
        auto* outlines = root->GetDictionary().FindKey("Outlines");
        QVERIFY2(outlines, "the catalog must still REFERENCE /Outlines (in-place)");
        QVERIFY2(!outlines->GetDictionary().FindKey("First")
                     && !outlines->GetDictionary().FindKey("Last")
                     && !outlines->GetDictionary().FindKey("Count"),
                 "zero bookmark DATA may survive in /Outlines");
        QCOMPARE(outDoc.GetPages().GetCount(), 1);

        PdfiumBackend backend;
        QVERIFY(backend.loadDocument(out2));
        const QString text = backend.extractText(0);
        QVERIFY2(text.contains(QStringLiteral("GsdUafProbe")),
                 qPrintable(QStringLiteral("document text must survive sanitization; got: %1")
                                .arg(text)));
    }

    // ── ri-fix (e620757b): MY between-lines geometry passes; overlap fails ──
    void rifix_markBetweenLinesProofPasses_andOverlapStillFails()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        // My own fixture: secret at baseline 700, public neighbor at 660
        // (12pt Helvetica: old headroom 660+36=696; real ink top ~668.6).
        const QString src = tmp.filePath(QStringLiteral("rifix-src.pdf"));
        try {
            PoDoFo::PdfMemDocument doc;
            auto& font = doc.GetFonts().GetStandard14Font(
                PoDoFo::PdfStandard14FontType::Helvetica);
            auto draw = [&](PoDoFo::PdfPage& page, const char* t, double y) {
                PoDoFo::PdfPainter p;
                p.SetCanvas(page);
                p.TextState.SetFont(font, 12.0);
                (p.DrawText)(t, 72.0, y);
                p.FinishDrawing();
            };
            auto& page = doc.GetPages().CreatePage(
                PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
            draw(page, "GsdSecretZulu classified line", 700.0);
            draw(page, "GsdPublicEcho visible neighbor", 660.0);
            doc.Save(src.toUtf8().constData());
        } catch (const std::exception& e) {
            QFAIL(qPrintable(QStringLiteral("fixture failed: %1").arg(e.what())));
        }

        // Leg 1 — the mark's user band [682..727] dips 14pt into the OLD
        // 3*fs headroom (696 >= 682) while staying >13pt clear of the
        // neighbor's REAL ink (top ~668.6): must PASS, secret-only attribution.
        {
            QMap<int, QList<QRectF>> rects;
            rects[0].append(QRectF(60.0, 842.0 - 727.0, 320.0, 45.0));
            gp::RedactRequest req;
            req.sourcePath = src;
            req.destinationPath = tmp.filePath(QStringLiteral("rifix-out1.pdf"));
            req.redactionsByPage = rects;
            req.produceProof = true;
            gp::RedactOperation op(req);
            const gp::RedactResult r = runOp(&op);
            QCOMPARE(r.outcome, gp::RedactOutcome::Completed);
            QVERIFY2(r.proofPassed,
                     qPrintable(QStringLiteral("GSD ri: a between-lines mark must "
                                              "not false-alarm: %1")
                                    .arg(joinedProofFailures(r))));
            const QJsonObject root = jsonRoot(r.proofJsonPath);
            const QJsonArray removed = root["excisions"].toArray()
                                           .at(0).toObject()["removed_strings"].toArray();
            bool sawSecret = false, sawPublic = false;
            for (const auto& v : removed) {
                const QString t = v.toString();
                if (t.contains(QStringLiteral("GsdSecretZulu"))) sawSecret = true;
                if (t.contains(QStringLiteral("GsdPublicEcho"))) sawPublic = true;
            }
            QVERIFY2(sawSecret, "the secret line must be attributed");
            QVERIFY2(!sawPublic, "the 660-neighbor inside the old 3*fs headroom "
                                 "must NOT be attributed");
            PdfiumBackend backend;
            QVERIFY(backend.loadDocument(req.destinationPath));
            const QString text = backend.extractText(0);
            QVERIFY(!text.contains(QStringLiteral("GsdSecretZulu")));
            QVERIFY(text.contains(QStringLiteral("GsdPublicEcho")));
        }

        // Leg 2 — honesty direction: a sloppy mark whose band CUTS the
        // neighbor's real ink WITHOUT covering its baseline (user band
        // [663..695]: ink top ~668.6 > 663, baseline 660 < 663) must still
        // attribute it (band-overlap) — and since the excision is
        // baseline-based the neighbor survives, so the proof must FAIL
        // naming it (no containment-style over-tightening may silence a
        // real survivor).
        {
            QMap<int, QList<QRectF>> rects;
            rects[0].append(QRectF(60.0, 842.0 - 695.0, 320.0, 32.0));
            gp::RedactRequest req;
            req.sourcePath = src;
            req.destinationPath = tmp.filePath(QStringLiteral("rifix-out2.pdf"));
            req.redactionsByPage = rects;
            req.produceProof = true;
            gp::RedactOperation op(req);
            const gp::RedactResult r = runOp(&op);
            QCOMPARE(r.outcome, gp::RedactOutcome::Completed);
            QVERIFY2(!r.proofPassed,
                     "a mark overlapping the neighbor's ink must not pass while "
                     "the neighbor's text survives");
            const QString failures = joinedProofFailures(r);
            QVERIFY2(failures.contains(QStringLiteral("GsdPublicEcho")),
                     qPrintable(QStringLiteral("the failure must name the surviving "
                                              "neighbor line, got: %1").arg(failures)));
        }
    }

    // ── F2b-D1 (72069bd8): deep root, file-blocked root, recreated root ─────
    void f2bd1_storeRootSurvivesDeepMissingAndBlockedPaths()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        // (a) THREE missing levels deep — mkpath must create the chain.
        const QString deep = tmp.path() + QStringLiteral("/a/b/c/presets");
        gp::BatchPresetStore deepStore(deep);
        gp::BatchPreset p;
        p.name = QStringLiteral("Gsd Deep");
        p.steps.append({ QStringLiteral("compress"), {}, { { "quality", 50 } } });
        QString err;
        QVERIFY2(deepStore.save(&p, &err),
                 qPrintable(QStringLiteral("deep first-save must succeed: %1").arg(err)));
        QVERIFY(deepStore.contains(p.id));

        // (b) the root path is BLOCKED by an existing FILE — an honest
        // refusal, never a crash, never a silent false success.
        const QString blocked = tmp.filePath(QStringLiteral("blocked-file"));
        { QFile f(blocked); QVERIFY(f.open(QIODevice::WriteOnly)); f.write("x"); }
        gp::BatchPresetStore blockedStore(blocked);
        gp::BatchPreset pb;
        pb.name = QStringLiteral("Gsd Blocked");
        pb.steps.append({ QStringLiteral("compress"), {}, { { "quality", 50 } } });
        QString err2;
        QVERIFY2(!blockedStore.save(&pb, &err2),
                 "a file occupying the root path must refuse the save");
        QVERIFY2(err2.contains(QStringLiteral("cannot create store directory")),
                 qPrintable(QStringLiteral("the refusal must be the honest mkpath "
                                          "failure, got: %1").arg(err2)));

        // (c) the root VANISHES under a live session: renaming the now-missing
        // file must fail HONESTLY (error names the path, no crash), and a NEW
        // save must re-create the root (the save boundary owns it).
        QVERIFY(QDir(deep).removeRecursively());
        QVERIFY2(!QFileInfo::exists(deep), "precondition: root gone");
        QString err3;
        QVERIFY2(!deepStore.rename(p.id, QStringLiteral("Gsd Deep Renamed"), &err3),
                 "renaming a preset whose file vanished must refuse");
        QVERIFY2(!err3.isEmpty(), "the refusal must carry the honest path error");
        gp::BatchPreset p2;
        p2.name = QStringLiteral("Gsd Deep Two");
        p2.steps.append({ QStringLiteral("compress"), {}, { { "quality", 70 } } });
        QVERIFY2(deepStore.save(&p2, &err),
                 qPrintable(QStringLiteral("save after the root vanished must "
                                          "re-create it: %1").arg(err)));
        QVERIFY(QFileInfo::exists(deep));
        QVERIFY(deepStore.contains(p2.id));
    }

    // ── F2 three-state (2d29a16): the attained-label truth table ────────────
    void timestampAttemptedLabelTruthTable()
    {
        SignatureOutcomeDetail fresh; // no sign ran: everything false
        fresh.timestampMissing = false;
        fresh.timestampAttempted = false;
        fresh.timestampTokenValid = false;
        // Absence of an attempt NEVER floors: a pre-sign B_T preview reads B-T.
        QCOMPARE(SecurityController::attainedLevelLabel(PAdESLevel::B_T, fresh),
                 QStringLiteral("B-T"));

        // KNOWN-failed attempt (the embed's detail on a garbage TSA body):
        // bytes present, token invalid -> the honest B-B floor.
        SignatureOutcomeDetail garbage = fresh;
        garbage.timestampAttempted = true;
        garbage.timestampTokenValid = false;
        QCOMPARE(SecurityController::attainedLevelLabel(PAdESLevel::B_T, garbage),
                 QStringLiteral("B-B"));
        QCOMPARE(SecurityController::attainedLevelLabel(PAdESLevel::B_LT, garbage),
                 QStringLiteral("B-B"));

        // Explicit known-missing keeps the floor.
        SignatureOutcomeDetail missing = fresh;
        missing.timestampMissing = true;
        QCOMPARE(SecurityController::attainedLevelLabel(PAdESLevel::B_T, missing),
                 QStringLiteral("B-B"));

        // A validated token attains the requested level.
        SignatureOutcomeDetail valid = fresh;
        valid.timestampAttempted = true;
        valid.timestampTokenValid = true;
        QCOMPARE(SecurityController::attainedLevelLabel(PAdESLevel::B_T, valid),
                 QStringLiteral("B-T"));

        // B_B never claims above its level under any detail.
        QCOMPARE(SecurityController::attainedLevelLabel(PAdESLevel::B_B, garbage),
                 QStringLiteral("B-B"));
    }

private:
    static gp::RedactResult runOp(gp::RedactOperation* op)
    {
        gp::RedactResult captured;
        QObject::connect(op, &gp::RedactOperation::finished, op,
                         [&captured](const gp::RedactResult& r) { captured = r; });
        op->run();
        return captured;
    }
    static QString joinedProofFailures(const gp::RedactResult& r)
    {
        return r.proofFailures.join(QStringLiteral(" || "));
    }
    static QJsonObject jsonRoot(const QString& jsonPath)
    {
        return QJsonDocument::fromJson(fileBytes(jsonPath)).object();
    }
    static void churnHeap()
    {
        for (int i = 0; i < 200; ++i) {
            auto* p = new int[64];
            p[0] = i;
            delete[] p;
        }
    }
    // A tiny PDF whose bytes embed `marker` (equal-length markers give
    // equal-size files) — my own builder for the EM-6 writer scenario.
    static QString makeMarkedPdf(const QString& dir, const QString& name,
                                 const QString& marker)
    {
        const QString p = dir + "/" + name;
        QByteArray pdf = "%PDF-1.4\n";
        QList<qint64> offs;
        const auto add = [&](const QByteArray& body) {
            offs.append(pdf.size());
            pdf += QByteArray::number(offs.size()) + " 0 obj\n" + body + "\nendobj\n";
        };
        const QByteArray text = "BT /F1 12 Tf 72 700 Td (" + marker.toUtf8()
                              + ") Tj ET\n";
        add("<</Type/Catalog/Pages 2 0 R>>");
        add("<</Type/Pages/Kids[3 0 R]/Count 1>>");
        add("<</Type/Page/Parent 2 0 R/MediaBox[0 0 612 792]/Contents 4 0 R"
            "/Resources<</Font<</F1 5 0 R>>>>>>");
        add("<</Length " + QByteArray::number(text.size()) + ">>stream\n"
            + text + "endstream");
        add("<</Type/Font/Subtype/Type1/BaseFont/Helvetica>>");
        const qint64 xref = pdf.size();
        pdf += QString("xref\n0 %1\n").arg(offs.size() + 1).toLatin1();
        pdf += "0000000000 65535 f \n";
        for (qint64 o : offs)
            pdf += QString("%1 00000 n \n").arg(o, 10, 10, QChar('0')).toLatin1();
        pdf += QString("trailer<</Size %1/Root 1 0 R>>\nstartxref\n%2\n%%EOF\n")
                   .arg(offs.size() + 1).arg(xref).toLatin1();
        QFile f(p);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return {};
        if (f.write(pdf) != pdf.size()) return {};
        return p;
    }
};

QTEST_MAIN(TestGsdW2Probe)
#include "TestGsdW2Probe.moc"
