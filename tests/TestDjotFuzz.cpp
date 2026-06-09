// SPDX-License-Identifier: MIT
// TestDjotFuzz.cpp  — G-07: DocumentFuzzer round-trip fuzz harness
//
// Tests the documentToDjot → djotToDocument round-trip using pseudo-random
// SemanticDocument instances produced by DocumentFuzzer.
//
// NOTE on structural equivalence:
//   djotToDocument currently parses the Djot AST via Lua but does NOT yet
//   walk the parsed AST back into SemanticDocument nodes — it returns an empty
//   document (placeholder pending M5 AST-walking implementation). Therefore:
//   - Tests that assert "encode must produce non-empty output for non-empty input"
//     run as hard QVERIFY assertions.
//   - Tests that assert structural equivalence (section count, block count, text)
//     are marked QEXPECT_FAIL with a clear label so they become hard failures
//     automatically once djotToDocument is fully implemented.
//   - The round-trip must NEVER throw, crash, or produce null — that is always a
//     hard failure regardless of the decode stub status.
//
// Wired by CMakeLists.txt (same pattern as TestDjotRoundtrip).

#include <QtTest>
#include <QCoreApplication>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "docmodel/ProvenanceTag.h"
#include "docmodel/SemanticDocument.h"
#include "docmodel/Block.h"
#include "docmodel/Inline.h"
#include "docmodel/DocumentFuzzer.h"
#include "pdfws_djot/LuaDjotCodec.h"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static std::string djotLibPath()
{
    // The djot Lua library is vendored at <src_root>/third_party/djot.
    // Under ctest the CWD is build/; the test exe is in build/ or a subdir.
    // applicationDirPath() gives the dir of the test binary; go one level up
    // to reach the source root (same pattern as TestDjotRoundtrip).
    return (QCoreApplication::applicationDirPath() + "/../third_party/djot").toStdString();
}

static docmodel::Provenance anyProv()
{
    return docmodel::Provenance{docmodel::ProvenanceTag::BornDjot, "", -1, {}};
}

// Count all top-level blocks across all sections in a document.
static int countTopLevelBlocks(const docmodel::SemanticDocument& doc)
{
    int count = 0;
    for (const auto& sec : doc.getSections()) {
        if (sec) count += static_cast<int>(sec->getBlocks().size());
    }
    return count;
}

// Collect all leaf text strings (TextInline nodes) from the document.
static std::vector<std::string> collectLeafTexts(const docmodel::SemanticDocument& doc)
{
    std::vector<std::string> texts;

    std::function<void(const docmodel::Inline&)> visitInline;
    visitInline = [&](const docmodel::Inline& inl) {
        if (inl.getType() == docmodel::Inline::Type::Text ||
            inl.getType() == docmodel::Inline::Type::Code) {
            const std::string t = inl.getText();
            if (!t.empty()) texts.push_back(t);
        }
        for (const auto& child : inl.getChildren()) {
            if (child) visitInline(*child);
        }
    };

    std::function<void(const docmodel::Block&)> visitBlock;
    visitBlock = [&](const docmodel::Block& blk) {
        for (const auto& inl : blk.getInlines()) {
            if (inl) visitInline(*inl);
        }
        for (const auto& child : blk.getBlocks()) {
            if (child) visitBlock(*child);
        }
    };

    std::function<void(const docmodel::Section&)> visitSection;
    visitSection = [&](const docmodel::Section& sec) {
        for (const auto& blk : sec.getBlocks()) {
            if (blk) visitBlock(*blk);
        }
        for (const auto& sub : sec.getSubsections()) {
            if (sub) visitSection(*sub);
        }
    };

    for (const auto& sec : doc.getSections()) {
        if (sec) visitSection(*sec);
    }
    return texts;
}

// ---------------------------------------------------------------------------
// Test class
// ---------------------------------------------------------------------------

class TestDjotFuzz : public QObject
{
    Q_OBJECT

private slots:

    // -----------------------------------------------------------------------
    // F-01 through F-10: Fuzz with 10 different random seeds.
    //
    // For each seed:
    //   1. generateRandomDocument(seed) → model
    //   2. documentToDjot(model) → djotText  (hard assertions: non-null, non-empty)
    //   3. djotToDocument(djotText) → reparsed  (hard assertions: no throw, non-null)
    //   4. Structural equivalence checks (QEXPECT_FAIL until djotToDocument is complete)
    // -----------------------------------------------------------------------

    void testFuzzSeed0()  { runFuzzSeed(0);  }
    void testFuzzSeed1()  { runFuzzSeed(1);  }
    void testFuzzSeed2()  { runFuzzSeed(2);  }
    void testFuzzSeed3()  { runFuzzSeed(3);  }
    void testFuzzSeed4()  { runFuzzSeed(4);  }
    void testFuzzSeed5()  { runFuzzSeed(5);  }
    void testFuzzSeed6()  { runFuzzSeed(6);  }
    void testFuzzSeed7()  { runFuzzSeed(7);  }
    void testFuzzSeed8()  { runFuzzSeed(8);  }
    void testFuzzSeed9()  { runFuzzSeed(9);  }
    void testFuzzSeed42() { runFuzzSeed(42); }
    void testFuzzSeed99() { runFuzzSeed(99); }

    // -----------------------------------------------------------------------
    // E-01: Empty document — encode must produce empty string; decode must
    //       not crash and must return non-null.
    // -----------------------------------------------------------------------
    void testEdgeCaseEmptyDocument()
    {
        pdfws::LuaDjotCodec codec(djotLibPath());

        docmodel::SemanticDocument emptyDoc({}, anyProv());

        // Encode: empty doc must produce empty string (no sections).
        const std::string djotText = codec.documentToDjot(emptyDoc);
        QVERIFY2(djotText.empty(),
                 qPrintable(QString("Empty document must encode to empty string, got: '%1'")
                            .arg(QString::fromStdString(djotText))));

        // Decode empty string: must not throw, must return non-null.
        std::unique_ptr<docmodel::SemanticDocument> reparsed;
        try {
            reparsed = codec.djotToDocument("");
        } catch (const std::exception& e) {
            QFAIL(qPrintable(QString("djotToDocument(\"\") threw: %1").arg(e.what())));
        } catch (...) {
            QFAIL("djotToDocument(\"\") threw an unknown exception");
        }
        QVERIFY2(reparsed != nullptr,
                 "djotToDocument(\"\") must return non-null document");

        // Decode of empty-string produces an empty document (no sections to recover).
        QCOMPARE(static_cast<int>(reparsed->getSections().size()), 0);
    }

