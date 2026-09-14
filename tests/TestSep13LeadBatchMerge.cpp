// SPDX-License-Identifier: Apache-2.0
// SEP13 leads 9 + 10 — BatchMode async merge accounting.
//
// PARITY-GLM-REVIEW-2026-09-13 leads (BatchMode.cpp, startMergeWorker):
//   lead 9: the merge worker emits a phantom extra BatchFileResult beyond
//           one-per-input-file when the final save fails (failOutput adds an
//           N+1th result while total accounting is m_filesToProcess.size()).
//   lead 10: a merge cancelled at a file boundary `return`s WITHOUT saving,
//           but the per-input results already added (success=true,
//           outputPath=mergeOutPath) are drained by onBatchFinished and
//           counted as successes pointing at an output that was never
//           written. Only dssMissing-style accounting exists; nothing checks
//           future().isCanceled() before the summary.
//
// The cancellation probe and the phantom probe assert the CORRECT contract
// and are expected to FAIL on candidate 83be3c2 — that failure is the
// confirmation evidence. Uses ONLY public test seams (addFilesForTest,
// setOperationForTest, setMergeBoundaryHookForTest, onCancelBatch,
// successCount/failCount/skipCount) — no production edits.
#include <QtTest/QtTest>
#include <QApplication>
#include <QFile>
#include <QLineEdit>
#include <QMetaObject>
#include <QPointer>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QThread>

#include <algorithm>

#include "core/AppContext.h"
#include "modes/BatchMode.h"

using namespace gp;

namespace {

// Minimal valid single-page PDF (byte-accurate xref, repo-standard fixture).
QString createMinimalPdf(const QString& dir, const QString& name) {
    const QString path = dir + "/" + name;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return {};
    f.write(
        "%PDF-1.4\n"
        "1 0 obj<</Type/Catalog/Pages 2 0 R>>endobj\n"
        "2 0 obj<</Type/Pages/Kids[3 0 R]/Count 1>>endobj\n"
        "3 0 obj<</Type/Page/Parent 2 0 R/MediaBox[0 0 612 792]>>endobj\n"
        "xref\n0 4\n"
        "0000000000 65535 f \n"
        "0000000009 00000 n \n"
        "0000000058 00000 n \n"
        "0000000115 00000 n \n"
        "trailer<</Size 4/Root 1 0 R>>\n"
        "startxref\n183\n%%EOF\n");
    f.close();
    return path;
}

} // namespace

class TestSep13LeadBatchMerge : public QObject {
    Q_OBJECT

    AppContext m_ctx;

    static QString defaultMergeOut(const QTemporaryDir& tmp) {
        return QDir(tmp.path()).filePath(QStringLiteral("a_merged.pdf"));
    }

    // The merge out dir QLineEdit is located by its (untranslated in test
    // env) placeholder — visibility cannot be used because the panel is never
    // shown() under the offscreen platform.
    QLineEdit* mergeDirEdit(BatchMode& bm) {
        bm.setOperationForTest(4); // OpMerge
        const auto edits = bm.findChildren<QLineEdit*>();
        for (auto* le : edits) {
            if (le->placeholderText().contains(QLatin1String("Same folder as first source")))
                return le;
        }
        return nullptr;
    }

private slots:
    // Control pin (must PASS): a plain 2-file merge writes the output and
    // counts exactly the input files — harness sanity.
    void happyMergeControl() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString a = createMinimalPdf(tmp.path(), "a.pdf");
        const QString b = createMinimalPdf(tmp.path(), "b.pdf");
        QVERIFY(!a.isEmpty() && !b.isEmpty());

        BatchMode bm;
        bm.setAppContext(&m_ctx);
        bm.addFilesForTest({a, b});
        bm.setOperationForTest(4);

        QSignalSpy finishedSpy(&bm, &BatchMode::batchFinished);
        bm.onRunBatch();
        QVERIFY2(finishedSpy.wait(20000), "merge did not finish");

