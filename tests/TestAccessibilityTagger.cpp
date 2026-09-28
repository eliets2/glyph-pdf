// SPDX-License-Identifier: Apache-2.0
// T2-4 accessibility P2: pins for the AUTO-TAGGING engine
// (gp::tagDocumentAccessibility + gp::validateTaggedStructureTree +
// gp::preflightTagging).
//
// The acceptance shape follows the design's §6 (design doc:
// docs/research/accessibility-auto-tagging-plan-2026-09-21.md):
//   * the self-owned structural walk validates the tagged tree
//     (/StructTreeRoot → /K → P/H1–H6 elements reference marked-content IDs),
//   * the text-preservation invariant has a fault-seam negative control
//     (a perturbed rewrite MUST fail the candidate, original byte-identical),
//   * the ToUnicode honesty gate skips undecodable pages and DISCLOSES,
//   * /Alt is never invented: described images (through the real P1 fix
//     seam) become Figures, undescribed ones stay out and are counted,
//   * already-tagged documents are refused; commit faults leave the original
//     byte-identical; the corrupted-tree control fails BOTH verifiers.
//
// Fixtures are hand-built with raw content streams + raw font dicts (the
// checker-fixture idiom): each seeded input is deterministic — standard-14
// Helvetica/Helvetica-Bold fonts (honestly decodable without /ToUnicode, the
// design's §3.2 "standard-14 encodings otherwise"), an Identity-H font
// WITHOUT /ToUnicode for the honesty gate, and image XObjects for the Alt
// policy. Nothing here depends on a real-world PDF.
//
// NOTE (veraPDF): the §6.2 subprocess checks live in this suite's
// corruptedTree + veraPdf tests: the CLI is located at runtime
// (VeraPdfValidator::locateCli) and CLI-dependent pins QSKIP when absent —
// the offline structural-walk teeth always run.
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QCryptographicHash>
#include <podofo/podofo.h>

#include "engines/AccessibilityTagger.h"
#include "engines/AccessibilityChecker.h"
#include "engines/AccessibilityFixes.h"
#include "engines/SafeSave.h"
#include "engines/VeraPdfValidator.h"
#include "engines/pdfium/PdfiumBackend.h"

using PoDoFo::PdfObject;
using PoDoFo::PdfName;
using PoDoFo::PdfString;
using PoDoFo::PdfDictionary;
using PoDoFo::PdfArray;

namespace {

// ── fixture builder ──────────────────────────────────────────────────────────

struct TextLine {   // one BT … ET block, standard-14 font
    double y;
    double size;
    const char* text;
    bool bold = false;
    double x = 60;
};

struct FixtureOpts {
    QList<QList<TextLine>> pages;// per-page lines
    QList<QByteArray> extra;     // per-page RAW content appended after lines
    int images = 0;              // image XObjects "Im0…" on page 0 (no /Alt)
    bool drawImages = false;     // reference the images via `q … cm /ImN Do Q`
    bool rawFontOnLastPage = false; // add the no-ToUnicode Identity-H font
                                    // (resource /FX) + one show with it
};

QByteArray pageContent(const QList<TextLine>& lines, const FixtureOpts& o,
                       int pageIndex, bool withRawFont) {
    QByteArray c;
    for (const TextLine& l : lines) {
        c += "BT\n";
        c += QByteArray("/") + (l.bold ? "F2" : "F1") + " "
             + QByteArray::number(l.size, 'f') + " Tf\n";
        c += QByteArray::number(l.x, 'f') + " " + QByteArray::number(l.y, 'f')
             + " Td\n";
        c += QByteArray("(") + l.text + ") Tj\n";
        c += "ET\n";
    }
    if (o.images > 0 && o.drawImages) {
        for (int i = 0; i < o.images; ++i) {
            c += "q\n100 0 0 100 50 " + QByteArray::number(50 + i * 120)
                 + " cm\n/Im" + QByteArray::number(i) + " Do\nQ\n";
        }
    }
    if (pageIndex < o.extra.size())
        c += o.extra.at(pageIndex);
    if (withRawFont) {
        // A show with the undecodable font (2-byte codes <0001 0002>).
        c += "BT\n/FX 10 Tf\n60 60 Td\n<00010002> Tj\nET\n";
    }
    return c;
}

void addStandardFont(PoDoFo::PdfMemDocument& doc, PdfDictionary& fontDictRes,
                     const char* name, const char* baseFont) {
    auto& font = doc.GetObjects().CreateDictionaryObject();
    font.GetDictionary().AddKey("Type", PdfObject(PdfName("Font")));
    font.GetDictionary().AddKey("Subtype", PdfObject(PdfName("Type1")));
    font.GetDictionary().AddKey("BaseFont", PdfObject(PdfName(baseFont)));
    fontDictRes.AddKey(PdfName(name), font.GetIndirectReference());
}

// The deliberately undecodable Type0/Identity-H font: NO /ToUnicode (the
// design's §3.2 honesty-gate fixture: text MUST NOT be extracted).
void addRawFont(PoDoFo::PdfMemDocument& doc, PdfDictionary& fontDictRes) {
    auto& cid = doc.GetObjects().CreateDictionaryObject();
    cid.GetDictionary().AddKey("Type", PdfObject(PdfName("Font")));
    cid.GetDictionary().AddKey("Subtype", PdfObject(PdfName("CIDFontType2")));
    cid.GetDictionary().AddKey("BaseFont", PdfObject(PdfName("AAAAAA+Secret")));
    PdfDictionary sysinfo;
    sysinfo.AddKey("Registry", PdfString("Adobe"));
    sysinfo.AddKey("Ordering", PdfString("Identity"));
    sysinfo.AddKey("Supplement", PdfObject(static_cast<int64_t>(0)));
    cid.GetDictionary().AddKey("CIDSystemInfo", PdfObject(sysinfo));
    cid.GetDictionary().AddKey("DW", PdfObject(static_cast<int64_t>(1000)));

    auto& type0 = doc.GetObjects().CreateDictionaryObject();
    type0.GetDictionary().AddKey("Type", PdfObject(PdfName("Font")));
    type0.GetDictionary().AddKey("Subtype", PdfObject(PdfName("Type0")));
    type0.GetDictionary().AddKey("BaseFont",
                                 PdfObject(PdfName("AAAAAA+Secret")));
    type0.GetDictionary().AddKey("Encoding", PdfObject(PdfName("Identity-H")));
    PdfArray descendants;
    descendants.Add(cid.GetIndirectReference());
    type0.GetDictionary().AddKey("DescendantFonts", PdfObject(descendants));

    fontDictRes.AddKey(PdfName("FX"), type0.GetIndirectReference());
}

void addImage(PoDoFo::PdfMemDocument& doc, PdfDictionary& xobjs,
              const std::string& name) {
    auto& img = doc.GetObjects().CreateDictionaryObject();
    img.GetDictionary().AddKey("Type", PdfObject(PdfName("XObject")));
    img.GetDictionary().AddKey("Subtype", PdfObject(PdfName("Image")));
    img.GetDictionary().AddKey("Width", PdfObject(static_cast<int64_t>(4)));
    img.GetDictionary().AddKey("Height", PdfObject(static_cast<int64_t>(4)));
    img.GetDictionary().AddKey("ColorSpace", PdfObject(PdfName("DeviceGray")));
    img.GetDictionary().AddKey("BitsPerComponent",
                               PdfObject(static_cast<int64_t>(8)));
    const unsigned char px[16] = {0};
    img.GetOrCreateStream().SetData(
        PoDoFo::bufferview(reinterpret_cast<const char*>(px), sizeof(px)));
    xobjs.AddKey(PdfName(name), img.GetIndirectReference());
}

bool buildPdf(const QString& path, const FixtureOpts& o) {
    try {
        PoDoFo::PdfMemDocument doc;
        for (int pi = 0; pi < o.pages.size(); ++pi) {
            auto& page = doc.GetPages().CreatePage(
                PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));

            const bool withRawFont =
                o.rawFontOnLastPage && pi == o.pages.size() - 1;

            PdfDictionary fonts;
            addStandardFont(doc, fonts, "F1", "Helvetica");
            addStandardFont(doc, fonts, "F2", "Helvetica-Bold");
            if (withRawFont) addRawFont(doc, fonts);
            page.GetResources().GetDictionary().AddKey("Font",
                                                       PdfObject(fonts));

            if (o.images > 0) {
                PdfDictionary xobjs;
                for (int i = 0; i < o.images; ++i)
                    addImage(doc, xobjs, "Im" + std::to_string(i));
                page.GetResources().GetDictionary().AddKey("XObject",
                                                           PdfObject(xobjs));
            }

            auto& contents = page.GetOrCreateContents();
            auto& stream = contents.CreateStreamForAppending(
                PoDoFo::PdfStreamAppendFlags::None);
            const QByteArray c = pageContent(o.pages.at(pi), o, pi, withRawFont);
            stream.SetData(PoDoFo::bufferview(c.constData(),
                                              static_cast<size_t>(c.size())));
        }

        doc.Save(path.toUtf8().constData());
        return true;
    } catch (const PoDoFo::PdfError& e) {
        qWarning() << "buildPdf failed:" << e.what();
        return false;
    } catch (...) {
        return false;
    }
}

