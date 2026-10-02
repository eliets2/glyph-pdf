// SPDX-License-Identifier: Apache-2.0
// TestFileHandleCoordination: the SafeSave GUI-held-handle coordinator
// (GpMainWindow.cpp, F4d-D1) against the engine's serialization lock.
//
// K5 (ADR-UI-03 §3.4, found by review 2026-09-28): the coordinator's worker
// path does a BLOCKING hop to the GUI thread (Qt::BlockingQueuedConnection)
// for every background commit, even to a path no viewer displays. The
// autosave worker reaches that commit while holding PdfEditorEngine's mutex
// (saveDocumentIfCurrent -> saveDocument -> backend save -> SafeSave commit).
// If the GUI thread entered the engine after the worker took the lock (a
// page change -> refreshPageLinks -> extractLinks; a pane refresh ->
// getEmbeddedFiles / getLayers), it is waiting on that mutex and can never
// run the queued hop, so both threads wait for each other forever. Unlike
// the modal background writers the coordinator's comment relies on,
// autosave shows no dialog.
//
// The scenario runs in a CHILD process (this binary, relaunched with
// --k5-child), because a real deadlock cannot be recovered in-process. The
// parent gives it a bounded time and kills it if it hangs. K5_CONTROL=1 runs
// the same rounds WITHOUT the GUI-thread engine call (the control).
#include <QtTest/QtTest>
#include <QApplication>
#include <QPainter>
#include <QPdfWriter>
#include <QProcess>
#include <QTemporaryDir>
#include <QElapsedTimer>
#include <QThread>
#include <QtConcurrent/QtConcurrentRun>
#include <cstdio>
#include <cstring>

#include "GpMainWindow.h"
#include "app/Bootstrapper.h"
#include "core/AppContext.h"
#include "engines/PdfEditorEngine.h"
#include "engines/SafeSave.h"
#include "ui/PdfViewerWidget.h"

using gp::MainWindow;

namespace {

void stage(const char *what)
{
    std::fprintf(stderr, "K5 child: %s\n", what);
    std::fflush(stderr);
}

// A document heavy enough that serializing it keeps the engine lock held for
// a comfortable window (hundreds of pages of vector content).
void makeHeavyPdf(const QString &path, int pages)
{
    QPdfWriter w(path);
    w.setPageSize(QPageSize(QPageSize::A4));
    QPainter p(&w);
    for (int i = 0; i < pages; ++i) {
        if (i > 0) w.newPage();
        for (int line = 0; line < 60; ++line)
            p.drawText(80, 120 + line * 120,
                       QStringLiteral("K5 page %1 line %2 - the quick brown fox jumps over the lazy dog").arg(i + 1).arg(line));
    }
    p.end();
}

// The child: build the real MainWindow (it installs the coordinator), load
// the document into the engine, then interleave an autosave-style save on a
// worker with GUI-thread engine calls. Returns 0 when every round completes.
int runK5Child(int argc, char **argv)
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("GlyphPDFTests"));
    QCoreApplication::setApplicationName(QStringLiteral("TestFileHandleCoordinationChild"));
    const bool control = qEnvironmentVariableIsSet("K5_CONTROL");
    const QString dir = qEnvironmentVariable("K5_DIR");
    const QString pdf = dir + QStringLiteral("/heavy.pdf");
    const int pages = qEnvironmentVariableIntValue("K5_PAGES") > 0 ? qEnvironmentVariableIntValue("K5_PAGES") : 300;
    makeHeavyPdf(pdf, pages);
    stage("pdf written");

    auto win = std::make_unique<MainWindow>(Bootstrapper::createContext());
    win->show();
    stage("window shown");
    win->openDocument(pdf);
    stage("document opened");
    auto engine = win->appContext()->pdfEditor;
    if (!engine || !engine->loadDocumentForEditing(pdf)) {
        stage("engine could not load the document");
        return 2;
    }
    stage("engine loaded");
    const QString current = engine->currentFile();
    const qint64 loadId = engine->documentLoadId();

    for (int round = 0; round < 5; ++round) {
        const QString out = dir + QStringLiteral("/autosave-%1.pdf").arg(round);
        QElapsedTimer roundTimer;
        roundTimer.start();
        // Autosave shape (AutosaveManager.cpp): identity-guarded save of the
        // resident document to a temp path, on a worker, with no dialog.
        QFuture<bool> save = QtConcurrent::run([engine, current, loadId, out] {
            return engine->saveDocumentIfCurrent(current, loadId, out);
        });
        // Let the worker take the engine lock and start serializing, then
        // enter the engine from the GUI thread exactly as a page change does
        // (refreshPageLinks -> engine->extractLinks). No event processing in
        // between: the GUI thread is busy, as it is inside any slot.
        QThread::msleep(15);
        std::fprintf(stderr, "K5 child: round %d save started%s\n", round,
                     control ? " (control: no GUI engine call)" : "");
        std::fflush(stderr);
        if (!control)
            (void)engine->extractLinks(current, 0);
        std::fprintf(stderr, "K5 child: round %d GUI engine call returned\n", round);
        std::fflush(stderr);
        while (!save.isFinished())
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        std::fprintf(stderr, "K5 child: round %d completed (save %s) in %lld ms\n", round,
                     save.result() ? "ok" : "refused", static_cast<long long>(roundTimer.elapsed()));
        std::fflush(stderr);
    }
    return 0;
}

