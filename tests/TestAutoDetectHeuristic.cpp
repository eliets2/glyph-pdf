// SPDX-License-Identifier: Apache-2.0
// Audit 9.6 P0 regression test: auto-detect must be content-aware. A page
// containing label/blank patterns yields suggestions derived from that
// content; the former hardcoded dummy trio (AutoName/AutoDate/AutoCheck) is
// gone, and pages without form-like content yield no fake fields.
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QTextStream>
#include <podofo/podofo.h>
#include "engines/FormManager.h"

class TestAutoDetectHeuristic : public QObject {
    Q_OBJECT
private slots:
    void labelPatternYieldsSuggestions();
    void plainPageYieldsNoFakeFields();
private:
    static QString createPdfWithText(const QString& dir, const QString& name,
                                     const QByteArray& contentStream);
};
QString TestAutoDetectHeuristic::createPdfWithText(const QString& dir, const QString& name,
                                                   const QByteArray& contentStream) {
    const QString path = dir + "/" + name;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return {};
    QByteArray out = "%PDF-1.4\n";
    QList<qint64> off;
    auto addObj = [&](const QByteArray& body) {
        off.append(out.size());
        out += QByteArray::number(off.size()) + " 0 obj\n" + body + "\nendobj\n";
    };
    addObj("<</Type/Catalog/Pages 2 0 R>>");
    addObj("<</Type/Pages/Kids[3 0 R]/Count 1>>");
    addObj("<</Type/Page/Parent 2 0 R/MediaBox[0 0 612 792]/Contents 4 0 R/Resources<</Font<</F1 5 0 R>>>>>>");
    const QByteArray stream = "BT /F1 12 Tf 72 700 Td (" + contentStream + ") Tj ET\n";
    addObj("<</Length " + QByteArray::number(stream.size()) + ">>stream\n" + stream + "endstream");
    addObj("<</Type/Font/Subtype/Type1/BaseFont/Helvetica>>");

    const qint64 xrefPos = out.size();
    out += QString("xref\n0 %1\n").arg(off.size() + 1).toLatin1();
    out += "0000000000 65535 f \n";
    for (qint64 o : off)
        out += QString("%1 00000 n \n").arg(o, 10, 10, QChar('0')).toLatin1();
    out += QString("trailer<</Size %1/Root 1 0 R>>\nstartxref\n%2\n%%EOF\n")
              .arg(off.size() + 1).arg(xrefPos).toLatin1();
    f.write(out);
    return path;
}
void TestAutoDetectHeuristic::labelPatternYieldsSuggestions() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = createPdfWithText(tmp.path(), "formlike.pdf",
                                          QByteArray("Name: "));
    QVERIFY(!pdf.isEmpty());
    FormManager fm;
    const auto suggestions = fm.autoDetectFields(pdf, 0);
    QFile diag(QStringLiteral("ad_diag.txt"));
    QString loadNote;
    try {
        PoDoFo::PdfMemDocument probe;
        probe.Load(pdf.toUtf8().constData());
        PoDoFo::PdfContentStreamReader reader(probe.GetPages().GetPageAt(0));
        PoDoFo::PdfContent c;
        int ops = 0;
        QString firstText;
        while (reader.TryReadNext(c)) {
            if (c.GetType() == PoDoFo::PdfContentType::Operator) {
                ++ops;
                if (firstText.isEmpty() && c.GetKeyword() == "Tj" && c.GetStack().GetSize() > 0
                    && c.GetStack()[0].IsString())
                    firstText = QString::fromStdString(std::string(c.GetStack()[0].GetString().GetString()));
            }
        }
        loadNote = QStringLiteral("ops=%1 text=%2").arg(ops).arg(firstText);
        // Replicate heuristic decision for the first Tj run.
        const bool isLabel = firstText.trimmed().endsWith(QLatin1Char(':'));
        const bool hasBlank = firstText.contains(QStringLiteral("___"));
        loadNote += QStringLiteral(" isLabel=%1 hasBlank=%2").arg(isLabel).arg(hasBlank);
        // Full inline replication of the heuristic decision math:
        double cx = 0, cy = 0, fs = 10.0;
        // Re-walk ops tracking positions like FormManager does.
        PoDoFo::PdfContentStreamReader r2(probe.GetPages().GetPageAt(0));
        PoDoFo::PdfContent c2;
        int suggested = 0;
        while (r2.TryReadNext(c2)) {
            if (c2.GetType() != PoDoFo::PdfContentType::Operator) continue;
            const std::string_view k2 = c2.GetKeyword();
            const auto& st2 = c2.GetStack();
            if (k2 == "Tm" && st2.GetSize() >= 6) {
                if (st2[4].IsNumberOrReal()) cx = st2[4].GetReal();
                if (st2[5].IsNumberOrReal()) cy = st2[5].GetReal();
            } else if ((k2 == "Td" || k2 == "TD") && st2.GetSize() >= 2) {
                if (st2[0].IsNumberOrReal()) cx += st2[0].GetReal();
                if (st2[1].IsNumberOrReal()) cy += st2[1].GetReal();
            } else if (k2 == "Tf" && st2.GetSize() >= 2) {
                if (st2[1].IsNumberOrReal()) fs = st2[1].GetReal();
            } else {
                QString t2;
                if (k2 == "Tj" && st2.GetSize() >= 1 && st2[0].IsString())
                    t2 = QString::fromStdString(std::string(st2[0].GetString().GetString()));
                if (t2.isEmpty()) continue;
                const bool lab = t2.trimmed().endsWith(QLatin1Char(':'));
                const bool blk = t2.contains(QStringLiteral("___"));
                if (!lab && !blk) continue;
                const double pw = probe.GetPages().GetPageAt(0).GetMediaBox().Width;
                const double fw = qMin(180.0, qMax(80.0, pw - cx - fs * 2));
                loadNote += QStringLiteral(" [cx=%1 cy=%2 fs=%3 fw=%4]").arg(cx).arg(cy).arg(fs).arg(fw);
                if (fw >= 40.0) ++suggested;
            }
        }
        loadNote += QStringLiteral(" inlineSuggested=%1").arg(suggested);
    } catch (const PoDoFo::PdfError& e) {
        loadNote = QStringLiteral("LOAD FAIL: %1").arg(QString::fromLatin1(e.what()));
    }
    if (diag.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream ts(&diag);
        ts << "count=" << suggestions.size() << " " << loadNote << "\n";
    }
    QVERIFY2(!suggestions.isEmpty(),
             "a 'Name:' label must produce at least one suggestion");
    for (const auto& s : suggestions) {
        QCOMPARE(s.type, QStringLiteral("Text"));
        QVERIFY(!s.suggestedName.startsWith(QStringLiteral("AutoDate")));
        QVERIFY(s.rect.isValid());
    }
}

void TestAutoDetectHeuristic::plainPageYieldsNoFakeFields() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = createPdfWithText(tmp.path(), "plain.pdf",
                                          QByteArray("The quick brown fox jumps."));
    QVERIFY(!pdf.isEmpty());
    FormManager fm;
    const auto suggestions = fm.autoDetectFields(pdf, 0);
    QVERIFY2(suggestions.isEmpty(),
             "a page with no form-like content must not yield fake fields");
}
QTEST_MAIN(TestAutoDetectHeuristic)
#include "TestAutoDetectHeuristic.moc"
