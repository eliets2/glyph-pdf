#include <QtTest>
#include <QCoreApplication>
#include <memory>

#include "docmodel/ProvenanceTag.h"
#include "docmodel/SemanticDocument.h"
#include "pdfws_djot/LuaDjotCodec.h"
#include "pdfws_djot/ProvenanceGuard.h"

static std::string djotLibPath() {
    return (QCoreApplication::applicationDirPath() + "/../third_party/djot").toStdString();
}

class TestDjotRoundtrip : public QObject {
    Q_OBJECT

private slots:
    void testDecodeSimpleDjot() {
        pdfws::LuaDjotCodec codec(djotLibPath());
        auto doc = codec.djotToDocument("# Hello\n\nA paragraph.");
        QVERIFY2(doc != nullptr, "djotToDocument should parse valid djot and return non-null document");
    }

    void testDecodeEmptyDjot() {
        // Empty string is valid input; should not throw
        pdfws::LuaDjotCodec codec(djotLibPath());
        try {
            auto doc = codec.djotToDocument("");
            // null or empty document both acceptable
        } catch (const std::exception& e) {
            QFAIL(qPrintable(QString("djotToDocument(\"\") threw: %1").arg(e.what())));
        }
    }

    void testEncodeIsStubbed() {
        // LuaDjotCodec::documentToDjot is not yet implemented (LuaDjotCodec.cpp:54 TODO).
        // M5-PROMPT-4 (OCR→Djot mapping) is blocked until this stub is closed.
        // Remove this test once encode is fully implemented.
        pdfws::LuaDjotCodec codec(djotLibPath());
        docmodel::Provenance prov;
        docmodel::SemanticDocument doc({}, prov);
        const std::string result = codec.documentToDjot(doc);
        QVERIFY2(result.empty(),
            "Encode returns empty until LuaDjotCodec::documentToDjot is implemented (M4-P7 known stub)");
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
