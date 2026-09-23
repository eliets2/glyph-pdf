#include <QtTest>
#include <QCoreApplication>
#include <QApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QTemporaryDir>
#include <QImage>
#include <QPainter>
#include <QProcess>

#include "engines/ConversionManager.h"
#include "engines/SafeSave.h"
#include "core/interfaces/IConversionEngine.h"
#include <podofo/podofo.h>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static QString sofficePath()
{
    // Same runtime detection the application uses.
    return ConversionManager::locateSoffice();
}

// sweep-legacy: sentinel/sha helpers (R04 fake-writer idiom).
static QByteArray sha256Of(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(&f);
    return hash.result();
}

static QByteArray sha256OfBytes(const QByteArray& bytes)
{
    return QCryptographicHash::hash(bytes, QCryptographicHash::Sha256);
}

static const QByteArray kPrevOutputSentinel = QByteArrayLiteral("PREVIOUS-CONVERTED-PDF-v1");

static QByteArray plantSentinelBytes(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        qFatal("plantSentinelBytes: cannot write fixture %s", qPrintable(path));
    f.write(kPrevOutputSentinel);
    f.close();
    return kPrevOutputSentinel;
}

static bool writeDummyDocx(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return false;
    f.write("PK");   // minimal ZIP header — soffice never reads it (fake writer)
    f.close();
    return true;
}

// RAII fake-soffice plant: a copy of THIS test binary named soffice.exe in a
// private dir, prepended to PATH so ConversionManager::locateSoffice resolves
// it. The child intercepts --sweep-legacy-fake-soffice in main().
class PlantFakeSoffice {
public:
    PlantFakeSoffice() = default;
    ~PlantFakeSoffice() { restore(); }
    PlantFakeSoffice(const PlantFakeSoffice&) = delete;
    PlantFakeSoffice& operator=(const PlantFakeSoffice&) = delete;

    bool plant(const QString& mode = QStringLiteral("ok"))
    {
        if (!m_dir.isValid())
            return false;
        const QString exe = m_dir.filePath(QStringLiteral("soffice.exe"));
        if (!QFile::copy(QCoreApplication::applicationFilePath(), exe))
            return false;
        if (!setMode(mode))
            return false;
        m_oldPath = qEnvironmentVariable("PATH");
        qputenv("PATH", (m_dir.path() + QLatin1Char(';') + m_oldPath).toLocal8Bit());
        m_planted = true;
        return true;
    }
    // The child reads its mode from mode.txt BESIDE ITS OWN EXE (argv[0]) --
    // no process-environment involvement, so slot ordering cannot leak state.
    bool setMode(const QString& mode)
    {
        QFile f(m_dir.filePath(QStringLiteral("mode.txt")));
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
            return false;
        f.write(mode.toUtf8());
        f.close();
        return true;
    }
    void restore()
    {
        if (m_planted) {
            qputenv("PATH", m_oldPath.toLocal8Bit());
            m_planted = false;
        }
    }

private:
    QTemporaryDir m_dir;
    QString m_oldPath;
    bool m_planted = false;
};


// Create a minimal 3-page synthetic PNG image for testing.
static bool writeTestPng(const QString& path, QColor color, int w = 100, int h = 80)
{
    QImage img(w, h, QImage::Format_RGB888);
    img.fill(color);
    QPainter p(&img);
    p.setPen(Qt::black);
    p.drawText(10, 40, path.section('/', -1));
    p.end();
    return img.save(path, "PNG");
}

// ---------------------------------------------------------------------------
// Test class
// ---------------------------------------------------------------------------

class TestOfficeImport : public QObject {
    Q_OBJECT

private slots:

    // D3 — convertImagesToPdf: 3 PNG inputs → 3-page PDF
    void testImagesToPdf_threePages()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        QStringList pngPaths;
        for (int i = 0; i < 3; ++i) {
            const QString path = tmp.filePath(QString("img%1.png").arg(i));
            QVERIFY(writeTestPng(path, QColor(50 + i * 60, 100, 200)));
            pngPaths << path;
        }

        const QString outPdf = tmp.filePath("out.pdf");
        ConversionManager mgr;
        QVERIFY(mgr.convertImagesToPdf(pngPaths, outPdf));
        QVERIFY(QFileInfo::exists(outPdf));
        QVERIFY(QFileInfo(outPdf).size() > 0);

