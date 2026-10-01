// SPDX-License-Identifier: Apache-2.0
// TestHotFolder.cpp — characterization pins for the hot-folder watch as it
// lives inside BatchMode (PARITY-SCORECARD-2026-09-30 §4 row 9). This is the
// archaeology phase: pins what IS, before the HotFolderController extraction
// (PROGRAM-CONSOLIDATION §4), green against the unmodified code.
//
// Pinned here (everything reachable through the existing seams):
//   * processed-set seeding — pre-existing files never ingest
//   * new-drop ingest + processed-set dedup (second ingest is a no-op)
//   * modified same-name file re-ingests (filename+mtime key) while the batch
//     model dedupes by path (count stays 1 — pins what IS)
//   * extension filter (*.pdf AND *.PDF; non-PDF ignored)
//   * CURRENT non-recursive gap (subdirectory PDFs are NOT ingested today)
//   * error paths: unarmed ingest no-op; watch dir deleted between ingests
//   * seam contract: armHotFolderForTest forces auto-run ON
//
// NOT pinned here, honestly: the fs-watch trigger + 500 ms debounce
// lifecycle. The only activation path is onToggleHotFolder's ON branch, which
// blocks on QFileDialog::getExistingDirectory — unreachable headlessly — and
// no seam exposes m_hotFolderWatcher / m_hotFolderDebounce. Those behaviors
// get pinned against HotFolderController once the watch logic is extracted
// (this suite grows the controller pins in the extraction commit).
//
// Run: QT_QPA_PLATFORM=offscreen ctest -R TestHotFolder --output-on-failure
#include <QtTest/QtTest>
#include <QCheckBox>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTextEdit>

#include "modes/BatchMode.h"
#include "modes/HotFolderController.h"

using namespace gp;

// ── Fixtures ───────────────────────────────────────────────────────────────────

// Minimal valid single-page PDF (TestBatchMode byte-accurate idiom). The
// hot-folder ingest never parses content, but a real PDF keeps the pins
// honest about what a watched drop looks like.
static QString createMinimalPdf(const QString& dir, const QString& name) {
    QDir().mkpath(dir);
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
    return path;
}

static bool writeFile(const QString& path, const QByteArray& bytes) {
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return false;
    return f.write(bytes) == bytes.size();
}

// Forge an exact mtime so the filename+mtime hot-file key is deterministic
// (two writes in the same wall-clock window would otherwise share a key).
static bool forgeMTime(const QString& path, const QDateTime& when) {
    QFile f(path);
    if (!f.open(QIODevice::ReadWrite)) return false;
    return f.setFileTime(when, QFileDevice::FileModificationTime);
}

static QTextEdit* logView(BatchMode& bm) {
    return bm.findChildren<QTextEdit*>().isEmpty()
               ? nullptr : bm.findChildren<QTextEdit*>().first();
}

static QCheckBox* autoRunCheck(BatchMode& bm) {
    for (QCheckBox* cb : bm.findChildren<QCheckBox*>())
        if (cb->text().contains(QStringLiteral("Auto-run on new files")))
            return cb;
    return nullptr;
}

// Arm + silence the auto-run the arming seam forces ON, so ingest pins
// observe pure ingest (auto-run end-to-end is TestBatchPresetsP2's pin).
static void armSilently(BatchMode& bm, const QString& dir) {
    bm.armHotFolderForTest(dir);
    QCheckBox* autoRun = autoRunCheck(bm);
    QVERIFY2(autoRun, "auto-run checkbox not found in BatchMode children");
    autoRun->setChecked(false);  // no connected slots — pure state flip
}

static int ingestLogCount(BatchMode& bm) {
    const QTextEdit* log = logView(bm);
    if (!log) return 0;
    return log->toPlainText().count(QStringLiteral("Hot folder: ingested"));
}

// ── Test class ─────────────────────────────────────────────────────────────────

class TestHotFolder : public QObject {
    Q_OBJECT

    QTemporaryDir m_root;  // per-suite temp root; each test gets its own subroot