// A real MainWindow displaying `pdf`, the exact coordinator owner the engine
// writers reach through SafeSave. Returns null (with `stage` output) when the
// document could not be opened.
std::unique_ptr<MainWindow> openDisplayedDocument(const QString &pdf, const char *tag)
{
    auto win = std::make_unique<MainWindow>(Bootstrapper::createContext());
    win->show();
    win->openDocument(pdf);
    stage(tag);
    return win;
}

// ── K1 (ADR-UI-03 step 2b): the hop for commits to the DISPLAYED file was an
// UNBOUNDED Qt::BlockingQueuedConnection. An in-place engine save to the
// displayed path holds PdfEditorEngine's mutex across the whole save; the hop
// then waits for the GUI thread, which — exactly as in K5 — may itself be
// blocked ON that mutex (page change → refreshPageLinks → extractLinks). The
// K5 fix (skip the hop for undisplayed destinations) cannot help here: the
// park is required for the atomic replace, so the coordinator kept the
// blocking hop and this deadlock with it. The fix replaces the hop with a
// bounded wait: the writer gives the GUI thread its deadline, then proceeds —
// the commit fails honestly ("Access is denied", destination untouched)
// instead of hanging both threads.
//
// Child: rounds of {worker: engine in-place save to the DISPLAYED path;
// GUI: engine->extractLinks (the K5 lock shape)}. Pre-fix this hangs and the
// parent kills it. Post-fix each round completes in about the hop deadline and
// the displayed file's bytes are untouched (the refused in-place save).
int runK1Child(int argc, char **argv)
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("GlyphPDFTests"));
    QCoreApplication::setApplicationName(QStringLiteral("TestFileHandleCoordinationChildK1"));
    const QString dir = qEnvironmentVariable("K1_DIR");
    const QString pdf = dir + QStringLiteral("/displayed.pdf");
    makeHeavyPdf(pdf, 6);
    stage("pdf written");
    auto win = openDisplayedDocument(pdf, "window shown, document displayed");
    auto engine = win->appContext()->pdfEditor;
    if (!engine || !engine->loadDocumentForEditing(pdf)) {
        stage("engine could not load the document");
        return 2;
    }
    const QString current = engine->currentFile();
    const qint64 loadId = engine->documentLoadId();
    const auto before = gp::SafeSave::captureDestinationIdentity(pdf);
    bool posixAnyOk = false;

    // The bound under test. Production uses 10 s; the child shrinks it so the
    // rounds are quick while keeping the shape identical.
    MainWindow::setFileHandleHopTimeoutForTesting(3000);

    for (int round = 0; round < 2; ++round) {
        QElapsedTimer roundTimer;
        roundTimer.start();
        // In-place save onto the displayed path on a worker: the engine mutex
        // is held from here until the save returns (including the hop).
        QFuture<bool> save = QtConcurrent::run([engine, current, loadId] {
            return engine->saveDocumentIfCurrent(current, loadId, current);
        });
        // Let the worker take the engine lock, then enter the engine from the
        // GUI thread exactly as a page change does. The GUI thread is now
        // blocked on the worker's mutex and cannot run the queued hop — the
        // K5 lock shape, with the destination displayed.
        QThread::msleep(15);
        (void)engine->extractLinks(current, 0);
        while (!save.isFinished())
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        std::fprintf(stderr, "K1 child: round %d completed (save %s) in %lld ms\n", round,
                     save.result() ? "ok" : "refused", static_cast<long long>(roundTimer.elapsed()));
        std::fflush(stderr);
        posixAnyOk = posixAnyOk || save.result();
    }
    const auto after = gp::SafeSave::captureDestinationIdentity(pdf);
