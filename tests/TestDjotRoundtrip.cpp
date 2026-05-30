#include <QtTest>
#include <QCoreApplication>
#include <memory>

#include "docmodel/ProvenanceTag.h"
#include "docmodel/SemanticDocument.h"
#include "docmodel/Block.h"
#include "docmodel/Inline.h"
#include "pdfws_djot/LuaDjotCodec.h"
#include "pdfws_djot/ProvenanceGuard.h"

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
    // D1 — decode still works after encode implementation
    void testDecodeSimpleDjot() {
        pdfws::LuaDjotCodec codec(djotLibPath());
        auto doc = codec.djotToDocument("# Hello\n\nA paragraph.");
        QVERIFY2(doc != nullptr, "djotToDocument should parse valid djot and return non-null document");
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
        QVERIFY2(result.find("**bold**") != std::string::npos,
                 qPrintable(QString("Expected '**bold**' in: %1").arg(result.c_str())));
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

    // D2 — parse Djot → encode → result contains core content (decode stub produces empty doc)
    // NOTE: True structural round-trip (decode(encode(doc)) == doc) is deferred until
    // djotToDocument fully walks the parsed AST (currently creates empty SemanticDocument).
    // This spec round-trip test validates that encode output is parseable Djot (no Lua errors).
    void testSpecRoundtripNoCrash() {
        pdfws::LuaDjotCodec codec(djotLibPath());
        auto sec = makeSection("Title", {makeParagraph("Content.")});
        docmodel::SemanticDocument doc({sec}, anyProv());
        const std::string djotOut = codec.documentToDjot(doc);
        // Re-parse the encoded output — must not throw
        try {
            auto reparsed = codec.djotToDocument(djotOut);
            QVERIFY2(reparsed != nullptr, "re-parsing encoded Djot must not produce null");
        } catch (const std::exception& e) {
            QFAIL(qPrintable(QString("re-parsing encoded Djot threw: %1").arg(e.what())));
        }
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
};

QTEST_MAIN(TestDjotRoundtrip)
#include "TestDjotRoundtrip.moc"
