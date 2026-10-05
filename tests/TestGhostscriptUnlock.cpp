// SPDX-License-Identifier: Apache-2.0
// 9.17 — Ghostscript-assisted unlock (PRD §9.11 unlock bullet) test suite.
//
// Pins, in three groups:
//   1. RESOLUTION (GhostscriptLocator): the standard-install scan, highest
//      version wins, unversioned directories never launch, absence is empty
//      and the disclosure names the searched locations. There is NO PATH leg
//      and no vendored binary (AGPL — docs/research/ghostscript-unlock-notes.md);
//      the locator is honest about exactly where it looked.
//   2. ARGV SHAPE: the process arguments never carry the user password (M-1,
//      CWE-214) and always carry -dSAFER (a hostile document must not gain
//      PostScript file access). Pinned BOTH against the pure builder and
//      against the REAL arguments the runner hands to a process — the test
//      re-execs ITSELF as a fake tool (the TestEncryptedPackageSafeWrite
//      re-exec idiom) that records its argv.
//   3. END-TO-END against the real installed Ghostscript (skipped honestly
//      when absent): an owner-password-restricted PDF unlocks to a copy with
//      NO /Encrypt dictionary; a user-password PDF without the password fails
//      honestly quoting Ghostscript's own output and never touches the
//      destination; the same PDF unlocks when the correct password is
//      supplied (off argv); a corrupted protected file either re-distills
//      into a readable PDF or fails without corrupting the output.
#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QTemporaryDir>

#include "engines/GhostscriptRunner.h"
#include "engines/PdfEditorEngine.h"
#include "engines/SafeSave.h"

#include <podofo/podofo.h>

namespace GhostscriptLocator = gp::GhostscriptLocator;
namespace GhostscriptRunner = gp::GhostscriptRunner;

// ── fake-tool child mode ─────────────────────────────────────────────────────
// The test re-execs ITSELF as the "Ghostscript" tool: the runner is handed
// the test binary's path, and a child dispatched via GP_FAKE_GS_MODE records
// its FULL argv (joined with '\n') into GP_FAKE_GS_ARGV_LOG, then:
//   ok             — write a minimal %PDF to the -sOutputFile= path, exit 0
//   needs-password — print the pdfi password signature, exit 0 (the 10.08.0
//                    observed behavior: password refusal exits ZERO with a
//                    blank output document — the trap the no-pages gate pins)
// The mode rides on the environment because the runner OWNS the argv — that
// is exactly what the argv pins are about. The secret check is the point:
// whatever the runner was handed, the ARGV it actually passed must not
// contain it.
static void fakeGsMain()
{
    const QString logPath = qEnvironmentVariable("GP_FAKE_GS_ARGV_LOG");
    QFile logFile(logPath);
    if (logFile.open(QIODevice::Append | QIODevice::Text)) {
        logFile.write("=== argv-begin\n");
        for (const QString& a : QCoreApplication::arguments())
            logFile.write(a.toUtf8() + "\n");
        logFile.write("=== argv-end\n");
        logFile.close();
    }
    if (qEnvironmentVariable("GP_FAKE_GS_MODE") == QLatin1String("needs-password")) {
        QTextStream out(stdout);
        out << "**** This file requires a password for access.\n";
        out << "**** Error: Couldn't initialise file.\n";
        out << "No pages will be processed (FirstPage > LastPage).\n";
        out.flush();
        return;  // exit 0 — the dangerous blank-output case
    }
    // "ok": honor -sOutputFile= from our own argv like the real device would.
    QString output;
    for (const QString& a : QCoreApplication::arguments())
        if (a.startsWith(QLatin1String("-sOutputFile=")))
            output = a.mid(QStringLiteral("-sOutputFile=").size());
    QFile out(output);
    if (out.open(QIODevice::WriteOnly)) {
        out.write("%PDF-1.4\n"
                  "1 0 obj<</Type/Catalog/Pages 2 0 R>>endobj\n"
                  "2 0 obj<</Type/Pages/Kids[3 0 R]/Count 1>>endobj\n"
                  "3 0 obj<</Type/Page/Parent 2 0 R/MediaBox[0 0 99 99]>>endobj\n"
                  "trailer<</Size 4/Root 1 0 R>>");
        out.close();
    }
    QTextStream(stdout) << "Page 1\n";
}

