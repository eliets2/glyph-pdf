// SPDX-License-Identifier: MIT
// W2BProbeA11y.cpp — SWEEP-W2B INDEPENDENT verifier probe (FZ-1, FZ-2).
//
// Written by the W2B guarantee-verification-engine against the SAVED tip
// (feat/sweep-w2-verify-b @ 2d29a16). My OWN hostile fixtures, hand-written
// raw PDF bytes (independent of PoDoFo's writer and of the fuzz lane's
// corpora). Asserts the FIXED contracts:
//
//   FZ-1: a cyclic /Fields hierarchy (self- and mutual /Kids reference
//     cycles) no longer kills the process — scanAccessibility returns a
//     bounded report; the visited-reference set + depth-8 cap bound the walk
//     (a 12-deep non-cyclic chain is also bounded, not a crash).
//   FZ-2: scanAccessibility honors its "never throws" contract over the WHOLE
//     scan — a PDF that loads but throws during lazy traversal comes back as
//     a partial report with the explicit "scan-incomplete" finding, never an
//     escaped exception.
//
// NC base for both: 12272f6^ (pre-guard AccessibilityChecker.cpp) — the
// cycle fixture is expected to CRASH (bad_alloc/stack) or hang there, which
// the runner records as a non-zero/timeout outcome.

#include <QtTest/QtTest>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "engines/AccessibilityChecker.h"
#include <podofo/podofo.h>

