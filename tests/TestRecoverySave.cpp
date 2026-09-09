// SPDX-License-Identifier: Apache-2.0
// G05 (P1, QUALITY-GATE-2026-09-09) — recovered-document Save commits to the
// original destination and the close handler verifies the contract.
//
// The gate's architecture/probe reproduction (RECOVERY_SAVE_OUTCOME /
// RECOVERY_CLOSE_ACCEPTED), pinned as a regression suite. Pre-fix recovery
// left the session path at the original while viewer/engine held
// `<original>.autosave.pdf`; Save used the viewer's path, returned Saved, and
// left the ORIGINAL byte-identical (still ORIGINAL_BEFORE_RECOVERY), the
// session dirty — and the close handler accepted anyway.
//
// Contract pinned here: a successful Save of a recovered document
//   * commits the recovered content to the selected/original destination,
//   * synchronizes viewer, engine and session onto the committed original,
//   * clears dirty for that committed revision, and
//   * consumes the recovery copy,
// so a close-after-Save accepts against a truthful state.
//
// Merge-break integration contract (2026-09-09): the STARTUP orphan-recovery
// prompt must never fire for the LIVE recovery pair. recoverDocument()
// publishes the original to recents while the recovery input
// (`<original>.autosave.pdf`) is newer than the original BY CONSTRUCTION, so
// the deferred startup detector flagged the very document being recovered and
// popped a modal over the active session (a hang in a harness that does not
// dismiss it; the pre-fix misfire was mtime-racy because NTFS quantization
// sometimes made the fixture timestamps equal). The fixture backdates the
// original so "autosave strictly newer" is DETERMINISTIC, and the dismissal
// watchdog records whether any startup prompt listed the live pair.
//
// Real MainWindow over the real Bootstrapper context, real generated PDFs,
// offscreen. No post-fix-only API is referenced (revert-verification safe).
#include <QtTest/QtTest>
#include <QApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>
#include <QCloseEvent>
#include <QDateTime>
#include <QSettings>
#include <QTimer>

#include "GpMainWindow.h"
#include "app/Bootstrapper.h"
#include "core/AppContext.h"
#include "shell/controllers/HomeController.h"
#include "engines/DocumentSession.h"
#include "ui/PdfViewerWidget.h"
#include "ui/RecoveryDialog.h"
#include "engines/pdfium/PdfiumBackend.h"

using gp::MainWindow;

namespace {

// Hand-built one-page PDF with one Helvetica marker line (byte-exact xref).
QString makePdf(const QString &path, const QString &marker)
{
    QByteArray lit = marker.toLatin1();
    lit.replace('\\', "\\\\").replace('(', "\\(").replace(')', "\\)");
    const QByteArray content = "BT /F1 12 Tf 72 720 Td (" + lit + ") Tj ET\n";
    QByteArray pdf = "%PDF-1.4\n";
    QList<qint64> offsets;
    offsets.append(pdf.size());
    pdf += "1 0 obj<</Type/Catalog/Pages 2 0 R>>endobj\n";
    offsets.append(pdf.size());
    pdf += "2 0 obj<</Type/Pages/Kids[3 0 R]/Count 1>>endobj\n";
    offsets.append(pdf.size());
    pdf += "3 0 obj<</Type/Page/Parent 2 0 R/MediaBox[0 0 612 792]/Contents 4 0 R"
           "/Resources<</Font<</F1 5 0 R>>>>>>endobj\n";
    offsets.append(pdf.size());
    pdf += "4 0 obj<</Length " + QByteArray::number(content.size())
         + ">>stream\n" + content + "endstream endobj\n";
    offsets.append(pdf.size());
    pdf += "5 0 obj<</Type/Font/Subtype/Type1/BaseFont/Helvetica/Encoding/WinAnsiEncoding>>endobj\n";
    const qint64 xref = pdf.size();
    pdf += "xref\n0 6\n0000000000 65535 f \n";
    for (qint64 off : offsets)
        pdf += QByteArray::number(static_cast<qulonglong>(off))
                   .rightJustified(10, '0') + " 00000 n \n";
    pdf += "trailer<</Size 6/Root 1 0 R>>\nstartxref\n"
         + QByteArray::number(static_cast<qulonglong>(xref)) + "\n%%EOF\n";
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly) || f.write(pdf) != pdf.size()) return {};
    return path;
}

