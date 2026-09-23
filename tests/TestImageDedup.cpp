// SPDX-License-Identifier: Apache-2.0
// Audit 9.13 P0 regression test (follow-up to 482d6d6): optimizeDocument's
// duplicate-image dedup must complete the last mile — page /XObject entries
// pointing at duplicate image objects are rewritten to reference the
// canonical copy, the duplicate object is dropped, and both contracts
// SURVIVE the save + reload round trip.
//
// gateD (2026-09-09) classification note: an earlier draft of this test
// asserted IsReference() on PdfDictionary::FindKey() results and failed.
// That was a test defect, not a save-path defect: in PoDoFo 1.1.0 FindKey()
// is a RESOLVING lookup — findKey() calls TryGetReference() and returns the
// referenced target object itself, so an inlined (referenced) image dict is
// returned for a /XObject entry that is stored as "N 0 R". The shallow/raw
// accessor is GetKey() (same convention TestBatchOpsCoverage uses for its
// "must BE a reference" /DestOutputProfile assertion). The optimize+save
// artifact was verified byte-correct: /XObject<</Im0 5 0 R/Im1 5 0 R>>.
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>
#include <podofo/podofo.h>
#include "engines/podofo/PoDoFoBackend.h"

using namespace PoDoFo;

namespace {
// Raw /XObject entry lookup: GetKey returns the STORED entry (a
// reference-typed PdfObject for "N 0 R"), FindKey would return the resolved
// target and could never pin the indirect-reference contract.
PdfObject* rawXObjectsEntry(PdfObject* pageObj, const char* name) {
    auto* res = pageObj->GetDictionary().GetKey("Resources");
    if (!res) return nullptr;
    if (res->IsReference())
        res = &pageObj->GetDocument()->GetObjects().MustGetObject(res->GetReference());
    if (!res || !res->IsDictionary()) return nullptr;
    auto* xobjs = res->GetDictionary().GetKey("XObject");
    if (!xobjs) return nullptr;
    if (xobjs->IsReference())
        xobjs = &pageObj->GetDocument()->GetObjects().MustGetObject(xobjs->GetReference());
    if (!xobjs || !xobjs->IsDictionary()) return nullptr;
    return xobjs->GetDictionary().GetKey(name);
}

unsigned countImageObjects(PdfMemDocument& doc) {
    unsigned n = 0;
    for (auto obj : doc.GetObjects()) {
        if (!obj->IsDictionary() || !obj->HasStream()) continue;
        auto* subtype = obj->GetDictionary().FindKey("Subtype");
        if (subtype && subtype->IsName() && subtype->GetName().GetString() == "Image")
            n++;
    }
    return n;
}
} // namespace

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
        PdfMemDocument doc;
        auto& page = doc.GetPages().CreatePage(
            PdfPage::CreateStandardPageSize(PdfPageSize::A4));
        auto makeImage = [&]() {
            auto& obj = doc.GetObjects().CreateDictionaryObject();
            obj.GetDictionary().AddKey("Type", PdfName("XObject"));
            obj.GetDictionary().AddKey("Subtype", PdfName("Image"));
            obj.GetDictionary().AddKey("Width", static_cast<int64_t>(4));
            obj.GetDictionary().AddKey("Height", static_cast<int64_t>(4));
            obj.GetDictionary().AddKey("ColorSpace", PdfName("DeviceRGB"));
            obj.GetDictionary().AddKey("BitsPerComponent", static_cast<int64_t>(8));
            obj.GetOrCreateStream().SetData(
                bufferview(imgBytes.constData(), static_cast<size_t>(imgBytes.size())));
            return obj.GetIndirectReference();
        };
        const auto ref0 = makeImage();
        const auto ref1 = makeImage();

        PdfDictionary xobjs;
        xobjs.AddKey(PdfName("Im0"), PdfObject(ref0));
        xobjs.AddKey(PdfName("Im1"), PdfObject(ref1));
        PdfDictionary res;
        res.AddKey("XObject", PdfObject(xobjs));
        page.GetDictionary().AddKey("Resources", PdfObject(res));

        doc.Save(src.toUtf8().constData());
    }

    // Pre-control: the SOURCE has two distinct image objects, and the two raw
    // /XObject entries are indirect references to DIFFERENT objects.
    PdfObject* im0 = nullptr;
    PdfObject* im1 = nullptr;
    {
        PdfMemDocument before;
        before.Load(src.toUtf8().constData());
        auto* pageObj = &before.GetObjects().MustGetObject(
            before.GetPages().GetPageAt(0).GetObject().GetIndirectReference());
        im0 = rawXObjectsEntry(pageObj, "Im0");
        im1 = rawXObjectsEntry(pageObj, "Im1");
        QVERIFY(im0 && im1);
        QVERIFY2(im0->IsReference(), "source /Im0 must be an indirect reference");
        QVERIFY2(im1->IsReference(), "source /Im1 must be an indirect reference");
        QVERIFY2(im0->GetReference() != im1->GetReference(),
                 "source control: the two images must be DISTINCT objects");
        QCOMPARE(countImageObjects(before), 2u);
    }

    PoDoFoBackend backend;
    QVERIFY(backend.loadDocument(src));
    OptimizeOptions opts;
    opts.deduplicateImages = true;
    const QString out = tmp.filePath("deduped.pdf");
    QVERIFY(backend.optimizeDocument(out, opts));
    QVERIFY(QFileInfo::exists(out));

    // Reload the artifact: both RAW resource entries must still be indirect
    // references, now pointing at the SAME canonical object; exactly one
    // image object must remain (the duplicate is dropped by the sweep).
    PdfMemDocument check;
    check.Load(out.toUtf8().constData());
    auto& page = check.GetPages().GetPageAt(0);
    auto* pageObj = &check.GetObjects().MustGetObject(
        page.GetObject().GetIndirectReference());
    im0 = rawXObjectsEntry(pageObj, "Im0");
    im1 = rawXObjectsEntry(pageObj, "Im1");
    QVERIFY(im0 && im1);
    QVERIFY2(im0->IsReference(),
             "after optimize+save /Im0 must STILL be an indirect reference");
    QVERIFY2(im1->IsReference(),
             "after optimize+save /Im1 must STILL be an indirect reference");
    QCOMPARE(im1->GetReference(), im0->GetReference());

    // The canonical target is a real image XObject carrying the payload.
    auto* canonical = check.GetObjects().GetObject(im0->GetReference());
    QVERIFY(canonical && canonical->IsDictionary() && canonical->HasStream());
    auto* subtype = canonical->GetDictionary().FindKey("Subtype");
    QVERIFY(subtype && subtype->IsName()
            && subtype->GetName().GetString() == "Image");

    // Last mile: the deduplicated copy is GONE — exactly one image object
    // survives in the saved artifact (sweep drops the unreferenced twin).
    QCOMPARE(countImageObjects(check), 1u);
}
QTEST_MAIN(TestImageDedup)
#include "TestImageDedup.moc"