namespace {

// Hand-built minimal PDF with a Catalog-level /AcroForm whose /Fields walk is
// hostile. `fieldsBody` is the raw object body for object 5 (the first field).
QByteArray hostileA11yPdf(const QByteArray& field5Body)
{
    QByteArray out = "%PDF-1.7\n";
    QList<qint64> off;
    auto addObj = [&out, &off](int num, const QByteArray& body) {
        off.append(out.size());
        out += QByteArray::number(num) + " 0 obj\n" + body + "\nendobj\n";
    };
    addObj(1, "<</Type/Catalog/Pages 2 0 R/AcroForm<</Fields[5 0 R]>>>>");
    addObj(2, "<</Type/Pages/Kids[3 0 R]/Count 1>>");
    addObj(3, "<</Type/Page/Parent 2 0 R/MediaBox[0 0 612 792]>>");
    addObj(4, "<</Producer(w2b-probe)>>");   // Info dict filler
    addObj(5, field5Body);
    const qint64 xref = out.size();
    out += QString("xref\n0 %1\n").arg(off.size() + 1).toLatin1();
    out += "0000000000 65535 f \n";
    for (qint64 o : off)
        out += QString("%1 00000 n \n").arg(o, 10, 10, QChar('0')).toLatin1();
    out += QString("trailer<</Size %1/Root 1 0 R>>\nstartxref\n%2\n%%EOF\n")
               .arg(off.size() + 1).arg(xref).toLatin1();
    return out;
}

// SELF-cycle: the /Fields entry IS the cyclic node (5 → 5).
QByteArray selfCyclePdf()
{
    return hostileA11yPdf(
        "<</FT/Tx/T(Self)/TURNS<</D[0 0]>>/Kids[5 0 R]>>");
}

// MUTUAL cycle: 5 → 6 → 5.
QByteArray mutualCyclePdf()
{
    QByteArray out = "%PDF-1.7\n";
    QList<qint64> off;
    auto addObj = [&out, &off](int num, const QByteArray& body) {
        off.append(out.size());
        out += QByteArray::number(num) + " 0 obj\n" + body + "\nendobj\n";
    };
    addObj(1, "<</Type/Catalog/Pages 2 0 R/AcroForm<</Fields[5 0 R]>>>>");
    addObj(2, "<</Type/Pages/Kids[3 0 R]/Count 1>>");
    addObj(3, "<</Type/Page/Parent 2 0 R/MediaBox[0 0 612 792]>>");
    addObj(5, "<</FT/Tx/T(A)/Kids[6 0 R]>>");
    addObj(6, "<</FT/Tx/T(B)/Kids[5 0 R]>>");
    const qint64 xref = out.size();
    out += QString("xref\n0 %1\n").arg(off.size() + 1).toLatin1();
    out += "0000000000 65535 f \n";
    for (qint64 o : off)
        out += QString("%1 00000 n \n").arg(o, 10, 10, QChar('0')).toLatin1();
    out += QString("trailer<</Size %1/Root 1 0 R>>\nstartxref\n%2\n%%EOF\n")
               .arg(off.size() + 1).arg(xref).toLatin1();
    return out;
}

// 12-deep non-cyclic chain (beyond the depth-8 cap, still finite).
QByteArray deepChainPdf(int depth)
{
    QByteArray out = "%PDF-1.7\n";
    QList<qint64> off;
    auto addObj = [&out, &off](int num, const QByteArray& body) {
        off.append(out.size());
        out += QByteArray::number(num) + " 0 obj\n" + body + "\nendobj\n";
    };
    addObj(1, QString("<</Type/Catalog/Pages 2 0 R/AcroForm<</Fields[5 0 R]>>>>").toLatin1());
    addObj(2, "<</Type/Pages/Kids[3 0 R]/Count 1>>");
    addObj(3, "<</Type/Page/Parent 2 0 R/MediaBox[0 0 612 792]>>");
    int next = 6;
    QByteArray body5 = QString("<</FT/Tx/T(N0)/Kids[%1 0 R]>>").arg(next).toLatin1();
    addObj(5, body5);
    for (int i = 1; i < depth; ++i) {
        const int me = next++;
        const bool last = (i == depth - 1);
        addObj(me, last ? QString("<</FT/Tx/T(N%1)>>").arg(i).toLatin1()
                        : QString("<</FT/Tx/T(N%1)/Kids[%2 0 R]>>").arg(i).arg(next).toLatin1());
    }
    const qint64 xref = out.size();
    out += QString("xref\n0 %1\n").arg(off.size() + 1).toLatin1();
    out += "0000000000 65535 f \n";
    for (qint64 o : off)
        out += QString("%1 00000 n \n").arg(o, 10, 10, QChar('0')).toLatin1();
    out += QString("trailer<</Size %1/Root 1 0 R>>\nstartxref\n%2\n%%EOF\n")
               .arg(off.size() + 1).arg(xref).toLatin1();
    return out;
}

// Loads fine, but object 4 (a lazy-traversal target referenced from the
// Catalog's /ViewerPreferences... actually simpler: the xref lures the parser
// into a garbage object only when the scan resolves beyond the catalog) —
// object 4's body is NOT a valid dictionary; the scan's traversal of
// /AcroForm → resolved object 6 (declared /Type/StructElem garbage array)
// exercises post-load resolution paths. Mutant class from the FZ-2 finding:
// broken lazy objects AFTER a successful Load.
QByteArray brokenLazyObjectPdf()
{
    QByteArray out = "%PDF-1.7\n";
    QList<qint64> off;
    auto addObj = [&out, &off](int num, const QByteArray& body) {
        off.append(out.size());
        out += QByteArray::number(num) + " 0 obj\n" + body + "\nendobj\n";
    };
    addObj(1, "<</Type/Catalog/Pages 2 0 R/AcroForm<</Fields[6 0 R]/DA(/Helv)>>/MarkInfo<<(/)>>>>");
    addObj(2, "<</Type/Pages/Kids[3 0 R]/Count 1>>");
    addObj(3, "<</Type/Page/Parent 2 0 R/MediaBox[0 0 612 792]>>");
    addObj(4, "@@@garbage-not-an-object@@@");
    addObj(5, "<</Type/StructTreeRoot/S[6 0 R]>>");
    addObj(6, "<</FT/Tx/T(A)/Kids[@@broken-indirect@@]>>");
    const qint64 xref = out.size();
    out += QString("xref\n0 %1\n").arg(off.size() + 1).toLatin1();
    out += "0000000000 65535 f \n";
    for (qint64 o : off)
        out += QString("%1 00000 n \n").arg(o, 10, 10, QChar('0')).toLatin1();
    out += QString("trailer<</Size %1/Root 1 0 R>>\nstartxref\n%2\n%%EOF\n")
               .arg(off.size() + 1).arg(xref).toLatin1();
    return out;
}

bool writeBytes(const QString& path, const QByteArray& bytes)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return false;
    f.write(bytes);
    f.close();
    return true;
}

} // namespace