        // Verify page count via PoDoFo
        try {
            PoDoFo::PdfMemDocument doc;
            doc.Load(outPdf.toUtf8().constData());
            QCOMPARE(static_cast<int>(doc.GetPages().GetCount()), 3);

            // Each page should have at least one XObject in its resources
            for (unsigned pg = 0; pg < doc.GetPages().GetCount(); ++pg) {
                const PoDoFo::PdfPage& page = doc.GetPages().GetPageAt(pg);
                const PoDoFo::PdfDictionary& dict = page.GetDictionary();
                auto* res = dict.FindKey("Resources");
                bool hasXObj = false;
                if (res && res->GetDictionary().HasKey("XObject"))
                    hasXObj = true;
                QVERIFY2(hasXObj,
                    qPrintable(QString("Page %1 has no XObject resource").arg(pg)));
            }
        } catch (const std::exception& e) {
            QFAIL(qPrintable(QString("PoDoFo load failed: %1").arg(e.what())));
        }
    }

    // D3 — empty input list should return false gracefully
    void testImagesToPdf_emptyList()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        ConversionManager mgr;
        QVERIFY(!mgr.convertImagesToPdf({}, tmp.filePath("out.pdf")));
    }

    // D2 — OfficeToPdf via convertTo: QSKIP when soffice unavailable
    void testOfficeToPdf_realConversion()
    {
        const QString soffice = sofficePath();
        if (soffice.isEmpty() || !QFileInfo::exists(soffice)) {
            QSKIP("LibreOffice (soffice) not available at runtime — skipping real-conversion test");
        }

        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        // Write a trivial RTF document (no binary fixtures needed)
        const QString rtfPath = tmp.filePath("test.rtf");
        QFile f(rtfPath);
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
        f.write("{\\rtf1\\ansi\\deff0 {\\fonttbl {\\f0 Times New Roman;}} "
                "{\\pard Hello World\\par}}");
        f.close();

        const QString outPdf = tmp.filePath("test.pdf");
        ConversionManager mgr;
        const bool ok = mgr.convertTo(rtfPath, outPdf, IConversionEngine::TargetFormat::OfficeToPdf);
        QVERIFY2(ok, "OfficeToPdf conversion should succeed with a valid RTF input");
        QVERIFY(QFileInfo::exists(outPdf));
        QVERIFY(QFileInfo(outPdf).size() > 0);

        // Verify output is parseable as PDF
        try {
            PoDoFo::PdfMemDocument doc;
            doc.Load(outPdf.toUtf8().constData());
            QVERIFY(doc.GetPages().GetCount() >= 1);
        } catch (const std::exception& e) {
            QFAIL(qPrintable(QString("Output PDF not valid: %1").arg(e.what())));
        }
    }

    // D2 — OfficeToPdf with non-existent file returns false gracefully
    void testOfficeToPdf_missingFile()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        ConversionManager mgr;
        // File does not exist — should return false without crashing
        const bool ok = mgr.convertTo("/nonexistent/path/test.docx",
                                      tmp.filePath("out.pdf"),
                                      IConversionEngine::TargetFormat::OfficeToPdf);
        QVERIFY2(!ok, "convertTo(OfficeToPdf) should return false for missing input");
    }

    // D3 — Guard test: Ensure no global soffice taskkill in ConversionManager.cpp
    void testOfficeToPdf_NoGlobalTaskkill()
    {
        // Simple static analysis of the source file to ensure the blanket taskkill was removed
        QFile sourceFile("../../src/engines/ConversionManager.cpp");
        if (sourceFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QString content = QString::fromUtf8(sourceFile.readAll());
            QVERIFY2(!content.contains("\"taskkill\", {\"/F\", \"/IM\", \"soffice.bin\""), 
                     "ConversionManager.cpp must not contain a global soffice taskkill");
        }
    }

    // D2 — OfficeToPdf returns false gracefully when no converter is present
    void testOfficeToPdf_noLibreOffice()
    {
        if (ConversionManager::isOfficeImportAvailable())
            QSKIP("A LibreOffice converter is available — cannot exercise the no-converter path");

        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        // Create a dummy .docx file
        const QString docxPath = tmp.filePath("test.docx");
        QFile f(docxPath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("PK");   // minimal ZIP header
        f.close();

        ConversionManager mgr;
        const bool ok = mgr.convertTo(docxPath, tmp.filePath("out.pdf"),
                                      IConversionEngine::TargetFormat::OfficeToPdf);
        QVERIFY2(!ok, "OfficeToPdf should return false when no soffice converter is found");
    }

    // ── sweep-legacy: the destination-commit tail of convertOfficeToPdf ────────
    //
    // The legacy tail QFile::remove(outputPath) BEFORE QFile::rename(expectedOut,
    // outputPath): whenever the rename failed after the remove (converter exits 0
    // but writes nothing, an open handle on the source, a cross-volume rename),
    // the PREVIOUS output at the caller's destination was DESTROYED and the
    // function reported false — the exact destructive class WP-R04 (A03) fixed
    // for the encrypted-package flow. The repair validates the converter's
    // product and commits it through SafeSave::commitFileToDestination (atomic
    // replace; the destination is never removed or truncated before commit).
    //
    // The pins drive the REAL convertOfficeToPdf with a re-exec'd fake soffice
    // (this binary, fake-soffice child mode in main()) planted on
    // PATH — no LibreOffice installation is involved.

    // The decisive data-loss repro: the converter "succeeds" (exit 0) but
    // produces no file; the operation must fail with the previous output
    // byte-identical. Pre-fix: the tail removed the destination first, the
    // rename then failed on the missing source — sentinel gone, false returned.
    void officeConvertNoOutputKeepsPreviousDestination()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        PlantFakeSoffice fake;
        QVERIFY(fake.plant(QStringLiteral("silent")));

        const QString dest = tmp.filePath("report.pdf");
        const QByteArray sentinel = plantSentinelBytes(dest);

        const QString docx = tmp.filePath("input.docx");
        QVERIFY(writeDummyDocx(docx));

        QVERIFY(fake.setMode(QStringLiteral("silent")));
        ConversionManager mgr;
        const bool ok = mgr.convertTo(docx, dest,
                                      IConversionEngine::TargetFormat::OfficeToPdf);

        QVERIFY2(!ok, "a converter run that produced no output must fail");
        // THE anchor: the previous output survives the failed run.
        QVERIFY2(QFileInfo::exists(dest),
                 "DATA LOSS: the previous output was destroyed by the failed conversion");
        QCOMPARE(sha256Of(dest), sha256OfBytes(sentinel));
    }

    // The SafeSave commit seam must be the ONE boundary the tail commits
    // through: an injected commit fault leaves the destination byte-identical
    // (pre-fix the tail never routed through SafeSave and "succeeded" over the
    // sentinel with the fault armed — the boundary wasn't engaged).
    void officeConvertCommitFaultKeepsDestinationByteIdentical()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        PlantFakeSoffice fake;
        QVERIFY(fake.plant(QStringLiteral("ok")));

        const QString dest = tmp.filePath("report.pdf");
        const QByteArray sentinel = plantSentinelBytes(dest);
        const QString docx = tmp.filePath("input.docx");
        QVERIFY(writeDummyDocx(docx));

        gp::SafeSave::setCommitFaultForTesting(
            gp::SafeSave::CommitFaultForTesting::FailBeforeCommit);
        ConversionManager mgr;
        const bool ok = mgr.convertTo(docx, dest,
                                      IConversionEngine::TargetFormat::OfficeToPdf);
        gp::SafeSave::setCommitFaultForTesting(gp::SafeSave::CommitFaultForTesting::None);

        QVERIFY2(!ok, "with the commit fault armed the conversion must report failure");
        QVERIFY2(QFileInfo::exists(dest), "destination must survive a failed commit");
        QCOMPARE(sha256Of(dest), sha256OfBytes(sentinel));

        // Retry without the fault: the SAME run now succeeds and the sentinel
        // is legitimately replaced by the converter's product.
        QVERIFY(fake.setMode(QStringLiteral("ok")));
        const bool retry = mgr.convertTo(docx, dest,
                                         IConversionEngine::TargetFormat::OfficeToPdf);
        QVERIFY2(retry, "retry after disarming the fault must succeed");
        QVERIFY(QFileInfo(dest).size() > 0);
        QCOMPARE(sha256Of(dest) != sha256OfBytes(sentinel), true);
    }

    // Control: a valid conversion replaces the destination with a loadable PDF,
    // and the converter's product beside the destination is consumed by the
    // commit (outDir == the destination's directory here) — no stray copy.
    void officeConvertSuccessReplacesDestinationAndCleansCandidate()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        PlantFakeSoffice fake;
        QVERIFY(fake.plant(QStringLiteral("ok")));

        const QString dest = tmp.filePath("report.pdf");
        plantSentinelBytes(dest);
        const QString docx = tmp.filePath("input.docx");
        QVERIFY(writeDummyDocx(docx));

        ConversionManager mgr;
        const bool ok = mgr.convertTo(docx, dest,
                                      IConversionEngine::TargetFormat::OfficeToPdf);
        QVERIFY2(ok, "a valid soffice product must be committed");

        // The destination holds the converter's product, verified through
        // PoDoFo (independent of the writer).
        QVERIFY(QFileInfo(dest).size() > 0);
        try {
            PoDoFo::PdfMemDocument doc;
            doc.Load(dest.toUtf8().constData());
            QCOMPARE(static_cast<int>(doc.GetPages().GetCount()), 1);
        } catch (const std::exception& e) {
            QFAIL(qPrintable(QString("committed output is not a loadable PDF: %1").arg(e.what())));
        }
        QCOMPARE(QFileInfo(tmp.filePath("input.pdf")).exists(), false);
    }
};

