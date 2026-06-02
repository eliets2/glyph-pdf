// TestOcrDjotMapper — structural + roundtrip tests for OcrDjotMapper (M5-P4 D4).
//
// Tests:
//   1. Empty input → empty SemanticDocument (no sections).
//   2. Single-page Paragraph region → 1 section, 1 Paragraph block.
//   3. Title region → Heading block.
//   4. Per-word Inline spans carry BornOCR provenance with pdf-page/bbox/conf attrs.
//   5. Multiple regions on one page: block order matches readingOrderIndex.
//   6. Header region → Paragraph with page-header attr in provenance source_file.
//   7. Footer region → Paragraph with page-footer attr in provenance source_file.
//   8. List region → List (ContainerBlock) with ListItem children.
//   9. Table region → Table (ContainerBlock) block with pipe-table in Djot output.
//  10. Encode roundtrip: Paragraph text survives documentToDjot.
//  11. Table Djot output contains "|" pipe-table syntax.
//  12. Multi-page: N pages → N sections, blocks per page match region count.
//  13. Failed page (success=false, no regions) → section with error paragraph.
//  14. Figure region → Figure block.

#include <QtTest>
#include <QCoreApplication>
#include <memory>
#include <string>

#include "engines/ocr/OcrDjotMapper.h"
#include "engines/ocr/OcrPipeline.h"
#include "engines/ocr/ILayoutDetector.h"
#include "docmodel/SemanticDocument.h"
#include "docmodel/Block.h"
#include "docmodel/Inline.h"
#include "docmodel/ProvenanceTag.h"
#include "pdfws_djot/LuaDjotCodec.h"

// ── Test helpers ────────────────────────────────────────────────────────────

/// Build a minimal LayoutRegion.
static LayoutRegion makeRegion(RegionType type, int readingOrder,
                               QRectF bbox = QRectF(0, 0, 100, 20))
{
    LayoutRegion r;
    r.type               = type;
    r.readingOrderIndex  = readingOrder;
    r.bbox               = bbox;
    r.confidence         = 0.9;
    return r;
}

/// Build a MergedOcrWord at a given position.
static MergedOcrWord makeWord(const QString& text, QRectF bbox, int confidence = 90)
{
    MergedOcrWord w;
    w.text         = text;
    w.boundingBox  = bbox;
    w.confidence   = confidence;
    w.sourceEngine = "ROVER";
    return w;
}

/// Build a PageOcrResult with given regions and words.
static PageOcrResult makePage(int pageIndex,
                              QList<LayoutRegion> regions,
                              QList<MergedOcrWord> words)
{
    PageOcrResult pr;
    pr.pageIndex     = pageIndex;
    pr.layoutRegions = std::move(regions);
    pr.words         = std::move(words);
    pr.success       = true;
    return pr;
}

// ── Test class ──────────────────────────────────────────────────────────────

class TestOcrDjotMapper : public QObject {
    Q_OBJECT

private slots:
    void test01_emptyInput();
    void test02_singleParagraphRegion();
    void test03_titleRegionMapsToHeading();
    void test04_wordInlineHasBornOCRProvenance();
    void test05_blockOrderFollowsReadingOrderIndex();
    void test06_headerRegionAttr();
    void test07_footerRegionAttr();
    void test08_listRegionMapsToList();
    void test09_tableRegionMapsToTableBlock();
    void test10_encodeRoundtripParagraphText();
    void test11_tableEncodeContainsPipeSyntax();
    void test12_multiplePagesMakeSections();
    void test13_failedPageProducesErrorSection();
    void test14_figureRegionMapsToFigureBlock();
};

// ── Tests ────────────────────────────────────────────────────────────────────

void TestOcrDjotMapper::test01_emptyInput()
{
    OcrDjotMapper mapper;
    docmodel::SemanticDocument doc = mapper.fromOcrResults({}, "test.pdf");
    QVERIFY(doc.getSections().empty());
}

