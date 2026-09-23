// SPDX-License-Identifier: Apache-2.0
// TestPgr35BatchCollision.cpp
//
// PGR-35 (D2 delta review 2026-09-23) — cross-file output-path collision in
// the batch worker. Two inputs resolving to the SAME output path used to run
// "successfully": each commit overwrote the previous output, the G12 ledger
// claimed N successes, and exactly one artifact existed. No warning fired —
// the AR-8 overwrite pre-check only looks at files that ALREADY exist.
//
// Reproducing seed (the hostile shareable preset below IS the seed — a
// preset author (or attacker) only has to omit {basename}/{n} from the
// naming template; "{date}.pdf" is schema-v1-valid and collapses EVERY file
// of the run onto one name):
//
//   {
//     "glyphpreset": {"schemaVersion": 1, "kind": "batch-preset"},
//     "id": "collide", "name": "Collide",
//     "created": "...", "modified": "...",
//     "steps": [{"op": "watermark", "params": {"text": "X", "opacity": 30}}],
//     "output": {"naming": "{date}.pdf"}
//   }
//
// driven through the REAL BatchMode worker (offscreen) over two distinct
// fixtures. Fail-before: successCount==2, failCount==0 (the second watermark
// silently overwrote the first output). Post-fix: the first file (list
// order) keeps the path, the second is staged as a pre-flight failure whose
// reason names the collision.
//
// Run: QT_QPA_PLATFORM=offscreen ctest -R TestPgr35BatchCollision

#include <QtTest/QtTest>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QLineEdit>
#include <QTemporaryDir>

#include "core/AppContext.h"
#include "core/BatchPreset.h"
#include "modes/BatchMode.h"
#include "mocks/MockPdfEditorEngine.h"

using gp::BatchMode;

namespace {

AppContext makeCtx() {
    AppContext ctx;
    ctx.pdfEditor = std::shared_ptr<IPdfEditorEngine>(
        new MockPdfEditorEngine, [](auto*){});
    return ctx;
}

// Minimal one-page PDF with a text line (QPdfDocument-readable; the preset
// chain's baseline probe opens every fixture).
QString createTextPdf(const QString& dir, const QString& name,
                      const QString& text)
{
    QDir().mkpath(dir);
    const QString path = QDir(dir).filePath(name);
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return {};
    QByteArray out = "%PDF-1.4\n";
    QList<qint64> off;
    auto addObj = [&out, &off](const QByteArray& body) {
        off.append(out.size());
        out += QByteArray::number(off.size()) + " 0 obj\n" + body + "\nendobj\n";
    };
    addObj("<</Type/Catalog/Pages 2 0 R>>");
    addObj("<</Type/Pages/Kids[3 0 R]/Count 1>>");
    addObj("<</Type/Page/Parent 2 0 R/MediaBox[0 0 612 792]/Contents 4 0 R"
           "/Resources<</Font<</F1 5 0 R>>>>>>");
    const QByteArray stream = "BT /F1 12 Tf 72 700 Td (" + text.toUtf8() + ") Tj ET\n";
    addObj("<</Length " + QByteArray::number(stream.size()) + ">>stream\n" + stream + "endstream");
    addObj("<</Type/Font/Subtype/Type1/BaseFont/Helvetica>>");
    const qint64 xref = out.size();
    out += QString("xref\n0 %1\n").arg(off.size() + 1).toLatin1();
    out += "0000000000 65535 f \n";
    for (qint64 o : off)
        out += QString("%1 00000 n \n").arg(o, 10, 10, QChar('0')).toLatin1();
    out += QString("trailer<</Size %1/Root 1 0 R>>\nstartxref\n%2\n%%EOF\n")
               .arg(off.size() + 1).arg(xref).toLatin1();
    f.write(out);
    return path;
}

// The hostile (or merely careless) preset seed, rooted into the test store.
bool writeCollidePreset(const QString& storeDir)
{
    QDir().mkpath(storeDir);
    QJsonObject envelope;
    envelope.insert(QStringLiteral("schemaVersion"), 1);
    envelope.insert(QStringLiteral("kind"), QStringLiteral("batch-preset"));
    QJsonObject root;
    root.insert(QStringLiteral("glyphpreset"), envelope);
    root.insert(QStringLiteral("id"), QStringLiteral("collide"));
    root.insert(QStringLiteral("name"), QStringLiteral("Collide"));
    const QString now = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    root.insert(QStringLiteral("created"), now);
    root.insert(QStringLiteral("modified"), now);
    QJsonArray steps;
    QJsonObject step;
    step.insert(QStringLiteral("op"), QStringLiteral("watermark"));
    QJsonObject params;
    params.insert(QStringLiteral("text"), QStringLiteral("X"));
    params.insert(QStringLiteral("opacity"), 30);
    step.insert(QStringLiteral("params"), params);
    steps.append(step);
    root.insert(QStringLiteral("steps"), steps);
    QJsonObject output;
    output.insert(QStringLiteral("naming"), QStringLiteral("{date}.pdf"));
    root.insert(QStringLiteral("output"), output);

    QFile f(QDir(storeDir).filePath(QStringLiteral("collide.glyphpreset.json")));
    if (!f.open(QIODevice::WriteOnly)) return false;
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}

} // namespace

