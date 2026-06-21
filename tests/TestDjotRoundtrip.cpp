#include <QtTest>
#include <QCoreApplication>
#include <memory>

#include "docmodel/ProvenanceTag.h"
#include "docmodel/SemanticDocument.h"
#include "docmodel/Block.h"
#include "docmodel/Inline.h"
#include "pdfws_djot/LuaDjotCodec.h"
#include "pdfws_djot/ProvenanceGuard.h"
#include "pdfws_djot/IDjotMapper.h"
#include "pdfws_djot/PdfStructureMapper.h"

static std::string djotLibPath() {
    return (QCoreApplication::applicationDirPath() + "/../third_party/djot").toStdString();
}

// ── Helpers for building minimal SemanticDocument trees ──────────────────────

static docmodel::Provenance anyProv() {
    return docmodel::Provenance{docmodel::ProvenanceTag::BornDjot, "", -1, {}};
}

static std::shared_ptr<docmodel::TextInline> makeText(const std::string& t) {
    return std::make_shared<docmodel::TextInline>(t, anyProv());
}

static std::shared_ptr<docmodel::TextBlock> makeParagraph(const std::string& text) {
    return std::make_shared<docmodel::TextBlock>(
        docmodel::Block::Type::Paragraph,
        std::vector<std::shared_ptr<docmodel::Inline>>{makeText(text)},
        anyProv());
}

static std::shared_ptr<docmodel::Section> makeSection(
    const std::string& title,
    std::vector<std::shared_ptr<docmodel::Block>> blocks = {},
    std::vector<std::shared_ptr<docmodel::Section>> subs = {})
{
    return std::make_shared<docmodel::Section>(title, std::move(blocks), std::move(subs), anyProv());
}

// ── Test class ────────────────────────────────────────────────────────────────

class TestDjotRoundtrip : public QObject {
    Q_OBJECT

private slots:
    // D1 — decode produces a real tree (structure, not just non-null).
    void testDecodeSimpleDjot() {
        pdfws::LuaDjotCodec codec(djotLibPath());
        auto doc = codec.djotToDocument("# Hello\n\nA paragraph.");
        QVERIFY2(doc != nullptr, "djotToDocument should parse valid djot and return non-null document");

        // One section titled "Hello" containing one paragraph block "A paragraph."
        QCOMPARE(static_cast<int>(doc->getSections().size()), 1);
        const auto& sec = doc->getSections()[0];
        QVERIFY(sec != nullptr);
        QCOMPARE(sec->getTitle(), std::string("Hello"));
        QCOMPARE(static_cast<int>(sec->getBlocks().size()), 1);
        const auto& blk = sec->getBlocks()[0];
        QVERIFY(blk != nullptr);
        QCOMPARE(static_cast<int>(blk->getType()), static_cast<int>(docmodel::Block::Type::Paragraph));
        QCOMPARE(static_cast<int>(blk->getInlines().size()), 1);
        QCOMPARE(blk->getInlines()[0]->getText(), std::string("A paragraph."));
    }

    void testDecodeEmptyDjot() {
        pdfws::LuaDjotCodec codec(djotLibPath());
        try {
            auto doc = codec.djotToDocument("");
        } catch (const std::exception& e) {
            QFAIL(qPrintable(QString("djotToDocument(\"\") threw: %1").arg(e.what())));
        }
    }

    // D2 — encode empty document produces empty output
    void testEncodeEmptyDocument() {
        pdfws::LuaDjotCodec codec(djotLibPath());
        docmodel::Provenance prov = anyProv();
        docmodel::SemanticDocument doc({}, prov);
        QCOMPARE(codec.documentToDjot(doc), std::string{});
    }

    // D2 — heading in section title appears in output
    void testEncodeSectionTitleAsHeading() {
        pdfws::LuaDjotCodec codec(djotLibPath());
        auto sec = makeSection("Hello World");
        docmodel::SemanticDocument doc({sec}, anyProv());
        const std::string result = codec.documentToDjot(doc);
        QVERIFY2(!result.empty(), "encode should produce non-empty output for titled section");
        QVERIFY2(result.find("# Hello World") != std::string::npos,
                 qPrintable(QString("Expected '# Hello World' in:\n%1").arg(result.c_str())));
    }

