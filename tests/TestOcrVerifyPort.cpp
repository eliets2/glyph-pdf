// SPDX-License-Identifier: Apache-2.0
// TestOcrVerifyPort — FOLD-2 port lane pins (archive/final/feat/ocr-verify-finereader).
//
// Each test group pins one ported B-item capability on the CURRENT seams
// (OcrReviewedWord stable records, selectWord funnel, nextUncertainWord walk,
// OcrConfidence classifier). R7 discipline: every group must FAIL against
// pre-port main (capability absent) and pass ×3 serially after its commit.
//
// P1 (B10): per-language user dictionary — file-backed Add-to-Dictionary;
// dictionary words stop being flagged in the uncertain-word walk.
#include <QtTest>
#include <QFile>
#include <QFileInfo>
#include <QToolButton>

#include "modes/OCRMode.h"
#include "modes/OcrConfidence.h"
#include "modes/OcrReviewSession.h"

using ::MergedOcrWord;
using gp::OCRMode;
using gp::OcrReviewedWord;
using gp::OcrReviewSession;

namespace {

// Dedicated test language so the pin never touches a real dictionary.
const QLatin1String kLang("QT");

void removeTestDictionary()
{
    QFile::remove(OCRMode::userDictionaryPath(QLatin1String(kLang)));
}

MergedOcrWord makeWord(const QString& text, int confidence, QRectF box)
{
    MergedOcrWord w;
    w.text = text;
    w.confidence = confidence;
    w.boundingBox = box;
    w.sourceEngine = QStringLiteral("Tesseract");
    return w;
}

// {alpha 95, beta 65, gamma 40}: one certain word, two uncertain words.
QList<MergedOcrWord> makeWords()
{
    return {
        makeWord(QStringLiteral("alpha"), 95, QRectF(10, 10, 60, 14)),
        makeWord(QStringLiteral("beta"),  65, QRectF(10, 40, 40, 14)),
        makeWord(QStringLiteral("gamma"), 40, QRectF(10, 70, 50, 14)),
    };
}

OcrReviewSession makeSession(const QList<MergedOcrWord>& words)
{
    OcrReviewSession s;
    s.generation = 7;
    s.sourcePath = QStringLiteral("C:/scans/src.pdf");
    s.sourcePage = 2;
    s.sourcePageCount = 40;
    QImage img(400, 300, QImage::Format_RGB32);
    img.fill(QColor(245, 245, 240));
    s.pageImage = img;
    for (int i = 0; i < words.size(); ++i) {
        OcrReviewedWord rec;
        rec.stableId     = i;
        rec.originalText = words[i].text;
        rec.reviewedText = words[i].text;
        rec.deleted      = false;
        rec.boundingBox  = words[i].boundingBox;
        rec.confidence   = words[i].confidence;
        rec.sourceEngine = words[i].sourceEngine;
        s.words.append(rec);
    }
    return s;
}

QToolButton* addToDictButton(const OCRMode& panel)
{
    return panel.findChild<QToolButton*>(QStringLiteral("ocrBtnAddToDict"));
}

} // namespace

class TestOcrVerifyPort : public QObject
{
    Q_OBJECT

private slots:

    // ── P1 (B10): per-language user dictionary ─────────────────────────────

    // The dictionary is a real file, one word per line, duplicate-free.
    void userDictionaryRoundTrip()
    {
        removeTestDictionary();

        // Empty/whitespace words are rejected, nothing is written.
        QVERIFY(!OCRMode::addUserDictionaryWord(QLatin1String(kLang), QStringLiteral("   ")));
        QVERIFY(OCRMode::loadUserDictionary(QLatin1String(kLang)).isEmpty());

        // A word round-trips through the file.
        QVERIFY(OCRMode::addUserDictionaryWord(QLatin1String(kLang), QStringLiteral("Dolor")));
        const QStringList dict = OCRMode::loadUserDictionary(QLatin1String(kLang));
        QCOMPARE(dict.size(), 1);
        QCOMPARE(dict.first(), QStringLiteral("Dolor"));

        // Re-adding the same word (any case) does not duplicate it.
        QVERIFY(OCRMode::addUserDictionaryWord(QLatin1String(kLang), QStringLiteral("dolor")));
        QCOMPARE(OCRMode::loadUserDictionary(QLatin1String(kLang)).size(), 1);

        removeTestDictionary();
    }

    // A word in the per-language user dictionary is vouched for by the user:
    // it leaves the uncertain walk (while its engine confidence stays Low for
    // provenance), and Add-to-Dictionary demotes the selected word live.
    void dictionarySuppressesUncertainWalk()
    {
        removeTestDictionary();

        OCRMode panel;
        panel.setUserDictionaryLanguage(QLatin1String(kLang));
        panel.setReviewSession(makeSession(makeWords()));

        // Baseline: beta and gamma are Low-band and flagged.
        QCOMPARE(panel.nextUncertainWord(-1, true), 1);   // beta
        QCOMPARE(panel.nextUncertainWord(1, true), 2);    // gamma

        // Vouch for "beta": it stops being navigated as uncertain.
        QVERIFY(OCRMode::addUserDictionaryWord(QLatin1String(kLang), QStringLiteral("beta")));
        panel.setUserDictionaryLanguage(QLatin1String(kLang));   // reload
        QCOMPARE(panel.nextUncertainWord(-1, true), 2);   // gamma is the only one left
        QCOMPARE(panel.nextUncertainWord(2, true), 2);    // sole uncertain word wraps to itself
        QCOMPARE(panel.nextUncertainWord(2, false), 2);

        // The suppression is review-status only: beta's confidence stays Low.
        QCOMPARE(gp::OcrConfidence::bandFor(panel.reviewedWords()[1].confidence),
                 gp::OcrConfidence::Band::Low);

        removeTestDictionary();
    }

    // Add-to-Dictionary in the word inspector: the selected word's text is
    // persisted for the session language and immediately leaves the walk.
    void addToDictionaryActionPersistsAndSuppresses()
    {
        removeTestDictionary();

        OCRMode panel;
        panel.setUserDictionaryLanguage(QLatin1String(kLang));
        panel.setReviewSession(makeSession(makeWords()));

        QToolButton* btn = addToDictButton(panel);
        QVERIFY(btn);

        // Select gamma (Low, not in dictionary) and vouch for it.
        panel.selectWord(2);
        QVERIFY(btn->isEnabled());
        btn->click();

        // Persisted for the session language…
        QVERIFY(OCRMode::loadUserDictionary(QLatin1String(kLang)).contains(QStringLiteral("gamma")));
        // …and live-suppressed: gamma leaves the walk, beta (still unvouched)
        // remains.
        QCOMPARE(panel.nextUncertainWord(-1, true), 1);   // beta
        QCOMPARE(panel.nextUncertainWord(2, true), 1);    // wraps past gamma to beta
        QCOMPARE(panel.nextUncertainWord(1, true), 1);    // beta is the sole one left

        // The Next/Prev buttons follow: something is still uncertain → enabled.
        QToolButton* next = panel.findChild<QToolButton*>(QStringLiteral("ocrBtnNextUncertain"));
        QVERIFY(next);
        QVERIFY(next->isEnabled());

        removeTestDictionary();
    }

protected:
    // Every dictionary test cleans its file even on failure paths above;
    // this teardown is a belt-and-braces guard for the whole suite.
    void init()
    {
        removeTestDictionary();
    }
    void cleanup()
    {
        removeTestDictionary();
    }
};

QTEST_MAIN(TestOcrVerifyPort)
#include "TestOcrVerifyPort.moc"
