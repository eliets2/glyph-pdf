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
};

int main(int argc, char **argv)
{
    if (argc > 1 && std::strcmp(argv[1], "--k5-child") == 0)
        return runK5Child(argc, argv);
    QApplication app(argc, argv);
    TestFileHandleCoordination t;
    return QTest::qExec(&t, argc, argv);
}

#include "TestFileHandleCoordination.moc"
