// SPDX-License-Identifier: MIT
// W2BProbeOfficeSave.cpp — SWEEP-W2B INDEPENDENT verifier probe (SL2).
//
// Written by the W2B guarantee-verification-engine against the SAVED tip
// (feat/sweep-w2-verify-b @ 2d29a16). The R04 re-exec'd fake-writer pattern
// (this binary copied to soffice.exe, planted on PATH; the child reads its
// mode from a file beside argv[0]) with MY OWN sentinel bytes and — new
// against the fix lane's suite — an EXIT-0-GARBAGE mode: the converter
// "succeeds" and writes a NON-PDF product. The fix's product validation
// (readable + %PDF header BEFORE any destination touch) must refuse it
// honestly and leave the previous output byte-identical.
//
// Contract (23bc970): convertOfficeToPdf validates the product first and
// commits through SafeSave — a failed or refused run NEVER touches the
// destination; a successful run replaces it and consumes the candidate.
//
// NC base: 23bc970^ (the tail removed the destination before renaming).

#include <QtTest>
#include <QApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QTemporaryDir>

#include "engines/ConversionManager.h"
#include "engines/SafeSave.h"
#include "core/interfaces/IConversionEngine.h"
#include <podofo/podofo.h>

namespace {

QByteArray sha256Of(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(&f);
    return hash.result();
}

QByteArray sha256OfBytes(const QByteArray& bytes)
{
    return QCryptographicHash::hash(bytes, QCryptographicHash::Sha256);
}

// MY sentinel (different bytes than the fix lane's suite).
static const QByteArray kSentinel =
    QByteArrayLiteral("W2B-PREVIOUS-OUTPUT-SENTINEL-do-not-destroy-2026-09-20");

QByteArray plantSentinel(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        qFatal("plantSentinel: cannot write %s", qPrintable(path));
    f.write(kSentinel);
    f.close();
    return kSentinel;
}

bool writeDummyDocx(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return false;
    f.write("PK");   // minimal ZIP header — the fake soffice never reads it
    f.close();
    return true;
}

// RAII fake-soffice plant.
class PlantFakeSoffice {
public:
    PlantFakeSoffice() = default;
    ~PlantFakeSoffice() { restore(); }
    PlantFakeSoffice(const PlantFakeSoffice&) = delete;
    PlantFakeSoffice& operator=(const PlantFakeSoffice&) = delete;

    bool plant()
    {
        if (!m_dir.isValid()) return false;
        const QString exe = m_dir.filePath(QStringLiteral("soffice.exe"));
        if (!QFile::copy(QCoreApplication::applicationFilePath(), exe)) return false;
        m_oldPath = qEnvironmentVariable("PATH");
        qputenv("PATH", (m_dir.path() + QLatin1Char(';') + m_oldPath).toLocal8Bit());
        m_planted = true;
        return setMode(QStringLiteral("ok"));
    }
    bool setMode(const QString& mode)
    {
        QFile f(m_dir.filePath(QStringLiteral("mode.txt")));
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
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

} // namespace

class W2BProbeOfficeSave : public QObject
{
    Q_OBJECT

private slots:
    // ── phantom success (exit 0, no product): destination survives ─────────
    void silentRunKeepsPreviousDestinationByteIdentical()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        PlantFakeSoffice fake;
        QVERIFY(fake.plant());
        QVERIFY(fake.setMode(QStringLiteral("silent")));

        const QString dest = tmp.filePath("out.pdf");
        const QByteArray sentinel = plantSentinel(dest);
        const QString docx = tmp.filePath("in.docx");
        QVERIFY(writeDummyDocx(docx));

        ConversionManager mgr;
        const bool ok = mgr.convertTo(docx, dest,
                                      IConversionEngine::TargetFormat::OfficeToPdf);
        QVERIFY2(!ok, "a run that produced no output must report failure");
        QVERIFY2(QFileInfo::exists(dest),
                 "SL2 REGRESSION: DATA LOSS — the failed run destroyed the "
                 "previous output");
        QCOMPARE(sha256Of(dest), sha256OfBytes(sentinel));
    }

    // ── MY decisive addition: exit-0 GARBAGE product is refused ────────────
    // Pre-fix, the tail validated only exists/non-empty (after already
    // REMOVING the destination): a garbage product was renamed into place and
    // the run reported SUCCESS — a destroyed output plus a false claim.
    void garbageProductRefusedAndDestinationSurvives()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        PlantFakeSoffice fake;
        QVERIFY(fake.plant());
        QVERIFY(fake.setMode(QStringLiteral("garbage")));

        const QString dest = tmp.filePath("out.pdf");
        const QByteArray sentinel = plantSentinel(dest);
        const QString docx = tmp.filePath("in.docx");
        QVERIFY(writeDummyDocx(docx));

        ConversionManager mgr;
        const bool ok = mgr.convertTo(docx, dest,
                                      IConversionEngine::TargetFormat::OfficeToPdf);
        QVERIFY2(!ok,
                 "SL2 REGRESSION: an exit-0 run whose product has no %PDF "
                 "header was committed over the destination — the phantom "
                 "success is back");
        QVERIFY2(QFileInfo::exists(dest),
                 "the refused product must never replace the previous output");
        QCOMPARE(sha256Of(dest), sha256OfBytes(sentinel));
    }

