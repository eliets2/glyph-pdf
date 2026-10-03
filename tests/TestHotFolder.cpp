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
//   * the pre-recursion non-recursive gap (subdirectoryPdfNotIngested) —
//     FLIPPED by the recursion capability into subdirectoryPdfIsIngestedRecursive
//   * error paths: unarmed ingest no-op; watch dir deleted between ingests
//   * seam contract: armHotFolderForTest forces auto-run ON
//
// Since the capability commit this suite also pins the two enterprise
// capabilities (scorecard §4 row 9): RECURSIVE watch (root + every
// subdirectory, live discovery of dirs created after start, subtree-unique
// identity keys) and the POLLING fallback for network shares (deterministic
// timer ingest + dedup, BatchMode checkbox wiring).
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
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QTemporaryDir>
#include <QTextEdit>

#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <unistd.h>
#endif

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

static QCheckBox* pollingCheck(BatchMode& bm) {
    for (QCheckBox* cb : bm.findChildren<QCheckBox*>())
        if (cb->text().contains(QStringLiteral("network drives")))
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

    // The recursion capability (scorecard §3 row 84) CLOSES the gap this pin
    // used to characterize: a PDF in a subdirectory is now ingested. The old
    // subdirectoryPdfNotIngested pin flipped here — deliberate behavior
    // change, justified in the capability commit (fail-before captured).
    void subdirectoryPdfIsIngestedRecursive() {
        BatchMode bm;
        armSilently(bm, hotDir());

        QVERIFY(!createMinimalPdf(hotDir(QStringLiteral("nested")),
                                  QStringLiteral("deep.pdf")).isEmpty());

        bm.runHotFolderIngestForTest();
        QCOMPARE(bm.fileCount(), 1);
        QCOMPARE(ingestLogCount(bm), 1);
        QVERIFY(logView(bm)->toPlainText()
                    .contains(QStringLiteral("ingested 1 new file.")));
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

    // ── Capability pins: RECURSIVE watch + POLLING fallback ───────────────────
    // (PARITY-SCORECARD-2026-09-30 §4 row 9; the two enterprise watched-folder
    // gaps — nested dirs and fs-event-unreliable network shares.)

    // Recursive fs-watch coverage: subdirectories present at start are under
    // watch (the wiring that carries nested-drop events).
    void controllerWatchesSubdirectories() {
        HotFolderController c;
        QDir().mkpath(hotDir(QStringLiteral("nested/deeper")));
        QVERIFY(c.start(hotDir()));
        const QStringList watched = c.watchedDirectoriesForTest();
        QVERIFY2(watched.contains(hotDir()),
                 qPrintable(QStringLiteral("root not watched: %1").arg(watched.join(','))));
        QVERIFY2(watched.contains(hotDir(QStringLiteral("nested"))),
                 qPrintable(QStringLiteral("nested dir not watched: %1").arg(watched.join(','))));
        QVERIFY2(watched.contains(hotDir(QStringLiteral("nested/deeper"))),
                 qPrintable(QStringLiteral("deep dir not watched: %1").arg(watched.join(','))));
        c.stop();
    }

    // A subdirectory created AFTER start gets watched on the next debounce
    // fire (wave-2b F-7 moved the watch refresh OUT of the per-event path
    // into the debounce fire, together with the ingest pass it always
    // accompanied — discovery is asserted AFTER the debounce window now, not
    // synchronously per event).
    void controllerDiscoversSubdirCreatedAfterStart() {
        HotFolderController c;
        QDir().mkpath(hotDir());
        QVERIFY(c.start(hotDir()));
        QVERIFY(!c.watchedDirectoriesForTest()
                     .contains(hotDir(QStringLiteral("late"))));

        QDir().mkpath(hotDir(QStringLiteral("late")));
        c.triggerDirectoryChangedForTest();  // what a parent-change event does
        QTest::qWait(700);  // one full debounce window: the refresh fires there
        QVERIFY2(c.watchedDirectoriesForTest().contains(hotDir(QStringLiteral("late"))),
                 qPrintable(c.watchedDirectoriesForTest().join(',')));
        c.stop();
    }

    // OS-level proof of the recursive fs-watch: a drop into a NESTED dir is
    // delivered through the real QFileSystemWatcher + debounce and ingested.
    // (Bounded wait on real OS fs-events — generous ceiling, serial run.)
    void recursiveWatchIngestsNestedDropEndToEnd() {
        HotFolderController c;
        QStringList ingested;
        c.setIngestHandler([&ingested](const QStringList& files) {
            ingested << files;
        });

        QDir().mkpath(hotDir(QStringLiteral("nested")));
        QVERIFY(c.start(hotDir()));

        QVERIFY(!createMinimalPdf(hotDir(QStringLiteral("nested")),
                                  QStringLiteral("deep.pdf")).isEmpty());

        const int ceilingMs = 10000;
        int waited = 0;
        while (ingested.isEmpty() && waited < ceilingMs) {
            QTest::qWait(100);
            waited += 100;
        }
        QVERIFY2(!ingested.isEmpty(),
                 "nested drop not ingested via fs-events within ceiling");
        QCOMPARE(ingested.size(), 1);
        QVERIFY2(ingested.first().contains(QStringLiteral("deep.pdf")),
                 qPrintable(ingested.first()));
        c.stop();
    }

    // Polling fallback — deterministic (pure timer, no OS fs-events): a
    // NESTED drop is discovered on a later tick (the network-share path).
    void pollingFallbackDiscoversNestedDrop() {
        HotFolderController c;
        QStringList ingested;
        c.setIngestHandler([&ingested](const QStringList& files) {
            ingested << files;
        });

        QDir().mkpath(hotDir(QStringLiteral("nested")));
        QVERIFY(c.startPolling(hotDir(), /*intervalMs=*/200));
        QVERIFY(c.isWatching());
        QVERIFY(c.isPolling());

        QTest::qWait(450);  // ≥2 ticks with nothing new
        QVERIFY(ingested.isEmpty());

        QVERIFY(!createMinimalPdf(hotDir(QStringLiteral("nested")),
                                  QStringLiteral("polled.pdf")).isEmpty());
        const int ceilingMs = 5000;
        int waited = 0;
        while (ingested.isEmpty() && waited < ceilingMs) {
            QTest::qWait(100);
            waited += 100;
        }
        QVERIFY2(!ingested.isEmpty(),
                 "polling fallback missed a nested drop within ceiling");
        QCOMPARE(ingested.size(), 1);
        QVERIFY(ingested.first().contains(QStringLiteral("polled.pdf")));
        c.stop();
        QVERIFY(!c.isWatching());
    }

    // Polling also covers flat drops and processed-set dedup: a file dropped
    // AFTER start ingests on the next tick and never re-ingests (pre-start
    // files are seeded by design — the pinned seeding semantics).
    void pollingDedupsAndFlatDrops() {
        HotFolderController c;
        QStringList ingested;
        c.setIngestHandler([&ingested](const QStringList& files) {
            ingested << files;
        });

        QVERIFY(c.startPolling(hotDir(), /*intervalMs=*/200));
        QTest::qWait(450);  // ≥2 ticks: an empty folder delivers nothing
        QVERIFY(ingested.isEmpty());

        QVERIFY(!createMinimalPdf(hotDir(), QStringLiteral("flat.pdf")).isEmpty());
        const int ceilingMs = 5000;
        int waited = 0;
        while (ingested.isEmpty() && waited < ceilingMs) {
            QTest::qWait(100);
            waited += 100;
        }
        QVERIFY2(!ingested.isEmpty(), "polling missed the flat drop");
        QCOMPARE(ingested.size(), 1);  // ingested exactly once...

        QTest::qWait(600);  // several more ticks
        QCOMPARE(ingested.size(), 1);  // ...and never re-ingested
        c.stop();
    }

    // r3-api harmonization pin — CROSS-PATH DEDUP: the fs-event pass (debounce
    // fire) and the poll tick share ONE processed set, so each drop is
    // delivered exactly once even with BOTH paths live at once (the
    // network-share reality: a stray fs-event arriving while polling, or a
    // poll tick landing between an fs-event burst). Whichever pass runs first
    // delivers; the other must stay silent — per file, in both directions.
    void crossPathIngestSharesOneProcessedSet() {
        HotFolderController c;
        QStringList ingested;
        c.setIngestHandler([&ingested](const QStringList& files) {
            ingested << files;
        });

        QDir().mkpath(hotDir());
        QVERIFY(c.start(hotDir()));                              // fs-event path live
        QVERIFY(c.startPolling(hotDir(), /*intervalMs=*/200));   // poll path ALSO live

        // Drop A: delivered exactly once, no matter which pass wins the race.
        QVERIFY(!createMinimalPdf(hotDir(), QStringLiteral("cross-a.pdf")).isEmpty());
        c.triggerDirectoryChangedForTest();                      // arms the debounce too
        const int ceilingMs = 5000;
        int waited = 0;
        while (ingested.size() < 1 && waited < ceilingMs) {
            QTest::qWait(100);
            waited += 100;
        }
        QVERIFY2(ingested.size() == 1, "cross-path drop A was never delivered");
        QTest::qWait(700);   // a full debounce window + ≥2 poll ticks
        QVERIFY2(ingested.size() == 1,
                 "drop A was re-ingested by the OTHER live path");

        // Drop B: same exactly-once contract in the other direction's shadow.
        QVERIFY(!createMinimalPdf(hotDir(), QStringLiteral("cross-b.pdf")).isEmpty());
        waited = 0;
        while (ingested.size() < 2 && waited < ceilingMs) {
            QTest::qWait(100);
            waited += 100;
        }
        QVERIFY2(ingested.size() == 2, "cross-path drop B was never delivered");
        QTest::qWait(700);   // a full debounce window + ≥2 poll ticks
        QVERIFY2(ingested.size() == 2,
                 "a delivered file was re-ingested by the other path");
        QVERIFY2(c.ingestDeliver().isEmpty(),
                 "a direct ingest pass after both paths delivered must find nothing new");
        c.stop();
    }

    // The identity key is subtree-unique: the same stem in different
    // subdirectories ingests BOTH (filename|mtime alone would collide — the
    // recursion capability fixed the key to the root-relative path). The
    // drops happen AFTER start: pre-start files are seeded by design (the
    // pinned seeding semantics), so only post-start drops can prove the key
    // split. Same premise fix pollingDedupsAndFlatDrops needed.
    void sameStemInDifferentSubdirsBothIngest() {
        HotFolderController c;
        QStringList ingested;
        c.setIngestHandler([&ingested](const QStringList& files) {
            ingested << files;
        });

        QDir().mkpath(hotDir());  // root must exist to be watched at start
        QVERIFY(c.start(hotDir()));

        QVERIFY(!createMinimalPdf(hotDir(QStringLiteral("d1")),
                                  QStringLiteral("same.pdf")).isEmpty());
        QVERIFY(!createMinimalPdf(hotDir(QStringLiteral("d2")),
                                  QStringLiteral("same.pdf")).isEmpty());
        // Forge IDENTICAL mtimes: under the historical filename|mtime key the
        // two drops share one identity and only one would ingest — the
        // root-relative key is what must tell them apart.
        const QDateTime shared = QDateTime::currentDateTimeUtc().addSecs(-60);
        QVERIFY2(forgeMTime(hotDir(QStringLiteral("d1"))
                                + QStringLiteral("/same.pdf"), shared),
                 "mtime forge failed — pin premise broken");
        QVERIFY2(forgeMTime(hotDir(QStringLiteral("d2"))
                                + QStringLiteral("/same.pdf"), shared),
                 "mtime forge failed — pin premise broken");

        const int ceilingMs = 10000;
        int waited = 0;
        // Wave-2b testing audit H1: wait for BOTH same-stem deliveries, not
        // just the first — asserting after the first delivery left the second
        // count to luck on slow/loaded machines.
        auto sameStemCount = [&ingested]() {
            int n = 0;
            for (const QString& f : ingested)
                if (f.contains(QStringLiteral("same.pdf"))) ++n;
            return n;
        };
        while (sameStemCount() < 2 && waited < ceilingMs) {
            QTest::qWait(100);
            waited += 100;
        }
        QVERIFY2(sameStemCount() == 2, "nested same-stem drops not both ingested");
        c.stop();
    }

    // ── Wave-2b F-5: the identity key must carry content-bearing state ──────
    // path+mtime swallows rewrites with preserved mtimes (cp -p, archive
    // restore, robocopy /COPYTIMES) and same-tick rewrites on the NAS-class
    // shares polling targets (1–2 s timestamp granularity): the replaced
    // file's key equals the old key and is silently NEVER re-ingested. With
    // size in the key, a rewrite that changes the file's length re-ingests
    // even under a forged-identical mtime. Residual, by design (poll tick
    // cadence): a same-size same-mtime rewrite is still one ingest.
    void pollingRewriteForgedMtimeDifferentSizeReingests() {
        HotFolderController c;
        QStringList ingested;
        c.setIngestHandler([&ingested](const QStringList& files) {
            ingested << files;
        });

        QDir().mkpath(hotDir());
        const QString pdf = createMinimalPdf(hotDir(), QStringLiteral("r.pdf"));
        QVERIFY(!pdf.isEmpty());
        // Forge a stable, already-stale mtime BEFORE start: seeding marks the
        // pre-start file, and the same forged time is restored after the
        // rewrite (exactly what cp -p / archive restore produce).
        const QDateTime forged = QDateTime::currentDateTimeUtc().addSecs(-60);
        QVERIFY2(forgeMTime(pdf, forged), "mtime forge failed — pin premise broken");
        const qint64 oldSize = QFileInfo(pdf).size();

        QVERIFY(c.startPolling(hotDir(), /*intervalMs=*/200));
        QVERIFY(c.isPolling());
        QTest::qWait(450);  // ≥2 ticks: the pre-start file stays seeded
        QVERIFY2(ingested.isEmpty(), "seeded file re-ingested — seeding broken");

        // Rewrite in place: different content (different size), mtime forged
        // back to the exact pre-rewrite value. Under the pre-fix path+mtime
        // key this rewrite is invisible forever.
        QVERIFY(writeFile(pdf,
            "%PDF-1.4\n%rewritten-in-place-with-a-different-length-body\n"
            "%second-line-to-change-the-size\n%%EOF\n"));
        QVERIFY2(forgeMTime(pdf, forged), "mtime forge failed — pin premise broken");
        QVERIFY2(QFileInfo(pdf).lastModified() == forged,
                 "premise: forged mtime not preserved across the rewrite");
        QVERIFY2(QFileInfo(pdf).size() != oldSize,
                 "premise: the rewrite must change the size for this pin");

        const int ceilingMs = 5000;
        int waited = 0;
        while (ingested.isEmpty() && waited < ceilingMs) {
            QTest::qWait(100);
            waited += 100;
        }
        QVERIFY2(!ingested.isEmpty(),
                 "mtime-preserved size-changing rewrite never re-ingested (F-5)");
        QCOMPARE(ingested.size(), 1);
        QVERIFY(ingested.first().contains(QStringLiteral("r.pdf")));
        c.stop();
    }

    // ── Wave-2b F-7: the watch refresh must NOT walk the whole tree per ─────
    // fs-event. A bulk FILE copy-in (no new directories) fires one
    // directoryChanged per file; the pre-fix code ran the full-tree
    // QDirIterator walk synchronously for EVERY event (O(events x dirs) on
    // the GUI thread — a local DoS for any 10k-file drop). Counted via the
    // watchWalkCountForTest seam: events inside one debounce window must
    // produce ZERO synchronous walks and exactly ONE walk at the fire.
    void controllerBulkFileEventsDoNotWalkPerEvent() {
        HotFolderController c;
        int deliveries = 0;
        c.setIngestHandler([&deliveries](const QStringList&) { ++deliveries; });

        QDir().mkpath(hotDir());
        QVERIFY(c.start(hotDir()));
        const int walksAtStart = c.watchWalkCountForTest();
        QVERIFY2(walksAtStart >= 1, "start must perform the initial watch walk");

        for (int i = 0; i < 4; ++i) {
            QVERIFY(!createMinimalPdf(hotDir(),
                                      QStringLiteral("bulk%1.pdf").arg(i)).isEmpty());
            c.triggerDirectoryChangedForTest();  // one per copied-in file
        }
        QVERIFY2(c.watchWalkCountForTest() == walksAtStart,
                 "directory-changed walked the whole tree synchronously per "
                 "event (F-7)");
        QTest::qWait(700);  // one full debounce window + margin
        QVERIFY2(c.watchWalkCountForTest() == walksAtStart + 1,
                 "the debounce fire must refresh the watch exactly once");
        QCOMPARE(c.debouncePassesForTest(), 1);
        QCOMPARE(deliveries, 1);  // the four drops still land in one pass
        c.stop();
    }

    // ── R3-sec F-6: watcher fan-out backstop ────────────────────────────────
    // addPaths fails SILENTLY beyond the OS watch budget (inotify caps,
    // ReadDirectoryChangesW handle budgets) — subtrees became permanently
    // unwatched with no backstop in watcher mode, and a drop there raised no
    // event, ever. After the fix the controller counts addPaths refusals,
    // engages the existing polling fallback for the degraded (sub)tree, and
    // discloses through the degraded-watch log channel. The cap is simulated
    // with the addPaths hook seam — fully deterministic.

    // The real-exhaustion shape: root and early subtrees made it under the
    // cap, later ones did not. A drop into the UNWATCHED subtree must still
    // be delivered — by the engaged poll, not by fs-events.
    void controllerWatchCapEngagesPollingForDegradedSubtree() {
        HotFolderController c;
        QStringList ingested;
        QList<QStringList> disclosures;
        c.setIngestHandler([&ingested](const QStringList& files) {
            ingested << files;
        });
        c.setWatchDegradedHandler([&disclosures](const QStringList& unwatched) {
            disclosures << unwatched;
        });

        QDir().mkpath(hotDir(QStringLiteral("nested")));
        QVERIFY(c.start(hotDir()));  // start-time adds succeed (hook not armed)
        QVERIFY(!c.isPolling());     // premise: healthy watch mode polls nothing

        // From here on, every addPaths "hits the cap": all paths refused.
        c.setWatchAddPathsHookForTest([](const QStringList& paths) { return paths; });

        QDir().mkpath(hotDir(QStringLiteral("late")));  // created after start
        c.triggerDirectoryChangedForTest();  // what a parent-change event does
        QTest::qWait(700);  // one full debounce window: the refresh fires there

        QVERIFY2(!c.watchedDirectoriesForTest().contains(hotDir(QStringLiteral("late"))),
                 "premise: the capped subtree is not under native watch");
        QVERIFY2(c.isPolling(),
                 "silent addPaths refusal must engage the polling fallback (F-6)");
        QVERIFY2(!disclosures.isEmpty(),
                 "unwatchable subtrees must be disclosed (F-6)");
        bool named = false;
        for (const QStringList& d : disclosures)
            if (d.contains(hotDir(QStringLiteral("late")))) named = true;
        QVERIFY2(named, "the disclosure must name the degraded subtree");
        QVERIFY2(!c.unwatchedSubtreesForTest().isEmpty(),
                 "the degradation accounting must record the refused subtree");
        QVERIFY2(c.watchFailureCountForTest() >= 1, "refusal must be counted");

        // The backstop closes the ingest gap: a drop into the un-watched
        // subtree is delivered by the engaged poll (ceiling wait on the
        // kPollIntervalMs cadence — same discipline as the polling pins).
        QVERIFY(!createMinimalPdf(hotDir(QStringLiteral("late")),
                                  QStringLiteral("capped.pdf")).isEmpty());
        const int ceilingMs = 8000;
        int waited = 0;
        while (ingested.isEmpty() && waited < ceilingMs) {
            QTest::qWait(100);
            waited += 100;
        }
        QVERIFY2(!ingested.isEmpty(),
                 "polling backstop missed a drop into the unwatched subtree (F-6)");
        QVERIFY(ingested.first().contains(QStringLiteral("capped.pdf")));
        c.stop();
        QVERIFY(!c.isWatching());
    }

    // The debounce fire re-runs the watch refresh (F-7 wiring) and re-learns
    // the same losses every pass; the disclosure fires ONCE per subtree — the
    // log channel must not spam per refresh — while the failure counter keeps
    // the true cumulative accounting.
    void controllerWatchCapDisclosureFiresOncePerSubtree() {
        HotFolderController c;
        QList<QStringList> disclosures;
        c.setWatchDegradedHandler([&disclosures](const QStringList& unwatched) {
            disclosures << unwatched;
        });

        QDir().mkpath(hotDir(QStringLiteral("nested")));
        QVERIFY(c.start(hotDir()));
        c.setWatchAddPathsHookForTest([](const QStringList& paths) { return paths; });

        QDir().mkpath(hotDir(QStringLiteral("late")));
        c.triggerDirectoryChangedForTest();
        QTest::qWait(700);   // refresh pass 1: "late" refused → disclosed
        QCOMPARE(disclosures.size(), 1);
        const int failuresAfterPass1 = c.watchFailureCountForTest();
        QVERIFY2(failuresAfterPass1 >= 1, "pass-1 refusal must be counted");

        c.triggerDirectoryChangedForTest();
        QTest::qWait(700);   // refresh pass 2: "late" refused AGAIN → silent
        QCOMPARE(disclosures.size(), 1);  // still once
        QVERIFY2(c.watchFailureCountForTest() > failuresAfterPass1,
                 "the counter keeps cumulative accounting across passes");
        c.stop();
    }

    // The degenerate fan-out: the ROOT itself cannot be watched (the whole
    // tree is beyond the budget). That is the "or the whole root" leg —
    // polling engages for everything, disclosed.
    void controllerRootWatchFailureDegradesToPolling() {
        HotFolderController c;
        QList<QStringList> disclosures;
        c.setWatchDegradedHandler([&disclosures](const QStringList& unwatched) {
            disclosures << unwatched;
        });
        QDir().mkpath(hotDir());
        c.setWatchAddPathsHookForTest([](const QStringList& paths) { return paths; });

        QVERIFY(c.start(hotDir()));
        QVERIFY2(c.isPolling(),
                 "a refused root must degrade the whole watch to polling (F-6)");
        QVERIFY2(!disclosures.isEmpty(), "a refused root must be disclosed");
        QVERIFY(c.watchedDirectoriesForTest().isEmpty());  // premise: nothing watched
        c.stop();
    }
    // ── R3-perf (audit finding 6): re-entrant ingest does not rescan ──────────
    // The ingest handler walking back into ingestDeliver() (a batch auto-run
    // that re-enters the controller mid-delivery) must not trigger a second
    // full-tree scan — the nested call is absorbed by the re-entrancy guard
    // and delivers nothing (the outer pass owns the tick). Per-tick cost
    // stays ONE stat walk; nothing decodes or re-reads file CONTENT either
    // way — the identity keys are pure stat fields (relpath|mtime|size).
    void reentrantIngestDoesNotRescanTree() {
        HotFolderController c;
        QDir().mkpath(hotDir());

        QStringList outer, nested;
        int deliveries = 0;
        c.setIngestHandler([&](const QStringList& files) {
            ++deliveries;
            nested = c.ingestDeliver();   // the re-entrant call under test
        });
        c.arm(hotDir());                  // seeds: nothing is new yet
        const int scansBefore = c.ingestScansForTest();

        QVERIFY(!createMinimalPdf(hotDir(),
                                  QStringLiteral("reentrant.pdf")).isEmpty());
        outer = c.ingestDeliver();

        QCOMPARE(outer.size(), 1);
        QVERIFY(outer.first().contains(QStringLiteral("reentrant.pdf")));
        QCOMPARE(deliveries, 1);
        QVERIFY2(nested.isEmpty(),
                 "the nested pass must not re-deliver (the outer pass owns "
                 "the tick)");
        QVERIFY2(c.ingestScansForTest() == scansBefore + 1,
                 "a re-entrant ingestDeliver ran a second full-tree scan "
                 "(RED pre-fix: the handler's nested call re-walked the "
                 "whole tree)");
        c.stop();
    }

    // ── r4-misc (security-auditor finding 3): the processed-set is BOUNDED ───
    // A months-long enterprise watch accumulates one entry per ingested file
    // forever — a slow memory leak. The set is now bounded two ways: a TTL
    // sweep (an entry whose file stopped being OBSERVED expires) and a hard
    // entry cap (oldest-observed evicted past the cap). The re-ingest
    // tradeoff is deliberate and honest: once an entry is evicted, a
    // byte-identical re-drop of the same file (same relpath|mtime|size key)
    // re-ingests — at-least-once delivery after eviction, never memory
    // without bound.

    // TTL eviction: a key whose file vanished (or changed) stops being
    // refreshed; past the TTL the sweep evicts it, and an identical re-drop
    // of the same file WORKS again (re-ingests). Pre-fix RED: the key lived
    // forever, so the identical re-drop stayed deduped and never re-ingested.
    void processedSetEvictsEntriesPastTtlAndReingests() {
        HotFolderController c;
        QStringList ingested;
        c.setIngestHandler([&ingested](const QStringList& files) {
            ingested << files;
        });

        QDir().mkpath(hotDir());
        c.setProcessedLimitsForTest(/*ttlMs=*/150, /*maxEntries=*/0);
        c.arm(hotDir());

        // Drop + ingest: the key enters the set (lastSeen = now).
        const QDateTime forged = QDateTime::currentDateTimeUtc().addSecs(-60);
        QVERIFY(!createMinimalPdf(hotDir(), QStringLiteral("evict.pdf")).isEmpty());
        QVERIFY2(forgeMTime(hotDir() + QStringLiteral("/evict.pdf"), forged),
                 "mtime forge failed — pin premise broken");
        QCOMPARE(c.ingestDeliver().size(), 1);
        QCOMPARE(c.processedCountForTest(), 1);

        // The file VANISHES: the scan stops observing (refreshing) the key.
        QVERIFY(QFile::remove(hotDir() + QStringLiteral("/evict.pdf")));
        QTest::qWait(400);  // ≥ 2 TTL windows with the key unobserved
        QVERIFY(c.ingestDeliver().isEmpty());  // the sweep pass: nothing present

        // Byte-identical re-drop with the SAME forged mtime → the SAME key.
        QVERIFY(!createMinimalPdf(hotDir(), QStringLiteral("evict.pdf")).isEmpty());
        QVERIFY2(forgeMTime(hotDir() + QStringLiteral("/evict.pdf"), forged),
                 "mtime forge failed — pin premise broken");
        const QStringList redelivered = c.ingestDeliver();
        QVERIFY2(redelivered.size() == 1,
                 "an EVICTED identity must be re-ingestable (the set evicted "
                 "it past the TTL — pre-fix the key lived forever and the "
                 "identical re-drop stayed deduped)");
        QVERIFY(redelivered.first().contains(QStringLiteral("evict.pdf")));
        QCOMPARE(c.processedCountForTest(), 1);  // bounded again, not 2
        c.stop();
    }

    // Guard for the TTL design: a file that stays PRESENT is refreshed by
    // every scan pass and NEVER expires — a static archive in the watched
    // tree must not re-ingest every TTL window. Green pre-fix (nothing
    // evicted at all) and post-fix (the refresh keeps the entry alive).
    void presentFilesAreNotReingestedAcrossTtlWindows() {
        HotFolderController c;
        QDir().mkpath(hotDir());
        c.setProcessedLimitsForTest(/*ttlMs=*/100, /*maxEntries=*/0);
        c.arm(hotDir());

        QVERIFY(!createMinimalPdf(hotDir(), QStringLiteral("static.pdf")).isEmpty());
        QCOMPARE(c.ingestDeliver().size(), 1);

        QTest::qWait(300);  // 3 TTL windows, file present throughout
        QVERIFY2(c.ingestDeliver().isEmpty(),
                 "a present file must not re-ingest past the TTL (the scan "
                 "refreshes its entry)");
        QTest::qWait(300);
        QVERIFY2(c.ingestDeliver().isEmpty(),
                 "a present file must not re-ingest past the TTL");
        QCOMPARE(c.processedCountForTest(), 1);
        c.stop();
    }

    // Entry cap: drops beyond the cap are all DELIVERED (delivery is never
    // throttled) but the set trims its oldest-observed entries to stay
    // bounded — and the trimmed identities are honestly re-ingestable on the
    // next pass (documented at-least-once tradeoff). Pre-fix RED: the set
    // kept every entry, unbounded.
    void processedSetEntryCapBoundsMemoryWhileDeliveringAll() {
        HotFolderController c;
        QDir().mkpath(hotDir());
        c.setProcessedLimitsForTest(/*ttlMs=*/0, /*maxEntries=*/3);
        c.arm(hotDir());

        for (int i = 0; i < 5; ++i)
            QVERIFY(!createMinimalPdf(hotDir(),
                                      QStringLiteral("cap%1.pdf").arg(i)).isEmpty());
        const QStringList delivered = c.ingestDeliver();
        QCOMPARE(delivered.size(), 5);  // the cap never throttles delivery
        QVERIFY2(c.processedCountForTest() <= 3,
                 qPrintable(QStringLiteral("processed set grew past the entry "
                                          "cap: %1 entries (unbounded memory, "
                                          "security-auditor finding 3)")
                                .arg(c.processedCountForTest())));

        // The trimmed identities are honestly re-ingestable: the next pass
        // re-delivers exactly the evicted remainder and trims again.
        const QStringList second = c.ingestDeliver();
        QVERIFY2(second.size() == 2,
                 qPrintable(QStringLiteral("expected exactly the 2 evicted "
                                          "identities to re-ingest, got %1")
                                .arg(second.size())));
        QVERIFY2(c.processedCountForTest() <= 3,
                 "bounded after the re-ingest pass too");
        c.stop();
    }

    // ── r4-misc dynamic probe (adversary: "needs dynamic probe") ─────────────
    // A directory JUNCTION LOOP inside the hot-folder root (root/loop → root)
    // must not hang, crash or infinitely enumerate the ingest scan / watch
    // walk. QDirIterator does not descend links without FollowSymlinks; this
    // probe pins that the adversary's loop shape stays bounded end-to-end.
    void directoryJunctionLoopScanTerminatesBounded() {
        QDir().mkpath(hotDir(QStringLiteral("real")));
        const QString loop = hotDir(QStringLiteral("loop"));
#ifdef Q_OS_WIN
        // A junction needs no privileges — exactly what an adversary dropping
        // folders into the watched share can create.
        const bool created =
            QProcess::execute(QStringLiteral("cmd.exe"),
                              {QStringLiteral("/c"), QStringLiteral("mklink"),
                               QStringLiteral("/J"),
                               QDir::toNativeSeparators(loop),
                               QDir::toNativeSeparators(hotDir())}) == 0;
#else
        const bool created =
            ::symlink(QFile::encodeName(hotDir()).constData(),
                      QFile::encodeName(loop).constData()) == 0;
#endif
        if (!created)
            QSKIP("directory loop could not be created on this filesystem — "
                  "probe premise unavailable");
        // The loop must not outlive the probe (QTemporaryDir cannot remove a
        // reparse point — the residue would fail the suite's temp cleanup).
        struct LoopCleanup {
            QString path;
            ~LoopCleanup() {
#ifdef Q_OS_WIN
                QDir().rmdir(path);   // RemoveDirectory: removes the junction, not the target
#else
                ::unlink(QFile::encodeName(path).constData());
#endif
            }
        } loopCleanup{loop};

        // Arm FIRST (the seed walk already traverses the loop shape), then
        // drop: the timed scan must find exactly the real drop through the
        // real path.
        HotFolderController c;
        QStringList ingested;
        c.setIngestHandler([&ingested](const QStringList& files) {
            ingested << files;
        });
        c.arm(hotDir());

        QVERIFY(!createMinimalPdf(hotDir(QStringLiteral("real")),
                                  QStringLiteral("real.pdf")).isEmpty());
        QElapsedTimer timer;
        timer.start();
        const QStringList delivered = c.ingestDeliver();
        QVERIFY2(timer.elapsed() < 30000,
                 qPrintable(QStringLiteral("the ingest scan over a directory "
                                          "loop ran %1 ms — unbounded")
                                .arg(timer.elapsed())));
        QCOMPARE(delivered.size(), 1);  // the real drop, exactly once
        QVERIFY(delivered.first().contains(QStringLiteral("real.pdf")));
        QVERIFY2(!delivered.first().contains(QStringLiteral("/loop/")),
                 qPrintable(QStringLiteral("the junction must not be descended "
                                          "into: %1").arg(delivered.first())));
        QVERIFY2(c.ingestDeliver().isEmpty(),
                 "a second pass must not re-deliver through the loop");

        // The watch-mode walk (start = seed + full-tree watch refresh) over
        // the same loop terminates too.
        HotFolderController w;
        QElapsedTimer watchTimer;
        watchTimer.start();
        QVERIFY(w.start(hotDir()));
        QVERIFY2(watchTimer.elapsed() < 30000,
                 qPrintable(QStringLiteral("the watch walk over a directory "
                                          "loop ran %1 ms — unbounded")
                                .arg(watchTimer.elapsed())));
        w.stop();
        c.stop();
    }

    // BatchMode wiring: the polling-fallback checkbox exists, the getter
    // tracks it, and startHotFolderForTest (the post-picker half of the
    // toggle's ON branch) starts POLLING when it is checked — watch mode
    // otherwise.
    void batchModePollingOptionWiring() {
        BatchMode bm;
        QVERIFY(!bm.hotFolderPollingEnabled());
        QCheckBox* poll = pollingCheck(bm);
        QVERIFY2(poll, "polling-fallback checkbox not found in BatchMode");

        QDir().mkpath(hotDir());

        poll->setChecked(true);
        QVERIFY(bm.hotFolderPollingEnabled());
        QVERIFY(bm.startHotFolderForTest(hotDir()));
        QVERIFY2(bm.hotFolderForTest() && bm.hotFolderForTest()->isPolling(),
                 "polling checkbox did not start the controller in poll mode");
        bm.hotFolderForTest()->stop();

        poll->setChecked(false);
        QVERIFY(!bm.hotFolderPollingEnabled());
        QVERIFY(bm.startHotFolderForTest(hotDir()));
        QVERIFY2(bm.hotFolderForTest() && !bm.hotFolderForTest()->isPolling()
                     && bm.hotFolderForTest()->isWatching(),
                 "unchecked polling did not start fs-watch mode");
        bm.hotFolderForTest()->stop();
    }

    // R3-sec F-6 end-to-end wiring: BatchMode owns the hot-folder LOG — the
    // controller's degraded-watch disclosure must surface there (and the
    // controller BatchMode holds must be the one that engaged polling).
    void batchModeDisclosesWatchDegradationInLog() {
        BatchMode bm;
        QDir().mkpath(hotDir(QStringLiteral("nested")));
        QVERIFY(bm.startHotFolderForTest(hotDir()));  // real watch, adds succeed
        HotFolderController* hot = bm.hotFolderForTest();
        QVERIFY2(hot, "startHotFolderForTest did not expose the controller");

        // Cap everything added from now on (deterministic cap simulation).
        hot->setWatchAddPathsHookForTest(
            [](const QStringList& paths) { return paths; });

        QDir().mkpath(hotDir(QStringLiteral("late")));
        hot->triggerDirectoryChangedForTest();
        QTest::qWait(700);  // one full debounce window: refresh + disclosure

        QVERIFY2(hot->isPolling(),
                 "BatchMode's controller did not engage the polling backstop");
        QTextEdit* log = logView(bm);
        QVERIFY2(log, "BatchMode log not found");
        QVERIFY2(log->toPlainText().contains(QStringLiteral("polling fallback")),
                 qPrintable(QStringLiteral("degradation not disclosed in the "
                                           "BatchMode log:\n%1")
                                .arg(log->toPlainText().right(400))));
        bm.hotFolderForTest()->stop();
    }
};

QTEST_MAIN(TestHotFolder)
#include "TestHotFolder.moc"