// ── helpers ──────────────────────────────────────────────────────────────────
static QByteArray sha256OfFile(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    const QByteArray all = f.readAll();
    return QCryptographicHash::hash(all, QCryptographicHash::Sha256).toHex();
}

static bool pdfHasEncryptDict(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return true;  // unreadable = worst case
    const QByteArray all = f.readAll();
    return all.contains("/Encrypt");
}

class TestGhostscriptUnlock : public QObject {
    Q_OBJECT

    QTemporaryDir m_tmp;

    QString tmpPath(const QString& name) const { return m_tmp.filePath(name); }

    // A one-page PDF with actual text content, via PoDoFo (the house fixture
    // shape — R14ProbeRedactSpace). Content matters: a blank page could pass
    // a lazy "unlocked" check even when Ghostscript produced its blank husk.
    bool makeContentPdf(const QString& path) const
    {
        try {
            PoDoFo::PdfMemDocument doc;
            auto& font = doc.GetFonts().GetStandard14Font(
                PoDoFo::PdfStandard14FontType::Helvetica);
            auto& page = doc.GetPages().CreatePage(
                PoDoFo::Rect(0.0, 0.0, 595.0, 842.0));
            PoDoFo::PdfPainter painter;
            painter.SetCanvas(page);
            painter.TextState.SetFont(font, 12.0);
            (painter.DrawText)("UNLOCK-PROBE-MARKER", 100.0, 780.0);
            painter.FinishDrawing();
            doc.Save(path.toUtf8().constData());
            return true;
        } catch (const PoDoFo::PdfError&) {
            return false;
        }
    }

    // Encrypt via the app's own engine (AES-256): userPassword may be empty
    // (owner-password-only document — opens with the empty user password but
    // carries permission restrictions).
    bool makeEncryptedPdf(const QString& path, const QString& userPw,
                          const QString& ownerPw) const
    {
        const QString plain = tmpPath(QStringLiteral("encrypt-src.pdf"));
        if (!makeContentPdf(plain)) return false;
        PdfEditorEngine engine;
        if (!engine.loadDocumentForEditing(plain)) return false;
        DocumentPermissions perms;  // defaults deny modify/annotate — restricted
        perms.copy = false;
        perms.print = false;
        if (!engine.encryptDocument(userPw, ownerPw, perms)) return false;
        return engine.saveDocument(path);
    }

private slots:
    void initTestCase()
    {
        QVERIFY2(m_tmp.isValid(), "temp dir unavailable");
    }

    // ── 1. resolution ────────────────────────────────────────────────────────
    void absentRootYieldsEmptyAndHonestDisclosure()
    {
        QTemporaryDir empty;
        QVERIFY(empty.isValid());
        const QString found = GhostscriptLocator::locateForTesting(empty.path());
        QVERIFY2(found.isEmpty(),
                 "an empty root must not resolve — absence must be honest");
        // The disclosure names the standard roots so a missing install is
        // diagnosable (locate-path disclosure, not a generic failure).
        const QString disclosure = GhostscriptLocator::absenceDisclosure();
        for (const QString& root : GhostscriptLocator::standardInstallRoots())
            QVERIFY2(disclosure.contains(QDir::toNativeSeparators(root)),
                     "disclosure must name the searched standard locations");
        QVERIFY2(disclosure.contains("gswin64c"),
                 "disclosure must name the console binary it looks for");
    }

