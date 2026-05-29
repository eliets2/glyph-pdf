#include <QtTest/QtTest>
#include "mocks/MockSignatureManager.h"

class TestSignatureValidationMock : public QObject {
    Q_OBJECT

private slots:
    // -- Basic validation behavior --

    void testNoSignaturesOnFreshDocument()
    {
        MockSignatureManager mock;
        auto sigs = mock.validateSignatures("fresh.pdf");

        QVERIFY2(sigs.isEmpty(), "Fresh document should have no signatures");
        QCOMPARE(mock.m_validateCalls, 1);
        QCOMPARE(mock.m_lastInputPath, QString("fresh.pdf"));
    }

    void testValidateReturnsConfiguredSignatures()
    {
        MockSignatureManager mock;

        SignatureInfo sig;
        sig.fieldName = "Sig1";
        sig.signerName = "Test Signer";
        sig.reason = "Approval";
        sig.location = "Office";
        sig.date = QDateTime(QDate(2025, 1, 15), QTime(10, 30, 0));
        sig.isValid = true;
        sig.trustStatus = "Trusted";
        mock.m_signatures.append(sig);

        auto result = mock.validateSignatures("signed.pdf");
        QCOMPARE(result.size(), 1);
        QCOMPARE(result[0].fieldName, QString("Sig1"));
        QCOMPARE(result[0].signerName, QString("Test Signer"));
        QCOMPARE(result[0].reason, QString("Approval"));
        QCOMPARE(result[0].location, QString("Office"));
        QVERIFY(result[0].isValid);
        QCOMPARE(result[0].trustStatus, QString("Trusted"));
    }

    // -- Invalid/corrupt signature scenarios (should not crash) --

    void testInvalidSignatureDoesNotCrash()
    {
        MockSignatureManager mock;

        SignatureInfo badSig;
        badSig.fieldName = "";
        badSig.signerName = "";
        badSig.isValid = false;
        badSig.trustStatus = "INVALID";
        badSig.date = QDateTime(); // null datetime
        mock.m_signatures.append(badSig);

        auto result = mock.validateSignatures("corrupt.pdf");
        QCOMPARE(result.size(), 1);
        QVERIFY2(!result[0].isValid, "Invalid signature should report as invalid");
        QVERIFY(result[0].fieldName.isEmpty());
    }

    void testMultipleSignaturesMixedValidity()
    {
        MockSignatureManager mock;

        SignatureInfo validSig;
        validSig.fieldName = "Sig1";
        validSig.signerName = "Valid Signer";
        validSig.isValid = true;
        validSig.trustStatus = "Trusted";

        SignatureInfo invalidSig;
        invalidSig.fieldName = "Sig2";
        invalidSig.signerName = "Expired Signer";
        invalidSig.isValid = false;
        invalidSig.trustStatus = "CertificateExpired";

        SignatureInfo tampered;
        tampered.fieldName = "Sig3";
        tampered.signerName = "Tampered";
        tampered.isValid = false;
        tampered.trustStatus = "ByteRangeMismatch";

        mock.m_signatures = { validSig, invalidSig, tampered };

        auto result = mock.validateSignatures("multi.pdf");
        QCOMPARE(result.size(), 3);
        QVERIFY(result[0].isValid);
        QVERIFY(!result[1].isValid);
        QCOMPARE(result[1].trustStatus, QString("CertificateExpired"));
        QVERIFY(!result[2].isValid);
        QCOMPARE(result[2].trustStatus, QString("ByteRangeMismatch"));
    }

    // -- Byte range attack simulations --

    void testBadByteRangeSignatureReportsInvalid()
    {
        MockSignatureManager mock;

        // Simulate a signature with a byte range mismatch (e.g., an attacker
        // modified bytes outside the signed range). The mock models this as
        // an invalid signature with a specific trust status.
        SignatureInfo brokenSig;
        brokenSig.fieldName = "Sig_ByteRange";
        brokenSig.signerName = "Attacker";
        brokenSig.isValid = false;
        brokenSig.trustStatus = "ByteRangeMismatch";
        mock.m_signatures.append(brokenSig);

        auto result = mock.validateSignatures("tampered.pdf");
        QCOMPARE(result.size(), 1);
        QVERIFY2(!result[0].isValid, "Tampered byte range should be invalid");
        QCOMPARE(result[0].trustStatus, QString("ByteRangeMismatch"));
    }