    // -----------------------------------------------------------------------
    // E-02: Document with every supported Block::Type.
    //       documentToDjot must include all content; djotToDocument must not crash.
    // -----------------------------------------------------------------------
    void testEdgeCaseAllBlockTypes()
    {
        pdfws::LuaDjotCodec codec(djotLibPath());

        // Build one block of each type.
        auto makeText = [](const std::string& t) {
            return std::make_shared<docmodel::TextInline>(t, anyProv());
        };
        auto makeItem = [&](const std::string& text) {
            return std::make_shared<docmodel::TextBlock>(
                docmodel::Block::Type::ListItem,
                std::vector<std::shared_ptr<docmodel::Inline>>{makeText(text)},
                anyProv());
        };

        std::vector<std::shared_ptr<docmodel::Block>> blocks;

        // Paragraph
        blocks.push_back(std::make_shared<docmodel::TextBlock>(
            docmodel::Block::Type::Paragraph,
            std::vector<std::shared_ptr<docmodel::Inline>>{makeText("Para text")},
            anyProv()));

        // Heading
        blocks.push_back(std::make_shared<docmodel::TextBlock>(
            docmodel::Block::Type::Heading,
            std::vector<std::shared_ptr<docmodel::Inline>>{makeText("Heading text")},
            anyProv()));

        // List
        blocks.push_back(std::make_shared<docmodel::ContainerBlock>(
            docmodel::Block::Type::List,
            std::vector<std::shared_ptr<docmodel::Block>>{makeItem("Item A"), makeItem("Item B")},
            anyProv()));

        // CodeBlock (ContainerBlock with Paragraph children as lines)
        auto codeLine = std::make_shared<docmodel::TextBlock>(
            docmodel::Block::Type::Paragraph,
            std::vector<std::shared_ptr<docmodel::Inline>>{makeText("code line")},
            anyProv());
        blocks.push_back(std::make_shared<docmodel::ContainerBlock>(
            docmodel::Block::Type::CodeBlock,
            std::vector<std::shared_ptr<docmodel::Block>>{codeLine},
            anyProv()));

        // Figure (TextBlock with caption as inlines)
        blocks.push_back(std::make_shared<docmodel::TextBlock>(
            docmodel::Block::Type::Figure,
            std::vector<std::shared_ptr<docmodel::Inline>>{makeText("Figure caption")},
            anyProv()));

        // Table (ContainerBlock with row ContainerBlocks, each row has cell TextBlocks)
        auto cell1 = std::make_shared<docmodel::TextBlock>(
            docmodel::Block::Type::Paragraph,
            std::vector<std::shared_ptr<docmodel::Inline>>{makeText("Col1")},
            anyProv());
        auto cell2 = std::make_shared<docmodel::TextBlock>(
            docmodel::Block::Type::Paragraph,
            std::vector<std::shared_ptr<docmodel::Inline>>{makeText("Col2")},
            anyProv());
        auto row = std::make_shared<docmodel::ContainerBlock>(
            docmodel::Block::Type::ListItem,
            std::vector<std::shared_ptr<docmodel::Block>>{cell1, cell2},
            anyProv());
        blocks.push_back(std::make_shared<docmodel::ContainerBlock>(
            docmodel::Block::Type::Table,
            std::vector<std::shared_ptr<docmodel::Block>>{row},
            anyProv()));

        auto sec = std::make_shared<docmodel::Section>(
            "AllTypes", std::move(blocks), std::vector<std::shared_ptr<docmodel::Section>>{}, anyProv());
        docmodel::SemanticDocument doc({sec}, anyProv());

        // Encode
        std::string djotText;
        try {
            djotText = codec.documentToDjot(doc);
        } catch (const std::exception& e) {
            QFAIL(qPrintable(QString("documentToDjot threw: %1").arg(e.what())));
        }
        QVERIFY2(!djotText.empty(),
                 "documentToDjot must produce non-empty output for an all-types document");

        // Check content presence for key block types
        QVERIFY2(djotText.find("Para text") != std::string::npos,
                 "Paragraph content must appear in encoded Djot");
        QVERIFY2(djotText.find("Item A") != std::string::npos,
                 "List item A must appear in encoded Djot");
        QVERIFY2(djotText.find("Item B") != std::string::npos,
                 "List item B must appear in encoded Djot");
        QVERIFY2(djotText.find("code line") != std::string::npos,
                 "CodeBlock content must appear in encoded Djot");

        // Decode: must not throw and must return non-null
        try {
            auto reparsed = codec.djotToDocument(djotText);
            QVERIFY2(reparsed != nullptr,
                     "djotToDocument must return non-null for all-types Djot output");
        } catch (const std::exception& e) {
            QFAIL(qPrintable(QString("djotToDocument(allTypes) threw: %1").arg(e.what())));
        }
    }