#include "TestOfficeImport.moc"

// ── sweep-legacy: re-exec'd fake soffice (the R04 fake-writer pattern) ───────
static QByteArray fakeSofficeProductBytes()
{
    // Minimal valid 1-page PDF (hand-built, with xref) — loadable by PoDoFo.
    QByteArray out = "%PDF-1.4\n";
    QList<qint64> off;
    auto addObj = [&](const QByteArray& body) {
        off.append(out.size());
        out += QByteArray::number(off.size()) + " 0 obj\n" + body + "\nendobj\n";
    };
    addObj("<</Type/Catalog/Pages 2 0 R>>");
    addObj("<</Type/Pages/Kids[3 0 R]/Count 1>>");
    addObj("<</Type/Page/Parent 2 0 R/MediaBox[0 0 612 792]>>");
    const qint64 xref = out.size();
    out += QString("xref\n0 %1\n").arg(off.size() + 1).toLatin1();
    out += "0000000000 65535 f \n";
    for (qint64 o : off)
        out += QString("%1 00000 n \n").arg(o, 10, 10, QChar('0')).toLatin1();
    out += QString("trailer<</Size %1/Root 1 0 R>>\nstartxref\n%2\n%%EOF\n")
               .arg(off.size() + 1).arg(xref).toLatin1();
    return out;
}