QTemporaryDir& fixtureDir() {
    static QTemporaryDir dir;
    static const bool ok = dir.isValid();
    Q_ASSERT(ok);
    return dir;
}

// PR-review §3.2 fixture: a taggable PDF that also carries a signature
// field. With signedField the widget's /V resolves to a dictionary with
// /ByteRange (the marker every consumer — SignatureManager included — uses
// for "already signed"); without it the field stays unsigned so the pin
// proves the refusal is about SIGNED fields, not signature fields at all.
bool makeSignedTaggablePdf(const QString& path, bool signedField) {
    try {
        PoDoFo::PdfMemDocument doc;
        auto& page = doc.GetPages().CreatePage(
            PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
        PdfDictionary fonts;
        addStandardFont(doc, fonts, "F1", "Helvetica");
        page.GetResources().GetDictionary().AddKey("Font", PdfObject(fonts));
        auto& contents = page.GetOrCreateContents();
        auto& stream = contents.CreateStreamForAppending(
            PoDoFo::PdfStreamAppendFlags::None);
        const char* c =
            "BT\n/F1 10 Tf\n60 700 Td\n(Signed taggable line.) Tj\nET\n";
        stream.SetData(PoDoFo::bufferview(c, std::strlen(c)));

        PoDoFo::PdfArray rect;
        rect.Add(60.0); rect.Add(600.0); rect.Add(300.0); rect.Add(650.0);
        auto& widget = doc.GetObjects().CreateDictionaryObject();
        widget.GetDictionary().AddKey("Type", PdfObject(PdfName("Annot")));
        widget.GetDictionary().AddKey("Subtype", PdfObject(PdfName("Widget")));
        widget.GetDictionary().AddKey("FT", PdfObject(PdfName("Sig")));
        widget.GetDictionary().AddKey("T", PdfObject(PdfString("Sig1")));
        widget.GetDictionary().AddKey("Rect", PdfObject(rect));
        if (signedField) {
            PoDoFo::PdfArray byteRange;
            byteRange.Add(static_cast<int64_t>(0));
            byteRange.Add(static_cast<int64_t>(120));
            byteRange.Add(static_cast<int64_t>(220));
            byteRange.Add(static_cast<int64_t>(80));
            auto& sigVal = doc.GetObjects().CreateDictionaryObject();
            sigVal.GetDictionary().AddKey("Type", PdfObject(PdfName("Sig")));
            sigVal.GetDictionary().AddKey(
                "Filter", PdfObject(PdfName("Adobe.PPKLite")));
            sigVal.GetDictionary().AddKey(
                "Contents", PdfObject(PdfString("sig-placeholder-bytes")));
            sigVal.GetDictionary().AddKey("ByteRange", PdfObject(byteRange));
            widget.GetDictionary().AddKey("V", sigVal.GetIndirectReference());
        }
        PoDoFo::PdfArray annots;
        annots.Add(widget.GetIndirectReference());
        page.GetDictionary().AddKey("Annots", PdfObject(annots));
        auto& acro = doc.GetObjects().CreateDictionaryObject();
        PoDoFo::PdfArray fields;
        fields.Add(widget.GetIndirectReference());
        acro.GetDictionary().AddKey("Fields", PdfObject(fields));
        doc.GetCatalog().GetDictionary().AddKey("AcroForm",
                                                acro.GetIndirectReference());
        doc.Save(path.toUtf8().constData());
        return true;
    } catch (...) {
        return false;
    }
}

QString make(const QString& name, const FixtureOpts& o) {
    const QString path = fixtureDir().filePath(name);
    if (!buildPdf(path, o)) return {};
    return path;
}

// ── independent tree-walk helpers (the test's OWN PoDoFo reopen; the
//    engine's verifier is gp::validateTaggedStructureTree) ───────────────────

const PdfObject* resolve(const PdfObject* obj, PoDoFo::PdfMemDocument& doc) {
    if (obj == nullptr) return nullptr;
    if (obj->IsReference()) {
        try {
            return &doc.GetObjects().MustGetObject(obj->GetReference());
        } catch (const PoDoFo::PdfError&) {
            return nullptr;
        }
    }
    return obj;
}

struct ElementView {
    QString type;        // /S: "P", "H1".."H6", "Figure"
    QList<int> mcids;    // /K, normalized to a list
    bool hasAlt = false;
    QString alt;
};

// Top-level structure elements in reading order (flat /K of the root).
QList<ElementView> rootElements(const QString& path, bool* markedInfo,
                                QString* err) {
    QList<ElementView> out;
    if (markedInfo) *markedInfo = false;
    try {
        PoDoFo::PdfMemDocument doc;
        doc.Load(path.toUtf8().constData());
        const PdfObject* root = resolve(
            doc.GetCatalog().GetDictionary().FindKey(PdfName("StructTreeRoot")),
            doc);
        if (root == nullptr || !root->IsDictionary()) {
            if (err) *err = QStringLiteral("no /StructTreeRoot");
            return out;
        }
        const PdfObject* markInfo = resolve(
            doc.GetCatalog().GetDictionary().FindKey(PdfName("MarkInfo")), doc);
        const PdfObject* marked =
            markInfo != nullptr && markInfo->IsDictionary()
                ? markInfo->GetDictionary().FindKey(PdfName("Marked"))
                : nullptr;
        if (markedInfo)
            *markedInfo = marked != nullptr && marked->IsBool()
                              && marked->GetBool();

        const PdfObject* kids = resolve(
            root->GetDictionary().FindKey(PdfName("K")), doc);
        if (kids == nullptr || !kids->IsArray()) {
            if (err) *err = QStringLiteral("root /K missing");
            return out;
        }
        for (const PdfObject& k : kids->GetArray()) {
            const PdfObject* el = resolve(&k, doc);
            if (el == nullptr || !el->IsDictionary()) continue;
            ElementView v;
            const PdfObject* s = el->GetDictionary().FindKey(PdfName("S"));
            if (s != nullptr && s->IsName())
                v.type = QString::fromLatin1(
                    s->GetName().GetString().data(),
                    static_cast<qsizetype>(s->GetName().GetString().size()));
            const PdfObject* kk =
                el->GetDictionary().FindKey(PdfName("K"));
            if (kk != nullptr && kk->IsNumber()) {
                v.mcids.append(static_cast<int>(kk->GetNumber()));
            } else if (kk != nullptr && kk->IsArray()) {
                for (const PdfObject& m : kk->GetArray())
                    if (m.IsNumber())
                        v.mcids.append(static_cast<int>(m.GetNumber()));
            }
            const PdfObject* alt = el->GetDictionary().FindKey(PdfName("Alt"));
            if (alt != nullptr && alt->IsString()) {
                v.hasAlt = true;
                v.alt = QString::fromUtf8(
                    alt->GetString().GetString().data(),
                    static_cast<qsizetype>(alt->GetString().GetString().size()));
            }
            out.append(v);
        }
    } catch (const std::exception& e) {
        if (err) *err = QString::fromUtf8(e.what());
    }
    return out;
}

QByteArray sha256(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    return QCryptographicHash::hash(f.readAll(),
                                    QCryptographicHash::Sha256).toHex();
}

} // namespace