QByteArray readFileBytes(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    return f.readAll();
}

QString textOf(const QString &pdf)
{
    PdfiumBackend reader;
    if (!reader.loadDocument(pdf)) return QStringLiteral("<load failed>");
    QString text;
    for (const auto &run : reader.extractPageTextRuns(0)) text += run.text;
    return text;
}

} // namespace

class TestRecoverySave : public QObject {
    Q_OBJECT

    // Dismisses any STARTUP recovery prompt (RecoveryDialog) that a previous
    // killed run's leftover pair may legitimately trigger. "Decide Later"
    // touches no files. The dismissal RECORDS whether a prompt ever listed
    // the CURRENT test's live recovery pair — that is the integration bug.
    QTimer *m_modalDismiss = nullptr;
    bool m_livePairFlagged = false;
    QString m_livePairOriginal;

    void beginLivePair(const QString &originalPath)
    {
        m_livePairFlagged = false;
        m_livePairOriginal = originalPath;
    }

    void endLivePair()
    {
        m_livePairOriginal.clear();
    }

private slots:
    void initTestCase()
    {
        QApplication::setQuitOnLastWindowClosed(false);
        QApplication::setAttribute(Qt::AA_DontUseNativeDialogs, true);
        QCoreApplication::setOrganizationName(QStringLiteral("GlyphPDFTests"));
        QCoreApplication::setApplicationName(QStringLiteral("TestRecoverySave"));
        QSettings::setDefaultFormat(QSettings::IniFormat);

        // gateC (QUALITY-GATE-2026-09-09) insurance on top of the upstream
        // hardening: this suite never relies on recents, so start clean — a
        // cross-run leftover pair (dirs survive interrupted runs) must not
        // reach the startup prompt at all.
        {
            QSettings settings;
            settings.remove(QStringLiteral("recentFiles"));
        }

        m_modalDismiss = new QTimer(this);
        m_modalDismiss->setInterval(10);
        connect(m_modalDismiss, &QTimer::timeout, this, [this] {
            for (auto *w : QApplication::topLevelWidgets()) {
                if (auto *dlg = qobject_cast<RecoveryDialog *>(w)) {
                    // selectedFiles() returns ALL listed orphans (every item
                    // defaults to checked) — the honest record of what the
                    // startup detector flagged.
                    const QStringList listed = dlg->selectedFiles();
                    if (!m_livePairOriginal.isEmpty()
                        && listed.contains(m_livePairOriginal))
                        m_livePairFlagged = true;
                    qWarning() << "TestRecoverySave: dismissing a startup "
                                  "recovery prompt listing" << listed;
                    dlg->done(RecoveryDialog::Later);   // touches no files
                }
            }
        });
        m_modalDismiss->start();
    }

    void cleanupTestCase()
    {
        if (m_modalDismiss) m_modalDismiss->stop();
    }

    // THE G05 reproduction: recover → Save → close.
    // ALSO the merge-break integration regression: with the fixture pair's
    // original backdated (recovery pairs are autosave-newer BY DEFINITION),
    // the pre-fix startup orphan detector flagged the LIVE pair and popped a
    // modal over the active recovery session. Post-fix it must never fire.
    void recoverySaveCommitsToOriginalAndCloseVerifies()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString original = makePdf(dir.filePath("recover.pdf"),
                                         QStringLiteral("ORIGINAL_BEFORE_RECOVERY"));
        QVERIFY(!original.isEmpty());
        const QString recovery = makePdf(original + ".autosave.pdf",
                                         QStringLiteral("RECOVERED_NEW_CONTENT"));
        QVERIFY(!recovery.isEmpty());
        const QByteArray originalBefore = readFileBytes(original);

        // Deterministic misfire precondition: the recovery input is strictly
        // newer than the original (backdate the original by a minute).
        {
            QFile originalFile(original);
            // ReadWrite: setFileTime() requires a handle with write access
            // (no truncate flag — the fixture bytes are preserved).
            QVERIFY(originalFile.open(QIODevice::ReadWrite));
            QVERIFY(originalFile.setFileTime(
                QDateTime::currentDateTimeUtc().addSecs(-60),
                QFileDevice::FileModificationTime));
            originalFile.close();
        }