    // -----------------------------------------------------------------------
    // E-03: Deeply nested sections (max_depth=5).
    //       documentToDjot must not stack-overflow; djotToDocument must not crash.
    // -----------------------------------------------------------------------
    void testEdgeCaseDeeplyNested()
    {
        pdfws::LuaDjotCodec codec(djotLibPath());

        // DocumentFuzzer with max_depth=5 produces deeply nested sections.
        auto model = docmodel::DocumentFuzzer::generateRandomDocument(/*seed=*/777, /*max_depth=*/5);
        QVERIFY2(model != nullptr, "DocumentFuzzer::generateRandomDocument must return non-null");

        std::string djotText;
        try {
            djotText = codec.documentToDjot(*model);
        } catch (const std::exception& e) {
            QFAIL(qPrintable(QString("documentToDjot (deeply nested) threw: %1").arg(e.what())));
        } catch (...) {
            QFAIL("documentToDjot (deeply nested) threw an unknown exception");
        }
        QVERIFY2(!djotText.empty(),
                 "documentToDjot must produce non-empty output for a deeply-nested document");

        try {
            auto reparsed = codec.djotToDocument(djotText);
            QVERIFY2(reparsed != nullptr,
                     "djotToDocument must return non-null for deeply-nested Djot input");
        } catch (const std::exception& e) {
            QFAIL(qPrintable(QString("djotToDocument (deeply nested) threw: %1").arg(e.what())));
        } catch (...) {
            QFAIL("djotToDocument (deeply nested) threw an unknown exception");
        }
    }

    // -----------------------------------------------------------------------
    // E-04: Special-character text (Djot metacharacters).
    //       Escaped correctly; decode must not crash.
    // -----------------------------------------------------------------------
    void testEdgeCaseSpecialChars()
    {
        pdfws::LuaDjotCodec codec(djotLibPath());

        // These characters are special in Djot and must be escaped by documentToDjot.
        const std::string specialText = "a*b_c`d[e]f{g}h^i~j<k>l\\m";
        auto para = std::make_shared<docmodel::TextBlock>(
            docmodel::Block::Type::Paragraph,
            std::vector<std::shared_ptr<docmodel::Inline>>{
                std::make_shared<docmodel::TextInline>(specialText, anyProv())
            },
            anyProv());
        auto sec = std::make_shared<docmodel::Section>(
            "SpecialChars", std::vector<std::shared_ptr<docmodel::Block>>{para},
            std::vector<std::shared_ptr<docmodel::Section>>{}, anyProv());
        docmodel::SemanticDocument doc({sec}, anyProv());

        std::string djotText;
        try {
            djotText = codec.documentToDjot(doc);
        } catch (const std::exception& e) {
            QFAIL(qPrintable(QString("documentToDjot (special chars) threw: %1").arg(e.what())));
        }
        QVERIFY2(!djotText.empty(), "documentToDjot must produce output for special-char content");

        // The encoded output must contain backslash-escaped versions.
        QVERIFY2(djotText.find("\\*") != std::string::npos,
                 "asterisk must be backslash-escaped in Djot output");
        QVERIFY2(djotText.find("\\_") != std::string::npos,
                 "underscore must be backslash-escaped in Djot output");
        QVERIFY2(djotText.find("\\`") != std::string::npos,
                 "backtick must be backslash-escaped in Djot output");

        // Decode must not throw or crash
        try {
            auto reparsed = codec.djotToDocument(djotText);
            QVERIFY2(reparsed != nullptr,
                     "djotToDocument must return non-null for special-char Djot");
        } catch (const std::exception& e) {
            QFAIL(qPrintable(QString("djotToDocument (special chars) threw: %1").arg(e.what())));
        }
    }