class TestAccessibilityTagger : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() {
        QVERIFY2(fixtureDir().isValid(), "Temp directory creation failed");
    }

    // §6.1 — tagged fixture → the walk validates; expected element sequence
    // H1,P,P,H2,P; heading levels match the fixture's size ladder; MCIDs in
    // reading order; /Marked true present.
    void taggedTreeValidatesWithExpectedSequence();

    // §3.4 — already-tagged refusal; original byte-identical after refusal.
    void alreadyTaggedDocumentIsRefused();

    // §2.2/§6.1 — the text-preservation invariant negative control: a
    // perturbed operator during rewrite MUST fail the candidate and leave
    // the original byte-identical.
    void perturbedRewriteFailsCandidateByteIdentical();

    // §6.1 — commit fault mid-tag: original byte-identical.
    void commitFaultLeavesOriginalByteIdentical();

    // §3.2/§6.1 — ToUnicode honesty: the undecodable page is skipped and
    // disclosed, the OTHER page is still tagged, and no /ActualText exists
    // anywhere in the output.
    void toUnicodeHonestySkipsPageAndDiscloses();

    // §4 — Alt policy: the image described through the REAL P1 fix seam
    // becomes a Figure with /Alt; the undescribed one is excluded + counted;
    // an empty-string /Alt is treated as undescribed (never a false claim).
    void altPolicyDescribedFigureUndescribedExcluded();

    // §3.1/§3.5 — paragraph recognition + split-element /K: two shows in one
    // text object of the same paragraph → one P with /K array of 2 MCIDs;
    // a paragraph separated by a gap → its own P with an int /K.
    void paragraphSplittingAndSplitElementK();

    // §3.4 — single-column assumption: two-column signature sets the flag +
    // disclosure; a single-column document does not.
    void columnSuspectSignatureDisclosed();

    // §6.1 — truncation/caps: the pre-flight prompt list is a bounded sample
    // with a disclosed total.
    void preflightImageListTruncatedWithDisclosure();

    // §6.3 (engine half) — pre-flight reports the cluster table; a tagged
    // document preflights as alreadyTagged; a text-less document reports
    // anyText=false; after tagging the P1 checker reports tagged=true with
    // the struct-tree finding resolved.
    void preflightClustersRefusalsAndRescan();

    // §6.1 — nothing taggable (image-only document) → honest refusal; an
    // unloadable path refuses honestly too.
    void nothingTaggableIsRefusedHonesty();

    // §6.2 — the corrupted-tree control: a dropped /ParentTree entry (built
    // by a real scoped mutation of a tagged file) must FAIL the structural
    // walk; the engine-side DropParentTreeEntry seam must self-catch
    // (candidate rejected, original byte-identical).
    void corruptedTreeFailsWalkAndSeamSelfCatches();

    // §6.2 — independent reader: tagged fixtures through the veraPDF
    // subprocess where the bundled CLI is present; the corrupted fixture
    // must FAIL validation (the validator actually reads what we wrote).
    // QSKIP when no CLI is available (same discipline as TestVeraPdf).
    void veraPdfReadsTheTaggedTree();

    // PR-review §3.3 — matrix operand order: a relative Td AFTER a scaled
    // Tm must move by the SCALED offset (12 0 0 12 60 700 Tm then 0 -1.2 Td
    // moves 14.4pt, not 1.2pt). PDF composes the NEW operand first
    // (Tlm′ = T × Tlm; CTM′ = M × CTM); the line-merge clustering sees the
    // two runs as separate lines only when the displacement is honest.
    void scaledTextMatrixRelativeTdMovesScaled();

    // PR-review §3.3 — matrix operand order: a NESTED scaled cm must apply
    // the new operand first (CTM′ = M × CTM). Two runs of the same effective
    // size: one absolute, one under nested scaled cm — the x-left family
    // check (2pt tolerance) splits them into separate paragraphs only when
    // the composed translation is honest.
    void nestedScaledCmComposesNewOperandFirst();

    // PR-review §3.2 — signed-doc refusal: the transaction rewrites all
    // content streams and full-saves in place, which would invalidate every
    // existing signature. A signed fixture is refused at pre-flight AND
    // transaction, byte-identical; the unsigned-field control still tags.
    void signedDocumentTaggingRefusedByteIdentical();

    // CX-01 — tagging must not destroy inline images: the inline dictionary
    // is emitted as BARE key/value pairs (PDF 32000 §8.9.7), the data bytes
    // survive byte-for-byte, and PDFium renders the page pixel-identically
    // before and after (the old text-only invariant never caught this).
    void inlineImageSurvivesTagging();

    // CX-07 — MCIDs inside Form XObjects: a bare integer /K with the page's
    // /Pg is unresolvable (a consumer would search the page stream and find
    // nothing); the tree must emit <</Type/MCR /Pg <page> /Stm <form>
    // /MCID n>>, the form carries /StructParents + a ParentTree entry, and
    // the validator checks /Stm — proved with an independent structure walk
    // over a page + Form fixture and a Form-only fixture.
    void formXObjectMcidsUseMcrReferences();
    void formOnlyMcidsAlsoUseMcrReferences();

private:
    static FixtureOpts headingParagraphFixture() {
        // Exact ladder from the design's §6.1 pin: H1, P, P, H2, P.
        FixtureOpts o;
        o.pages = {QList<TextLine>{
            { 780, 18.0, "Quarterly Report"},              // H1
            { 740, 10.0, "First paragraph line one."},     // P (line 1)
            { 726, 10.0, "line two continues."},           // P (line 2)
            { 680, 10.0, "Second paragraph after a gap."}, // P (gap → new)
            { 640, 14.4, "Section Heading", true},         // H2 (bold)
            { 600, 10.0, "After the heading."},            // P
        }};
        return o;
    }
};

// ── tests ────────────────────────────────────────────────────────────────────

void TestAccessibilityTagger::taggedTreeValidatesWithExpectedSequence() {
    const QString pdf = make("ladder.pdf", headingParagraphFixture());
    QVERIFY(!pdf.isEmpty());

    const gp::TaggerReport r = gp::tagDocumentAccessibility(pdf);
    QVERIFY2(r.ok, qPrintable(r.message));
    QCOMPARE(r.elementsTagged, 5);
    QCOMPARE(r.headingsTagged, 2);
    QCOMPARE(r.paragraphsTagged, 3);

    // The engine's own structural walk validates the saved artifact.
    const QString invalid = gp::validateTaggedStructureTree(pdf);
    QVERIFY2(invalid.isEmpty(), qPrintable(invalid));

    // Independent reopen: exact element sequence + reading-order MCIDs.
    bool marked = false;
    QString err;
    const QList<ElementView> els = rootElements(pdf, &marked, &err);
    QVERIFY2(err.isEmpty(), qPrintable(err));
    QVERIFY(marked);  // /MarkInfo /Marked true (§3.5)
    QCOMPARE(els.size(), 5);
    QCOMPARE(els[0].type, QStringLiteral("H1"));   // 18pt
    QCOMPARE(els[1].type, QStringLiteral("P"));
    QCOMPARE(els[2].type, QStringLiteral("P"));
    QCOMPARE(els[3].type, QStringLiteral("H2"));   // 14.4pt bold
    QCOMPARE(els[4].type, QStringLiteral("P"));
    // MCIDs in reading order: the two-line paragraph carries TWO marked-
    // content sequences (one per BT…ET block — /K is the array §3.5), every
    // other element one. Flattened, the MCIDs are 0..5 in element order.
    const QList<int> expectedMcids = {0, 1, 2, 3, 4, 5};
    QList<int> flattened;
    for (int i = 0; i < els.size(); ++i) {
        QVERIFY2(!els[i].mcids.isEmpty(), qPrintable(els[i].type));
        flattened += els[i].mcids;
    }
    QCOMPARE(flattened, expectedMcids);
    QCOMPARE(els[0].mcids.size(), 1);   // H1: single show
    QCOMPARE(els[1].mcids.size(), 2);   // P: two BT…ET blocks, one element
    QCOMPARE(els[2].mcids.size(), 1);
    QCOMPARE(els[3].mcids.size(), 1);
    QCOMPARE(els[4].mcids.size(), 1);

    // The cluster table is in the report (the user SEES the classification).
    QCOMPARE(r.sizeClusters.size(), 3);  // 18.0, 14.4, 10.0
    bool sawH1 = false, sawH2 = false, sawBody = false;
    for (const auto& c : r.sizeClusters) {
        if (c.level == QStringLiteral("H1")) {
            sawH1 = true;
            QCOMPARE(c.runCount, 1);
        } else if (c.level == QStringLiteral("H2")) {
            sawH2 = true;
            QVERIFY(c.bold);  // the bold hint fired on the 14.4 cluster
        } else if (c.level == QStringLiteral("body")) {
            sawBody = true;
            QCOMPARE(c.runCount, 4);
        }
    }
    QVERIFY(sawH1 && sawH2 && sawBody);
}

void TestAccessibilityTagger::alreadyTaggedDocumentIsRefused() {
    const QString pdf = make("already.pdf", headingParagraphFixture());
    QVERIFY(!pdf.isEmpty());
    QVERIFY(gp::tagDocumentAccessibility(pdf).ok);
    const QByteArray before = sha256(pdf);

    const gp::TaggerReport r2 = gp::tagDocumentAccessibility(pdf);
    QVERIFY2(!r2.ok, "re-tagging must refuse");
    QVERIFY2(r2.message.contains(QLatin1String("already tagged")),
             qPrintable(r2.message));
    QCOMPARE(sha256(pdf), before);  // refusal = byte-identical
}

void TestAccessibilityTagger::perturbedRewriteFailsCandidateByteIdentical() {
    const QString pdf = make("perturb.pdf", headingParagraphFixture());
    QVERIFY(!pdf.isEmpty());
    const QByteArray before = sha256(pdf);

    const gp::TaggerReport r = gp::tagDocumentAccessibility(
        pdf, gp::TaggerFaultForTesting::PerturbOneShowOperator);
    QVERIFY2(!r.ok, "a perturbed rewrite MUST fail the transaction");
    QVERIFY2(!r.message.isEmpty(), qPrintable(r.message));
    QCOMPARE(sha256(pdf), before);  // original left byte-identical

    // …and the untouched original still validates as "no tree" (honest
    // reason), never as a broken tree.
    const QString invalid = gp::validateTaggedStructureTree(pdf);
    QVERIFY2(!invalid.isEmpty(), qPrintable(invalid));
    QVERIFY(invalid.contains(QLatin1String("StructTreeRoot")));
}

void TestAccessibilityTagger::commitFaultLeavesOriginalByteIdentical() {
    const QString pdf = make("commitfault.pdf", headingParagraphFixture());
    QVERIFY(!pdf.isEmpty());
    const QByteArray before = sha256(pdf);

    gp::SafeSave::setCommitFaultForTesting(
        gp::SafeSave::CommitFaultForTesting::FailBeforeCommit);
    const gp::TaggerReport r = gp::tagDocumentAccessibility(pdf);
    gp::SafeSave::setCommitFaultForTesting(
        gp::SafeSave::CommitFaultForTesting::None);

    QVERIFY2(!r.ok, "commit fault must fail the transaction");
    QVERIFY2(!r.message.isEmpty(), qPrintable(r.message));
    QCOMPARE(sha256(pdf), before);  // the destination is untouched
}