void TestOcrDjotMapper::test02_singleParagraphRegion()
{
    OcrDjotMapper mapper;

    auto word = makeWord("Hello", QRectF(10, 5, 40, 12), 85);
    auto region = makeRegion(RegionType::Paragraph, 0, QRectF(0, 0, 100, 20));

    auto page = makePage(0, {region}, {word});
    docmodel::SemanticDocument doc = mapper.fromOcrResults({page}, "test.pdf");

    QCOMPARE(doc.getSections().size(), size_t(1));
    const auto& section = doc.getSections()[0];
    QVERIFY(section);
    QCOMPARE(section->getBlocks().size(), size_t(1));
    const auto& block = section->getBlocks()[0];
    QVERIFY(block);
    QCOMPARE(block->getType(), docmodel::Block::Type::Paragraph);
    // The word should be an inline
    QVERIFY(!block->getInlines().empty());
    QCOMPARE(block->getInlines()[0]->getText(), std::string("Hello"));
}

void TestOcrDjotMapper::test03_titleRegionMapsToHeading()
{
    OcrDjotMapper mapper;

    auto word   = makeWord("TITLE", QRectF(5, 3, 60, 15), 95);
    auto region = makeRegion(RegionType::Title, 0, QRectF(0, 0, 100, 20));

    auto page = makePage(0, {region}, {word});
    docmodel::SemanticDocument doc = mapper.fromOcrResults({page}, "test.pdf");

    QVERIFY(!doc.getSections().empty());
    const auto& blocks = doc.getSections()[0]->getBlocks();
    QVERIFY(!blocks.empty());
    QCOMPARE(blocks[0]->getType(), docmodel::Block::Type::Heading);
}

void TestOcrDjotMapper::test04_wordInlineHasBornOCRProvenance()
{
    OcrDjotMapper mapper;

    auto word   = makeWord("hello", QRectF(10, 5, 40, 12), 92);
    auto region = makeRegion(RegionType::Paragraph, 0, QRectF(0, 0, 100, 20));

    auto page = makePage(3, {region}, {word});
    docmodel::SemanticDocument doc = mapper.fromOcrResults({page}, "my_doc.pdf");

    const auto& section = doc.getSections()[0];
    const auto& block   = section->getBlocks()[0];
    QVERIFY(!block->getInlines().empty());

    const auto& inl  = block->getInlines()[0];
    const auto& prov = inl->getProvenance();

    // Must be BornOCR
    QCOMPARE(prov.tag, docmodel::ProvenanceTag::BornOCR);

    // Page index must match
    QCOMPARE(prov.page_index, 3);

    // source_file must contain the pdf-page attribute and the pdf path
    QVERIFY(prov.source_file.find("pdf-page=3") != std::string::npos);
    QVERIFY(prov.source_file.find("pdf-bbox") != std::string::npos);
    QVERIFY(prov.source_file.find("ocr-conf") != std::string::npos);
    QVERIFY(prov.source_file.find("my_doc.pdf") != std::string::npos);
}

void TestOcrDjotMapper::test05_blockOrderFollowsReadingOrderIndex()
{
    OcrDjotMapper mapper;

    // Title at readingOrder=1 (second), Paragraph at readingOrder=0 (first)
    auto titleRegion = makeRegion(RegionType::Title,     1, QRectF(0,  50, 100, 20));
    auto paraRegion  = makeRegion(RegionType::Paragraph, 0, QRectF(0,   0, 100, 20));

    auto titleWord = makeWord("Title text",     QRectF(5,  55, 60, 12), 90);
    auto paraWord  = makeWord("Paragraph text", QRectF(5,   5, 70, 12), 88);

    auto page = makePage(0, {titleRegion, paraRegion}, {titleWord, paraWord});
    docmodel::SemanticDocument doc = mapper.fromOcrResults({page}, "test.pdf");

    const auto& blocks = doc.getSections()[0]->getBlocks();
    QVERIFY(blocks.size() >= 2);

    // First block must be Paragraph (readingOrderIndex=0), second must be Heading (=1)
    QCOMPARE(blocks[0]->getType(), docmodel::Block::Type::Paragraph);
    QCOMPARE(blocks[1]->getType(), docmodel::Block::Type::Heading);
}

void TestOcrDjotMapper::test06_headerRegionAttr()
{
    OcrDjotMapper mapper;

    auto word   = makeWord("Page Header", QRectF(5, 3, 80, 12), 80);
    auto region = makeRegion(RegionType::Header, 0, QRectF(0, 0, 100, 20));

    auto page = makePage(0, {region}, {word});
    docmodel::SemanticDocument doc = mapper.fromOcrResults({page}, "doc.pdf");

    const auto& blocks = doc.getSections()[0]->getBlocks();
    QVERIFY(!blocks.empty());
    QCOMPARE(blocks[0]->getType(), docmodel::Block::Type::Paragraph);

    // Provenance source_file must contain page-header attr
    const auto& prov = blocks[0]->getProvenance();
    QVERIFY(prov.source_file.find("page-header") != std::string::npos);
}