    // D2 — paragraph block produces plain text followed by blank line
    void testEncodeParagraph() {
        pdfws::LuaDjotCodec codec(djotLibPath());
        auto para = makeParagraph("This is a paragraph.");
        auto sec = makeSection("", {para});
        docmodel::SemanticDocument doc({sec}, anyProv());
        const std::string result = codec.documentToDjot(doc);
        QVERIFY2(result.find("This is a paragraph.") != std::string::npos,
                 "paragraph text must appear in output");
        QVERIFY2(result.find("\n\n") != std::string::npos,
                 "paragraph must be followed by blank line");
    }

    // D2 — emph inline wraps content in underscores
    void testEncodeEmphInline() {
        pdfws::LuaDjotCodec codec(djotLibPath());
        auto emphContent = makeText("important");
        auto emphSpan = std::make_shared<docmodel::ContainerInline>(
            docmodel::Inline::Type::Emph,
            std::vector<std::shared_ptr<docmodel::Inline>>{emphContent},
            anyProv());
        auto para = std::make_shared<docmodel::TextBlock>(
            docmodel::Block::Type::Paragraph,
            std::vector<std::shared_ptr<docmodel::Inline>>{emphSpan},
            anyProv());
        auto sec = makeSection("", {para});
        docmodel::SemanticDocument doc({sec}, anyProv());
        const std::string result = codec.documentToDjot(doc);
        QVERIFY2(result.find("_important_") != std::string::npos,
                 qPrintable(QString("Expected '_important_' in: %1").arg(result.c_str())));
    }

    // D2 — strong inline wraps content in double asterisks
    void testEncodeStrongInline() {
        pdfws::LuaDjotCodec codec(djotLibPath());
        auto strongContent = makeText("bold");
        auto strongSpan = std::make_shared<docmodel::ContainerInline>(
            docmodel::Inline::Type::Strong,
            std::vector<std::shared_ptr<docmodel::Inline>>{strongContent},
            anyProv());
        auto para = std::make_shared<docmodel::TextBlock>(
            docmodel::Block::Type::Paragraph,
            std::vector<std::shared_ptr<docmodel::Inline>>{strongSpan},
            anyProv());
        auto sec = makeSection("", {para});
        docmodel::SemanticDocument doc({sec}, anyProv());
        const std::string result = codec.documentToDjot(doc);
        // Djot strong is single-asterisk; `**` would parse back as nested strong.
        QVERIFY2(result.find("*bold*") != std::string::npos,
                 qPrintable(QString("Expected '*bold*' in: %1").arg(result.c_str())));
    }

    // D2 — nested sections produce subordinate headings
    void testEncodeNestedSections() {
        pdfws::LuaDjotCodec codec(djotLibPath());
        auto sub = makeSection("Sub");
        auto top = makeSection("Top", {}, {sub});
        docmodel::SemanticDocument doc({top}, anyProv());
        const std::string result = codec.documentToDjot(doc);
        QVERIFY2(result.find("# Top") != std::string::npos,
                 "top-level section should have # prefix");
        QVERIFY2(result.find("## Sub") != std::string::npos,
                 "nested section should have ## prefix");
    }

    // D2 — list block: children produce "- " prefixed lines
    void testEncodeList() {
        pdfws::LuaDjotCodec codec(djotLibPath());
        auto item1 = std::make_shared<docmodel::TextBlock>(
            docmodel::Block::Type::ListItem,
            std::vector<std::shared_ptr<docmodel::Inline>>{makeText("Alpha")},
            anyProv());
        auto item2 = std::make_shared<docmodel::TextBlock>(
            docmodel::Block::Type::ListItem,
            std::vector<std::shared_ptr<docmodel::Inline>>{makeText("Beta")},
            anyProv());
        auto list = std::make_shared<docmodel::ContainerBlock>(
            docmodel::Block::Type::List,
            std::vector<std::shared_ptr<docmodel::Block>>{item1, item2},
            anyProv());
        auto sec = makeSection("", {list});
        docmodel::SemanticDocument doc({sec}, anyProv());
        const std::string result = codec.documentToDjot(doc);
        QVERIFY2(result.find("- Alpha") != std::string::npos, "first list item missing");
        QVERIFY2(result.find("- Beta") != std::string::npos, "second list item missing");
    }