void TestAccessibilityTagger::toUnicodeHonestySkipsPageAndDiscloses() {
    FixtureOpts o;
    o.pages = {
        QList<TextLine>{{ 700, 10.0, "Normal page text."}},
        QList<TextLine>{{ 700, 10.0, "Secret page text."}},
    };
    o.rawFontOnLastPage = true;  // page 2 also shows /FX <00010002> Tj
    const QString pdf = make("honesty.pdf", o);
    QVERIFY(!pdf.isEmpty());

    const gp::TaggerReport r = gp::tagDocumentAccessibility(pdf);
    QVERIFY2(r.ok, qPrintable(r.message));  // page 1 IS taggable

    // The undecodable page is skipped and DISCLOSED, naming the font.
    bool disclosed = false;
    for (const auto& n : r.pageNotes) {
        if (n.page == 1) {
            disclosed = true;
            QVERIFY2(n.note.contains(QLatin1String("not tagged")),
                     qPrintable(n.note));
            QVERIFY2(n.note.contains(QLatin1String("Secret")),
                     qPrintable(n.note));
        }
    }
    QVERIFY2(disclosed, "page 2 must carry a skip disclosure");

    // The tree contains elements ONLY for page 1.
    const QString invalid = gp::validateTaggedStructureTree(pdf);
    QVERIFY2(invalid.isEmpty(), qPrintable(invalid));
    bool marked = false;
    QString err;
    const QList<ElementView> els = rootElements(pdf, &marked, &err);
    QVERIFY2(err.isEmpty(), qPrintable(err));
    QCOMPARE(els.size(), 1);  // only the honest page's paragraph
    QCOMPARE(els[0].type, QStringLiteral("P"));

    // No /ActualText anywhere in P2 output (P3 candidate, never emitted).
    try {
        PoDoFo::PdfMemDocument doc;
        doc.Load(pdf.toUtf8().constData());
        for (const PdfObject* obj : doc.GetObjects()) {
            if (obj == nullptr || !obj->IsDictionary()) continue;
            // Direct dictionary keys only is enough: the engine writes no
            // /ActualText anywhere, so none can exist at any level.
            QVERIFY2(obj->GetDictionary().FindKey(PdfName("ActualText"))
                         == nullptr,
                     "P2 must never emit /ActualText");
        }
    } catch (const std::exception& e) {
        QFAIL(qPrintable(QString::fromUtf8(e.what())));
    }
}

void TestAccessibilityTagger::altPolicyDescribedFigureUndescribedExcluded() {
    FixtureOpts o;
    o.pages = {QList<TextLine>{{ 700, 10.0, "A page with two images."}}};
    o.images = 2;
    o.drawImages = true;
    const QString pdf = make("alts.pdf", o);
    QVERIFY(!pdf.isEmpty());

    // Describe ONE image through the REAL P1 fix seam (§4.2). The other one
    // gets an EMPTY description through the same seam — which must be
    // treated as undescribed (an empty /Alt is a false statement).
    gp::A11yFixRequest describe;
    describe.kind = gp::A11yFixKind::SetImageAltText;
    describe.page = 0;
    describe.resourceName = QStringLiteral("Im1");
    describe.text = QStringLiteral("a bar chart of quarterly sales");
    const gp::A11yFixOutcome out = gp::applyAccessibilityFix(pdf, describe);
    QVERIFY2(out.ok, qPrintable(out.message));

    gp::A11yFixRequest empty;
    empty.kind = gp::A11yFixKind::SetImageAltText;
    empty.page = 0;
    empty.resourceName = QStringLiteral("Im0");
    empty.text = QStringLiteral("   ");  // whitespace only → empty
    const gp::A11yFixOutcome out2 = gp::applyAccessibilityFix(pdf, empty);
    QVERIFY2(out2.ok, qPrintable(out2.message));

    const gp::TaggerReport r = gp::tagDocumentAccessibility(pdf);
    QVERIFY2(r.ok, qPrintable(r.message));
    QCOMPARE(r.figuresTagged, 1);
    QCOMPARE(r.imagesExcluded, 1);  // the empty-/Alt image is excluded + counted

    bool marked = false;
    QString err;
    const QList<ElementView> els = rootElements(pdf, &marked, &err);
    QVERIFY2(err.isEmpty(), qPrintable(err));
    int figures = 0;
    for (const ElementView& el : els) {
        if (el.type == QStringLiteral("Figure")) {
            ++figures;
            QVERIFY(el.hasAlt);
            QCOMPARE(el.alt, QStringLiteral("a bar chart of quarterly sales"));
        }
    }
    QCOMPARE(figures, 1);  // exactly one Figure; no placeholder Figures
}

void TestAccessibilityTagger::paragraphSplittingAndSplitElementK() {
    FixtureOpts o;
    o.pages = {QList<TextLine>{{ 600, 10.0, "separate paragraph"}}};
    // Two shows in ONE text object: Td between them, same size, normal
    // leading → same paragraph, but TWO marked-content sequences.
    o.extra = {QByteArray(
        "BT\n/F1 10 Tf\n60 700 Td\n(chunk one) Tj\n0 -14 Td\n"
        "(chunk two) Tj\nET\n")};
    const QString pdf = make("split.pdf", o);
    QVERIFY(!pdf.isEmpty());

    const gp::TaggerReport r = gp::tagDocumentAccessibility(pdf);
    QVERIFY2(r.ok, qPrintable(r.message));
    QCOMPARE(r.paragraphsTagged, 2);
    QCOMPARE(r.elementsTagged, 2);

    bool marked = false;
    QString err;
    const QList<ElementView> els = rootElements(pdf, &marked, &err);
    QVERIFY2(err.isEmpty(), qPrintable(err));
    QCOMPARE(els.size(), 2);
    // Element 0: the split paragraph — /K is an ARRAY of two MCIDs.
    QCOMPARE(els[0].type, QStringLiteral("P"));
    auto mcidStr = [](const QList<int>& v) {
        QStringList parts;
        for (int m : v) parts << QString::number(m);
        return parts.join(QLatin1Char(','));
    };
    QVERIFY2(els[0].mcids.size() == 2,
             qPrintable(QStringLiteral("el0=[%1] el1=[%2]")
                            .arg(mcidStr(els[0].mcids), mcidStr(els[1].mcids))));
    QCOMPARE(els[0].mcids[0], 0);
    QCOMPARE(els[0].mcids[1], 1);
    // Element 1: its own paragraph — /K is one MCID.
    QCOMPARE(els[1].type, QStringLiteral("P"));
    QCOMPARE(els[1].mcids.size(), 1);
    QCOMPARE(els[1].mcids.first(), 2);
}

void TestAccessibilityTagger::columnSuspectSignatureDisclosed() {
    // Two x-left families, same baselines → the column-suspect signature.
    FixtureOpts twoCol;
    twoCol.pages = {QList<TextLine>{
        { 700, 10.0, "left col line one.", false, 50.0},
        { 700, 10.0, "right col line one.", false, 350.0},
        { 686, 10.0, "left col line two.", false, 50.0},
        { 686, 10.0, "right col line two.", false, 350.0},
    }};
    const QString colPdf = make("twocol.pdf", twoCol);
    QVERIFY(!colPdf.isEmpty());
    const gp::TaggerReport r = gp::tagDocumentAccessibility(colPdf);
    QVERIFY2(r.ok, qPrintable(r.message));
    QVERIFY2(r.columnSuspect, "two-column signature must be flagged");
    bool noteFound = false;
    for (const auto& n : r.pageNotes) {
        if (n.page == 0
            && n.note.contains(QLatin1String("column"))) {
            noteFound = true;
            QVERIFY2(n.note.contains(QLatin1String("review")),
                     qPrintable(n.note));
        }
    }
    QVERIFY2(noteFound, "the column suspicion must be disclosed per page");

    // Single column → no suspicion.
    const QString onePdf = make("onecol.pdf", headingParagraphFixture());
    QVERIFY(!onePdf.isEmpty());
    const gp::TaggerReport r2 = gp::tagDocumentAccessibility(onePdf);
    QVERIFY2(r2.ok, qPrintable(r2.message));
    QVERIFY(!r2.columnSuspect);
}

void TestAccessibilityTagger::preflightImageListTruncatedWithDisclosure() {
    FixtureOpts o;
    o.pages = {QList<TextLine>{{ 700, 10.0, "Many images."}}};
    o.images = gp::kA11yMaxImageFindings + 5;
    const QString pdf = make("manyimg.pdf", o);
    QVERIFY(!pdf.isEmpty());

    const gp::TaggerPreflight p = gp::preflightTagging(pdf);
    QVERIFY(p.loadOk);
    QVERIFY(!p.alreadyTagged);
    QCOMPARE(p.imageGaps.size(), gp::kA11yMaxImageFindings);  // bounded sample
    QCOMPARE(p.imagesTotal, gp::kA11yMaxImageFindings + 5);   // disclosed total
}