    void testOverlappingByteRangeSignatureReportsInvalid()
    {
        MockSignatureManager mock;

        SignatureInfo overlapSig;
        overlapSig.fieldName = "Sig_Overlap";
        overlapSig.signerName = "Attacker";
        overlapSig.isValid = false;
        overlapSig.trustStatus = "ByteRangeOverlap";
        mock.m_signatures.append(overlapSig);

        auto result = mock.validateSignatures("overlap_attack.pdf");
        QCOMPARE(result.size(), 1);
        QVERIFY(!result[0].isValid);
        QCOMPARE(result[0].trustStatus, QString("ByteRangeOverlap"));
    }

    void testZeroLengthByteRangeDoesNotCrash()
    {
        MockSignatureManager mock;

        // A zero-length byte range is malformed -- engine should handle gracefully
        SignatureInfo zeroRange;
        zeroRange.fieldName = "Sig_ZeroRange";
        zeroRange.isValid = false;
        zeroRange.trustStatus = "MalformedByteRange";
        mock.m_signatures.append(zeroRange);

        auto result = mock.validateSignatures("zero_range.pdf");
        QCOMPARE(result.size(), 1);
        QVERIFY(!result[0].isValid);
    }

    void testSelfSignedUntrustedCertReportsInvalid()
    {
        MockSignatureManager mock;

        SignatureInfo untrustedSig;
        untrustedSig.fieldName = "Sig_Untrusted";
        untrustedSig.signerName = "SelfSignedUser";
        untrustedSig.isValid = false;
        untrustedSig.trustStatus = "UntrustedRoot";
        mock.m_signatures.append(untrustedSig);

        auto result = mock.validateSignatures("untrusted.pdf");
        QCOMPARE(result.size(), 1);
        QVERIFY2(!result[0].isValid, "Self-signed untrusted certificate should be marked invalid");
        QCOMPARE(result[0].trustStatus, QString("UntrustedRoot"));
    }

    void testIncompleteByteRangeReportsInvalid()
    {
        MockSignatureManager mock;

        SignatureInfo incompleteSig;
        incompleteSig.fieldName = "Sig_Incomplete";
        incompleteSig.signerName = "Attacker";
        incompleteSig.isValid = false;
        incompleteSig.trustStatus = "IncompleteByteRange";
        mock.m_signatures.append(incompleteSig);

        auto result = mock.validateSignatures("incomplete_range.pdf");
        QCOMPARE(result.size(), 1);
        QVERIFY2(!result[0].isValid, "Incomplete byte range should be marked invalid");
        QCOMPARE(result[0].trustStatus, QString("IncompleteByteRange"));
    }

    // -- Sign document tests --

    void testSignDocumentSuccess()
    {
        MockSignatureManager mock;
        mock.m_signResult = true;

        bool ok = mock.signDocument("input.pdf", "signed.pdf",
                                    "cert.p12", "password",
                                    "Approval", "Office");
        QVERIFY(ok);
        QCOMPARE(mock.m_signCalls, 1);
        QCOMPARE(mock.m_lastInputPath, QString("input.pdf"));
        QCOMPARE(mock.m_lastOutputPath, QString("signed.pdf"));
        QCOMPARE(mock.m_lastCertPath, QString("cert.p12"));
        QCOMPARE(mock.m_lastReason, QString("Approval"));
        QCOMPARE(mock.m_lastLocation, QString("Office"));
    }

    void testSignDocumentFailure()
    {
        MockSignatureManager mock;
        mock.m_signResult = false;

        bool ok = mock.signDocument("input.pdf", "signed.pdf",
                                    "bad_cert.p12", "wrong", "", "");
        QVERIFY2(!ok, "Sign should fail with bad cert");
        QCOMPARE(mock.m_signCalls, 1);
    }

    void testSignWithEmptyPaths()
    {
        MockSignatureManager mock;
        mock.m_signResult = true;

        // Mock does not validate paths -- that is the real engine's job.
        // This test verifies the mock tracks calls correctly with empty input.
        bool ok = mock.signDocument("", "", "", "", "", "");
        QVERIFY(ok);
        QCOMPARE(mock.m_lastInputPath, QString(""));
        QCOMPARE(mock.m_lastOutputPath, QString(""));
    }

