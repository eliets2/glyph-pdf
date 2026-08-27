// SPDX-License-Identifier: Apache-2.0
// §9.12 P0 regression test: Batch mode must expose the OCR language selection
// and feed the resolved engine code into the batch OCR run — batch OCR was
// hard-wired to initialize("eng"), making it unusable for non-English
// document sets. The language table and UI-code→engine-code mapping come from
// core/OcrTypes.h (the same single source of truth the interactive path uses).
#include <QtTest/QtTest>
#include <QComboBox>
#include <QSettings>
#include <QStackedWidget>
#include "modes/BatchMode.h"
#include "core/OcrTypes.h"

class TestBatchOcrLanguage : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void cleanupTestCase();
    void ocrPanelHasLanguageCombo();
    void comboListsAllTwelveLanguages();
    void comboDefaultsToPersistedPreference();
    void uiCodeMapsToEngineCode();
};

void TestBatchOcrLanguage::initTestCase() {
    // Isolate QSettings so the test neither reads the user's real preference
    // nor clobbers it.
    QCoreApplication::setOrganizationName(QStringLiteral("GlyphPDFTests"));
    QCoreApplication::setApplicationName(QStringLiteral("TestBatchOcrLanguage"));
}

void TestBatchOcrLanguage::cleanupTestCase() {
    QSettings().remove(QStringLiteral("ocr/language"));
}

void TestBatchOcrLanguage::ocrPanelHasLanguageCombo() {
    QSettings().setValue(QStringLiteral("ocr/language"), QStringLiteral("EN"));
    gp::BatchMode bm;
    // The OCR panel (cfg-stack index 5) must carry a language combo.
    auto* stack = bm.findChild<QStackedWidget*>();
    QVERIFY(stack);
    QComboBox* langCombo = nullptr;
    for (int i = 0; i < stack->count(); ++i) {
        const auto combos = stack->widget(i)->findChildren<QComboBox*>();
        for (auto* c : combos) {
            // The language combo holds the shared language table's UI codes.
            if (c->count() == ocrLanguages().size() && c->currentData().toString()
                    == QStringLiteral("EN")) {
                langCombo = c;
            }
        }
    }
    QVERIFY2(langCombo, "Batch OCR panel must expose a language combo");
}

void TestBatchOcrLanguage::comboListsAllTwelveLanguages() {
    QSettings().setValue(QStringLiteral("ocr/language"), QStringLiteral("EN"));
    gp::BatchMode bm;
    auto* stack = bm.findChild<QStackedWidget*>();
    QVERIFY(stack);
    QComboBox* langCombo = nullptr;
    int best = 0;
    for (int i = 0; i < stack->count(); ++i) {
        for (auto* c : stack->widget(i)->findChildren<QComboBox*>()) {
            if (c->count() > best) { best = c->count(); langCombo = c; }
        }
    }
    QVERIFY(langCombo);
    QCOMPARE(langCombo->count(), ocrLanguages().size());
    // Spot-check the table is the source (first = English, data = UI code).
    QCOMPARE(langCombo->itemData(0).toString(), QStringLiteral("EN"));
}

void TestBatchOcrLanguage::comboDefaultsToPersistedPreference() {
    QSettings().setValue(QStringLiteral("ocr/language"), QStringLiteral("DE"));
    gp::BatchMode bm;
    auto* stack = bm.findChild<QStackedWidget*>();
    QVERIFY(stack);
    QComboBox* langCombo = nullptr;
    for (int i = 0; i < stack->count(); ++i) {
        for (auto* c : stack->widget(i)->findChildren<QComboBox*>()) {
            if (c->count() == ocrLanguages().size()) langCombo = c;
        }
    }
    QVERIFY(langCombo);
    QCOMPARE(langCombo->currentData().toString(), QStringLiteral("DE"));
}

void TestBatchOcrLanguage::uiCodeMapsToEngineCode() {
    // The mapping the worker consumes: UI code → Tesseract engine code.
    QCOMPARE(ocrEngineLanguageCode(QStringLiteral("DE")), QStringLiteral("deu"));
    QCOMPARE(ocrEngineLanguageCode(QStringLiteral("JA")), QStringLiteral("jpn"));
    QCOMPARE(ocrEngineLanguageCode(QStringLiteral("ZH")), QStringLiteral("chi_sim"));
    // Unknown/empty falls back to English rather than garbage.
    QCOMPARE(ocrEngineLanguageCode(QStringLiteral("XX")), QStringLiteral("eng"));
    QCOMPARE(ocrEngineLanguageCode(QString()), QStringLiteral("eng"));
}

QTEST_MAIN(TestBatchOcrLanguage)
#include "TestBatchOcrLanguage.moc"
