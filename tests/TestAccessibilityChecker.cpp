// SPDX-License-Identifier: Apache-2.0
// T2-4 accessibility P1: pins for gp::scanAccessibility() — the DETECTION
// engine behind the Accessibility panel.
//
// Honesty contract pinned here (mirrors the panel's disclosure):
//   * The checker reports GAPS (what a tagged document needs). It never
//     certifies PDF/UA and never claims the document is accessible.
//   * Content tagging (structure-tree construction) is explicitly OUT of
//     scope — the checker only detects that a document is untagged.
//   * Large documents: per-object findings (image /Alt, field /TU) are a
//     bounded SAMPLE capped at the named constants; truncation is reported
//     in the report (truncated()/totals) — never silently dropped.
//
// Fixtures are hand-built with PoDoFo (same idiom as
// TestReadingOrderThreshold): each seeded defect is independent, so every
// check can be pinned in isolation against a clean sibling document.
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <podofo/podofo.h>

#include "engines/AccessibilityChecker.h"

using PoDoFo::PdfObject;
using PoDoFo::PdfName;
using PoDoFo::PdfString;
using PoDoFo::PdfDictionary;
using PoDoFo::PdfArray;

namespace {

// Options for the fixture builder. Every field seeds (or heals) exactly one
// checker input so pins can isolate single defects.
struct FixtureOpts {
    bool tagged = false;            // /StructTreeRoot + /MarkInfo /Marked true
    bool structRootOnly = false;    // /StructTreeRoot WITHOUT /MarkInfo
    bool lang = false;              // catalog /Lang
    bool title = false;             // /Info /Title
    bool displayDocTitle = false;   // /ViewerPreferences /DisplayDocTitle true
    int images = 0;                 // image XObjects on the page
    bool imageAlt = false;          // ... carrying /Alt
    bool acroForm = false;          // present at all
    bool fieldTu = false;           // field carries /TU
    unsigned extraPages = 0;        // additional empty pages
};

bool buildPdf(const QString& path, const FixtureOpts& o) {
    try {
        PoDoFo::PdfMemDocument doc;

        if (o.lang)
            doc.GetCatalog().GetDictionary().AddKey("Lang", PdfObject(PdfString("en-US")));

        if (o.title)
            doc.GetMetadata().SetTitle(PdfString("Quarterly Report"));

        if (o.displayDocTitle) {
            auto& vp = doc.GetObjects().CreateDictionaryObject();
            vp.GetDictionary().AddKey("DisplayDocTitle", PdfObject(true));
            doc.GetCatalog().GetDictionary().AddKey("ViewerPreferences",
                                                    vp.GetIndirectReference());
        }

        if (o.tagged || o.structRootOnly) {
            if (o.tagged) {
                PdfDictionary markInfo;
                markInfo.AddKey("Marked", PdfObject(true));
                doc.GetCatalog().GetDictionary().AddKey("MarkInfo", PdfObject(markInfo));
            }
            auto& root = doc.GetObjects().CreateDictionaryObject();
            root.GetDictionary().AddKey("Type", PdfObject(PdfName("StructTreeRoot")));
            doc.GetCatalog().GetDictionary().AddKey("StructTreeRoot",
                                                    root.GetIndirectReference());
        }

        auto& page = doc.GetPages().CreatePage(
            PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));

        if (o.images > 0) {
            // Image XObjects under the page /Resources /XObject dict — built
            // as a plain dictionary (the repo idiom) so resource names are
            // deterministic for the "findings name their target" pin.
            auto& xobjs = doc.GetObjects().CreateDictionaryObject();
            for (unsigned i = 0; i < o.images; ++i) {
                auto& img = doc.GetObjects().CreateDictionaryObject();
                img.GetDictionary().AddKey("Type", PdfObject(PdfName("XObject")));
                img.GetDictionary().AddKey("Subtype", PdfObject(PdfName("Image")));
                img.GetDictionary().AddKey("Width", PdfObject(static_cast<int64_t>(4)));
                img.GetDictionary().AddKey("Height", PdfObject(static_cast<int64_t>(4)));
                img.GetDictionary().AddKey("ColorSpace", PdfObject(PdfName("DeviceGray")));
                img.GetDictionary().AddKey("BitsPerComponent", PdfObject(static_cast<int64_t>(8)));
                if (o.imageAlt)
                    img.GetDictionary().AddKey("Alt", PdfString("scan of a receipt"));
                const unsigned char px[16] = {0};
                img.GetOrCreateStream().SetData(
                    PoDoFo::bufferview(reinterpret_cast<const char*>(px), sizeof(px)));
                xobjs.GetDictionary().AddKey(
                    PdfName("Im" + std::to_string(i)), img.GetIndirectReference());
            }
            page.GetResources().GetDictionary().AddKey(
                "XObject", xobjs.GetIndirectReference());

            // Reference the first image from the page content so the page is
            // genuinely well-formed (the checker walks resources, not content).
            auto& contents = page.GetOrCreateContents();
            auto& stream = contents.CreateStreamForAppending(
                PoDoFo::PdfStreamAppendFlags::None);
            stream.SetData(PoDoFo::bufferview("q\n100 0 0 100 50 50 cm\n/Im0 Do\nQ\n", 30));
        }

        if (o.acroForm) {
            PoDoFo::PdfDictionary acro;
            PdfArray fields;
            auto& field = doc.GetObjects().CreateDictionaryObject();
            field.GetDictionary().AddKey("FT", PdfObject(PdfName("Tx")));
            field.GetDictionary().AddKey("T", PdfString("f1"));
            if (o.fieldTu)
                field.GetDictionary().AddKey("TU", PdfString("Your full name"));
            fields.Add(field.GetIndirectReference());
            acro.AddKey("Fields", PdfObject(fields));
            doc.GetCatalog().GetDictionary().AddKey("AcroForm", PdfObject(acro));
        }

        for (unsigned p = 0; p < o.extraPages; ++p)
            doc.GetPages().CreatePage(
                PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));

