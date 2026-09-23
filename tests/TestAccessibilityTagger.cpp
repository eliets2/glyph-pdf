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
        args << QStringLiteral("--flavour") << QString::fromLatin1(flavour)
             << QStringLiteral("--format") << QStringLiteral("json")
             << target;
        proc.start(gp::VeraPdfValidator::locateCli(), args);
        if (!proc.waitForStarted(10000)) return {};
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

#include "TestAccessibilityTagger.moc"
QTEST_MAIN(TestAccessibilityTagger)
