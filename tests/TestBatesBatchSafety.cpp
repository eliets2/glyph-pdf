// SPDX-License-Identifier: Apache-2.0
// G03 (P1, QUALITY-GATE-2026-09-09) — Bates batch input/output collision safety.
//
// The gate's features/bates-probe reproduction, pinned as a regression suite:
// the old PagesController batch loop removed each output BEFORE validating its
// input, so
//   * a nonexistent input deleted its existing `<stem>_bated.pdf` destination
//     (BATES_MISSING_INPUT_EXISTING_DEST_PRESERVED false), and
//   * with inputs first.pdf + first_bated.pdf, processing the first overwrote
//     the second ORIGINAL (SECOND_SOURCE_MUST_SURVIVE lost; the stale output
//     carried FIRST_SOURCE with two stamps).
// The repaired controller preflights the WHOLE input/output identity set
// (Windows-alias tolerant, outputs distinct from every input and from each
// other) BEFORE touching anything, and processes each file through a uniquely
// owned SafeSave candidate with a checked replacement.
//
// Real MainWindow over the real Bootstrapper context, real dialog flow
// (zero-timeout accept timer — no sleeps), real generated PDFs, offscreen.
// No post-fix-only API is referenced so the binary also builds against the
// pre-fix baseline for revert verification.
#include <QtTest/QtTest>
#include <QApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>
#include <QTimer>
#include <QList>
#include <QPushButton>
#include <QSettings>

#include "GpMainWindow.h"
#include "app/Bootstrapper.h"
#include "core/AppContext.h"
#include "shell/controllers/PagesController.h"
#include "shell/ToolRegistry.h"
#include "ui/BatesNumberingDialog.h"
#include "ui/PdfViewerWidget.h"
#include "engines/pdfium/PdfiumBackend.h"

using gp::MainWindow;

namespace {

// Hand-built one-page PDF with one Helvetica marker line (byte-exact xref —
// the TestPagesMode/TestDocumentIdentity idiom; no QFontDatabase needed).
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

class TestBatesBatchSafety : public QObject {
    Q_OBJECT

    std::unique_ptr<MainWindow> m_win;

    // Run a batch through the REAL controller + REAL dialog: a zero-timeout
    // timer accepts the BatesNumberingDialog with the given batch files while
    // activate() sits in its modal exec (gate probe's exact mechanism).
    void runBatch(const QStringList &files)
    {
        auto *controller = m_win->findChild<gp::PagesController *>();
        if (!controller) {
            // The window owns it by member name in the app; construct against
            // the app context if lookup fails (mirrors the probe).
            static gp::PagesController fallback(m_win->appContext(), m_win.get());
            controller = &fallback;
        }
        QTimer t;
        QObject::connect(&t, &QTimer::timeout, [this, &files]() {
            for (auto *w : QApplication::topLevelWidgets()) {
                if (auto *d = qobject_cast<gp::BatesNumberingDialog *>(w)) {
                    d->setBatchFiles(files);
                    d->accept();
                }
            }
        });
        t.start(5);
        controller->activate(ToolId::BatesNumber);
        t.stop();
    }

private slots:
    void initTestCase()
    {
        QApplication::setQuitOnLastWindowClosed(false);
        QApplication::setAttribute(Qt::AA_DontUseNativeDialogs, true);
        // Isolate QSettings from the user's real profile.
        QCoreApplication::setOrganizationName(QStringLiteral("GlyphPDFTests"));
        QCoreApplication::setApplicationName(QStringLiteral("TestBatesBatchSafety"));
        QSettings::setDefaultFormat(QSettings::IniFormat);
    }

    // Control (passes before and after the repair): a normal two-file batch
    // produces two stamped outputs with ONE continuous sequence and leaves
    // both inputs untouched.
    void normalBatchStampsBothOutputs()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString active = makePdf(dir.filePath("active.pdf"), QStringLiteral("ACTIVE"));
        const QString x = makePdf(dir.filePath("x.pdf"), QStringLiteral("X_SOURCE"));
        const QString y = makePdf(dir.filePath("y.pdf"), QStringLiteral("Y_SOURCE"));
        QVERIFY(!active.isEmpty() && !x.isEmpty() && !y.isEmpty());
        const QByteArray xBefore = readFileBytes(x);
        const QByteArray yBefore = readFileBytes(y);