    // Per-test subroot — no cross-test coupling; seeding (part of the pinned
    // behavior) makes arm-time leftovers inert anyway.
    QString hotDir(const QString& sub = {}) const {
        const QString tn = QString::fromLatin1(QTest::currentTestFunction());
        QString base = m_root.filePath(QStringLiteral("hf-") + tn);
        return sub.isEmpty() ? base : base + QStringLiteral("/") + sub;
    }

private slots:
    void initTestCase() {
        QVERIFY2(m_root.isValid(), "per-suite temp root invalid");
    }

    // ── Characterization pins (pre-extraction BatchMode behavior) ─────────────

    // Error path: an ingest with no armed folder is a silent no-op.
    void unarmedIngestIsNoop() {
        BatchMode bm;
        bm.runHotFolderIngestForTest();
        QCOMPARE(bm.fileCount(), 0);
        QCOMPARE(ingestLogCount(bm), 0);
    }

    // Seeding: files present AT ARM TIME are marked processed — only NEW
    // drops ever ingest.
    void seededFilesAreNotIngested() {
        QVERIFY(!createMinimalPdf(hotDir(), QStringLiteral("seeded.pdf")).isEmpty());
        QVERIFY(!createMinimalPdf(hotDir(), QStringLiteral("seeded2.pdf")).isEmpty());

        BatchMode bm;
        armSilently(bm, hotDir());
        bm.runHotFolderIngestForTest();

        QCOMPARE(bm.fileCount(), 0);
        QCOMPARE(ingestLogCount(bm), 0);
    }

    // A new drop ingests exactly once; a second ingest run with nothing new
    // adds nothing and emits no second ingested line (processed-set dedup).
    void newDropIngestedOnceThenDeduped() {
        BatchMode bm;
        armSilently(bm, hotDir());

        QVERIFY(!createMinimalPdf(hotDir(), QStringLiteral("a.pdf")).isEmpty());
        bm.runHotFolderIngestForTest();
        QCOMPARE(bm.fileCount(), 1);
        QCOMPARE(ingestLogCount(bm), 1);

        bm.runHotFolderIngestForTest();
        QCOMPARE(bm.fileCount(), 1);
        QCOMPARE(ingestLogCount(bm), 1);  // no second line
    }

    // Several drops in one ingest pass land in a single ingested line.
    void multipleDropsIngestedInOnePass() {
        BatchMode bm;
        armSilently(bm, hotDir());

        QVERIFY(!createMinimalPdf(hotDir(), QStringLiteral("c.pdf")).isEmpty());
        QVERIFY(!createMinimalPdf(hotDir(), QStringLiteral("a.pdf")).isEmpty());
        QVERIFY(!createMinimalPdf(hotDir(), QStringLiteral("b.pdf")).isEmpty());

        bm.runHotFolderIngestForTest();
        QCOMPARE(bm.fileCount(), 3);
        QCOMPARE(ingestLogCount(bm), 1);
        // Plural form of the log line.
        QVERIFY(logView(bm)->toPlainText()
                    .contains(QStringLiteral("ingested 3 new files.")));
    }

    // The key is filename+mtime: REPLACING a file re-ingests it (new key),
    // while the batch model dedupes by path — the count stays 1. Pins what IS.
    void modifiedSameNameReingestsButModelDedupes() {
        BatchMode bm;
        armSilently(bm, hotDir());

        const QString pdf = createMinimalPdf(hotDir(), QStringLiteral("a.pdf"));
        QVERIFY(!pdf.isEmpty());
        const QDateTime original = QFileInfo(pdf).lastModified();

        bm.runHotFolderIngestForTest();
        QCOMPARE(bm.fileCount(), 1);
        QCOMPARE(ingestLogCount(bm), 1);

        // Replace in place with a deterministically different mtime.
        QVERIFY(QFile::remove(pdf));
        QVERIFY(!createMinimalPdf(hotDir(), QStringLiteral("a.pdf")).isEmpty());
        QVERIFY2(forgeMTime(pdf, original.addSecs(3600)),
                 "mtime forge failed — pin premise broken");
        QVERIFY(QFileInfo(pdf).lastModified() != original);  // premise check

        bm.runHotFolderIngestForTest();
        // Processed-set saw a NEW key → the ingested line fires again...
        QCOMPARE(ingestLogCount(bm), 2);
        // ...but the model keeps one entry (path-level dedup in addFilePaths).
        QCOMPARE(bm.fileCount(), 1);
    }