#ifdef Q_OS_WIN
    // The in-place save was refused (the park could not run in time) — the
    // displayed file must be byte-identical.
    if (!before.valid || after.sha256 != before.sha256) {
        stage("the refused in-place save changed the displayed file");
        return 4;
    }
#else
    // NATIVE-LINUX (2026-10-02): POSIX has no sharing-violation class. A
    // successful in-place save legitimately rewrites the displayed path
    // (writing over an open handle is legal); a refused save must leave it
    // byte-identical. Pin the consistent pair, not the Windows outcome.
    if (posixAnyOk && after.sha256 == before.sha256) {
        stage("POSIX: the in-place save reported ok but the displayed file is byte-identical");
        return 4;
    }
    if (!posixAnyOk && after.sha256 != before.sha256) {
        stage("POSIX: the in-place save was refused but the displayed file changed");
        return 4;
    }
#endif
    return 0;
}

// ── K2 (ADR-UI-03 step 2b): the hop's wait is UNBOUNDED, so a dialog-less
// background writer committing to the DISPLAYED file stalls for as long as
// the GUI thread stays busy — the coordinator's worker-path safety argument
// ("every background writer parks a WindowModal progress dialog, so the GUI
// thread stays in its event loop") covers only dialog'd writers; autosave
// shows no dialog (K5's own note), and the signing/tagging workers hold the
// transaction across the whole wait. A commit hopped out this late also LANDS
// late: the park finally runs whenever the GUI frees up and the write then
// replaces the displayed file unattended, long after the user's action.
//
// Child: a busy GUI (no event processing for 2 s) against a 250 ms hop bound
// and a direct SafeSave commit to the displayed file. Pre-fix the worker is
// still inside the hop when the busy window ends (exit 7 — the stall, and the
// proof the hop engaged), and the late commit lands (exit 10). Post-fix it
// returns inside the bound, the commit fails honestly, and the destination is
// byte-identical.
int runK2Child(int argc, char **argv)
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("GlyphPDFTests"));
    QCoreApplication::setApplicationName(QStringLiteral("TestFileHandleCoordinationChildK2"));
    const QString dir = qEnvironmentVariable("K2_DIR");
    const QString pdf = dir + QStringLiteral("/displayed.pdf");
    makeHeavyPdf(pdf, 3);
    stage("pdf written");
    auto win = openDisplayedDocument(pdf, "window shown, document displayed");
    // The direct SafeSave commit is a FOREIGN writer: like the signing fill
    // step, it must drop the editing engine's file-backed resident (its
    // lazy-parse device holds the file until dropped) so the VIEWER's handle —
    // the one the coordinator parks — is the only obstacle to the atomic
    // replace. The commit under test must fail (or succeed) for coordinator
    // reasons, not because a second device pinned the file.
    if (auto engine = win->appContext()->pdfEditor)
        engine->releaseResidentFile(pdf);
    MainWindow::setFileHandleHopTimeoutForTesting(250);
    const auto before = gp::SafeSave::captureDestinationIdentity(pdf);
    const QString candidate = dir + QStringLiteral("/candidate.pdf");
    makeHeavyPdf(candidate, 1);

    QFuture<bool> commit = QtConcurrent::run([pdf, candidate] {
        QString err;
        return gp::SafeSave::commitFileToDestination(candidate, pdf, &err);
    });
    // Busy GUI: no event processing for 2 s — 8× the hop bound.
    QThread::msleep(2000);
    if (!commit.isFinished()) {
        stage("the commit attempt is still stuck on the hop 2 s in (unbounded stall)");
        return 7;
    }
    stage("commit attempt returned while the GUI was still busy (bounded)");
    if (commit.result()) {
#ifdef Q_OS_WIN
        stage("the late-hopped commit landed although the park deadline passed long before");
        return 10;
#else
        // NATIVE-LINUX (2026-10-02): POSIX has no sharing-violation class —
        // the atomic replace does not need the viewer's park at all, so the
        // bounded commit LANDS while the GUI is busy. The honest POSIX pin is
        // the opposite of Windows': the destination must carry the candidate.
        const auto landed = gp::SafeSave::captureDestinationIdentity(pdf);
        if (!landed.valid || landed.sha256 == before.sha256) {
            stage("POSIX: the landed commit did not change the destination");
            return 11;
        }
        stage("POSIX: the bounded commit landed over the open viewer handle");
        return 0;
#endif
    }
    const auto after = gp::SafeSave::captureDestinationIdentity(pdf);
    if (!before.valid || after.sha256 != before.sha256) {
        stage("the refused commit changed the destination");
        return 11;
    }
    return 0;
}