        m_win = std::make_unique<MainWindow>(Bootstrapper::createContext());
        m_win->openDocument(active);
        QTest::qWait(50);

        runBatch({ x, y });

        const QString xOut = dir.filePath("x_bated.pdf");
        const QString yOut = dir.filePath("y_bated.pdf");
        QVERIFY2(QFile::exists(xOut), "first output must exist after a normal batch");
        QVERIFY2(QFile::exists(yOut), "second output must exist after a normal batch");
        QVERIFY2(textOf(xOut).contains(QStringLiteral("000001")),
                 qPrintable(QStringLiteral("first output must carry 000001; got: %1")
                                .arg(textOf(xOut))));
        QVERIFY2(textOf(yOut).contains(QStringLiteral("000002")),
                 qPrintable(QStringLiteral("second output must continue at 000002; got: %1")
                                .arg(textOf(yOut))));
        QCOMPARE(readFileBytes(x), xBefore);
        QCOMPARE(readFileBytes(y), yBefore);

        m_win.reset();
    }

    // THE G03 reproduction (part 1): a nonexistent input must never delete its
    // existing destination. Pre-fix: DEST_EXISTS false.
    void missingInputPreservesExistingDestination()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString active = makePdf(dir.filePath("active.pdf"), QStringLiteral("ACTIVE"));
        QVERIFY(!active.isEmpty());
        const QString dest = dir.filePath("missing_bated.pdf");
        QVERIFY(!makePdf(dest, QStringLiteral("KEEP_EXISTING_DESTINATION")).isEmpty());
        const QByteArray destBefore = readFileBytes(dest);

        m_win = std::make_unique<MainWindow>(Bootstrapper::createContext());
        m_win->openDocument(active);
        QTest::qWait(50);

        runBatch({ dir.filePath("missing.pdf") });

        QVERIFY2(QFile::exists(dest), "an existing destination must survive a "
                                      "batch whose input does not exist");
        QCOMPARE(readFileBytes(dest), destBefore);
        QVERIFY2(textOf(dest).contains(QStringLiteral("KEEP_EXISTING_DESTINATION")),
                 "the preserved destination must keep its content");

        m_win.reset();
    }

    // THE G03 reproduction (part 2): first.pdf + first_bated.pdf — the first
    // input's default output IS the second input. The batch must be refused
    // before any write: SECOND_SOURCE_MUST_SURVIVE stays byte-identical and
    // no stale first_bated_bated.pdf cascade is produced. Pre-fix the second
    // original was overwritten with FIRST_SOURCE000001.
    void collidingOutputRefusedAndSecondSourceSurvives()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString active = makePdf(dir.filePath("active.pdf"), QStringLiteral("ACTIVE"));
        const QString first = makePdf(dir.filePath("first.pdf"), QStringLiteral("FIRST_SOURCE"));
        const QString second = makePdf(dir.filePath("first_bated.pdf"),
                                       QStringLiteral("SECOND_SOURCE_MUST_SURVIVE"));
        QVERIFY(!active.isEmpty() && !first.isEmpty() && !second.isEmpty());
        const QByteArray secondBefore = readFileBytes(second);

        m_win = std::make_unique<MainWindow>(Bootstrapper::createContext());
        m_win->openDocument(active);
        QTest::qWait(50);

        runBatch({ first, second });

        QVERIFY2(!QFile::exists(dir.filePath("first_bated_bated.pdf")),
                 "a refused batch must not produce cascade outputs");
        QVERIFY2(QFile::exists(second), "the second original must survive");
        QCOMPARE(readFileBytes(second), secondBefore);
        QVERIFY2(textOf(second).contains(QStringLiteral("SECOND_SOURCE_MUST_SURVIVE")),
                 qPrintable(QStringLiteral("second source text must survive; got: %1")
                                .arg(textOf(second))));

        m_win.reset();
    }
};

QTEST_MAIN(TestBatesBatchSafety)
#include "TestBatesBatchSafety.moc"