        const QString out = defaultMergeOut(tmp);
        QVERIFY2(QFile::exists(out), "control: merged output must exist");
        QCOMPARE(bm.successCount(), 2);
        QCOMPARE(bm.failCount(), 0);
    }

    // LEAD 10 CONFIRMATION (expected FAILURE on the candidate): a merge
    // cancelled at a file boundary must not count the already-appended inputs
    // as successes pointing at an output that was never saved.
    void cancelledMergeMustNotReportSuccesses() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        QStringList files;
        for (const char* n : {"a.pdf", "b.pdf", "c.pdf"})
            files << createMinimalPdf(tmp.path(), QString::fromLatin1(n));
        QVERIFY(std::all_of(files.cbegin(), files.cend(),
                            [](const QString& f) { return QFile::exists(f); }));

        BatchMode bm;
        bm.setAppContext(&m_ctx);
        bm.addFilesForTest(files);
        bm.setOperationForTest(4);

        // Queue the cancel from the SECOND file boundary: file 0's per-item
        // result is already added (success=true, outputPath=mergeOutPath) when
        // boundary 1 runs; the sleeps give the GUI event loop (spy.wait below)
        // time to process the cancel, so the worker hits promise.isCanceled()
        // at the next loop-top check and returns WITHOUT saving.
        QPointer<BatchMode> bmPtr(&bm);
        bm.setMergeBoundaryHookForTest([bmPtr](int i) {
            if (i == 1 && bmPtr) {
                QMetaObject::invokeMethod(bmPtr, [bmPtr]() { bmPtr->onCancelBatch(); },
                                          Qt::QueuedConnection);
            }
            QThread::msleep(400);
        });

        const QString out = defaultMergeOut(tmp);
        QFile::remove(out);

        QSignalSpy finishedSpy(&bm, &BatchMode::batchFinished);
        bm.onRunBatch();
        QVERIFY2(finishedSpy.wait(30000), "cancelled merge did not finish");

        qInfo() << "post-cancel accounting: success =" << bm.successCount()
                << "fail =" << bm.failCount() << "skip =" << bm.skipCount()
                << "output exists =" << QFile::exists(out);

        // CORRECT contract: nothing succeeded (the output was never written).
        QVERIFY2(!QFile::exists(out),
                 "cancelled merge must never publish an output (this part holds)");
        QVERIFY2(bm.successCount() == 0,
                 QStringLiteral("SEP13 lead 10 CONFIRMED: cancelled merge counted %1 input(s) as "
                 "successes pointing at the never-written merge output")
                     .arg(bm.successCount()).toUtf8().constData());
    }

    // LEAD 9 CONFIRMATION (expected FAILURE on the candidate): when the final
    // save fails, the worker adds ONE EXTRA result (the "output artifact"
    // item) beyond one-per-input-file, so accounted items exceed fileCount()
    // — and the per-input successes still point at the nonexistent output.
    void failedSaveEmitsPhantomResultAndFalseSuccesses() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString a = createMinimalPdf(tmp.path(), "a.pdf");
        const QString b = createMinimalPdf(tmp.path(), "b.pdf");
        QVERIFY(!a.isEmpty() && !b.isEmpty());

        // Output goes into a directory that does not exist → PoDoFo Save
        // throws → failOutput() appends the extra result.
        const QString doomedDir = tmp.path() + "/no-such-dir";

        BatchMode bm;
        bm.setAppContext(&m_ctx);
        bm.addFilesForTest({a, b});

        QLineEdit* edit = mergeDirEdit(bm);
        QVERIFY2(edit, "merge out dir QLineEdit not uniquely identifiable");
        edit->setText(doomedDir);

        const QString out = doomedDir + "/a_merged.pdf";

        QSignalSpy finishedSpy(&bm, &BatchMode::batchFinished);
        bm.onRunBatch();
        QVERIFY2(finishedSpy.wait(30000), "merge (doomed save) did not finish");

        const int accounted = bm.successCount() + bm.failCount() + bm.skipCount();
        qInfo() << "save-failure accounting: success =" << bm.successCount()
                << "fail =" << bm.failCount() << "fileCount =" << bm.fileCount()
                << "output exists =" << QFile::exists(out);

        QVERIFY2(!QFile::exists(out), "precondition: doomed save must not produce output");

        // CORRECT contract (a): accounted items == input files (no phantom).
        QVERIFY2(accounted <= bm.fileCount(),
                 QStringLiteral("SEP13 lead 9 CONFIRMED: %1 results accounted for %2 input files — "
                 "the failed-save 'output artifact' item is a phantom beyond "
                 "one-per-input-file")
                     .arg(accounted).arg(bm.fileCount()).toUtf8().constData());
        // CORRECT contract (b): no successes may point at a nonexistent output.
        QVERIFY2(bm.successCount() == 0,
                 QStringLiteral("SEP13 lead 9 (companion of 10): %1 input(s) reported successful "
                 "against the never-written merge output")
                     .arg(bm.successCount()).toUtf8().constData());
    }
};

#include "TestSep13LeadBatchMerge.moc"
QTEST_MAIN(TestSep13LeadBatchMerge)
