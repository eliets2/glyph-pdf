// SPDX-License-Identifier: Apache-2.0
// G05 (P1, QUALITY-GATE-2026-09-09) — recovered-document Save commits to the
// original destination and the close handler verifies the contract.
//
// The gate's architecture/probe reproduction (RECOVERY_SAVE_OUTCOME /
// RECOVERY_CLOSE_ACCEPTED), pinned as a regression suite. Pre-fix recovery
// left the session path at the original while viewer/editor held
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
// Real MainWindow over the real Bootstrapper context, real generated PDFs,
// offscreen. No post-fix-only API is referenced (revert-verification safe).
#include <QtTest/QtTest>
#include <QApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>
#include <QCloseEvent>
#include <QSettings>

#include "GpMainWindow.h"
#include "app/Bootstrapper.h"
#include "core/AppContext.h"
#include "shell/controllers/HomeController.h"
#include "engines/DocumentSession.h"
#include "ui/PdfViewerWidget.h"
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

private slots:
    void initTestCase()
    {
        QApplication::setQuitOnLastWindowClosed(false);
        QApplication::setAttribute(Qt::AA_DontUseNativeDialogs, true);
        QCoreApplication::setOrganizationName(QStringLiteral("GlyphPDFTests"));
        QCoreApplication::setApplicationName(QStringLiteral("TestRecoverySave"));
        QSettings::setDefaultFormat(QSettings::IniFormat);

        // QUALITY-GATE-2026-09-09 infra pin (the "TestReadOnlyGate fails in
        // full suites with zero output / full-suite-only failures" family):
        // this suite's MainWindow ctor runs findOrphanedAutosaves() over the
        // PERSISTED recents and pops a RecoveryDialog for each recent whose
        // .autosave.pdf is newer. recoverDocument() records its temp paths in
        // recents, and ONE interrupted run (killed process, loader failure,
        // crash) leaves its QTemporaryDir — with that newer autosave — behind.
        // Every later suite run then blocks on a modal nobody dismisses
        // (reproduced here: 300 s test-function timeout, deterministic once
        // poisoned). This suite never relies on recents, so start clean.
        QSettings settings;
        settings.remove(QStringLiteral("recentFiles"));
    }

    // THE G05 reproduction: recover → Save → close.
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

        MainWindow win(Bootstrapper::createContext());
        const auto *ctx = win.appContext();
        QVERIFY(ctx && ctx->document && ctx->pdfEditor);

        win.recoverDocument(original);
        QTest::qWait(50);
        // Recovery state: session on the ORIGINAL, inputs on the recovery copy.
        QCOMPARE(ctx->document->path(), original);
        QCOMPARE(win.pdfViewer()->filePath(), recovery);
        QVERIFY(ctx->document->isDirty());

        gp::HomeController home(ctx, &win);
        const auto outcome = home.saveNow();
        QCOMPARE(outcome, gp::HomeController::SaveOutcome::Saved);

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
    // silent data destruction either way.
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

        MainWindow win(Bootstrapper::createContext());
        win.recoverDocument(original);
        QTest::qWait(50);
        // No Save: the original must remain byte-identical on disk.
        QCOMPARE(readFileBytes(original), originalBefore);
        QVERIFY(QFile::exists(original + ".autosave.pdf"));
    }
};

QTEST_MAIN(TestRecoverySave)
#include "TestRecoverySave.moc"