// ── K4 (ADR-UI-03 step 2b): the bounded hop's aftermath contract. When the
// GUI thread does not service the park within the deadline, the writer must
// PROCEED (bounded), the commit must fail honestly with the destination
// byte-identical, and the viewer must recover — the late park and the late
// restore run in queue order once the GUI thread frees up, leaving no
// stranded park (the K3 latch fix keeps a late stale restore from latching).
//
// Child: a busy GUI (no event processing for 1 s) against a 250 ms hop bound.
// Pre-fix the worker cannot return until the GUI drains (the commit then even
// SUCCEEDS — exit 4). Post-fix it returns inside the bound (exit-7 check),
// fails honestly (exit-4 check), leaves the destination byte-identical
// (exit-5 check), and a second in-place commit succeeds again (exit-6 check).
int runK4Child(int argc, char **argv)
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("GlyphPDFTests"));
    QCoreApplication::setApplicationName(QStringLiteral("TestFileHandleCoordinationChildK4"));
    const QString dir = qEnvironmentVariable("K4_DIR");
    const QString pdf = dir + QStringLiteral("/displayed.pdf");
    makeHeavyPdf(pdf, 3);
    stage("pdf written");
    auto win = openDisplayedDocument(pdf, "window shown, document displayed");
    // The direct SafeSave commit is a FOREIGN writer: like the signing fill
    // step, it must drop the editing engine's file-backed resident (its
    // lazy-parse device holds the file until dropped) so the VIEWER's handle —
    // the one the coordinator parks — is the only obstacle to the atomic
    // replace. The commit under test must fail (or succeed) for coordinator
    // reasons, not because a second device pinned the file.
    if (auto engine = win->appContext()->pdfEditor)
        engine->releaseResidentFile(pdf);
    MainWindow::setFileHandleHopTimeoutForTesting(250);
    const auto before = gp::SafeSave::captureDestinationIdentity(pdf);
    const QString candidate = dir + QStringLiteral("/candidate.pdf");
    makeHeavyPdf(candidate, 1);

    QFuture<bool> commit = QtConcurrent::run([pdf, candidate] {
        QString err;
        return gp::SafeSave::commitFileToDestination(candidate, pdf, &err);
    });
    // Busy GUI: no event processing for 1 s — 4× the hop bound.
    QThread::msleep(1000);
    if (!commit.isFinished()) {
        stage("the commit attempt outlived the hop deadline while the GUI was busy");
        return 7;
    }
    stage("commit attempt returned while the GUI was still busy (bounded)");
    if (commit.result()) {
#ifdef Q_OS_WIN
        stage("the commit succeeded although the park never ran in time");
        return 4;
#else
        // NATIVE-LINUX (2026-10-02): POSIX has no sharing-violation class —
        // the bounded commit lands while the GUI is busy. Pin the landing.
        const auto landed = gp::SafeSave::captureDestinationIdentity(pdf);
        if (!landed.valid || landed.sha256 == before.sha256) {
            stage("POSIX: the landed commit did not change the destination");
            return 5;
        }
        stage("POSIX: the bounded commit landed over the open viewer handle");
        return 0;
#endif
    }
    const auto after = gp::SafeSave::captureDestinationIdentity(pdf);
    if (!before.valid || after.sha256 != before.sha256) {
        stage("the refused commit changed the destination");
        return 5;
    }
    // Recovery: drain the events — the late park and the late restore run in
    // queue order — then require a second in-place commit to work again.
    QCoreApplication::processEvents(QEventLoop::AllEvents, 200);
    const QString candidate2 = dir + QStringLiteral("/candidate2.pdf");
    makeHeavyPdf(candidate2, 2);
    QFuture<bool> commit2 = QtConcurrent::run([pdf, candidate2] {
        QString err;
        return gp::SafeSave::commitFileToDestination(candidate2, pdf, &err);
    });
    while (!commit2.isFinished())
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    if (!commit2.result()) {
        stage("a second in-place commit after the timed-out hop failed (stranded park?)");
        return 6;
    }
    stage("viewer recovered: the second in-place commit went through");
    return 0;
}

} // namespace

