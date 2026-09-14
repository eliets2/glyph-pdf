// R14 INDEPENDENT REVIEWER probe — SEP13 fixes L1, L2, L9, L13, M7.
// NOT a lane artifact. Written by the independent reviewer (2026-09-14).
// Independence: own fixtures / own failure injection / independent read paths.
#include <QtTest/QtTest>
#include <QFile>
#include <QLineEdit>
#include <QSignalSpy>
#include <QTemporaryDir>

#include <podofo/podofo.h>

#include "engines/ConversionManager.h"
#include "engines/SignatureManager.h"
#include "core/AppContext.h"
#include "modes/BatchMode.h"

using namespace gp;
// TargetFormat is nested in IConversionEngine (global-namespace class).
using TargetFormat = IConversionEngine::TargetFormat;

// SignatureManager / SignOutcome / PAdESLevel live in the GLOBAL namespace.
namespace {

const QString kFixtureDir = QStringLiteral(SOURCE_DIR) + "/tests/fixtures/signing";
const QString kInputPdf = kFixtureDir + "/test_input.pdf";
const QString kP12Path = kFixtureDir + "/test_signer.p12";
const QString kP12Pass = QStringLiteral("test");
const char* kDeadTsa = "https://127.0.0.1:9/tsa";   // reserved port — refused

bool writeFile(const QString& path, const QByteArray& bytes) {
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return false;
    const bool ok = f.write(bytes) == bytes.size();
    f.close();
    return ok;
}

QString readFile(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    return QString::fromUtf8(f.readAll());
}

// Byte-accurate minimal single-page PDF builder (reviewer's own).
QByteArray onePagePdf(const QByteArray& contentStream, const QByteArray& baseFontName) {
    struct Obj { QByteArray head; };
    QByteArray out = "%PDF-1.4\n";
    qint64 offsets[6] = {0, 0, 0, 0, 0, 0};
    const QByteArray objs[6] = {
        "1 0 obj<</Type/Catalog/Pages 2 0 R>>endobj\n",
        "2 0 obj<</Type/Pages/Kids[3 0 R]/Count 1>>endobj\n",
        "3 0 obj<</Type/Page/Parent 2 0 R/MediaBox[0 0 612 792]"
            "/Resources<</Font<</F1 4 0 R>>>>/Contents 5 0 R>>endobj\n",
        "4 0 obj<</Type/Font/Subtype/Type1/BaseFont/" + baseFontName +
            "/Encoding/WinAnsiEncoding>>endobj\n",
        "5 0 obj<</Length " + QByteArray::number(contentStream.size()) +
            ">>\nstream\n" + contentStream + "\nendstream\nendobj\n",
        ""
    };
    for (int i = 0; i < 5; ++i) {
        offsets[i + 1] = out.size();
        out += objs[i];
    }
    const qint64 xref = out.size();
    out += "xref\n0 6\n0000000000 65535 f \n";
    for (int i = 1; i <= 5; ++i)
        out += QString::asprintf("%010lld 00000 n \n", offsets[i]).toLatin1();
    out += "trailer<</Size 6/Root 1 0 R>>\nstartxref\n" +
           QByteArray::number(xref) + "\n%%EOF\n";
    return out;
}

// Independent artifact reader: walk every object, look for DocTimeStamp.
int docTimestampObjects(const QString& pdfPath) {
    int hits = -1;
    try {
        PoDoFo::PdfMemDocument doc;
        doc.Load(pdfPath.toUtf8().constData());
        hits = 0;
        for (auto* obj : doc.GetObjects()) {
            if (!obj || !obj->IsDictionary()) continue;
            auto st = obj->GetDictionary().FindKey("Subtype");
            auto ty = obj->GetDictionary().FindKey("Type");
            const auto isName = [](const PoDoFo::PdfObject* o, const char* n) {
                return o && o->IsName() && o->GetName() == PoDoFo::PdfName(n);
            };
            if (isName(st, "DocTimeStamp") || isName(ty, "DocTimeStamp"))
                ++hits;
        }
    } catch (const std::exception& e) {
        qWarning() << "docTimestampObjects load failed:" << e.what();
    }
    return hits;
}

// Minimal valid single-page PDF (repo-standard fixture bytes).
QString createMinimalPdf(const QString& dir, const QString& name) {
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
    f.close();
    return path;
}

} // namespace

class R14ProbeSep13Fixes : public QObject {
    Q_OBJECT

    AppContext m_ctx;

private slots:
    // L1: B_T + dead TSA — the downgrade must be DISCLOSED (outcome not a
    // plain Success AND the new timestampMissing flag set) AND the artifact
    // must actually lack a document timestamp (independent PoDoFo object
    // walk). Control: no TSA requested → plain Success.
    void l1_deadTsaDisclosedAndArtifactHonest() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        QVERIFY(QFile::exists(kInputPdf) && QFile::exists(kP12Path));