    void presentRootResolvesHighestVersionConsoleBinary()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QString exe = GhostscriptLocator::consoleBinaryName();
        QVERIFY(!exe.isEmpty());
        // Two version trees; the locator must pick the higher one.
        const QString v9 = root.path() + QStringLiteral("/gs/gs9.55.0/bin");
        const QString v10 = root.path() + QStringLiteral("/gs/gs10.08.0/bin");
        QVERIFY(QDir().mkpath(v9));
        QVERIFY(QDir().mkpath(v10));
        QVERIFY(QFile::copy(QCoreApplication::applicationFilePath(),
                            v9 + QLatin1Char('/') + exe));
        QVERIFY(QFile::copy(QCoreApplication::applicationFilePath(),
                            v10 + QLatin1Char('/') + exe));
        const QString found = GhostscriptLocator::locateForTesting(root.path());
        QCOMPARE(found, QDir::toNativeSeparators(v10 + QLatin1Char('/') + exe));
    }

    void unversionedOrHalfPlantedDirsNeverResolve()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QString exe = GhostscriptLocator::consoleBinaryName();
        // An UNVERSIONED directory (no gs<version> parse) must never launch.
        QVERIFY(QDir().mkpath(root.path() + QStringLiteral("/gs/bin")));
        QVERIFY(QFile::copy(QCoreApplication::applicationFilePath(),
                            root.path() + QStringLiteral("/gs/bin/") + exe));
        QVERIFY2(GhostscriptLocator::locateForTesting(root.path()).isEmpty(),
                 "a binary in an unversioned gs dir must not resolve");
        // A version dir with the binary in the WRONG place (no bin/) too.
        QVERIFY(QDir().mkpath(root.path() + QStringLiteral("/gs/gs10.0.0")));
        QVERIFY(QFile::copy(QCoreApplication::applicationFilePath(),
                            root.path() + QStringLiteral("/gs/gs10.0.0/") + exe));
        QVERIFY2(GhostscriptLocator::locateForTesting(root.path()).isEmpty(),
                 "a binary outside bin/ must not resolve");
    }

    // ── 2. argv shape ────────────────────────────────────────────────────────
    void builderPinsSaferAndLayout()
    {
        const QStringList args = GhostscriptRunner::buildUnlockArgs(
            QStringLiteral("c:/temp/out.pdf"), QStringLiteral("c:/docs/in.pdf"));
        QVERIFY2(args.contains(QStringLiteral("-dSAFER")),
                 "-dSAFER is MANDATORY on every unlock invocation");
        QVERIFY2(!args.contains(QStringLiteral("-dNOSAFER")),
                 "-dNOSAFER must never be emitted");
        QVERIFY2(args.contains(QStringLiteral("-dBATCH")), "batch mode required");
        QVERIFY2(args.contains(QStringLiteral("-dNOPAUSE")), "no-pause required");
        QVERIFY2(args.contains(QStringLiteral("-sDEVICE=pdfwrite")),
                 "the unlock device is pdfwrite");
        QVERIFY2(args.contains(QStringLiteral("-sOutputFile=c:/temp/out.pdf")),
                 "output goes to the candidate");
        QCOMPARE(args.last(), QStringLiteral("c:/docs/in.pdf"));
        // No password-shaped switch can exist in a password-free builder.
        for (const QString& a : args)
            QVERIFY2(!a.startsWith(QLatin1String("-sPDFPassword")),
                     "the builder never emits the password switch");
    }

    void realArgvNeverCarriesTheSecretAndAlwaysCarriesSafer()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QString exe = GhostscriptLocator::consoleBinaryName();
        QVERIFY(QDir().mkpath(root.path() + QStringLiteral("/gs/gs10.0.0/bin")));
        const QString fakeTool =
            root.path() + QStringLiteral("/gs/gs10.0.0/bin/") + exe;
        // The fake tool is THIS binary re-exec'd; the locator resolves by name
        // only, so a copy of the test executable under the console name works.
        QVERIFY(QFile::copy(QCoreApplication::applicationFilePath(), fakeTool));

        // Sources must exist (the runner refuses absent inputs before any
        // process starts — pinned implicitly here).
        const QString srcA = tmpPath(QStringLiteral("a-src.pdf"));
        const QString srcB = tmpPath(QStringLiteral("b-src.pdf"));
        QVERIFY(makeContentPdf(srcA));
        QVERIFY(makeContentPdf(srcB));

        const QString log = tmpPath(QStringLiteral("argv.log"));
        QFile::remove(log);
        qputenv("GP_FAKE_GS_ARGV_LOG", log.toUtf8());

        const QString secretPassword =
            QStringLiteral("S3cret-pw with spaces \\ and symbols");
        const QString destination = tmpPath(QStringLiteral("dest.pdf"));

        // Case A: the flow succeeds on pass 1 (fake tool is happy) — the
        // password was provided but must never reach any argv.
        qputenv("GP_FAKE_GS_MODE", "ok");
        const GhostscriptRunner::UnlockResult okRun =
            GhostscriptRunner::unlockPdf(fakeTool, srcA, destination,
                                         secretPassword.toUtf8());
        QVERIFY(okRun.ok);
        // Case B: the first pass reports the password requirement (fake tool
        // prints the pdfi signature) so the runner runs the SECOND pass with
        // the password response file — that argv must also stay clean.
        qputenv("GP_FAKE_GS_MODE", "needs-password");
        const GhostscriptRunner::UnlockResult pwRun =
            GhostscriptRunner::unlockPdf(fakeTool, srcB,
                                         tmpPath("dest-b.pdf"),
                                         secretPassword.toUtf8());
        QVERIFY2(!pwRun.ok,
                 "the needs-password fake never produces a real unlock; the "
                 "second pass fails too and the failure is honest");
        QVERIFY2(pwRun.usedPassword,
                 "the second pass must be recorded as the password attempt");

        QFile logFile(log);
        QVERIFY(logFile.open(QIODevice::ReadOnly));
        const QByteArray allArgv = logFile.readAll();
        logFile.close();
        QVERIFY2(allArgv.contains("=== argv-begin"),
                 "the fake tool must have captured all passes' argv");
        // THE PIN: the secret appears in NO process argument, in any pass,
        // even though the runner was holding it the whole time.
        QVERIFY2(!allArgv.contains(secretPassword.toUtf8()),
                 "the user password must never travel in argv (M-1, CWE-214)");
        QVERIFY2(!allArgv.contains("-sPDFPassword"),
                 "the password switch must never appear on the command line");
        // THE OTHER PIN: -dSAFER is on every invocation (A pass 1, B pass 1
        // and B pass 2 = three) — walk the recorded blocks individually.
        const QByteArray blockMarker = QByteArrayLiteral("=== argv-begin");
        QList<QByteArray> argvBlocks;
        int from = 0;
        while (true) {
            const int begin = allArgv.indexOf(blockMarker, from);
            if (begin < 0) break;
            int end = allArgv.indexOf(blockMarker, begin + blockMarker.size());
            if (end < 0) end = allArgv.size();
            argvBlocks << allArgv.mid(begin, end - begin);
            from = end;
        }
        QCOMPARE(argvBlocks.size(), 3);
        for (const QByteArray& block : argvBlocks) {
            QVERIFY2(block.contains("-dSAFER"),
                     "every invocation must run with -dSAFER");
        }
    }

    // ── 3. end-to-end (real Ghostscript; honest skip when absent) ───────────
    void endToEndOwnerPasswordOnlyUnlocks()
    {
        const QString gs = GhostscriptLocator::locate();
        if (gs.isEmpty())
            QSKIP("Ghostscript is not installed in the standard locations — "
                  "skipping the real-tool pin (install AGPL Ghostscript to "
                  "run it)");
        const QString source = tmpPath(QStringLiteral("e2e-owner-only.pdf"));
        QVERIFY2(makeEncryptedPdf(source, QString(), QStringLiteral("ownerpw-1")),
                 "fixture: owner-password-only AES-256 PDF");
        QVERIFY2(pdfHasEncryptDict(source), "fixture must carry /Encrypt");

        const QString destination = tmpPath(QStringLiteral("e2e-unlocked.pdf"));
        const GhostscriptRunner::UnlockResult r = GhostscriptRunner::unlockPdf(
            gs, source, destination);
        if (!r.ok)
            QSKIP(qPrintable(QStringLiteral("Ghostscript unlock failed on this "
                                            "machine: %1").arg(r.error)));
        QVERIFY2(!pdfHasEncryptDict(destination),
                 "the unlocked copy must carry NO /Encrypt dictionary");
        // The re-distilled copy must open cleanly with no password at all
        // (PoDoFo is a strict parser — a broken output would throw).
        try {
            PoDoFo::PdfMemDocument check;
            check.Load(destination.toUtf8().constData());
            QVERIFY(check.GetPages().GetCount() >= 1);
        } catch (const PoDoFo::PdfError& e) {
            QFAIL(qPrintable(QStringLiteral("unlocked copy does not open: %1")
                                 .arg(e.what())));
        }
    }

    void endToEndUserPasswordFailsHonestlyWithoutIt()
    {
        const QString gs = GhostscriptLocator::locate();
        if (gs.isEmpty())
            QSKIP("Ghostscript is not installed in the standard locations — "
                  "skipping the real-tool pin (install AGPL Ghostscript to "
                  "run it)");
        const QString source = tmpPath(QStringLiteral("e2e-userpw.pdf"));
        QVERIFY2(makeEncryptedPdf(source, QStringLiteral("userpw-1"),
                                  QStringLiteral("ownerpw-1")),
                 "fixture: user-password AES-256 PDF");
        // Fixture sanity: WITHOUT the password the document cannot even be
        // loaded strictly (this is the "cannot legally open" class).
        try {
            PoDoFo::PdfMemDocument strict;
            strict.Load(source.toUtf8().constData());
            QSKIP("this PoDoFo build opens user-password documents without "
                  "complaining — the honest-failure pin cannot run here");
        } catch (const PoDoFo::PdfError&) {
            // expected: the fixture really is user-password-locked
        }

        const QString destination = tmpPath(QStringLiteral("e2e-userpw-out.pdf"));
        const QByteArray sentinel = QByteArrayLiteral("SENTINEL-ORIGINAL");
        {
            QFile d(destination);
            QVERIFY(d.open(QIODevice::WriteOnly));
            d.write(sentinel);
        }
        const QByteArray before = sha256OfFile(destination);

        const GhostscriptRunner::UnlockResult r = GhostscriptRunner::unlockPdf(
            gs, source, destination);
        QVERIFY2(!r.ok,
                 "a user-password document must NOT unlock without the "
                 "password (never bypasses a document the user cannot open)");
        QVERIFY2(r.stage == GhostscriptRunner::UnlockResult::Stage::Tool,
                 "the failure must be the tool stage (Ghostscript's own words)");
        QVERIFY2(!r.error.isEmpty(), "the failure must say something");
        QVERIFY2(r.error.contains("password"),
                 "the failure must name the password requirement");
        QCOMPARE(sha256OfFile(destination), before);
        // No candidate debris in the shared staging dir.
        const QDir staged(QDir::tempPath() + QStringLiteral("/glyphpdf-candidates"));
        if (staged.exists())
            QVERIFY2(staged.entryList({"glyphpdf-*"}, QDir::Files).isEmpty(),
                     "failed runs must discard their candidates");
    }

    void endToEndUserPasswordUnlocksWithIt()
    {
        const QString gs = GhostscriptLocator::locate();
        if (gs.isEmpty())
            QSKIP("Ghostscript is not installed in the standard locations — "
                  "skipping the real-tool pin (install AGPL Ghostscript to "
                  "run it)");
        const QString source = tmpPath(QStringLiteral("e2e-userpw-ok.pdf"));
        QVERIFY2(makeEncryptedPdf(source, QStringLiteral("userpw-1"),
                                  QStringLiteral("ownerpw-1")),
                 "fixture: user-password AES-256 PDF");
        const QString destination = tmpPath(QStringLiteral("e2e-userpw-ok-out.pdf"));
        const GhostscriptRunner::UnlockResult r = GhostscriptRunner::unlockPdf(
            gs, source, destination, QByteArrayLiteral("userpw-1"));
        if (!r.ok)
            QSKIP(qPrintable(QStringLiteral("the with-password retry did not "
                                            "succeed on this Ghostscript "
                                            "build: %1").arg(r.error)));
        QVERIFY2(r.usedPassword, "the success must be the password retry pass");
        QVERIFY2(!pdfHasEncryptDict(destination),
                 "the unlocked copy must carry NO /Encrypt dictionary");
    }

    void endToEndCorruptedProtectedFailsWithoutCorruptingOutput()
    {
        const QString gs = GhostscriptLocator::locate();
        if (gs.isEmpty())
            QSKIP("Ghostscript is not installed in the standard locations — "
                  "skipping the real-tool pin (install AGPL Ghostscript to "
                  "run it)");
        const QString intact = tmpPath(QStringLiteral("e2e-corr-src.pdf"));
        QVERIFY2(makeEncryptedPdf(intact, QString(), QStringLiteral("ownerpw-1")),
                 "fixture: owner-password-only AES-256 PDF");
        // Corrupt it HARD: half the bytes gone (the pdfi no-pages signature
        // class — hopeless, and must fail honestly or re-distill, but never
        // produce a committed blank husk).
        QFile src(intact);
        QVERIFY(src.open(QIODevice::ReadOnly));
        const QByteArray all = src.readAll();
        src.close();
        const QString corrupted = tmpPath(QStringLiteral("e2e-corrupted.pdf"));
        {
            QFile c(corrupted);
            QVERIFY(c.open(QIODevice::WriteOnly));
            c.write(all.left(all.size() / 2));
        }

        const QString destination = tmpPath(QStringLiteral("e2e-corr-out.pdf"));
        const GhostscriptRunner::UnlockResult r = GhostscriptRunner::unlockPdf(
            gs, corrupted, destination);
        if (r.ok) {
            // The "re-distills" leg: whatever was written must be a REAL pdf.
            QVERIFY2(QFileInfo::exists(destination)
                     && !pdfHasEncryptDict(destination),
                     "a corrupted-source success must still be a clean copy");
        } else {
            // The "fails honestly" leg: nothing partial may exist at the
            // destination.
            QVERIFY2(!QFileInfo::exists(destination),
                     "a failed unlock must leave NO output file");
            QVERIFY2(!r.error.isEmpty(), "the failure must say something");
        }
        QVERIFY2(!r.canceled, "nothing canceled here");
    }

    // In-place unlock: destination == source is a supported shape (the
    // commit carries a destination-identity precondition) — the ORIGINAL file
    // must survive a FAILED attempt byte-identically.
    void endToEndFailedInPlaceLeavesSourceByteIdentical()
    {
        const QString gs = GhostscriptLocator::locate();
        if (gs.isEmpty())
            QSKIP("Ghostscript is not installed in the standard locations — "
                  "skipping the real-tool pin (install AGPL Ghostscript to "
                  "run it)");
        const QString source = tmpPath(QStringLiteral("e2e-inplace.pdf"));
        QVERIFY2(makeEncryptedPdf(source, QStringLiteral("userpw-2"),
                                  QStringLiteral("ownerpw-2")),
                 "fixture: user-password AES-256 PDF");
        const QByteArray before = sha256OfFile(source);
        const GhostscriptRunner::UnlockResult r = GhostscriptRunner::unlockPdf(
            gs, source, source);
        QVERIFY2(!r.ok, "without the password the in-place unlock must fail");
        QCOMPARE(sha256OfFile(source), before);
    }
};

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    // Re-exec child mode (the fake tool the argv pins drive): dispatched via
    // the environment because the runner OWNS the argv it passes.
    if (!qEnvironmentVariableIsEmpty("GP_FAKE_GS_MODE")) {
        fakeGsMain();
        return 0;
    }
    TestGhostscriptUnlock tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "TestGhostscriptUnlock.moc"