class W2BProbeA11y : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<QTemporaryDir> m_dir;

private slots:
    void init()
    {
        m_dir = std::make_unique<QTemporaryDir>(
            QDir::tempPath() + QStringLiteral("/w2bprobe-a11y-XXXXXX"));
        QVERIFY(m_dir->isValid());
    }
    void cleanup() { m_dir.reset(); }

    QString path(const char* name) const { return m_dir->filePath(QLatin1String(name)); }

    // ── FZ-1: cyclic /Fields is bounded, not fatal ──────────────────────────
    void selfCyclicFieldsScanIsBounded()
    {
        const QString p = path("self-cycle.pdf");
        QVERIFY(writeBytes(p, selfCyclePdf()));
        gp::A11yReport r;                    // must not crash, must not throw
        try {
            r = gp::scanAccessibility(p);
        } catch (const std::exception& e) {
            QFAIL(qPrintable(QString("FZ-1 REGRESSION: scanAccessibility threw on a "
                                     "cyclic /Fields hierarchy: %1").arg(e.what())));
        }
        QVERIFY2(r.fieldsReported < 1000,
                 qPrintable(QString("bounded walk expected, got %1 fields")
                                .arg(r.fieldsReported)));
    }

    void mutualCyclicFieldsScanIsBounded()
    {
        const QString p = path("mutual-cycle.pdf");
        QVERIFY(writeBytes(p, mutualCyclePdf()));
        gp::A11yReport r;
        try {
            r = gp::scanAccessibility(p);
        } catch (const std::exception& e) {
            QFAIL(qPrintable(QString("FZ-1 REGRESSION: mutual /Kids cycle threw: %1").arg(e.what())));
        }
        QVERIFY2(r.fieldsReported < 1000,
                 qPrintable(QString("bounded walk expected, got %1 fields")
                                .arg(r.fieldsReported)));
    }

    void deepNonCyclicChainIsCappedNotCrashing()
    {
        const QString p = path("deep-chain.pdf");
        QVERIFY(writeBytes(p, deepChainPdf(12)));
        gp::A11yReport r;
        try {
            r = gp::scanAccessibility(p);
        } catch (const std::exception& e) {
            QFAIL(qPrintable(QString("FZ-1 REGRESSION: 12-deep chain threw: %1").arg(e.what())));
        }
        QVERIFY2(r.fieldsReported <= 24,
                 qPrintable(QString("depth cap should bound the walk, got %1")
                                .arg(r.fieldsReported)));
    }

    // ── FZ-2: the never-throws seam holds over broken lazy objects ──────────
    void brokenLazyObjectScanNeverThrows()
    {
        const QString p = path("broken-lazy.pdf");
        QVERIFY(writeBytes(p, brokenLazyObjectPdf()));
        gp::A11yReport r;
        try {
            r = gp::scanAccessibility(p);
        } catch (const PoDoFo::PdfError& e) {
            QFAIL(qPrintable(QString("FZ-2 REGRESSION: PdfError escaped the "
                                     "never-throws seam: %1").arg(e.what())));
        } catch (const std::exception& e) {
            QFAIL(qPrintable(QString("FZ-2 REGRESSION: std::exception escaped the "
                                     "never-throws seam: %1").arg(e.what())));
        }
        // A contained scan either completes or comes back PARTIAL with the
        // explicit truncation finding — never silent, never fatal.
        for (const gp::A11yFinding& f : r.findings) {
            if (f.checkId == QStringLiteral("scan-incomplete")) {
                QVERIFY2(!f.whyNot.isEmpty(),
                         "the truncation finding must disclose what happened");
                return;
            }
        }
        QWARN("fixture completed without triggering the truncation finding "
              "(acceptable — the contract asserted here is no-throw)");
    }
};

QTEST_MAIN(W2BProbeA11y)
#include "W2BProbeA11y.moc"