        doc.Save(path.toUtf8().constData());
        return true;
    } catch (const PoDoFo::PdfError& e) {
        qWarning() << "buildPdf failed:" << e.what();
        return false;
    } catch (...) {
        return false;
    }
}

int findingCount(const gp::A11yReport& r, const QString& checkId) {
    int n = 0;
    for (const auto& f : r.findings)
        if (f.checkId == checkId) ++n;
    return n;
}

const gp::A11yFinding* findFinding(const gp::A11yReport& r, const QString& checkId) {
    for (const auto& f : r.findings)
        if (f.checkId == checkId) return &f;
    return nullptr;
}

} // namespace

class TestAccessibilityChecker : public QObject {
    Q_OBJECT

private slots:
    // The sample caps must stay NAMED constants so truncation can never
    // silently drift (same discipline as kReadingOrderSlotTolerance).
    void sampleCapsAreNamed();

    // Every check fires on the fully-defective fixture, and each finding
    // carries a non-empty honest whyNot.
    void allChecksFireOnSeededDefects();

    // The clean sibling produces NO findings and the report never implies
    // conformance (that is the panel's job to word, but the report itself
    // must be empty here).
    void cleanDocumentProducesNoFindings();

    // Single-defect isolation: healing one defect removes exactly its finding.
    void singleDefectIsolation();

    // Tagged but missing /MarkInfo /Marked → the LOW variant of the
    // struct-tree check.
    void taggedButNotMarkedIsReported();

    // Location discipline: image findings name their page; field findings
    // name the field.
    void findingsNameTheirTarget();

    // Truncation disclosure: more defective images than the cap → sample is
    // bounded and the report DISCLOSES it (never silently dropped).
    void imageSampleIsTruncatedWithDisclosure();

    // A missing/corrupt file is an honest load failure, not an empty "clean".
    void unloadableDocumentIsReportedAsSuch();

private:
    static QString make(const QTemporaryDir& dir, const QString& name,
                        const FixtureOpts& o) {
        const QString path = dir.filePath(name);
        if (!buildPdf(path, o))
            return {};
        return path;
    }
};

void TestAccessibilityChecker::sampleCapsAreNamed() {
    QVERIFY(gp::kA11yMaxImageFindings > 0);
    QVERIFY(gp::kA11yMaxFieldFindings > 0);
    // Small enough to keep the panel responsive, large enough to be useful —
    // the exact value is a triage bound, not a conformance rule.
    QVERIFY(gp::kA11yMaxImageFindings <= 200);
    QVERIFY(gp::kA11yMaxFieldFindings <= 200);
}

void TestAccessibilityChecker::allChecksFireOnSeededDefects() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    FixtureOpts o;
    o.images = 2;
    o.acroForm = true;
    const QString pdf = make(tmp, "defective.pdf", o);
    QVERIFY(!pdf.isEmpty());

    const gp::A11yReport r = gp::scanAccessibility(pdf);
    QVERIFY(r.loadOk);
    QVERIFY(!r.tagged);
    QCOMPARE(findingCount(r, QStringLiteral("struct-tree")), 1);
    QCOMPARE(findingCount(r, QStringLiteral("doc-language")), 1);
    // No /Info /Title AND no /DisplayDocTitle → both title gaps reported.
    QCOMPARE(findingCount(r, QStringLiteral("doc-title")), 1);
    QCOMPARE(findingCount(r, QStringLiteral("display-doc-title")), 1);
    QCOMPARE(findingCount(r, QStringLiteral("image-alt")), 2);
    QCOMPARE(findingCount(r, QStringLiteral("field-tu")), 1);

    // Every finding must explain WHY it matters (the honest whyNot), never
    // just a bare rule id.
    for (const auto& f : r.findings) {
        QVERIFY2(!f.whyNot.isEmpty(), qPrintable(f.checkId));
        QVERIFY2(!f.where.isEmpty(), qPrintable(f.checkId));
    }
}