    // The explicit two-pattern filter admits upper-case .PDF too.
    void uppercaseExtensionIngested() {
        BatchMode bm;
        armSilently(bm, hotDir());

        QVERIFY(!createMinimalPdf(hotDir(), QStringLiteral("UPPER.PDF")).isEmpty());
        bm.runHotFolderIngestForTest();
        QCOMPARE(bm.fileCount(), 1);
        QCOMPARE(ingestLogCount(bm), 1);
    }

    // Non-PDF drops are invisible to the ingest.
    void nonPdfDropIgnored() {
        BatchMode bm;
        armSilently(bm, hotDir());

        QDir().mkpath(hotDir());
        QVERIFY(writeFile(hotDir() + QStringLiteral("/note.txt"), "hi"));
        QVERIFY(writeFile(hotDir() + QStringLiteral("/data.pdfx"), "hi"));

        bm.runHotFolderIngestForTest();
        QCOMPARE(bm.fileCount(), 0);
        QCOMPARE(ingestLogCount(bm), 0);
    }

    // CHARACTERIZES THE GAP (scorecard §3 row 84): today's ingest lists the
    // watch dir non-recursively — a PDF in a subdirectory is NOT ingested.
    // (This pin deliberately flips when the recursive capability lands.)
    void subdirectoryPdfNotIngested() {
        BatchMode bm;
        armSilently(bm, hotDir());

        QVERIFY(!createMinimalPdf(hotDir(QStringLiteral("nested")),
                                  QStringLiteral("deep.pdf")).isEmpty());

        bm.runHotFolderIngestForTest();
        QCOMPARE(bm.fileCount(), 0);
        QCOMPARE(ingestLogCount(bm), 0);
    }

    // Error path: the watch dir vanishing between ingests is a silent no-op
    // (empty listing, no crash, no log).
    void missingWatchDirIsSilentNoop() {
        BatchMode bm;
        armSilently(bm, hotDir());  // arms an EMPTY dir

        QVERIFY(!createMinimalPdf(hotDir(), QStringLiteral("a.pdf")).isEmpty());
        bm.runHotFolderIngestForTest();
        QCOMPARE(bm.fileCount(), 1);

        QVERIFY(QDir(hotDir()).removeRecursively());
        bm.runHotFolderIngestForTest();
        QCOMPARE(bm.fileCount(), 1);      // existing entries untouched
        QCOMPARE(ingestLogCount(bm), 1);  // no new ingested line
    }

    // Seam contract: arming forces the auto-run option ON (production ingest
    // auto-runs); a fresh BatchMode has it OFF.
    void armForcesAutoRunOn() {
        BatchMode bm;
        QCheckBox* autoRun = autoRunCheck(bm);
        QVERIFY2(autoRun, "auto-run checkbox not found in BatchMode children");
        QVERIFY(!autoRun->isChecked());

        QDir().mkpath(hotDir());
        bm.armHotFolderForTest(hotDir());
        QVERIFY(autoRun->isChecked());
    }

    // ── HotFolderController pins (the extracted watch lifecycle) ───────────────
    //
    // The fs-watch trigger + debounce were NOT reachable in BatchMode (the
    // only activation path blocks on QFileDialog). The controller exposes the
    // lifecycle without a dialog; these pins cover the same debounce/watch
    // wiring BatchMode has always used, now deterministically.