    // D2 — special chars in text are escaped
    void testEncodeTextEscaping() {
        pdfws::LuaDjotCodec codec(djotLibPath());
        auto para = makeParagraph("a*b_c`d");
        auto sec = makeSection("", {para});
        docmodel::SemanticDocument doc({sec}, anyProv());
        const std::string result = codec.documentToDjot(doc);
        QVERIFY2(result.find("a\\*b\\_c\\`d") != std::string::npos,
                 qPrintable(QString("Expected escaped special chars in: %1").arg(result.c_str())));
    }

    // D1 — STRUCTURAL round-trip: encode(doc) → decode → structurally equal doc.
    // The fixture is built from constructs the emitter handles faithfully
    // (titled sections, paragraphs with emph/strong, a list, and a nested
    // subsection). Asserts section/block/inline counts AND text, not non-null.
    void testStructuralRoundtrip() {
        pdfws::LuaDjotCodec codec(djotLibPath());

        // rich paragraph: "plain " _italic_ " and " *bold*
        auto emphSpan = std::make_shared<docmodel::ContainerInline>(
            docmodel::Inline::Type::Emph,
            std::vector<std::shared_ptr<docmodel::Inline>>{makeText("italic")}, anyProv());
        auto strongSpan = std::make_shared<docmodel::ContainerInline>(
            docmodel::Inline::Type::Strong,
            std::vector<std::shared_ptr<docmodel::Inline>>{makeText("bold")}, anyProv());
        auto richPara = std::make_shared<docmodel::TextBlock>(
            docmodel::Block::Type::Paragraph,
            std::vector<std::shared_ptr<docmodel::Inline>>{
                makeText("plain "), emphSpan, makeText(" and "), strongSpan}, anyProv());

        auto li1 = std::make_shared<docmodel::TextBlock>(
            docmodel::Block::Type::ListItem,
            std::vector<std::shared_ptr<docmodel::Inline>>{makeText("Alpha")}, anyProv());
        auto li2 = std::make_shared<docmodel::TextBlock>(
            docmodel::Block::Type::ListItem,
            std::vector<std::shared_ptr<docmodel::Inline>>{makeText("Beta")}, anyProv());
        auto list = std::make_shared<docmodel::ContainerBlock>(
            docmodel::Block::Type::List,
            std::vector<std::shared_ptr<docmodel::Block>>{li1, li2}, anyProv());

        auto sub = makeSection("Subsection", {makeParagraph("Nested para.")});
        auto top = makeSection("Top Title", {makeParagraph("First para."), richPara, list}, {sub});
        auto sec2 = makeSection("Second", {makeParagraph("Body two.")});
        docmodel::SemanticDocument doc({top, sec2}, anyProv());

        const std::string djotOut = codec.documentToDjot(doc);
        std::unique_ptr<docmodel::SemanticDocument> re;
        try {
            re = codec.djotToDocument(djotOut);
        } catch (const std::exception& e) {
            QFAIL(qPrintable(QString("decode threw: %1\nDjot was:\n%2")
                             .arg(e.what()).arg(djotOut.c_str())));
        }
        QVERIFY2(re != nullptr, "decode must return non-null");

        // ── Top-level sections ──
        QCOMPARE(static_cast<int>(re->getSections().size()), 2);
        const auto& rTop = re->getSections()[0];
        const auto& rSec2 = re->getSections()[1];
        QVERIFY(rTop && rSec2);
        QCOMPARE(rTop->getTitle(), std::string("Top Title"));
        QCOMPARE(rSec2->getTitle(), std::string("Second"));

        // ── Top section: 3 blocks (para, rich para, list) + 1 subsection ──
        QCOMPARE(static_cast<int>(rTop->getBlocks().size()), 3);
        QCOMPARE(static_cast<int>(rTop->getSubsections().size()), 1);

        // block 0: paragraph "First para."
        const auto& b0 = rTop->getBlocks()[0];
        QCOMPARE(static_cast<int>(b0->getType()), static_cast<int>(docmodel::Block::Type::Paragraph));
        QCOMPARE(b0->getInlines()[0]->getText(), std::string("First para."));

        // block 1: rich paragraph — 4 inlines: text, emph, text, strong
        const auto& b1 = rTop->getBlocks()[1];
        QCOMPARE(static_cast<int>(b1->getType()), static_cast<int>(docmodel::Block::Type::Paragraph));
        QCOMPARE(static_cast<int>(b1->getInlines().size()), 4);
        QCOMPARE(b1->getInlines()[0]->getText(), std::string("plain "));
        QCOMPARE(static_cast<int>(b1->getInlines()[1]->getType()),
                 static_cast<int>(docmodel::Inline::Type::Emph));
        QCOMPARE(b1->getInlines()[1]->getChildren()[0]->getText(), std::string("italic"));
        QCOMPARE(b1->getInlines()[2]->getText(), std::string(" and "));
        QCOMPARE(static_cast<int>(b1->getInlines()[3]->getType()),
                 static_cast<int>(docmodel::Inline::Type::Strong));
        QCOMPARE(b1->getInlines()[3]->getChildren()[0]->getText(), std::string("bold"));

        // block 2: list with two items "Alpha", "Beta"
        const auto& b2 = rTop->getBlocks()[2];
        QCOMPARE(static_cast<int>(b2->getType()), static_cast<int>(docmodel::Block::Type::List));
        QCOMPARE(static_cast<int>(b2->getBlocks().size()), 2);
        QCOMPARE(static_cast<int>(b2->getBlocks()[0]->getType()),
                 static_cast<int>(docmodel::Block::Type::ListItem));
        QCOMPARE(b2->getBlocks()[0]->getInlines()[0]->getText(), std::string("Alpha"));
        QCOMPARE(b2->getBlocks()[1]->getInlines()[0]->getText(), std::string("Beta"));

        // subsection: "Subsection" with one paragraph "Nested para."
        const auto& rSub = rTop->getSubsections()[0];
        QCOMPARE(rSub->getTitle(), std::string("Subsection"));
        QCOMPARE(static_cast<int>(rSub->getBlocks().size()), 1);
        QCOMPARE(rSub->getBlocks()[0]->getInlines()[0]->getText(), std::string("Nested para."));

        // ── Second section: one paragraph "Body two." ──
        QCOMPARE(static_cast<int>(rSec2->getBlocks().size()), 1);
        QCOMPARE(rSec2->getBlocks()[0]->getInlines()[0]->getText(), std::string("Body two."));
    }