void TestAccessibilityChecker::cleanDocumentProducesNoFindings() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    FixtureOpts o;
    o.tagged = true;
    o.lang = true;
    o.title = true;
    o.displayDocTitle = true;
    o.images = 1;
    o.imageAlt = true;
    o.acroForm = true;
    o.fieldTu = true;
    const QString pdf = make(tmp, "clean.pdf", o);
    QVERIFY(!pdf.isEmpty());

    const gp::A11yReport r = gp::scanAccessibility(pdf);
    QVERIFY(r.loadOk);
    QVERIFY(r.tagged);
    QVERIFY(!r.truncated());
    QCOMPARE(r.findings.size(), 0);
}

void TestAccessibilityChecker::singleDefectIsolation() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    // Healed document (all checks green) minus the language — only the
    // language finding may appear.
    FixtureOpts o;
    o.tagged = true;
    o.title = true;
    o.displayDocTitle = true;
    const QString pdf = make(tmp, "only-lang.pdf", o);
    QVERIFY(!pdf.isEmpty());
    const gp::A11yReport r = gp::scanAccessibility(pdf);
    QCOMPARE(r.findings.size(), 1);
    QCOMPARE(r.findings.first().checkId, QStringLiteral("doc-language"));
}

void TestAccessibilityChecker::findingsNameTheirTarget() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    FixtureOpts o;
    o.images = 1;
    o.acroForm = true;
    const QString pdf = make(tmp, "targets.pdf", o);
    QVERIFY(!pdf.isEmpty());

    const gp::A11yReport r = gp::scanAccessibility(pdf);
    const auto* img = findFinding(r, QStringLiteral("image-alt"));
    QVERIFY(img != nullptr);
    QVERIFY2(img->where.contains(QLatin1String("page 1")), qPrintable(img->where));
    QVERIFY2(img->where.contains(QLatin1String("Im0")), qPrintable(img->where));
    QCOMPARE(img->page, 0);  // 0-based machine-readable page, -1 = n/a

    const auto* fld = findFinding(r, QStringLiteral("field-tu"));
    QVERIFY(fld != nullptr);
    QVERIFY2(fld->where.contains(QLatin1String("f1")), qPrintable(fld->where));

    // Document-level findings say "document", not a page.
    const auto* st = findFinding(r, QStringLiteral("struct-tree"));
    QVERIFY(st != nullptr);
    QCOMPARE(st->page, -1);
}

void TestAccessibilityChecker::taggedButNotMarkedIsReported() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    // Structure tree present but /MarkInfo missing — a LOW finding, distinct
    // from the untagged HIGH finding.
    FixtureOpts o;
    o.structRootOnly = true;
    o.lang = true;
    o.title = true;
    o.displayDocTitle = true;
    const QString pdf = make(tmp, "root-only.pdf", o);
    QVERIFY(!pdf.isEmpty());

    const gp::A11yReport r = gp::scanAccessibility(pdf);
    QVERIFY(r.loadOk);
    QVERIFY(r.tagged);
    QCOMPARE(r.findings.size(), 1);
    QCOMPARE(r.findings.first().checkId, QStringLiteral("struct-tree"));
    QVERIFY(r.findings.first().severity == gp::A11ySeverity::Low);
}

void TestAccessibilityChecker::imageSampleIsTruncatedWithDisclosure() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    FixtureOpts o;
    o.tagged = true;
    o.lang = true;
    o.title = true;
    o.displayDocTitle = true;
    o.images = gp::kA11yMaxImageFindings + 7;  // definitely over the cap
    const QString pdf = make(tmp, "many-images.pdf", o);
    QVERIFY(!pdf.isEmpty());

    const gp::A11yReport r = gp::scanAccessibility(pdf);
    QCOMPARE(r.imagesTotal, gp::kA11yMaxImageFindings + 7);
    QCOMPARE(r.imagesReported, gp::kA11yMaxImageFindings);
    QCOMPARE(r.findings.size(), gp::kA11yMaxImageFindings);
    QVERIFY(r.truncated());
}

void TestAccessibilityChecker::unloadableDocumentIsReportedAsSuch() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = tmp.filePath("not-a-pdf.pdf");
    {
        QFile f(pdf);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("this is not a pdf");
    }
    const gp::A11yReport r = gp::scanAccessibility(pdf);
    QVERIFY(!r.loadOk);
    QVERIFY(!r.loadError.isEmpty());
    QVERIFY(r.findings.isEmpty());  // no findings ≠ clean: loadOk gates it
}

#include "TestAccessibilityChecker.moc"
QTEST_MAIN(TestAccessibilityChecker)