void TestOcrDjotMapper::test07_footerRegionAttr()
{
    OcrDjotMapper mapper;

    auto word   = makeWord("Page Footer", QRectF(5, 3, 80, 12), 80);
    auto region = makeRegion(RegionType::Footer, 0, QRectF(0, 0, 100, 20));

    auto page = makePage(0, {region}, {word});
    docmodel::SemanticDocument doc = mapper.fromOcrResults({page}, "doc.pdf");

    const auto& blocks = doc.getSections()[0]->getBlocks();
    QVERIFY(!blocks.empty());
    QCOMPARE(blocks[0]->getType(), docmodel::Block::Type::Paragraph);

    const auto& prov = blocks[0]->getProvenance();
    QVERIFY(prov.source_file.find("page-footer") != std::string::npos);
}

void TestOcrDjotMapper::test08_listRegionMapsToList()
{
    OcrDjotMapper mapper;

    // Two words on separate Y lines → two list items
    auto w1 = makeWord("Item one",   QRectF(5,   5, 70, 12), 87);
    auto w2 = makeWord("Item two",   QRectF(5,  25, 70, 12), 88);
    auto region = makeRegion(RegionType::List, 0, QRectF(0, 0, 100, 45));

    auto page = makePage(0, {region}, {w1, w2});
    docmodel::SemanticDocument doc = mapper.fromOcrResults({page}, "doc.pdf");

    const auto& blocks = doc.getSections()[0]->getBlocks();
    QVERIFY(!blocks.empty());
    QCOMPARE(blocks[0]->getType(), docmodel::Block::Type::List);
    // Must have at least one ListItem child
    QVERIFY(!blocks[0]->getBlocks().empty());
    QCOMPARE(blocks[0]->getBlocks()[0]->getType(), docmodel::Block::Type::ListItem);
}

void TestOcrDjotMapper::test09_tableRegionMapsToTableBlock()
{
    OcrDjotMapper mapper;

    // Two rows, two cells each (separated by X-gap)
    // Row 1: y=5, cells at x=5..40 and x=60..95
    // Row 2: y=25, cells at x=5..40 and x=60..95
    auto r1c1 = makeWord("Name",   QRectF( 5,  5, 35, 12), 90);
    auto r1c2 = makeWord("Value",  QRectF(60,  5, 35, 12), 90);
    auto r2c1 = makeWord("Alice",  QRectF( 5, 25, 35, 12), 88);
    auto r2c2 = makeWord("42",     QRectF(60, 25, 15, 12), 95);

    auto region = makeRegion(RegionType::Table, 0, QRectF(0, 0, 100, 45));

    auto page = makePage(0, {region}, {r1c1, r1c2, r2c1, r2c2});
    docmodel::SemanticDocument doc = mapper.fromOcrResults({page}, "doc.pdf");

    const auto& blocks = doc.getSections()[0]->getBlocks();
    QVERIFY(!blocks.empty());
    QCOMPARE(blocks[0]->getType(), docmodel::Block::Type::Table);
    // Should have at least 2 row children
    QVERIFY(blocks[0]->getBlocks().size() >= 2);
}

void TestOcrDjotMapper::test10_encodeRoundtripParagraphText()
{
    OcrDjotMapper mapper;

    auto word   = makeWord("Hello world", QRectF(5, 5, 80, 12), 90);
    auto region = makeRegion(RegionType::Paragraph, 0, QRectF(0, 0, 100, 20));

    auto page = makePage(0, {region}, {word});
    docmodel::SemanticDocument doc = mapper.fromOcrResults({page}, "test.pdf");

    // Encode via C++ emitter (no Lua runtime needed for documentToDjot)
    pdfws::LuaDjotCodec codec("");  // empty path — encode-only path doesn't use Lua
    std::string djotText = codec.documentToDjot(doc);

    // The paragraph text must appear in the Djot output
    QVERIFY(!djotText.empty());
    QVERIFY(djotText.find("Hello world") != std::string::npos);
}