    void testProvenanceGuardThrowsForSignedBornPdf() {
        pdfws::ProvenanceGuard guard;
        bool threw = false;
        try {
            guard.checkEditVia(
                docmodel::ProvenanceTag::BornPDF,
                true /* isSigned */,
                pdfws::EditPath::DjotThenSave
            );
        } catch (const pdfws::ProvenanceViolation&) {
            threw = true;
        }
        QVERIFY2(threw, "ProvenanceGuard must throw ProvenanceViolation for signed BornPDF + DjotThenSave");
    }

    void testProvenanceGuardWarnForUnsignedBornPdf() {
        pdfws::ProvenanceGuard guard;
        const auto result = guard.checkEditVia(
            docmodel::ProvenanceTag::BornPDF,
            false /* isSigned */,
            pdfws::EditPath::DjotThenSave
        );
        QVERIFY2(result.allowed, "Unsigned BornPDF + DjotThenSave should be allowed");
        QVERIFY2(result.requiresWarning, "Unsigned BornPDF + DjotThenSave should require a warning");
    }

    void testProvenanceGuardAllowsSaveAsCopy() {
        pdfws::ProvenanceGuard guard;
        const auto result = guard.checkEditVia(
            docmodel::ProvenanceTag::BornPDF,
            true /* isSigned */,
            pdfws::EditPath::DjotThenSaveAsCopy
        );
        QVERIFY2(result.allowed, "Signed BornPDF + DjotThenSaveAsCopy must be allowed (safe route)");
        QVERIFY2(!result.requiresWarning, "DjotThenSaveAsCopy should not require a warning");
    }