    // -- Validate call counting --

    void testMultipleValidateCalls()
    {
        MockSignatureManager mock;

        mock.validateSignatures("a.pdf");
        mock.validateSignatures("b.pdf");
        mock.validateSignatures("c.pdf");

        QCOMPARE(mock.m_validateCalls, 3);
        QCOMPARE(mock.m_lastInputPath, QString("c.pdf"));
    }

    // -- PAdES level selection --

    void testSetSignatureLevelDefaultIsB_T()
    {
        MockSignatureManager mock;
        QCOMPARE(mock.m_level, PAdESLevel::B_T);
    }

    void testSetSignatureLevelB_LT()
    {
        MockSignatureManager mock;
        mock.setSignatureLevel(PAdESLevel::B_LT);
        QCOMPARE(mock.m_level, PAdESLevel::B_LT);
    }

    void testSetSignatureLevelB_LTA()
    {
        MockSignatureManager mock;
        mock.setSignatureLevel(PAdESLevel::B_LTA);
        QCOMPARE(mock.m_level, PAdESLevel::B_LTA);
    }

    void testSetSignatureLevelB_B()
    {
        MockSignatureManager mock;
        mock.setSignatureLevel(PAdESLevel::B_B);
        QCOMPARE(mock.m_level, PAdESLevel::B_B);
    }

    // -- DSS / B-LT structure in SignatureInfo --

    void testValidationReportsHasDssWhenPresent()
    {
        MockSignatureManager mock;

        SignatureInfo sig;
        sig.fieldName = "Sig1";
        sig.signerName = "PKI Signer";
        sig.isValid = true;
        sig.trustStatus = "ValidWithDSS";
        sig.hasDss = true;         // B-LT: DSS dictionary present
        sig.hasDocTimestamp = false;
        mock.m_signatures.append(sig);

        auto result = mock.validateSignatures("b_lt.pdf");
        QCOMPARE(result.size(), 1);
        QVERIFY(result[0].isValid);
        QVERIFY(result[0].hasDss);
        QVERIFY(!result[0].hasDocTimestamp);
        QCOMPARE(result[0].trustStatus, QString("ValidWithDSS"));
    }

    void testValidationReportsDocTimestampForB_LTA()
    {
        MockSignatureManager mock;

        SignatureInfo sig;
        sig.fieldName = "Sig1";
        sig.signerName = "Archival Signer";
        sig.isValid = true;
        sig.trustStatus = "ValidWithDSS";
        sig.hasDss = true;
        sig.hasDocTimestamp = true; // B-LTA: DocTimeStamp present
        mock.m_signatures.append(sig);

        auto result = mock.validateSignatures("b_lta.pdf");
        QCOMPARE(result.size(), 1);
        QVERIFY(result[0].hasDss);
        QVERIFY(result[0].hasDocTimestamp);
    }

    void testBasicSignatureHasNoDss()
    {
        MockSignatureManager mock;

        SignatureInfo sig;
        sig.fieldName = "Sig1";
        sig.isValid = true;
        sig.trustStatus = "Valid";
        sig.hasDss = false;
        sig.hasDocTimestamp = false;
        mock.m_signatures.append(sig);

        auto result = mock.validateSignatures("b_b.pdf");
        QCOMPARE(result.size(), 1);
        QVERIFY(!result[0].hasDss);
        QVERIFY(!result[0].hasDocTimestamp);
    }

    void testSignLevelPassedCorrectly()
    {
        MockSignatureManager mock;
        mock.setSignatureLevel(PAdESLevel::B_LTA);
        mock.m_signResult = true;

        bool ok = mock.signDocument("in.pdf", "out.pdf", "cert.p12", "pw", "", "");
        QVERIFY(ok);
        // Level set independently — sign does not reset it
        QCOMPARE(mock.m_level, PAdESLevel::B_LTA);
        QCOMPARE(mock.m_signCalls, 1);
    }
};

QTEST_GUILESS_MAIN(TestSignatureValidationMock)
#include "TestSignatureValidationMock.moc"
