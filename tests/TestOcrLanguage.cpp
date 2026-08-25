// SPDX-License-Identifier: Apache-2.0
// Audit 9.4 P0 regression test: the OCR language selector must reach the
// engines. Pins the shared OcrLanguageInfo table + ocrEngineLanguageCode()
// mapping used by EditController::runOcr (was hard-coded "eng").
#include <QtTest>
#include <QSet>
#include "core/OcrTypes.h"
class TestOcrLanguage : public QObject {
    Q_OBJECT
private slots:
    void tableCoversSelectorLanguages();
    void engineCodesAreUnique();
    void mappingMatchesTable();
    void unknownFallsBackToEng();
};

void TestOcrLanguage::tableCoversSelectorLanguages() {
    const auto& langs = ocrLanguages();
    QCOMPARE(langs.size(), 12);
    for (const auto& l : langs) {
        QVERIFY(QByteArray(l.uiCode).size() == 2);
        QVERIFY(!QByteArray(l.engineCode).isEmpty());
        QVERIFY(!QByteArray(l.displayName).isEmpty());
    }
}

void TestOcrLanguage::engineCodesAreUnique() {
    QSet<QString> seen;
    for (const auto& l : ocrLanguages()) {
        const QString code = QLatin1String(l.engineCode);
        QVERIFY2(!seen.contains(code), qPrintable(QStringLiteral("duplicate engine code: %1").arg(code)));
        seen.insert(code);
    }
}

void TestOcrLanguage::mappingMatchesTable() {
    for (const auto& l : ocrLanguages()) {
        const QString ui = QLatin1String(l.uiCode);
        QCOMPARE(ocrEngineLanguageCode(ui), QLatin1String(l.engineCode));
        QCOMPARE(ocrEngineLanguageCode(ui.toLower()), QLatin1String(l.engineCode));
    }
}

void TestOcrLanguage::unknownFallsBackToEng() {
    QCOMPARE(ocrEngineLanguageCode(QStringLiteral("XX")), QStringLiteral("eng"));
    QCOMPARE(ocrEngineLanguageCode(QString()), QStringLiteral("eng"));
    QCOMPARE(ocrEngineLanguageCode(QStringLiteral("  en ")), QStringLiteral("eng"));
}

QTEST_MAIN(TestOcrLanguage)
#include "TestOcrLanguage.moc"
