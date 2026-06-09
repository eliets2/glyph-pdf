// SPDX-License-Identifier: Apache-2.0
// TestChain1.cpp — R2-1 CHAIN-1 closure tests (ProvenanceGuard gate logic)
//
// This test covers D1 (ProvenanceGuard logic), which requires only pdfws_djot
// and does NOT link against pdfws_engines or OpenSSL. It runs in all environments.
//
// D2 (writeUpdate routing) and D3 (signed-doc integration) are covered by:
//   - The existing testWriteUpdatePreservesSignature in TestSignatureRealCrypto
//   - TestChain1Crypto (requires fixtures; follows same skip pattern)
//
// Test B: ProvenanceGuard throws for DjotThenSave on a signed document (any origin)
// Test C: ProvenanceGuard allows DirectStructural on a signed document
// Test D: ProvenanceGuard allows DjotThenSaveAsCopy on a signed document
// Test E: ProvenanceGuard allows unsigned BornPDF + DjotThenSave (with warning)
// Test F: ProvenanceGuard allows unsigned BornDjot + DjotThenSave

#include <QtTest/QtTest>

#include "pdfws_djot/ProvenanceGuard.h"
#include "docmodel/ProvenanceTag.h"

class TestChain1 : public QObject
{
    Q_OBJECT

private slots:

    // -------------------------------------------------------------------
    // Test B (D1): ProvenanceGuard::checkEditVia must THROW for
    // DjotThenSave when isSigned=true, regardless of ProvenanceTag.
    // -------------------------------------------------------------------
    void testProvenanceGuard_DjotThenSave_Signed_BornPDF_Throws()
    {
        pdfws::ProvenanceGuard guard;
        QVERIFY_EXCEPTION_THROWN(
            guard.checkEditVia(docmodel::ProvenanceTag::BornPDF,
                               /*isSigned=*/true,
                               pdfws::EditPath::DjotThenSave),
            pdfws::ProvenanceViolation);
    }

    void testProvenanceGuard_DjotThenSave_Signed_BornDjot_Throws()
    {
        pdfws::ProvenanceGuard guard;
        // BornDjot + DjotThenSave + signed must also be refused (CHAIN-1 D1).
        QVERIFY_EXCEPTION_THROWN(
            guard.checkEditVia(docmodel::ProvenanceTag::BornDjot,
                               /*isSigned=*/true,
                               pdfws::EditPath::DjotThenSave),
            pdfws::ProvenanceViolation);
    }

    // -------------------------------------------------------------------
    // Test C (D1): DirectStructural on a signed document must be ALLOWED
    // (save routing handles it via writeUpdate).
    // -------------------------------------------------------------------
    void testProvenanceGuard_DirectStructural_Signed_Allowed()
    {
        pdfws::ProvenanceGuard guard;
        pdfws::ProvenanceGuardResult result;
        bool threw = false;
        try {
            result = guard.checkEditVia(docmodel::ProvenanceTag::BornPDF,
                                        /*isSigned=*/true,
                                        pdfws::EditPath::DirectStructural);
        } catch (...) {
            threw = true;
        }
        QVERIFY2(!threw, "DirectStructural on a signed doc must not throw");
        QVERIFY2(result.allowed,
                 "DirectStructural on a signed doc must be allowed (routes to writeUpdate)");
    }

    // -------------------------------------------------------------------
    // Test D (D1): DjotThenSaveAsCopy on a signed document is always safe
    // (writes a new file, doesn't touch the original).
    // -------------------------------------------------------------------
    void testProvenanceGuard_DjotThenSaveAsCopy_Signed_Allowed()
    {
        pdfws::ProvenanceGuard guard;
        pdfws::ProvenanceGuardResult result;
        bool threw = false;
        try {
            result = guard.checkEditVia(docmodel::ProvenanceTag::BornPDF,
                                        /*isSigned=*/true,
                                        pdfws::EditPath::DjotThenSaveAsCopy);
        } catch (...) {
            threw = true;
        }
        QVERIFY2(!threw, "DjotThenSaveAsCopy must not throw");
        QVERIFY2(result.allowed, "DjotThenSaveAsCopy must always be allowed");
    }

    // -------------------------------------------------------------------
    // Test E (D1): Unsigned BornPDF + DjotThenSave → allowed with warning.
    // -------------------------------------------------------------------
    void testProvenanceGuard_DjotThenSave_Unsigned_BornPDF_WarnButAllow()
    {
        pdfws::ProvenanceGuard guard;
        pdfws::ProvenanceGuardResult result;
        bool threw = false;
        try {
            result = guard.checkEditVia(docmodel::ProvenanceTag::BornPDF,
                                        /*isSigned=*/false,
                                        pdfws::EditPath::DjotThenSave);
        } catch (...) {
            threw = true;
        }
        QVERIFY2(!threw, "Unsigned BornPDF + DjotThenSave must not throw");
        QVERIFY2(result.allowed, "Unsigned BornPDF + DjotThenSave must be allowed");
        QVERIFY2(result.requiresWarning, "Unsigned BornPDF + DjotThenSave must require a warning");
    }

    // -------------------------------------------------------------------
    // Test F (D1): Unsigned BornDjot + DjotThenSave → allowed, no warning.
    // -------------------------------------------------------------------
    void testProvenanceGuard_DjotThenSave_Unsigned_BornDjot_Allowed()
    {
        pdfws::ProvenanceGuard guard;
        pdfws::ProvenanceGuardResult result;
        bool threw = false;
        try {
            result = guard.checkEditVia(docmodel::ProvenanceTag::BornDjot,
                                        /*isSigned=*/false,
                                        pdfws::EditPath::DjotThenSave);
        } catch (...) {
            threw = true;
        }
        QVERIFY2(!threw, "Unsigned BornDjot + DjotThenSave must not throw");
        QVERIFY2(result.allowed, "Unsigned BornDjot + DjotThenSave must be allowed");
    }
};

QTEST_GUILESS_MAIN(TestChain1)
#include "TestChain1.moc"