void TestAccessibilityTagger::preflightClustersRefusalsAndRescan() {
    const QString pdf = make("preflight.pdf", headingParagraphFixture());
    QVERIFY(!pdf.isEmpty());

    const gp::TaggerPreflight p = gp::preflightTagging(pdf);
    QVERIFY(p.loadOk);
    QVERIFY(!p.alreadyTagged);
    QVERIFY(p.anyText);
    // The cluster table: 18 → H1, 14.4 → H2, 10 → body.
    bool sawH1 = false, sawH2 = false, sawBody = false;
    for (const auto& c : p.sizeClusters) {
        if (c.size > 17.0) {
            QCOMPARE(c.level, QStringLiteral("H1"));
            sawH1 = true;
        } else if (c.size > 14.0) {
            QCOMPARE(c.level, QStringLiteral("H2"));
            sawH2 = true;
        } else if (c.size < 11.0) {
            QCOMPARE(c.level, QStringLiteral("body"));
            sawBody = true;
        }
    }
    QVERIFY(sawH1 && sawH2 && sawBody);

    // Tag → the P1 checker reports the document tagged, struct-tree finding
    // resolved; pre-flight now refuses (alreadyTagged).
    QVERIFY(gp::tagDocumentAccessibility(pdf).ok);
    const gp::A11yReport scan = gp::scanAccessibility(pdf);
    QVERIFY(scan.loadOk);
    QVERIFY(scan.tagged);
    for (const auto& f : scan.findings)
        QVERIFY2(f.checkId != QLatin1String("struct-tree"),
                 "the struct-tree finding must be resolved after tagging");
    const gp::TaggerPreflight p2 = gp::preflightTagging(pdf);
    QVERIFY(p2.alreadyTagged);

    // A text-less document: no taggable runs.
    FixtureOpts imgOnly;
    imgOnly.pages = {QList<TextLine>{}};
    imgOnly.images = 1;
    const QString imgPdf = make("imgonly.pdf", imgOnly);
    QVERIFY(!imgPdf.isEmpty());
    const gp::TaggerPreflight p3 = gp::preflightTagging(imgPdf);
    QVERIFY(p3.loadOk);
    QVERIFY(!p3.anyText);
}

void TestAccessibilityTagger::nothingTaggableIsRefusedHonesty() {
    FixtureOpts o;
    o.pages = {QList<TextLine>{}};  // no text at all
    o.images = 1;
    const QString pdf = make("notag.pdf", o);
    QVERIFY(!pdf.isEmpty());
    const QByteArray before = sha256(pdf);

    const gp::TaggerReport r = gp::tagDocumentAccessibility(pdf);
    QVERIFY2(!r.ok, "a document with no text runs must refuse");
    QVERIFY2(!r.message.isEmpty(), qPrintable(r.message));
    QCOMPARE(sha256(pdf), before);

    // Unloadable path refuses honestly too.
    const gp::TaggerReport r2 = gp::tagDocumentAccessibility(
        fixtureDir().filePath("does-not-exist.pdf"));
    QVERIFY(!r2.ok);
    QVERIFY2(!r2.message.isEmpty(), qPrintable(r2.message));
}

void TestAccessibilityTagger::corruptedTreeFailsWalkAndSeamSelfCatches() {
    // (a) The engine seam self-catches: the dropped /ParentTree entry never
    // commits; the original stays byte-identical.
    const QString seamPdf = make("seam-corrupt.pdf", headingParagraphFixture());
    QVERIFY(!seamPdf.isEmpty());
    const QByteArray seamBefore = sha256(seamPdf);
    const gp::TaggerReport r = gp::tagDocumentAccessibility(
        seamPdf, gp::TaggerFaultForTesting::DropParentTreeEntry);
    QVERIFY2(!r.ok, "the corrupted-tree seam MUST be self-caught");
    QCOMPARE(sha256(seamPdf), seamBefore);

    // (b) The teeth: a REALLY committed corrupted tree must FAIL the walk.
    const QString pdf = make("corrupt.pdf", headingParagraphFixture());
    QVERIFY(!pdf.isEmpty());
    QVERIFY(gp::tagDocumentAccessibility(pdf).ok);
    const QString corrupt = fixtureDir().filePath("corrupted-copy.pdf");
    {
        PoDoFo::PdfMemDocument doc;
        try {
            doc.Load(pdf.toUtf8().constData());
        } catch (const std::exception& e) {
            QFAIL(qPrintable(QString::fromUtf8(e.what())));
        }
        PdfObject* root = doc.GetCatalog().GetDictionary().FindKey(
            PdfName("StructTreeRoot"));
        if (root != nullptr && root->IsReference())
            root = &doc.GetObjects().MustGetObject(root->GetReference());
        QVERIFY(root != nullptr && root->IsDictionary());
        PdfObject* parentTree = root->GetDictionary().FindKey(
            PdfName("ParentTree"));
        if (parentTree != nullptr && parentTree->IsReference())
            parentTree = &doc.GetObjects().MustGetObject(
                parentTree->GetReference());
        QVERIFY(parentTree != nullptr && parentTree->IsDictionary());
        PdfObject* nums = parentTree->GetDictionary().FindKey(PdfName("Nums"));
        QVERIFY(nums != nullptr && nums->IsArray());
        PdfArray& numsArr = nums->GetArray();
        QVERIFY(numsArr.GetSize() >= 2);
        // Nums = [key0, arr0, key1, arr1 …]: drop the LAST element ref from
        // the first value array → one MCID loses its ParentTree entry.
        PdfObject* valueArr = &numsArr[1];
        if (valueArr->IsReference())
            valueArr = &doc.GetObjects().MustGetObject(valueArr->GetReference());
        QVERIFY(valueArr != nullptr && valueArr->IsArray());
        QVERIFY(valueArr->GetArray().GetSize() > 0);
        valueArr->GetArray().RemoveAt(valueArr->GetArray().GetSize() - 1);
        try {
            doc.Save(corrupt.toUtf8().constData());
        } catch (const std::exception& e) {
            QFAIL(qPrintable(QString::fromUtf8(e.what())));
        }
    }
    const QString invalid = gp::validateTaggedStructureTree(corrupt);
    QVERIFY2(!invalid.isEmpty(),
             "the walk MUST reject a tree with a dropped /ParentTree entry");
}

void TestAccessibilityTagger::veraPdfReadsTheTaggedTree() {
    if (!gp::VeraPdfValidator::isAvailable())
        QSKIP("veraPDF CLI not available offline (recorded CLI-dependence)");

    const QString pdf = make("verapdf.pdf", headingParagraphFixture());
    QVERIFY(!pdf.isEmpty());
    QVERIFY(gp::tagDocumentAccessibility(pdf).ok);

    // The corrupted fixture (same scoped mutation as the teeth pin).
    const QString corrupt = fixtureDir().filePath("corrupted-copy.pdf");
    QVERIFY(QFile::exists(corrupt));

    // Raw subprocess runs (same shape as TestVeraPdf's helper), through the
    // CLI VeraPdfValidator locates. veraPDF 1.26+ ships the ua1 flavour; a
    // CLI that rejects it is recorded as CLI-dependent and the PDF/A-2b
    // stand-in keeps the pipeline honest (the plan does not gate P2 on a
    // CLI upgrade).
    auto runCli = [&](const QString& target, const char* flavour) -> QByteArray {
        QProcess proc;
        proc.setProcessChannelMode(QProcess::MergedChannels);
        QStringList args;
        const QString cli = gp::VeraPdfValidator::locateCli();
        const QFileInfo cliInfo(cli);
        // Preferred shape: the bundled .bat launcher wraps a private JRE —
        // invoke the GreenfieldCliWrapper main class DIRECTLY (same jar,
        // same arguments) so the test does not depend on a shell.
        const QString javaExe = cliInfo.absolutePath()
                                + QStringLiteral("/jre/bin/java.exe");
        if (QFileInfo::exists(javaExe)) {
            const QString base = cliInfo.absolutePath();
            args << QStringLiteral("-classpath")
                 << base + QStringLiteral("/etc;") + base + QStringLiteral("/bin/*")
                 << QStringLiteral("-Dfile.encoding=UTF8")
                 << QStringLiteral("org.verapdf.apps.GreenfieldCliWrapper")
                 << QStringLiteral("--flavour") << QString::fromLatin1(flavour)
                 << QStringLiteral("--format") << QStringLiteral("json")
                 << target;
            proc.start(javaExe, args);
        } else if (cli.endsWith(QLatin1String(".bat"), Qt::CaseInsensitive)
                   || cli.endsWith(QLatin1String(".cmd"), Qt::CaseInsensitive)) {
            // Fallback: shell launcher (same shape as TestVeraPdf).
            args << QStringLiteral("/c") << QStringLiteral("call") << cli
                 << QStringLiteral("--flavour") << QString::fromLatin1(flavour)
                 << QStringLiteral("--format") << QStringLiteral("json")
                 << target;
            proc.start(QStringLiteral("cmd.exe"), args);
        } else {
            args << QStringLiteral("--flavour") << QString::fromLatin1(flavour)
                 << QStringLiteral("--format") << QStringLiteral("json")
                 << target;
            proc.start(cli, args);
        }
        if (!proc.waitForStarted(15000)) return {};
        if (!proc.waitForFinished(120000)) {
            proc.kill();
            return {};
        }
        return proc.readAllStandardOutput();
    };
    auto validReport = [&](const QByteArray& json) -> QJsonObject {
        const QJsonDocument doc = QJsonDocument::fromJson(json);
        if (!doc.isObject()) return {};
        const QJsonObject report =
            doc.object().value(QLatin1String("report")).toObject();
        const QJsonArray jobs = report.value(QLatin1String("jobs")).toArray();
        if (jobs.isEmpty()) return {};
        return jobs.at(0).toObject();
    };

    QJsonObject goodJob = validReport(runCli(pdf, "ua1"));
    if (goodJob.isEmpty()) {
        // The bundled CLI lacks the UA-1 flavour → PDF/A-2b stand-in run;
        // the UA pin is recorded as CLI-dependent in the skip text path
        // only if even that produces nothing (then the offline walk above
        // remains the sole verifier).
        goodJob = validReport(runCli(pdf, "2b"));
        if (goodJob.isEmpty())
            QSKIP("veraPDF CLI present but produced no usable report "
                  "(UA-1 dependence recorded)");
    }
    QVERIFY2(!goodJob.contains(QLatin1String("taskException")),
             "the tagged fixture must be parseable by veraPDF");

    // The corrupted fixture must FAIL the same profile run — either an
    // explicit parse exception or failed rules — never a silent pass.
    QJsonObject badJob = validReport(runCli(corrupt, "ua1"));
    if (badJob.isEmpty()) badJob = validReport(runCli(corrupt, "2b"));
    QVERIFY2(!badJob.isEmpty(), "veraPDF produced no report for the corrupt fixture");
    bool failed = badJob.contains(QLatin1String("taskException"));
    if (!failed) {
        const QJsonArray results =
            badJob.value(QLatin1String("validationResult")).toArray();
        for (const QJsonValue& v : results) {
            const QJsonObject res = v.toObject();
            const QJsonObject details =
                res.value(QLatin1String("details")).toObject();
            if (details.value(QLatin1String("failedRules")).toInt() > 0
                || res.value(QLatin1String("status")).toString()
                       == QLatin1String("failed"))
                failed = true;
        }
    }
    QVERIFY2(failed, "the corrupted tree must FAIL veraPDF validation");
}

