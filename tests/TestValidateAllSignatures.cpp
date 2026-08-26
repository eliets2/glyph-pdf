// SPDX-License-Identifier: Apache-2.0
// Audit 9.7 P0 regression test: Validate All Signatures must surface the
// already-computed SignatureInfo (isValid / trustStatus) as a bulk summary.
#include <QtTest/QtTest>
#include "shell/controllers/SecurityController.h"

class TestValidateAllSignatures : public QObject {
    Q_OBJECT
private slots:
    void summaryCountsValidAndListsEach();
    void unknownSignerAndTrustAreHonest();
};
void TestValidateAllSignatures::summaryCountsValidAndListsEach() {
    SignatureInfo a;
    a.signerName = QStringLiteral("Alice");
    a.isValid = true;
    a.trustStatus = QStringLiteral("Trusted");
    SignatureInfo b;
    b.signerName = QStringLiteral("Bob");
    b.isValid = false;
    b.trustStatus = QStringLiteral("UntrustedChain");

    const QString summary = gp::SecurityController::buildValidationSummary({a, b});
    QVERIFY2(summary.contains(QStringLiteral("1 of 2")), qPrintable(summary));
    QVERIFY2(summary.contains(QStringLiteral("Alice")), qPrintable(summary));
    QVERIFY2(summary.contains(QStringLiteral("Bob")), qPrintable(summary));
    QVERIFY2(summary.contains(QStringLiteral("VALID")), qPrintable(summary));
    QVERIFY2(summary.contains(QStringLiteral("INVALID")), qPrintable(summary));
    QVERIFY2(summary.contains(QStringLiteral("UntrustedChain")), qPrintable(summary));
}

void TestValidateAllSignatures::unknownSignerAndTrustAreHonest() {
    SignatureInfo anon;
    anon.signerName.clear();
    anon.isValid = true;
    anon.trustStatus.clear();

    const QString summary = gp::SecurityController::buildValidationSummary({anon});
    QVERIFY2(summary.contains(QStringLiteral("(unknown signer)")), qPrintable(summary));
    QVERIFY2(summary.contains(QStringLiteral("no trust check")), qPrintable(summary));
    QVERIFY2(summary.contains(QStringLiteral("1 of 1")), qPrintable(summary));
}
QTEST_MAIN(TestValidateAllSignatures)
#include "TestValidateAllSignatures.moc"
