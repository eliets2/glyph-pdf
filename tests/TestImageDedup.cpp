// SPDX-License-Identifier: Apache-2.0
// Audit 9.13 P0 regression test (follow-up to 482d6d6): optimizeDocument's
// duplicate-image dedup must complete the last mile — page /XObject entries
// pointing at duplicate image objects are rewritten to reference the
// canonical copy.
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <podofo/podofo.h>
#include "engines/podofo/PoDoFoBackend.h"

class TestImageDedup : public QObject {
    Q_OBJECT
private slots:
    void duplicatesRewiredToCanonical();
};
void TestImageDedup::duplicatesRewiredToCanonical() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString src = tmp.filePath("dupimg.pdf");

    // Build: page whose /Resources /XObject holds Im0 and Im1 — two DISTINCT
    // image objects with byte-identical streams (the dedup trigger).
    const QByteArray imgBytes("FAKEIMAGEBYTES-IDENTICAL-IN-BOTH");
    {
        PoDoFo::PdfMemDocument doc;
        auto& page = doc.GetPages().CreatePage(
            PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
        auto makeImage = [&](const char* name) {
            auto& obj = doc.GetObjects().CreateDictionaryObject();
            obj.GetDictionary().AddKey("Type", PoDoFo::PdfName("XObject"));
            obj.GetDictionary().AddKey("Subtype", PoDoFo::PdfName("Image"));
            obj.GetDictionary().AddKey("Width", static_cast<int64_t>(4));
            obj.GetDictionary().AddKey("Height", static_cast<int64_t>(4));
            obj.GetDictionary().AddKey("ColorSpace", PoDoFo::PdfName("DeviceRGB"));
            obj.GetDictionary().AddKey("BitsPerComponent", static_cast<int64_t>(8));
            obj.GetOrCreateStream().SetData(
                PoDoFo::bufferview(imgBytes.constData(), static_cast<size_t>(imgBytes.size())));
            return obj.GetIndirectReference();
        };
        const auto ref0 = makeImage("Im0");
        const auto ref1 = makeImage("Im1");

        PoDoFo::PdfDictionary xobjs;
        xobjs.AddKey(PoDoFo::PdfName("Im0"), PoDoFo::PdfObject(ref0));
        xobjs.AddKey(PoDoFo::PdfName("Im1"), PoDoFo::PdfObject(ref1));
        PoDoFo::PdfDictionary res;
        res.AddKey("XObject", PoDoFo::PdfObject(xobjs));
        page.GetDictionary().AddKey("Resources", PoDoFo::PdfObject(res));

        doc.Save(src.toUtf8().constData());
    }

    PoDoFoBackend backend;
    const bool loaded = backend.loadDocument(src);
    {
        QFile diag0(QStringLiteral("dd_diag0.txt"));
        if (diag0.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream ts(&diag0);
            ts << "loaded=" << loaded << "\n";
        }
    }
    QVERIFY(loaded);
    OptimizeOptions opts;
    opts.deduplicateImages = true;
    const QString out = tmp.filePath("deduped.pdf");
    const bool optimized = backend.optimizeDocument(out, opts);
    {
        QFile diag1(QStringLiteral("dd_diag1.txt"));
        if (diag1.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream ts(&diag1);
            ts << "optimized=" << optimized << " exists=" << QFileInfo::exists(out) << "\n";
        }
    }
    QVERIFY(optimized);

    // Reload: both resource entries must now point at the SAME object.
    PoDoFo::PdfMemDocument check;
    check.Load(out.toUtf8().constData());
    auto& page = check.GetPages().GetPageAt(0);
    QFile diagPre(QStringLiteral("dd_diagpre.txt"));
    if (diagPre.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream ts(&diagPre);
        auto* res = page.GetDictionary().FindKey("Resources");
        ts << "res=" << (res != nullptr);
        if (res) {
            auto* xo = res->GetDictionary().FindKey("XObject");
            ts << " xobj=" << (xo != nullptr);
            if (xo) {
                ts << " im0=" << (xo->GetDictionary().FindKey("Im0") != nullptr)
                   << " im1=" << (xo->GetDictionary().FindKey("Im1") != nullptr);
            }
        }
        ts << "\n";
    }
    auto* xobjs = page.GetDictionary().FindKey("Resources")
                        ->GetDictionary().FindKey("XObject");
    QVERIFY(xobjs && xobjs->IsDictionary());
    auto* im0 = xobjs->GetDictionary().FindKey("Im0");
    auto* im1 = xobjs->GetDictionary().FindKey("Im1");
    QVERIFY(im0 && im0->IsReference());
    QVERIFY(im1 && im1->IsReference());
    {
        QFile diag(QStringLiteral("dd_diag.txt"));
        if (diag.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream ts(&diag);
            ts << "im0=" << im0->GetReference().ObjectNumber()
               << " im1=" << im1->GetReference().ObjectNumber() << "\n";
        }
    }
    QCOMPARE(im1->GetReference(), im0->GetReference());
}
QTEST_MAIN(TestImageDedup)
#include "TestImageDedup.moc"