void TestOcrDjotMapper::test11_tableEncodeContainsPipeSyntax()
{
    OcrDjotMapper mapper;

    // Same table as test09
    auto r1c1 = makeWord("Name",  QRectF( 5,  5, 35, 12), 90);
    auto r1c2 = makeWord("Value", QRectF(60,  5, 35, 12), 90);
    auto r2c1 = makeWord("Alice", QRectF( 5, 25, 35, 12), 88);
    auto r2c2 = makeWord("42",    QRectF(60, 25, 15, 12), 95);

    auto region = makeRegion(RegionType::Table, 0, QRectF(0, 0, 100, 45));
    auto page   = makePage(0, {region}, {r1c1, r1c2, r2c1, r2c2});
    docmodel::SemanticDocument doc = mapper.fromOcrResults({page}, "test.pdf");

    pdfws::LuaDjotCodec codec("");
    std::string djotText = codec.documentToDjot(doc);

    // Must contain pipe-table syntax: '|' character
    QVERIFY(!djotText.empty());
    QVERIFY(djotText.find('|') != std::string::npos);

    // Must contain cell text
    QVERIFY(djotText.find("Name")  != std::string::npos);
    QVERIFY(djotText.find("Value") != std::string::npos);
    QVERIFY(djotText.find("Alice") != std::string::npos);
    QVERIFY(djotText.find("42")    != std::string::npos);
}

void TestOcrDjotMapper::test12_multiplePagesMakeSections()
{
    OcrDjotMapper mapper;

    // Page 0: one paragraph region with one word
    auto w0 = makeWord("page zero", QRectF(5, 5, 70, 12), 85);
    auto r0 = makeRegion(RegionType::Paragraph, 0, QRectF(0, 0, 100, 20));

    // Page 1: title + paragraph
    auto w1t = makeWord("Title",   QRectF(5,  5, 60, 15), 92);
    auto w1p = makeWord("Content", QRectF(5, 30, 60, 12), 87);
    auto r1t = makeRegion(RegionType::Title,     0, QRectF(0,  0, 100, 22));
    auto r1p = makeRegion(RegionType::Paragraph, 1, QRectF(0, 25, 100, 20));

    auto page0 = makePage(0, {r0},      {w0});
    auto page1 = makePage(1, {r1t, r1p}, {w1t, w1p});

    docmodel::SemanticDocument doc = mapper.fromOcrResults({page0, page1}, "doc.pdf");

    QCOMPARE(doc.getSections().size(), size_t(2));

    // Section 0: 1 block
    QCOMPARE(doc.getSections()[0]->getBlocks().size(), size_t(1));

    // Section 1: 2 blocks (title + paragraph)
    QCOMPARE(doc.getSections()[1]->getBlocks().size(), size_t(2));
    QCOMPARE(doc.getSections()[1]->getBlocks()[0]->getType(), docmodel::Block::Type::Heading);
    QCOMPARE(doc.getSections()[1]->getBlocks()[1]->getType(), docmodel::Block::Type::Paragraph);
}

void TestOcrDjotMapper::test13_failedPageProducesErrorSection()
{
    OcrDjotMapper mapper;

    PageOcrResult failPage;
    failPage.pageIndex    = 2;
    failPage.success      = false;
    failPage.errorMessage = "render timeout";

    docmodel::SemanticDocument doc = mapper.fromOcrResults({failPage}, "broken.pdf");

    // Should still have 1 section
    QCOMPARE(doc.getSections().size(), size_t(1));
    // That section should have 1 block (the error paragraph)
    const auto& section = doc.getSections()[0];
    QVERIFY(section);
    QVERIFY(!section->getBlocks().empty());
    QCOMPARE(section->getBlocks()[0]->getType(), docmodel::Block::Type::Paragraph);
}

void TestOcrDjotMapper::test14_figureRegionMapsToFigureBlock()
{
    OcrDjotMapper mapper;

    auto word   = makeWord("Figure caption", QRectF(5, 5, 80, 12), 85);
    auto region = makeRegion(RegionType::Figure, 0, QRectF(0, 0, 100, 20));

    auto page = makePage(0, {region}, {word});
    docmodel::SemanticDocument doc = mapper.fromOcrResults({page}, "doc.pdf");

    const auto& blocks = doc.getSections()[0]->getBlocks();
    QVERIFY(!blocks.empty());
    QCOMPARE(blocks[0]->getType(), docmodel::Block::Type::Figure);
}

QTEST_MAIN(TestOcrDjotMapper)
#include "TestOcrDjotMapper.moc"