        SignatureManager mgr;
        mgr.setSignatureLevel(PAdESLevel::B_T);
        mgr.setTsaUrl(QString::fromLatin1(kDeadTsa));
        const QString out = tmp.filePath("deadtsa.pdf");
        const SignOutcome outcome = mgr.signDocument(kInputPdf, out, kP12Path,
                                                     kP12Pass, "r14l1", "");
        const SignatureOutcomeDetail detail = mgr.lastSignOutcomeDetail();
        qInfo() << "dead-TSA outcome =" << int(outcome)
                << "timestampMissing =" << detail.timestampMissing
                << "docTimestampMissing =" << detail.docTimestampMissing;
        QVERIFY2(QFile::exists(out), "B-B bytes must be written");
        QVERIFY2(outcome == SignOutcome::PartialLtvMissing,
                 "dead TSA at B_T must NOT report plain Success (L1)");
        QVERIFY2(detail.timestampMissing,
                 "the new B-T disclosure flag must be set (L1)");
        // Independent artifact read: no DocTimeStamp object may exist.
        const int ts = docTimestampObjects(out);
        qInfo() << "DocTimeStamp objects in artifact:" << ts;
        QVERIFY2(ts == 0, "the artifact from a dead-TSA B_T sign carries no timestamp");

        // Control: without a TSA configured there is no requested enhancement
        // to lose — plain Success is honest.
        SignatureManager ctl;
        ctl.setSignatureLevel(PAdESLevel::B_T);
        const QString out2 = tmp.filePath("control.pdf");
        const SignOutcome oc2 = ctl.signDocument(kInputPdf, out2, kP12Path,
                                                 kP12Pass, "r14l1c", "");
        qInfo() << "control outcome =" << int(oc2);
        QVERIFY2(oc2 == SignOutcome::Success, "control: no-TSA B_T sign succeeds");
    }

    // L2 variant: different hostile name (brace + parens + url payload); the
    // HTML attribute must not break out — asserted by scanning every
    // style="..." attribute for unescaped quotes (reviewer's own parse).
    void l2_htmlStyleAttributeCannotBeBrokenOut() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        // '}' quotes parens are all legal name chars; the payload tries to
        // leave the style attribute and start a new one.
        const QByteArray evil = "A}'; color:red; x='(u)";
        const QByteArray pdf = onePagePdf("BT /F1 24 Tf 72 700 Td (HI) Tj ET", evil);
        const QString src = tmp.filePath("evil2.pdf");
        QVERIFY(writeFile(src, pdf));

        const QString out = tmp.filePath("evil2.html");
        ConversionManager conv;
        QVERIFY(conv.convertTo(src, out, TargetFormat::Html));
        const QString html = readFile(out);
        QVERIFY(!html.isEmpty());

        // (a) the raw payload must not survive verbatim inside an attribute
        QVERIFY2(!html.contains(QLatin1String("color:red"), Qt::CaseInsensitive),
                 "raw CSS payload must not survive into the HTML (L2)");
        // (b) reviewer's own attribute scan: every style="..." value must be
        // quote-balanced (no premature attribute termination).
        int idx = 0, scanned = 0;
        while ((idx = html.indexOf(QLatin1String("style=\""), idx)) >= 0) {
            const int start = idx + 7;
            int i = start;
            while (i < html.size() && html[i] != QLatin1Char('"')) ++i;
            QVERIFY2(i < html.size(), "style attribute must terminate");
            const QString value = html.mid(start, i - start);
            QVERIFY2(!value.contains(QLatin1String("'; ")),
                     QStringLiteral("style attribute value contains an unescaped "
                                    "breakout sequence: %1").arg(value).toUtf8().constData());
            idx = i + 1;
            ++scanned;
        }
        qInfo() << "style attributes scanned:" << scanned;
        QVERIFY2(scanned >= 1, "the export must carry at least one styled span");
    }

    // L13 variant: THREE ragged rows (different geometry from the lane's).
    void l13_threeRaggedRowsStayOneColumn() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QByteArray pdf = onePagePdf(
            "BT /F1 12 Tf 250 700 Td (Amount) Tj ET\n"
            "BT /F1 12 Tf 296 660 Td (55) Tj ET\n"
            "BT /F1 12 Tf 288 620 Td (7) Tj ET\n",
            "Helvetica");
        const QString src = tmp.filePath("ragged3.pdf");
        QVERIFY(writeFile(src, pdf));

        const QString out = tmp.filePath("ragged3.csv");
        ConversionManager conv;
        QVERIFY(conv.convertTo(src, out, TargetFormat::Csv));
        QStringList lines;
        for (const QString& raw : readFile(out).split(QLatin1Char('\n'), Qt::SkipEmptyParts))
            lines.append(raw.trimmed());
        QVERIFY(lines.size() >= 3);
        qInfo() << "rows:" << lines;
        QVERIFY2(lines.at(1) == QStringLiteral("\"55\""),
                 QStringLiteral("row2 split into invented columns: %1").arg(lines.at(1)).toUtf8().constData());
        // R14 boundary characterization (matches the ledger's documented L13
        // policy): a value starting STRICTLY PAST the running column extent
        // continues the column ("55" @296 vs extent [250..293]); a value
        // starting INSIDE the extent ("7" @288) still opens a NEW anchor —
        // the deliberate V03 overlapping-band preservation. Empirically pinned
        // here so any future policy change flips this pin.
        QVERIFY2(lines.at(2) == QStringLiteral("\"\",\"7\""),
                 QStringLiteral("boundary pin (inside-extent start → new column) moved: %1")
                     .arg(lines.at(2)).toUtf8().constData());
    }

    // M7 variant: 18pt heading + 7pt sub-line 11pt below → must stay 2 lines
    // (tolerance = 0.5*min = 3.5pt).
    void m7_smallLineUnderBigHeadingVariant() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QByteArray pdf = onePagePdf(
            "BT /F1 18 Tf 72 700 Td (HEADING) Tj ET\n"
            "BT /F1 7 Tf 72 689 Td (fine print) Tj ET\n",
            "Helvetica");
        const QString src = tmp.filePath("sizes2.pdf");
        QVERIFY(writeFile(src, pdf));

        const QString out = tmp.filePath("sizes2.txt");
        ConversionManager conv;
        QVERIFY(conv.convertTo(src, out, TargetFormat::Text));
        const QStringList nonEmpty =
            readFile(out).split(QLatin1Char('\n'), Qt::SkipEmptyParts)
                .filter(QRegularExpression("\\S"));
        qInfo() << "lines:" << nonEmpty;
        QVERIFY2(nonEmpty.size() >= 2, "two distinct lines must produce two rows");
        for (const QString& l : nonEmpty)
            QVERIFY2(!(l.contains(QLatin1String("HEADING"))
                       && l.contains(QLatin1String("fine print"))),
                     QStringLiteral("merged row: %1").arg(l.trimmed()).toUtf8().constData());
    }

    // L9 variant: DIFFERENT failure injection from the lane's (merge output
    // directory is an existing FILE, not a nonexistent dir) — the save must
    // fail and the accounting must stay honest: 0 successes, 0 skips, exactly
    // per-input failures, no output.
    void l9_mergeIntoFileAsDirectoryHonestAccounting() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString a = createMinimalPdf(tmp.path(), "a.pdf");
        const QString b = createMinimalPdf(tmp.path(), "b.pdf");
        QVERIFY(!a.isEmpty() && !b.isEmpty());

        BatchMode bm;
        bm.setAppContext(&m_ctx);
        bm.addFilesForTest({a, b});
        bm.setOperationForTest(4);   // OpMerge

        const QString doomedDir = tmp.path() + "/a.pdf"; // a FILE, not a dir
        const auto edits = bm.findChildren<QLineEdit*>();
        QLineEdit* edit = nullptr;
        for (auto* le : edits)
            if (le->placeholderText().contains(QLatin1String("Same folder as first source")))
                edit = le;
        QVERIFY2(edit, "merge out dir edit not found");
        edit->setText(doomedDir);
        const QString out = doomedDir + "/a_merged.pdf";

        QSignalSpy finishedSpy(&bm, &BatchMode::batchFinished);
        bm.onRunBatch();
        QVERIFY2(finishedSpy.wait(30000), "merge did not finish");

        const int accounted = bm.successCount() + bm.failCount() + bm.skipCount();
        qInfo() << "r14 doomed accounting: success =" << bm.successCount()
                << "fail =" << bm.failCount() << "skip =" << bm.skipCount()
                << "fileCount =" << bm.fileCount()
                << "output exists =" << QFile::exists(out);
        QVERIFY2(!QFile::exists(out), "no output may be written into a file-as-dir");
        QVERIFY2(bm.successCount() == 0,
                 "no success may be reported against a never-written output (L9/L10)");
        QVERIFY2(accounted <= bm.fileCount(),
                 "no phantom result beyond one per input file (L9)");
        QVERIFY2(bm.failCount() == bm.fileCount(),
                 "each input must be honestly reported failed (L9)");
    }
};

#include "R14ProbeSep13Fixes.moc"
QTEST_MAIN(R14ProbeSep13Fixes)