class TestFileHandleCoordination : public QObject {
    Q_OBJECT
private slots:
    // The K5 contract: a background save and a GUI-thread engine call must
    // never deadlock. On the unfixed coordinator the child hangs and is killed.
    void noDeadlockWhenGuiUsesEngineDuringBackgroundSave()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QProcess child;
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert(QStringLiteral("K5_DIR"), dir.path());
        env.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
        child.setProcessEnvironment(env);
        child.setProcessChannelMode(QProcess::MergedChannels);
        child.start(QCoreApplication::applicationFilePath(), { QStringLiteral("--k5-child") });
        QVERIFY(child.waitForStarted(10000));
        const bool finished = child.waitForFinished(45000);
        if (!finished) {
            child.kill();
            child.waitForFinished(5000);
        }
        qInfo().noquote() << "child log:\n" << QString::fromLocal8Bit(child.readAll());
        QVERIFY2(finished, "the child hung: background save and GUI engine call deadlocked (K5)");
        QCOMPARE(child.exitCode(), 0);
    }

    // K3 (ADR-UI-03 document-ownership design): the parked latch must not
    // survive a document switch. parkDocumentForWrite(P) swaps the resident
    // document to the parking device and latches m_parkedForWrite; if the user
    // switches to Q while a background commit holds the park, the commit's
    // restore(P) arrives when m_filePath is already Q — on the unfixed widget
    // that stale restore returned WITHOUT clearing the latch, so the next
    // parkDocumentForWrite(Q) returned "already parked" WITHOUT releasing Q's
    // handle, and every later in-place commit to the displayed file failed
    // "Access is denied" until a manual reload.
    void documentSwitchWhileParkedDoesNotLatchThePark()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString displayedP = dir.filePath(QStringLiteral("p.pdf"));
        const QString switchedQ = dir.filePath(QStringLiteral("q.pdf"));
        makeHeavyPdf(displayedP, 2);
        makeHeavyPdf(switchedQ, 3);
        const QString candidate1 = dir.filePath(QStringLiteral("cand1.pdf"));
        const QString candidate2 = dir.filePath(QStringLiteral("cand2.pdf"));
        makeHeavyPdf(candidate1, 1);
        makeHeavyPdf(candidate2, 1);

        PdfViewerWidget w;
        QVERIFY(w.loadDocument(displayedP));
        QCOMPARE(w.filePath(), displayedP);

        // Baseline (the established park contract): parking the displayed file
        // releases its handle, so an in-place commit over it succeeds.
        QVERIFY(w.parkDocumentForWrite(displayedP));
        {
            QString err;
            QVERIFY2(gp::SafeSave::commitFileToDestination(candidate1, displayedP, &err),
                     qPrintable(err));
        }

        // The interleaving: the user switches documents while the park is
        // held, then the background commit's stale restore lands.
        QVERIFY(w.loadDocument(switchedQ));
        w.restoreDocumentAfterWrite(displayedP);

        // The next in-place write to the NEW displayed file must still be able
        // to release it. On the unfixed widget the latched park made this park
        // a no-op, so the commit below hit the viewer's open handle and failed
        // "Access is denied" — the pin's red.
        QVERIFY(w.parkDocumentForWrite(switchedQ));
        {
            QString err;
            QVERIFY2(gp::SafeSave::commitFileToDestination(candidate2, switchedQ, &err),
                     qPrintable(QStringLiteral("commit after a document switch failed: %1").arg(err)));
        }
        // The viewer recovers: the switched document is displayed again.
        w.restoreDocumentAfterWrite(switchedQ);
        QVERIFY(w.isLoaded());
        QCOMPARE(w.filePath(), switchedQ);
    }

    // K1: an in-place worker save to the DISPLAYED file must not deadlock the
    // GUI thread, and a save the park could not be obtained for must be
    // refused with the destination untouched (the bounded-hop contract).
    void noDeadlockWhenBackgroundSavesTheDisplayedFile()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QProcess child;
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert(QStringLiteral("K1_DIR"), dir.path());
        env.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
        child.setProcessEnvironment(env);
        child.setProcessChannelMode(QProcess::MergedChannels);
        child.start(QCoreApplication::applicationFilePath(), { QStringLiteral("--k1-child") });
        QVERIFY(child.waitForStarted(10000));
        const bool finished = child.waitForFinished(120000);
        if (!finished) {
            child.kill();
            child.waitForFinished(5000);
        }
        qInfo().noquote() << "child log:\n" << QString::fromLocal8Bit(child.readAll());
        QVERIFY2(finished, "the child hung: an in-place worker save to the displayed file "
                           "deadlocked the GUI thread on the coordinator's unbounded hop (K1)");
        QCOMPARE(child.exitCode(), 0);
    }

    // K2: a background commit to the DISPLAYED file must not stall for as
    // long as the GUI thread stays busy, and a commit whose park deadline
    // passed must not land late and unattended.
    void boundedHopReturnsWhileTheGuiIsBusy()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QProcess child;
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert(QStringLiteral("K2_DIR"), dir.path());
        env.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
        child.setProcessEnvironment(env);
        child.setProcessChannelMode(QProcess::MergedChannels);
        child.start(QCoreApplication::applicationFilePath(), { QStringLiteral("--k2-child") });
        QVERIFY(child.waitForStarted(10000));
        const bool finished = child.waitForFinished(60000);
        if (!finished) {
            child.kill();
            child.waitForFinished(5000);
        }
        qInfo().noquote() << "child log:\n" << QString::fromLocal8Bit(child.readAll());
        QVERIFY2(finished, "the child hung: the coordinator's hop is not bounded (K2)");
        QVERIFY2(child.exitCode() != 7,
                 "the commit worker was still stuck on the hop long after its deadline: "
                 "the displayed-file hop is unbounded (K2)");
        QVERIFY2(child.exitCode() != 10,
                 "a commit whose park deadline passed landed late and unattended (K2)");
        QCOMPARE(child.exitCode(), 0);
    }

    // K4: a commit whose park hop times out must proceed boundedly, fail
    // honestly, leave the destination byte-identical, and leave the viewer
    // able to commit again afterwards (late park + late restore, no stranded
    // park).
    void timedOutHopKeepsTheDestinationUntouchedAndTheViewerRecovers()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QProcess child;
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert(QStringLiteral("K4_DIR"), dir.path());
        env.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
        child.setProcessEnvironment(env);
        child.setProcessChannelMode(QProcess::MergedChannels);
        child.start(QCoreApplication::applicationFilePath(), { QStringLiteral("--k4-child") });
        QVERIFY(child.waitForStarted(10000));
        const bool finished = child.waitForFinished(60000);
        if (!finished) {
            child.kill();
            child.waitForFinished(5000);
        }
        qInfo().noquote() << "child log:\n" << QString::fromLocal8Bit(child.readAll());
        QVERIFY2(finished, "the child hung: the coordinator's hop is not bounded (K4)");
        QCOMPARE(child.exitCode(), 0);
    }
};

int main(int argc, char **argv)
{
    if (argc > 1 && std::strcmp(argv[1], "--k5-child") == 0)
        return runK5Child(argc, argv);
    if (argc > 1 && std::strcmp(argv[1], "--k1-child") == 0)
        return runK1Child(argc, argv);
    if (argc > 1 && std::strcmp(argv[1], "--k2-child") == 0)
        return runK2Child(argc, argv);
    if (argc > 1 && std::strcmp(argv[1], "--k4-child") == 0)
        return runK4Child(argc, argv);
    QApplication app(argc, argv);
    TestFileHandleCoordination t;
    return QTest::qExec(&t, argc, argv);
}

#include "TestFileHandleCoordination.moc"