    void testProvenanceGuardAllowsBornDjot() {
        pdfws::ProvenanceGuard guard;
        const auto result = guard.checkEditVia(
            docmodel::ProvenanceTag::BornDjot,
            false,
            pdfws::EditPath::DjotThenSave
        );
        QVERIFY2(result.allowed, "BornDjot documents should be freely editable via DjotThenSave");
    }

    // ── AR-9 D2: ProvenanceToken is a real type-level chokepoint ─────────────

    // The guard mints an apply-token for an allowed edit; the token records the
    // approved (origin, isSigned, path) tuple. This token is the ONLY way to
    // construct a ProvenanceToken (its constructor is private, friend to the
    // guard) — see the compile-time evidence note below.
    void testProvenanceGuardMintsTokenForAllowedEdit() {
        pdfws::ProvenanceGuard guard;
        const pdfws::ProvenanceToken token = guard.mintApplyToken(
            docmodel::ProvenanceTag::BornDjot, false, pdfws::EditPath::DjotThenSave);
        QCOMPARE(static_cast<int>(token.origin()), static_cast<int>(docmodel::ProvenanceTag::BornDjot));
        QCOMPARE(token.isSigned(), false);
        QCOMPARE(static_cast<int>(token.path()), static_cast<int>(pdfws::EditPath::DjotThenSave));
    }

    // The guard REFUSES to mint a token for the forbidden edit (signed +
    // DjotThenSave) — it throws ProvenanceViolation rather than handing back a
    // capability. Because applySemanticToPdf() requires a ProvenanceToken, this
    // makes the lossy full-rewrite of a signed document type-level unreachable.
    void testProvenanceGuardRefusesTokenForSignedDjotSave() {
        pdfws::ProvenanceGuard guard;
        bool threw = false;
        try {
            (void)guard.mintApplyToken(
                docmodel::ProvenanceTag::BornPDF, true, pdfws::EditPath::DjotThenSave);
        } catch (const pdfws::ProvenanceViolation&) {
            threw = true;
        }
        QVERIFY2(threw, "mintApplyToken must throw for signed + DjotThenSave");
    }

    // Demonstrates the chokepoint end-to-end: a born-PDF mapper rejects the
    // SemanticDocument → PDF write with a DISTINCT NotSupported outcome (not a
    // bare false conflated with I/O failure), and the call is only reachable by
    // first obtaining a guard-minted token.
    void testApplySemanticToPdfRequiresTokenAndReportsNotSupported() {
        pdfws::ProvenanceGuard guard;
        pdfws::PdfStructureMapper mapper;
        docmodel::SemanticDocument doc({}, anyProv());

        // Obtaining the token is the gate; applySemanticToPdf cannot be called
        // without one (it does not compile — see EVIDENCE note in the cpp).
        const pdfws::ProvenanceToken token = guard.mintApplyToken(
            docmodel::ProvenanceTag::BornPDF, false, pdfws::EditPath::DjotThenSave);

        const pdfws::ApplyOutcome outcome =
            mapper.applySemanticToPdf(doc, "in.pdf", "out.pdf", token);
        QCOMPARE(static_cast<int>(outcome),
                 static_cast<int>(pdfws::ApplyOutcome::NotSupported));
    }
};

QTEST_MAIN(TestDjotRoundtrip)
#include "TestDjotRoundtrip.moc"