    // Deterministic wiring pin: the controller watches the folder and a
    // directory-changed delivery arms the 500 ms debounce — nothing ingests
    // synchronously, exactly one debounced pass runs after the window.
    void controllerWatchWiringAndDebounce() {
        HotFolderController c;
        QStringList ingested;
        c.setIngestHandler([&ingested](const QStringList& files) {
            ingested << files;
        });

        QDir().mkpath(hotDir());
        QVERIFY(!createMinimalPdf(hotDir(), QStringLiteral("drop.pdf")).isEmpty());
        QVERIFY(c.start(hotDir()));
        QCOMPARE(c.watchedPath(), hotDir());
        QVERIFY(c.isWatching());

        // Seed at start: the drop that existed BEFORE start is not re-ingested.
        c.triggerDirectoryChangedForTest();
        QTest::qWait(100);
        QVERIFY2(ingested.isEmpty(),
                 "directory-changed ingested synchronously — debounce lost");
        QTest::qWait(700);  // one full debounce window + margin
        QVERIFY2(ingested.isEmpty(),
                 "seeded file re-ingested — seeding contract broken");
        QCOMPARE(c.debouncePassesForTest(), 1);  // exactly one debounced pass

        // A NEW drop ingests exactly once through the same wiring.
        QVERIFY(!createMinimalPdf(hotDir(), QStringLiteral("drop2.pdf")).isEmpty());
        c.triggerDirectoryChangedForTest();
        QTest::qWait(100);
        QVERIFY2(ingested.isEmpty(), "debounce window shorter than 500 ms");
        QTest::qWait(700);
        QCOMPARE(ingested.size(), 1);
        QVERIFY(ingested.first().contains(QStringLiteral("drop2.pdf")));

        c.stop();
        QVERIFY(!c.isWatching());
    }

    // Rapid re-triggers coalesce into ONE debounce pass (restart semantics).
    void controllerDebounceCoalesces() {
        HotFolderController c;
        int deliveries = 0;
        QStringList lastDelivery;
        c.setIngestHandler([&deliveries, &lastDelivery](const QStringList& files) {
            ++deliveries;
            lastDelivery = files;
        });

        QDir().mkpath(hotDir());
        QVERIFY(c.start(hotDir()));
        QVERIFY(!createMinimalPdf(hotDir(), QStringLiteral("burst1.pdf")).isEmpty());
        c.triggerDirectoryChangedForTest();
        QTest::qWait(150);  // inside the window — a real watcher would re-fire
        QVERIFY(!createMinimalPdf(hotDir(), QStringLiteral("burst2.pdf")).isEmpty());
        c.triggerDirectoryChangedForTest();  // restarts the window
        QTest::qWait(800);                   // ≥ one full window past restart
        QCOMPARE(deliveries, 1);             // ONE pass carried BOTH drops
        QCOMPARE(lastDelivery.size(), 2);
        QCOMPARE(c.debouncePassesForTest(), 1);
        c.stop();
    }

    // OS-level fs-watch proof: a drop into the watched folder is delivered
    // through the real QFileSystemWatcher + debounce and ingested. (Bounded
    // wait on real OS fs-events — generous ceiling, serial run.)
    void controllerFlatDropEndToEnd() {
        HotFolderController c;
        QStringList ingested;
        c.setIngestHandler([&ingested](const QStringList& files) {
            ingested << files;
        });

        QDir().mkpath(hotDir());
        QVERIFY(c.start(hotDir()));
        QVERIFY(!createMinimalPdf(hotDir(), QStringLiteral("live.pdf")).isEmpty());

        const int ceilingMs = 10000;
        int waited = 0;
        while (ingested.isEmpty() && waited < ceilingMs) {
            QTest::qWait(100);
            waited += 100;
        }
        QVERIFY2(!ingested.isEmpty(),
                 "flat drop not ingested via OS fs-events within ceiling");
        QCOMPARE(ingested.size(), 1);
        QVERIFY(ingested.first().contains(QStringLiteral("live.pdf")));
        c.stop();
    }

    // stop() tears down watch + debounce: later triggers do nothing.
    void controllerStopClearsState() {
        HotFolderController c;
        int passes = 0;
        c.setIngestHandler([&passes](const QStringList&) { ++passes; });

        QDir().mkpath(hotDir());
        QVERIFY(c.start(hotDir()));
        QVERIFY(c.isWatching());
        QVERIFY(!c.watchedPath().isEmpty());
        c.stop();
        QVERIFY(!c.isWatching());
        QVERIFY(c.watchedPath().isEmpty());

        c.triggerDirectoryChangedForTest();
        QTest::qWait(700);
        QCOMPARE(passes, 0);
        QCOMPARE(c.debouncePassesForTest(), 0);
    }
};

QTEST_MAIN(TestHotFolder)
#include "TestHotFolder.moc"