    // -----------------------------------------------------------------------
    // E-05: Section count round-trip.
    //
    // QEXPECT_FAIL: djotToDocument currently returns an empty SemanticDocument
    // (AST walking not yet implemented). This test will fail until M5 wires up
    // the AST-to-SemanticDocument mapper. Marked so the CI failure is expected
    // now and becomes a hard failure once the implementation is complete.
    // -----------------------------------------------------------------------
    void testStructuralEquivalenceSectionCount()
    {
        pdfws::LuaDjotCodec codec(djotLibPath());

        // Build a document with exactly 3 top-level sections.
        auto makeSec = [](const std::string& title) {
            auto para = std::make_shared<docmodel::TextBlock>(
                docmodel::Block::Type::Paragraph,
                std::vector<std::shared_ptr<docmodel::Inline>>{
                    std::make_shared<docmodel::TextInline>("Content of " + title, anyProv())
                }, anyProv());
            return std::make_shared<docmodel::Section>(
                title,
                std::vector<std::shared_ptr<docmodel::Block>>{para},
                std::vector<std::shared_ptr<docmodel::Section>>{},
                anyProv());
        };
        docmodel::SemanticDocument doc(
            {makeSec("Alpha"), makeSec("Beta"), makeSec("Gamma")},
            anyProv());
        QCOMPARE(static_cast<int>(doc.getSections().size()), 3);

        const std::string djotText = codec.documentToDjot(doc);
        QVERIFY2(!djotText.empty(), "encode must produce non-empty output");

        auto reparsed = codec.djotToDocument(djotText);
        QVERIFY2(reparsed != nullptr, "decode must return non-null");

        // EXPECTED FAILURE: djotToDocument stub returns empty document.
        // Remove QEXPECT_FAIL once djotToDocument fully walks the Lua AST.
        QEXPECT_FAIL("", "djotToDocument AST-walking not yet implemented (M5); "
                         "section count will be 0 until then", Continue);
        QCOMPARE(static_cast<int>(reparsed->getSections().size()), 3);
    }

    // -----------------------------------------------------------------------
    // E-06: Fuzz round-trip structural assertion over 12 seeds.
    //       documentToDjot(fuzzed) must produce output whose section count
    //       equals the fuzzed document's section count after decode.
    //
    // QEXPECT_FAIL: same stub limitation as E-05.
    // -----------------------------------------------------------------------
    void testFuzzRoundtripSectionCount()
    {
        pdfws::LuaDjotCodec codec(djotLibPath());

        const std::vector<int> seeds = {0, 1, 2, 3, 7, 13, 42, 99, 128, 255, 1000, 9999};
        for (int seed : seeds) {
            auto model = docmodel::DocumentFuzzer::generateRandomDocument(seed);
            QVERIFY2(model != nullptr,
                     qPrintable(QString("Fuzzer returned null for seed %1").arg(seed)));

            const int originalSectionCount = static_cast<int>(model->getSections().size());

            std::string djotText;
            try {
                djotText = codec.documentToDjot(*model);
            } catch (const std::exception& e) {
                QFAIL(qPrintable(QString("seed %1: documentToDjot threw: %2")
                                 .arg(seed).arg(e.what())));
            }

            // Non-empty model must produce non-empty Djot.
            if (originalSectionCount > 0) {
                QVERIFY2(!djotText.empty(),
                         qPrintable(QString("seed %1: non-empty model must produce non-empty Djot").arg(seed)));
            }

            std::unique_ptr<docmodel::SemanticDocument> reparsed;
            try {
                reparsed = codec.djotToDocument(djotText);
            } catch (const std::exception& e) {
                QFAIL(qPrintable(QString("seed %1: djotToDocument threw: %2").arg(seed).arg(e.what())));
            }
            QVERIFY2(reparsed != nullptr,
                     qPrintable(QString("seed %1: djotToDocument returned null").arg(seed)));

            // Structural equivalence: section count must round-trip.
            // EXPECTED FAILURE until djotToDocument AST-walking is implemented.
            if (originalSectionCount > 0) {
                QEXPECT_FAIL("", "djotToDocument AST-walking not yet implemented (M5)", Continue);
                QCOMPARE(static_cast<int>(reparsed->getSections().size()),
                         originalSectionCount);
            }
        }
    }

private:

    // -----------------------------------------------------------------------
    // Core fuzz harness — called by testFuzzSeed*() slots.
    // -----------------------------------------------------------------------
    void runFuzzSeed(int seed)
    {
        pdfws::LuaDjotCodec codec(djotLibPath());

        // 1. Generate a random document.
        auto model = docmodel::DocumentFuzzer::generateRandomDocument(seed);
        QVERIFY2(model != nullptr,
                 qPrintable(QString("DocumentFuzzer::generateRandomDocument(%1) returned null")
                            .arg(seed)));

        const int numSections   = static_cast<int>(model->getSections().size());
        const int numBlocks     = countTopLevelBlocks(*model);
        const auto leafTexts    = collectLeafTexts(*model);

        // 2. Encode to Djot text.
        std::string djotText;
        try {
            djotText = codec.documentToDjot(*model);
        } catch (const std::exception& e) {
            QFAIL(qPrintable(QString("seed %1: documentToDjot threw: %2")
                             .arg(seed).arg(QString::fromStdString(e.what()))));
        } catch (...) {
            QFAIL(qPrintable(QString("seed %1: documentToDjot threw unknown exception").arg(seed)));
        }

        // A non-empty model must produce non-empty Djot.
        if (numSections > 0) {
            QVERIFY2(!djotText.empty(),
                     qPrintable(QString("seed %1: non-empty model must produce non-empty Djot")
                                .arg(seed)));
        }

        // Each leaf text found in the model must appear somewhere in the Djot output
        // (possibly escaped). This is a basic content-presence check.
        // We don't check byte-exact matches because escapeDjotText adds backslashes.
        for (const auto& rawText : leafTexts) {
            // Check just the first 10 chars (enough to detect total omission).
            // Escapable characters are skipped to avoid false negatives.
            std::string probe;
            for (char c : rawText) {
                if (c != '*' && c != '_' && c != '`' && c != '[' && c != ']' &&
                    c != '{' && c != '}' && c != '^' && c != '~' && c != '<' &&
                    c != '>' && c != '\\') {
                    probe += c;
                }
                if (static_cast<int>(probe.size()) >= 8) break;
            }
            if (probe.size() < 3) continue;  // too short to check meaningfully

            QVERIFY2(djotText.find(probe) != std::string::npos,
                     qPrintable(QString("seed %1: leaf text probe '%2' not found in Djot output")
                                .arg(seed)
                                .arg(QString::fromStdString(probe))));
        }

        // 3. Decode the Djot text back to a document — must not throw or return null.
        std::unique_ptr<docmodel::SemanticDocument> reparsed;
        try {
            reparsed = codec.djotToDocument(djotText);
        } catch (const std::exception& e) {
            QFAIL(qPrintable(QString("seed %1: djotToDocument threw: %2")
                             .arg(seed).arg(QString::fromStdString(e.what()))));
        } catch (...) {
            QFAIL(qPrintable(QString("seed %1: djotToDocument threw unknown exception").arg(seed)));
        }
        QVERIFY2(reparsed != nullptr,
                 qPrintable(QString("seed %1: djotToDocument must return non-null").arg(seed)));

        // 4. Structural equivalence (QEXPECT_FAIL until M5 AST-walking is done).
        //    Check section count only when the original had sections.
        if (numSections > 0) {
            QEXPECT_FAIL("",
                         "djotToDocument returns empty doc until AST-walking implemented (M5)",
                         Continue);
            QVERIFY2(static_cast<int>(reparsed->getSections().size()) == numSections,
                     qPrintable(QString("seed %1: section count mismatch: got %2 want %3")
                                .arg(seed)
                                .arg(reparsed->getSections().size())
                                .arg(numSections)));
        }

        // Block count check (same QEXPECT_FAIL guard).
        if (numBlocks > 0) {
            QEXPECT_FAIL("",
                         "djotToDocument returns empty doc until AST-walking implemented (M5)",
                         Continue);
            QVERIFY2(countTopLevelBlocks(*reparsed) == numBlocks,
                     qPrintable(QString("seed %1: block count mismatch: got %2 want %3")
                                .arg(seed)
                                .arg(countTopLevelBlocks(*reparsed))
                                .arg(numBlocks)));
        }

        Q_UNUSED(djotText);
    }
};

QTEST_MAIN(TestDjotFuzz)
#include "TestDjotFuzz.moc"