static int fakeSofficeChildMain(int argc, char** argv)
{
    QString mode;
    {
        const QString modeFile = QDir(QFileInfo(QString::fromLocal8Bit(argv[0])).absolutePath())
                                     .filePath(QStringLiteral("mode.txt"));
        QFile f(modeFile);
        if (f.open(QIODevice::ReadOnly))
            mode = QString::fromUtf8(f.readAll()).trimmed();
    }
    if (mode == QLatin1String("fail"))
        return 3;
    if (mode != QLatin1String("ok"))
        return 0;   // "silent": phantom success, no product

    // Parse the production command line: ... --outdir <dir> <officePath>
    QString outDir;
    for (int i = 0; i < argc; ++i) {
        if (qstrcmp(argv[i], "--outdir") == 0 && i + 1 < argc)
            outDir = QString::fromLocal8Bit(argv[i + 1]);
    }
    const QString input = QString::fromLocal8Bit(argv[argc - 1]);
    if (outDir.isEmpty() || input.isEmpty())
        return 4;
    const QString product = QDir(outDir).filePath(
        QFileInfo(input).completeBaseName() + ".pdf");
    QFile f(product);
    if (!f.open(QIODevice::WriteOnly))
        return 4;
    f.write(fakeSofficeProductBytes());
    f.close();
    return 0;
}

int main(int argc, char *argv[])
{
    // The fake soffice is identified by the PRODUCTION command-line shape
    // (convertOfficeToPdf always passes --headless); the mode comes through
    // the inherited GLYPHPDF_FAKE_SOFFICE_MODE environment.
    for (int i = 0; i < argc; ++i) {
        if (qstrcmp(argv[i], "--headless") == 0)
            return fakeSofficeChildMain(argc, argv);
    }
    QApplication app(argc, argv);
    TestOfficeImport tc;
    return QTest::qExec(&tc, argc, argv);
}