class TestPgr35BatchCollision : public QObject {
    Q_OBJECT
    QTemporaryDir m_tmpDir;

private slots:
    void initTestCase() {
        QVERIFY(m_tmpDir.isValid());
        BatchMode::setPresetStoreDirForTest(m_tmpDir.filePath(QStringLiteral("store")));
    }

    // The pure staging rule: first claim wins, later duplicates are blocked.
    // (Compiled only against the FIXED BatchMode — the seam does not exist
    // pre-fix; the fail-before record runs the behavior slot below.)
#ifdef PGR35_SEAM_APPLIED
    void pureCollisionRule() {
        const QStringList inputs = { QStringLiteral("dir1/a.pdf"),
                                     QStringLiteral("dir2/a.pdf"),
                                     QStringLiteral("dir3/b.pdf") };
        const QStringList outs = { QStringLiteral("out/a_compressed.pdf"),
                                   QStringLiteral("out/a_compressed.pdf"),
                                   QStringLiteral("out/b_compressed.pdf") };
        const auto blockers = BatchMode::outputCollisionBlockers(inputs, outs);
        QCOMPARE(blockers.size(), 1);
        QVERIFY2(blockers.contains(inputs.at(1)), "the SECOND claim is the blocked one");
        QVERIFY2(!blockers.contains(inputs.at(0)), "the first claim keeps the path");
        QVERIFY2(!blockers.contains(inputs.at(2)), "distinct outputs never collide");
        QVERIFY2(blockers.value(inputs.at(1)).contains(QStringLiteral("collision"),
                  Qt::CaseInsensitive),
                 qPrintable(blockers.value(inputs.at(1))));
        // Empty resolutions (unresolvable output) never collide here.
        QVERIFY(BatchMode::outputCollisionBlockers(
            { QStringLiteral("x.pdf"), QStringLiteral("y.pdf") },
            { QString(), QString() }).isEmpty());
        // Distinct inputs, distinct outputs: no blockers.
        QVERIFY(BatchMode::outputCollisionBlockers(inputs, {
            QStringLiteral("out/1.pdf"), QStringLiteral("out/2.pdf"),
            QStringLiteral("out/3.pdf") }).isEmpty());
    }
#endif

    // THE REGRESSION: a run over two distinct files through a preset whose
    // naming template omits {basename}/{n} — every file resolves to the SAME
    // output, and pre-fix the second commit silently destroyed the first
    // output while both were counted as successes.
    void collidingPresetRunStagesTheSecondFile() {
        const QString fixtures = m_tmpDir.filePath(QStringLiteral("fixtures35"));
        const QString f1 = createTextPdf(fixtures, QStringLiteral("one.pdf"),
                                         QStringLiteral("Invoice one"));
        const QString f2 = createTextPdf(fixtures, QStringLiteral("two.pdf"),
                                         QStringLiteral("Invoice two"));
        QVERIFY(!f1.isEmpty() && !f2.isEmpty());

        QVERIFY2(writeCollidePreset(m_tmpDir.filePath(QStringLiteral("store"))),
                 "seed preset write failed");

        BatchMode bm;
        AppContext ctx = makeCtx();
        bm.setAppContext(&ctx);
        bm.setOperationForTest(7 /* OpPresetPipeline — the combo defaults to OpConvert */);
        bm.refreshPresetsForTest();
        QVERIFY2(bm.selectPresetForTest(QStringLiteral("collide")),
                 "the hostile preset must load (it is schema-valid)");

        bm.addFilesForTest({ f1, f2 });

        bool finished = false;
        QObject::connect(&bm, &BatchMode::batchFinished, &bm,
                         [&finished] { finished = true; }, Qt::DirectConnection);
        bm.onRunBatch();
        int waited = 0;
        while (!finished && waited < 30000) {
            QTest::qWait(50);
            waited += 50;
        }
        QVERIFY2(finished, "batch did not finish within 30 seconds");
        qWarning() << "PGR-35 details:"
                   << bm.errorDetailForTest(0) << "|"
                   << bm.errorDetailForTest(1);

        // Post-fix contract: exactly ONE artifact — the first file in list
        // order ran, the colliding file is a STAGED FAILURE with the reason.
        QCOMPARE(bm.successCount(), 1);
        QCOMPARE(bm.failCount(), 1);
        const QString detail = bm.errorDetailForTest(0);
        QVERIFY2(detail.contains(QStringLiteral("collision"), Qt::CaseInsensitive),
                 qPrintable(QStringLiteral("failure reason must name the collision, got: %1")
                                .arg(detail.left(200))));
        // The single surviving output is the collapsed "{date}.pdf" — one
        // artifact beyond the two inputs, not two.
        const QStringList pdfs = QDir(fixtures)
            .entryList(QStringList() << QStringLiteral("*.pdf"), QDir::Files);
        QStringList produced;
        for (const QString& p : pdfs) {
            const QString full = QDir(fixtures).filePath(p);
            if (full != f1 && full != f2)
                produced << p;
        }
        QCOMPARE(produced.size(), 1);
    }
};

QTEST_MAIN(TestPgr35BatchCollision)
#include "TestPgr35BatchCollision.moc"