// ── PR-review §3.3: matrix operand order ─────────────────────────────────────
//
// Mat::operator* is the column-vector convention: (m * n) applied to p ==
// m applied to (n applied to p). PDF composes the NEW operand FIRST
// (CTM′ = M × CTM; Tlm′ = T × Tlm), so the accumulated matrix must sit on
// the LEFT and the new operand on the RIGHT at every composition site.
// The text-preservation invariant cannot catch a wrong order (the rewrite
// replays the same walk both sides), so these fixtures ARE the guard.
//
// Observable: run positions feed the line-merge tolerance
// (max(1.0, 0.5 × modalSize × 1.2)) and the paragraph x-left family (2pt).
// A wrong composition puts runs where the honest math would not.

void TestAccessibilityTagger::scaledTextMatrixRelativeTdMovesScaled() {
    // 12 0 0 12 60 700 Tm scales every subsequent text-space unit by 12:
    // the relative 0 -1.2 Td must move the second run 14.4pt down (y 685.6),
    // not 1.2pt (y 698.8). Tf 1 keeps the effective size at 12pt so the
    // line-merge tolerance is max(1.0, 0.5×12×1.2) = 7.2 — the honest
    // 14.4pt displacement splits the runs into TWO lines (cluster lineCount
    // 2); the un-scaled 1.2pt displacement merges them into one.
    FixtureOpts o;
    o.pages = {QList<TextLine>{}};
    o.extra = {
        "BT\n"
        "/F1 1 Tf\n"
        "12 0 0 12 60 700 Tm\n"
        "(AAAA) Tj\n"
        "0 -1.2 Td\n"
        "(BBBB) Tj\n"
        "ET\n",
    };
    const QString pdf = make("tm-td-scaled.pdf", o);
    QVERIFY(!pdf.isEmpty());

    const gp::TaggerPreflight p = gp::preflightTagging(pdf);
    QVERIFY(p.loadOk);
    QVERIFY(p.anyText);
    QVERIFY2(p.sizeClusters.size() == 1,
             qPrintable(QStringLiteral("expected one size cluster, got %1")
                            .arg(p.sizeClusters.size())));
    const gp::TaggerSizeCluster& c = p.sizeClusters.first();
    QVERIFY2(std::abs(c.size - 12.0) < 0.5,
             qPrintable(QStringLiteral("effective size must be 12 (Tf 1 × Tm 12), got %1")
                            .arg(c.size)));
    QVERIFY2(c.runCount == 2,
             qPrintable(QStringLiteral("the Td displacement must be SCALED by the "
                                      "text matrix (14.4pt = 2 lines), but the "
                                      "cluster saw %1 line(s)")
                            .arg(c.runCount)));

    // The full transaction stays consistent with the same walk: tagging
    // succeeds and the structural verifier accepts the tree.
    const gp::TaggerReport r = gp::tagDocumentAccessibility(pdf);
    QVERIFY2(r.ok, qPrintable(r.message));
    QVERIFY2(gp::validateTaggedStructureTree(pdf).isEmpty(),
             "the tagged tree must validate under the same matrix math");
}

void TestAccessibilityTagger::nestedScaledCmComposesNewOperandFirst() {
    // q 2 0 0 2 100 0 cm … q 1 0 0 1 1.5 6 cm … (NEST) — the nested cm is
    // applied to the point FIRST, so NEST lands at C1·C2·(0,0) = (103, 12):
    // its x-left is 3pt from REF's (100) and the 2pt x-left-family
    // tolerance puts it in its own paragraph. The reversed (old-first)
    // composition lands NEST at C2·C1·(0,0) = (101.5, 6) — 1.5pt from REF —
    // and both runs collapse into ONE paragraph.
    // Both runs carry the same effective size 12 (Tf 6 × ctm 2) so the only
    // element break is the x-left family; the single baseline gap is also
    // the modal gap, so the gap limit never splits.
    FixtureOpts o;
    o.pages = {QList<TextLine>{}};
    o.extra = {
        "q\n"
        "2 0 0 2 100 0 cm\n"
        "BT\n"
        "/F1 6 Tf\n"
        "0 50 Td\n"
        "(REF) Tj\n"
        "ET\n"
        "q\n"
        "1 0 0 1 1.5 6 cm\n"
        "BT\n"
        "/F1 6 Tf\n"
        "(NEST) Tj\n"
        "ET\n"
        "Q\n"
        "Q\n",
    };
    const QString pdf = make("cm-nested-scaled.pdf", o);
    QVERIFY(!pdf.isEmpty());

    const gp::TaggerPreflight p = gp::preflightTagging(pdf);
    QVERIFY(p.loadOk);
    QVERIFY(p.anyText);
    // Same effective size 12 either way — the composition order shows in the
    // translation, not the scale.
    QVERIFY2(p.sizeClusters.size() == 1,
             qPrintable(QStringLiteral("expected one size cluster, got %1")
                            .arg(p.sizeClusters.size())));

    const gp::TaggerReport r = gp::tagDocumentAccessibility(pdf);
    QVERIFY2(r.ok, qPrintable(r.message));
    QVERIFY2(r.paragraphsTagged == 2,
             qPrintable(QStringLiteral("the nested cm must be applied to the "
                                      "point FIRST (NEST x = 103, 3pt from REF "
                                      "→ 2 paragraphs), but %1 paragraph(s) "
                                      "were tagged")
                            .arg(r.paragraphsTagged)));
    QVERIFY2(gp::validateTaggedStructureTree(pdf).isEmpty(),
             "the tagged tree must validate under the same matrix math");
}