    // ── commit-fault injection: the tail MUST route through SafeSave ────────
    void commitFaultLeavesDestinationByteIdentical()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        PlantFakeSoffice fake;
        QVERIFY(fake.plant());
        QVERIFY(fake.setMode(QStringLiteral("ok")));

        const QString dest = tmp.filePath("out.pdf");
        const QByteArray sentinel = plantSentinel(dest);
        const QString docx = tmp.filePath("in.docx");
        QVERIFY(writeDummyDocx(docx));

        gp::SafeSave::setCommitFaultForTesting(
            gp::SafeSave::CommitFaultForTesting::FailBeforeCommit);
        ConversionManager mgr;
        const bool ok = mgr.convertTo(docx, dest,
                                      IConversionEngine::TargetFormat::OfficeToPdf);
        gp::SafeSave::setCommitFaultForTesting(gp::SafeSave::CommitFaultForTesting::None);

        QVERIFY2(!ok, "with the commit fault armed the run must report failure");
        QVERIFY2(QFileInfo::exists(dest), "destination must survive a failed commit");
        QCOMPARE(sha256Of(dest), sha256OfBytes(sentinel));

        // Retry with the fault disarmed: the sentinel is legitimately replaced.
        QVERIFY(mgr.convertTo(docx, dest, IConversionEngine::TargetFormat::OfficeToPdf));
        QVERIFY(sha256Of(dest) != sha256OfBytes(sentinel));
    }

    // ── control: a valid product replaces the destination, cleanly ─────────
    void successReplacesDestinationAndCleansCandidate()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        PlantFakeSoffice fake;
        QVERIFY(fake.plant());
        QVERIFY(fake.setMode(QStringLiteral("ok")));

        const QString dest = tmp.filePath("out.pdf");
        plantSentinel(dest);
        const QString docx = tmp.filePath("in.docx");
        QVERIFY(writeDummyDocx(docx));

        ConversionManager mgr;
        QVERIFY(mgr.convertTo(docx, dest, IConversionEngine::TargetFormat::OfficeToPdf));

        // Independent read: the destination is a loadable PDF.
        try {
            PoDoFo::PdfMemDocument d;
            d.Load(dest.toUtf8().constData());
            QCOMPARE(static_cast<int>(d.GetPages().GetCount()), 1);
        } catch (const std::exception& e) {
            QFAIL(qPrintable(QString("committed output is not a loadable PDF: %1").arg(e.what())));
        }
        // The converter's product beside the destination was consumed.
        QCOMPARE(QFileInfo(tmp.filePath("in.pdf")).exists(), false);
    }
};

// ── the re-exec'd fake soffice child ─────────────────────────────────────────
static QByteArray fakeProductBytes()
{
    // Minimal valid 1-page PDF (hand-built xref), loadable by PoDoFo.
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
    if (mode == QLatin1String("fail")) return 3;
    if (mode == QLatin1String("garbage")) {
        // Parse the command line to find the product location, then write
        // NON-PDF bytes (the phantom-success-with-garbage shape).
        QString outDir;
        for (int i = 0; i < argc; ++i)
            if (qstrcmp(argv[i], "--outdir") == 0 && i + 1 < argc)
                outDir = QString::fromLocal8Bit(argv[i + 1]);
        const QString input = QString::fromLocal8Bit(argv[argc - 1]);
        if (outDir.isEmpty() || input.isEmpty()) return 4;
        const QString product = QDir(outDir).filePath(
            QFileInfo(input).completeBaseName() + ".pdf");
        QFile f(product);
        if (!f.open(QIODevice::WriteOnly)) return 4;
        f.write("<html><body>not a pdf at all</body></html>");
        f.close();
        return 0;
    }
    if (mode != QLatin1String("ok")) return 0;   // "silent": nothing written

    QString outDir;
    for (int i = 0; i < argc; ++i)
        if (qstrcmp(argv[i], "--outdir") == 0 && i + 1 < argc)
            outDir = QString::fromLocal8Bit(argv[i + 1]);
    const QString input = QString::fromLocal8Bit(argv[argc - 1]);
    if (outDir.isEmpty() || input.isEmpty()) return 4;
    const QString product = QDir(outDir).filePath(
        QFileInfo(input).completeBaseName() + ".pdf");
    QFile f(product);
    if (!f.open(QIODevice::WriteOnly)) return 4;
    f.write(fakeProductBytes());
    f.close();
    return 0;
}

int main(int argc, char* argv[])
{
    // The fake soffice is identified by the PRODUCTION command-line shape
    // (convertOfficeToPdf always passes --headless).
    for (int i = 0; i < argc; ++i)
        if (qstrcmp(argv[i], "--headless") == 0)
            return fakeSofficeChildMain(argc, argv);
    QApplication app(argc, argv);
    W2BProbeOfficeSave tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "W2BProbeOfficeSave.moc"