        MainWindow win(Bootstrapper::createContext());
        const auto *ctx = win.appContext();
        QVERIFY(ctx && ctx->document && ctx->pdfEditor);

        beginLivePair(original);
        win.recoverDocument(original);
        QTest::qWait(120);   // let the deferred startup orphan check run

        // THE integration assertion: no startup prompt may have listed the
        // live recovery pair (a Discard dismissal would even have deleted
        // the recovery input out from under the active session).
        QVERIFY2(!m_livePairFlagged,
                 "the startup orphan-recovery prompt must never fire for the "
                 "live recovery pair");
        QVERIFY2(QFile::exists(recovery),
                 "the live recovery input must not be touched by a startup prompt");

        // Recovery state: session on the ORIGINAL, inputs on the recovery copy.
        QCOMPARE(ctx->document->path(), original);
        QCOMPARE(win.pdfViewer()->filePath(), recovery);
        QVERIFY(ctx->document->isDirty());

        gp::HomeController home(ctx, &win);
        const auto outcome = home.saveNow();
        QCOMPARE(outcome, gp::HomeController::SaveOutcome::Saved);
        endLivePair();

        // One recovered-document identity: viewer/engine/session all describe
        // the committed ORIGINAL (pre-fix the viewer stayed on the side file).
        QCOMPARE(ctx->document->path(), original);
        QCOMPARE(win.pdfViewer()->filePath(), original);

        // The recovered content was committed to the destination: the original
        // changed on disk and now carries RECOVERED_NEW_CONTENT (pre-fix it
        // stayed byte-identical with ORIGINAL_BEFORE_RECOVERY).
        QVERIFY2(readFileBytes(original) != originalBefore,
                 "a successful recovery Save must update the original");
        QVERIFY2(textOf(original).contains(QStringLiteral("RECOVERED_NEW_CONTENT")),
                 qPrintable(QStringLiteral("original must carry the recovered content; got: %1")
                                .arg(textOf(original))));

        // Dirty cleared for the committed revision; recovery copy consumed.
        QVERIFY2(!ctx->document->isDirty(),
                 "the session must be clean after a truthful committed Save");
        QVERIFY2(!QFile::exists(recovery),
                 "the recovery copy is consumed once its content is committed");

        // The close handler verifies the checked outcome: with the committed
        // revision anchored and the session clean, close is accepted.
        QCloseEvent close;
        QApplication::sendEvent(&win, &close);
        QVERIFY2(close.isAccepted(),
                 "close after a committed recovery Save must be accepted");
    }

    // Control: recovery followed by Discard-at-close keeps the original on
    // disk untouched (the un-committed recovery copy stays available) — no
    // silent data destruction either way. The live-pair prompt suppression
    // applies here too.
    void unrecoveredOriginalStaysUntouched()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString original = makePdf(dir.filePath("keep.pdf"),
                                         QStringLiteral("ORIGINAL_BEFORE_RECOVERY"));
        QVERIFY(!original.isEmpty());
        QVERIFY(!makePdf(original + ".autosave.pdf",
                         QStringLiteral("RECOVERED_NEW_CONTENT")).isEmpty());
        const QByteArray originalBefore = readFileBytes(original);
        {
            QFile originalFile(original);
            // ReadWrite: setFileTime() requires a handle with write access
            // (no truncate flag — the fixture bytes are preserved).
            QVERIFY(originalFile.open(QIODevice::ReadWrite));
            QVERIFY(originalFile.setFileTime(
                QDateTime::currentDateTimeUtc().addSecs(-60),
                QFileDevice::FileModificationTime));
            originalFile.close();
        }

        MainWindow win(Bootstrapper::createContext());
        beginLivePair(original);
        win.recoverDocument(original);
        QTest::qWait(120);

        QVERIFY2(!m_livePairFlagged,
                 "the startup orphan-recovery prompt must never fire for the "
                 "live recovery pair");
        // No Save: the original must remain byte-identical on disk.
        QCOMPARE(readFileBytes(original), originalBefore);
        QVERIFY(QFile::exists(original + ".autosave.pdf"));
        endLivePair();
    }
};

QTEST_MAIN(TestRecoverySave)
#include "TestRecoverySave.moc"