void TestAccessibilityTagger::signedDocumentTaggingRefusedByteIdentical() {
    const QString pdf = fixtureDir().filePath("signed-taggable.pdf");
    QVERIFY(makeSignedTaggablePdf(pdf, true));
    const QByteArray before = sha256(pdf);

    // The pre-flight reports the signed field (the panel refuses on this).
    const gp::TaggerPreflight p = gp::preflightTagging(pdf);
    QVERIFY(p.loadOk);
    QVERIFY2(p.signedDocument,
             "the pre-flight must report the signed signature field");

    // The transaction refuses BEFORE any candidate exists.
    const gp::TaggerReport r = gp::tagDocumentAccessibility(pdf);
    QVERIFY2(!r.ok, "a signed document must be refused, never rewritten");
    QVERIFY2(r.message.contains(QStringLiteral("signed"), Qt::CaseInsensitive),
             qPrintable(r.message));
    QVERIFY2(r.message.contains(QStringLiteral("Save As"), Qt::CaseInsensitive),
             "the refusal must name the escape hatch (redaction-grade wording)");

    // Byte-identity is the strongest validation guarantee: nothing changed,
    // so any real verifier over the original /ByteRange still validates.
    QCOMPARE(sha256(pdf), before);

    // The signature marker survives untouched in the refused file.
    {
        PoDoFo::PdfMemDocument reopen;
        reopen.Load(pdf.toUtf8().constData());
        const PdfObject* acro = resolve(
            reopen.GetCatalog().GetDictionary().FindKey(PdfName("AcroForm")),
            reopen);
        QVERIFY(acro != nullptr && acro->IsDictionary());
        const PdfObject* fields = resolve(
            acro->GetDictionary().FindKey(PdfName("Fields")), reopen);
        QVERIFY(fields != nullptr && fields->IsArray()
                && fields->GetArray().GetSize() == 1);
        const PdfObject* widget = resolve(&fields->GetArray()[0], reopen);
        QVERIFY(widget != nullptr && widget->IsDictionary());
        const PdfObject* v = resolve(
            widget->GetDictionary().FindKey(PdfName("V")), reopen);
        QVERIFY2(v != nullptr && v->IsDictionary()
                     && v->GetDictionary().HasKey(PdfName("ByteRange")),
                 "the signed field's /ByteRange must survive the refusal");
    }

    // Control: the same document with an UNSIGNED signature field tags fine —
    // the refusal is about signed fields, not signature fields at all.
    const QString unsignedPdf = fixtureDir().filePath("unsigned-taggable.pdf");
    QVERIFY(makeSignedTaggablePdf(unsignedPdf, false));
    const gp::TaggerReport okRun = gp::tagDocumentAccessibility(unsignedPdf);
    QVERIFY2(okRun.ok,
             qPrintable(QStringLiteral("an unsigned sig field must not block "
                                      "tagging (message: %1)").arg(okRun.message)));
    QVERIFY(gp::validateTaggedStructureTree(unsignedPdf).isEmpty());
}

// ── CX-01: tagging must not destroy inline images ────────────────────────────
// PDF 32000 §8.9.7: between BI and ID the inline-image dictionary is BARE
// key/value pairs; ID is followed by exactly one whitespace byte, the data
// bytes, whitespace, then EI. The writer used to re-emit "<< >>" around the
// pairs, which every renderer treats as unknown keys — a SUCCESSFUL
// transaction that destroyed every inline image on the page while the
// text-only invariant still passed. The pin: text + one AHx inline image +
// one unfiltered binary inline image + one image XObject Do; after tagging
// the emitted stream carries bare pairs only, both data extents are
// byte-identical, and PDFium renders the page pixel-identically.
void TestAccessibilityTagger::inlineImageSurvivesTagging() {
    FixtureOpts o;
    o.pages = {QList<TextLine>{{ 700, 10.0, "Inline image survival."}}};
    o.images = 1;
    o.drawImages = true;   // the XObject Do — name-preservation half
    o.extra = {QByteArray(
        "q\n100 0 0 100 300 40 cm\n"
        "BI /W 2 /H 2 /CS /RGB /BPC 8 /F /AHx ID "
        "FF0000FF0000FF0000FF0000> EI\n"
        "Q\n"
        "q\n100 0 0 100 300 240 cm\n"
        "BI /W 2 /H 1 /CS /RGB /BPC 8 ID \x01\x02\x03\x04\x05\x06 EI\n"
        "Q\n")};
    const QString pdf = make("inline.pdf", o);
    QVERIFY(!pdf.isEmpty());

    // Render BEFORE (PDFium): the fixture really paints its inline red ink.
    PdfiumBackend beforeRenderer;
    QVERIFY2(beforeRenderer.loadDocument(pdf), "pdfium load (before) failed");
    const QImage beforeImg = beforeRenderer.renderPage(0, 150);
    QVERIFY(!beforeImg.isNull());
    long redInk = 0;
    for (int y = 0; y < beforeImg.height(); ++y)
        for (int x = 0; x < beforeImg.width(); ++x) {
            const QRgb px = beforeImg.pixel(x, y);
            if (qRed(px) > 200 && qGreen(px) < 80 && qBlue(px) < 80) ++redInk;
        }
    QVERIFY2(redInk > 50,
             "fixture sanity: the inline red image region must render");
    // Release the pdfium file handle before the tagging transaction commits.
    beforeRenderer.closeDocument();

    const gp::TaggerReport r = gp::tagDocumentAccessibility(pdf);
    QVERIFY2(r.ok, qPrintable(r.message));
    QVERIFY2(gp::validateTaggedStructureTree(pdf).isEmpty(),
             "the tagged tree must still validate");

    // The tagged page stream: EVERY inline image emits bare pairs only.
    PoDoFo::PdfMemDocument doc;
    doc.Load(pdf.toUtf8().constData());
    auto& page = doc.GetPages().GetPageAt(0);
    PoDoFo::PdfObject* contents =
        page.GetDictionary().FindKey(PdfName("Contents"));
    QVERIFY(contents != nullptr);
    if (contents->IsReference())
        contents = &doc.GetObjects().MustGetObject(contents->GetReference());
    QVERIFY(contents->HasStream());
    PoDoFo::charbuff buf;
    contents->GetStream()->CopyTo(buf);
    const QByteArray out(buf.data(), static_cast<qsizetype>(buf.size()));

    int bi = 0;
    int inlineCount = 0;
    QList<QByteArray> dataRegions;
    while ((bi = out.indexOf("BI\n", bi)) >= 0) {
        const qsizetype id = out.indexOf("\nID\n", bi);
        QVERIFY2(id > bi, "every BI must be followed by ID");
        const QByteArray dictPart = out.mid(bi, static_cast<int>(id - bi));
        QVERIFY2(!dictPart.contains("<<"),
                 "CX-01: the inline dictionary must be bare key/value pairs, "
                 "not << >> delimited");
        QVERIFY2(!dictPart.contains(">>"),
                 "CX-01: the inline dictionary must be bare key/value pairs, "
                 "not << >> delimited");
        QVERIFY2(dictPart.contains("/W"), "bare keys must survive");
        const qsizetype ei = out.indexOf("\nEI\n", id);
        QVERIFY2(ei > id, "every ID must be followed by EI");
        dataRegions.append(out.mid(static_cast<int>(id) + 4,
                                   static_cast<int>(ei - id) - 4));
        ++inlineCount;
        bi = static_cast<int>(id);
    }
    QVERIFY2(inlineCount == 2,
             qPrintable(QStringLiteral("expected 2 inline images, saw %1")
                            .arg(inlineCount)));
    // The exact data bytes survive (AHx hex payload; six raw binary bytes).
    QVERIFY2(dataRegions[0].contains("FF0000FF0000FF0000FF0000"),
             "the AHx inline data bytes must be byte-identical");
    QVERIFY2(dataRegions[1].contains(QByteArray("\x01\x02\x03\x04\x05\x06", 6)),
             "the unfiltered inline data bytes must be byte-identical");

    // Render AFTER: the whole page — text, inline images, image XObject —
    // must be pixel-identical (PDFium), so the image region in particular.
    PdfiumBackend afterRenderer;
    QVERIFY2(afterRenderer.loadDocument(pdf), "pdfium load (after) failed");
    const QImage afterImg = afterRenderer.renderPage(0, 150);
    QVERIFY2(afterImg == beforeImg,
             "the tagged page must render pixel-identically (CX-01: the "
             "inline image and every other painting op must survive)");
}

// ── CX-07: MCIDs inside Form XObjects ────────────────────────────────────────
// CX-07: MCIDs inside Form XObjects ── local dict-name helper (the engine's
// nameAt is internal; the tests keep their own).
static QString dictNameAt(const PoDoFo::PdfDictionary& d, const char* key) {
    const PdfObject* o = d.FindKey(PdfName(key));
    if (o == nullptr || !o->IsName()) return {};
    const std::string_view s = o->GetName().GetString();
    return QString::fromUtf8(s.data(), static_cast<qsizetype>(s.size()));
}

// Fixture: one page, standard F1, plus a Form XObject /Fm0 with its OWN
// /Resources (F1) and one honestly-decodable line inside. pageTextToo adds
// a page-stream line ABOVE the form invocation (the page + Form fixture);
// without it every element's MCIDs live in the form (the Form-only fixture).
static bool makeFormPdf(const QString& path, bool pageTextToo) {
    try {
        PoDoFo::PdfMemDocument doc;
        auto& page = doc.GetPages().CreatePage(
            PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));

        // The Form XObject: own resources, own text line.
        auto& form = doc.GetObjects().CreateDictionaryObject();
        form.GetDictionary().AddKey("Type", PdfObject(PdfName("XObject")));
        form.GetDictionary().AddKey("Subtype", PdfObject(PdfName("Form")));
        PoDoFo::PdfArray bbox;
        bbox.Add(0.0); bbox.Add(0.0);
        bbox.Add(595.0); bbox.Add(842.0);
        form.GetDictionary().AddKey("BBox", PdfObject(bbox));
        PdfDictionary formFonts;
        addStandardFont(doc, formFonts, "F1", "Helvetica");
        PdfDictionary formRes;
        formRes.AddKey(PdfName("Font"), PdfObject(formFonts));
        form.GetDictionary().AddKey("Resources", PdfObject(formRes));
        const char* formContent =
            "BT\n/F1 10 Tf\n60 640 Td\n(Form text line.) Tj\nET\n";
        form.GetOrCreateStream().SetData(
            PoDoFo::bufferview(formContent, std::strlen(formContent)));

        // Page resources: F1 + /Fm0.
        PdfDictionary fonts;
        addStandardFont(doc, fonts, "F1", "Helvetica");
        page.GetResources().GetDictionary().AddKey("Font",
                                                   PdfObject(fonts));
        PdfDictionary xobjs;
        xobjs.AddKey(PdfName("Fm0"), form.GetIndirectReference());
        page.GetResources().GetDictionary().AddKey("XObject",
                                                   PdfObject(xobjs));

        QByteArray c;
        if (pageTextToo)
            c += "BT\n/F1 10 Tf\n60 700 Td\n(Page text line.) Tj\nET\n";
        c += "q\n/Fm0 Do\nQ\n";
        auto& contents = page.GetOrCreateContents();
        auto& stream = contents.CreateStreamForAppending(
            PoDoFo::PdfStreamAppendFlags::None);
        stream.SetData(PoDoFo::bufferview(c.constData(),
                                          static_cast<size_t>(c.size())));
        doc.Save(path.toUtf8().constData());
        return true;
    } catch (...) {
        return false;
    }
}

// Independent structure walk: for every top-level element, collect the /K
// entries as (mcid, MCR-shape) records and verify the MCR entries resolve:
// /Pg is the page, /Stm is a Form XObject carrying /StructParents whose
// ParentTree entry points back at the element. Reports whether any /MCR
// entry and any bare-integer /K entry were seen.
static bool walkMcrStructure(const QString& path, bool* sawMcrOut,
                             bool* sawBareIntOut, QString* err) {
    try {
        PoDoFo::PdfMemDocument doc;
        doc.Load(path.toUtf8().constData());
        const PdfObject* root =
            resolve(doc.GetCatalog().GetDictionary().FindKey(
                        PdfName("StructTreeRoot")),
                    doc);
        if (root == nullptr || !root->IsDictionary()) {
            if (err) *err = QStringLiteral("no /StructTreeRoot");
            return false;
        }
        // ParentTree, keyed by StructParents: key → (mcid → element ref).
        const PdfObject* pt = resolve(
            root->GetDictionary().FindKey(PdfName("ParentTree")), doc);
        if (pt == nullptr || !pt->IsDictionary()) {
            if (err) *err = QStringLiteral("no /ParentTree");
            return false;
        }
        const PdfObject* nums =
            resolve(pt->GetDictionary().FindKey(PdfName("Nums")), doc);
        std::map<int, std::map<int, PoDoFo::PdfReference>> parentTree;
        if (nums != nullptr && nums->IsArray()) {
            const PdfArray& arr = nums->GetArray();
            for (size_t i = 0; i + 1 < arr.GetSize(); i += 2) {
                const int key = static_cast<int>(arr[i].GetNumber());
                const PdfObject* val = resolve(&arr[i + 1], doc);
                if (val == nullptr || !val->IsArray()) continue;
                for (size_t m = 0; m < val->GetArray().GetSize(); ++m) {
                    const PdfObject& r = val->GetArray()[m];
                    if (r.IsReference())
                        parentTree[key][static_cast<int>(m)] =
                            r.GetReference();
                }
            }
        }

        const PdfObject* kids =
            resolve(root->GetDictionary().FindKey(PdfName("K")), doc);
        if (kids == nullptr || !kids->IsArray()) {
            if (err) *err = QStringLiteral("root /K missing");
            return false;
        }
        bool sawMcr = false;
        bool sawBareInt = false;
        for (const PdfObject& elRef : kids->GetArray()) {
            const PdfObject* el = resolve(&elRef, doc);
            if (el == nullptr || !el->IsDictionary()) {
                if (err) *err = QStringLiteral("root /K entry not a ref");
                return false;
            }
            const PdfDictionary& d = el->GetDictionary();
            const PdfObject* pg = resolve(d.FindKey(PdfName("Pg")), doc);
            const PdfObject* k = resolve(d.FindKey(PdfName("K")), doc);
            QList<const PdfObject*> kEntries;
            if (k != nullptr && k->IsArray()) {
                for (const PdfObject& m : k->GetArray())
                    kEntries.append(resolve(&m, doc));
            } else if (k != nullptr) {
                kEntries.append(k);
            }
            for (const PdfObject* entry : kEntries) {
                if (entry != nullptr && entry->IsNumber()) {
                    sawBareInt = true;
                    continue;
                }
                if (entry == nullptr || !entry->IsDictionary()) {
                    if (err) *err = QStringLiteral("bad /K entry");
                    return false;
                }
                const PdfDictionary& mcr = entry->GetDictionary();
                if (mcr.HasKey(PdfName("Type"))
                    && dictNameAt(mcr, "Type") != QLatin1String("MCR")) {
                    if (err) *err = QStringLiteral("/K dict not /MCR");
                    return false;
                }
                sawMcr = true;
                const PdfObject* stm = resolve(mcr.FindKey(PdfName("Stm")),
                                               doc);
                if (stm == nullptr || !stm->IsDictionary()
                    || dictNameAt(stm->GetDictionary(), "Subtype")
                           != QLatin1String("Form")) {
                    if (err) *err = QStringLiteral("/MCR /Stm not a form");
                    return false;
                }
                if (pg == nullptr
                    || mcr.FindKey(PdfName("Pg")) == nullptr) {
                    if (err) *err = QStringLiteral("/MCR without /Pg");
                    return false;
                }
                // The form carries its own StructParents, and the
                // ParentTree entry under it points back at THIS element.
                const PdfObject* sp =
                    stm->GetDictionary().FindKey(PdfName("StructParents"));
                if (sp == nullptr || !sp->IsNumber()) {
                    if (err) *err = QStringLiteral("form has no /StructParents");
                    return false;
                }
                const int key = static_cast<int>(sp->GetNumber());
                const PdfObject* mcidObj = mcr.FindKey(PdfName("MCID"));
                const int mcid = mcidObj != nullptr && mcidObj->IsNumber()
                                     ? static_cast<int>(mcidObj->GetNumber())
                                     : -1;
                const auto keyIt = parentTree.find(key);
                if (keyIt == parentTree.end()
                    || keyIt->second.find(mcid) == keyIt->second.end()) {
                    if (err) *err = QStringLiteral(
                        "no ParentTree[%1][%2]").arg(key).arg(mcid);
                    return false;
                }
                if (!(keyIt->second.at(mcid)
                      == el->GetIndirectReference())) {
                    if (err) *err = QStringLiteral(
                        "ParentTree[%1][%2] points elsewhere")
                            .arg(key).arg(mcid);
                    return false;
                }
            }
        }
        if (sawMcrOut != nullptr) *sawMcrOut = sawMcr;
        if (sawBareIntOut != nullptr) *sawBareIntOut = sawBareInt;
        return true;
    } catch (const PoDoFo::PdfError& e) {
        if (err) *err = QString::fromUtf8(e.what());
        return false;
    }
}

void TestAccessibilityTagger::formXObjectMcidsUseMcrReferences() {
    // The page + Form fixture: page-stream text AND a form invocation.
    QTemporaryDir& dir = fixtureDir();
    const QString pdf = dir.filePath("cx07-page-form.pdf");
    QVERIFY(makeFormPdf(pdf, true));

    const gp::TaggerReport r = gp::tagDocumentAccessibility(pdf);
    QVERIFY2(r.ok, qPrintable(r.message));
    QVERIFY2(gp::validateTaggedStructureTree(pdf).isEmpty(),
             "the tagged tree must validate (validator checks /Stm)");

    // Independent walk: the form's MCID is an /MCR with /Pg + /Stm, the
    // form carries /StructParents, and the ParentTree resolves to the
    // element. The page text keeps its bare-integer /K.
    bool sawMcr = false, sawBareInt = false;
    QString err;
    QVERIFY2(walkMcrStructure(pdf, &sawMcr, &sawBareInt, &err),
             qPrintable(err));
    QVERIFY2(sawMcr, "the form's MCID must be a /MCR marked-content reference");
    QVERIFY2(sawBareInt,
             "the page-stream text keeps its bare-integer /K");
}

void TestAccessibilityTagger::formOnlyMcidsAlsoUseMcrReferences() {
    // The Form-only fixture: EVERY element's MCIDs live in the form —
    // no bare-integer /K may appear anywhere.
    QTemporaryDir& dir = fixtureDir();
    const QString pdf = dir.filePath("cx07-form-only.pdf");
    QVERIFY(makeFormPdf(pdf, false));

    const gp::TaggerReport r = gp::tagDocumentAccessibility(pdf);
    QVERIFY2(r.ok, qPrintable(r.message));
    QVERIFY2(gp::validateTaggedStructureTree(pdf).isEmpty(),
             "the tagged tree must validate (validator checks /Stm)");

    bool sawMcr = false, sawBareInt = false;
    QString err;
    QVERIFY2(walkMcrStructure(pdf, &sawMcr, &sawBareInt, &err),
             qPrintable(err));
    QVERIFY2(sawMcr, "every MCID must be a /MCR marked-content reference");
    QVERIFY2(!sawBareInt,
             "a Form-only document must not carry bare-integer /K entries");
}

#include "TestAccessibilityTagger.moc"
QTEST_MAIN(TestAccessibilityTagger)
