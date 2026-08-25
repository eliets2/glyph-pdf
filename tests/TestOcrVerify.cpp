/**
 * TestOcrVerify — B1: uncertain-word highlighting in the OCR Verify text pane.
 *
 * Verifies that setOcrResults() flags low-confidence words (< 70), applies
 * matching ExtraSelection highlights to the editable text pane, honors the
 * Uncertain toggle, and clears state on reject.
 *
 * Run: QT_QPA_PLATFORM=offscreen ctest -R TestOcrVerify --output-on-failure
 */
#include <QtTest/QtTest>
#include <QApplication>
#include <QPlainTextEdit>
#include <QToolButton>

#include "modes/OCRMode.h"

using ::MergedOcrWord;

static MergedOcrWord makeWord(const QString& text, int confidence)
{
    MergedOcrWord w;
    w.text = text;
    w.confidence = confidence;
    w.sourceEngine = QStringLiteral("Tesseract");
    return w;
}

class TestOcrVerify : public QObject {
    Q_OBJECT

private slots:
    /** High-confidence-only input produces zero highlights. */
    void noHighlightsWhenAllConfident();

    /** Words below 70 are counted and highlighted in the text pane. */
    void lowConfidenceWordsHighlighted();

    /** The Uncertain toggle hides/reapplies the highlights. */
    void toggleHidesAndRestoresHighlights();

    /** Rejecting results clears counts and highlights. */
    void rejectClearsHighlightState();
};
void TestOcrVerify::noHighlightsWhenAllConfident()
{
    gp::OCRMode mode;
    QList<MergedOcrWord> words;
    words << makeWord("The", 95) << makeWord("quick", 92)
          << makeWord("brown", 88) << makeWord("fox", 90);
    mode.setOcrResults(words);

    QCOMPARE(mode.lowConfidenceWordCount(), 0);
    QCOMPARE(mode.uncertainHighlightCount(), 0);
    // Text pane still receives the full recognized text.
    QCOMPARE(mode.textPane()->toPlainText(), QStringLiteral("The quick brown fox"));
}

void TestOcrVerify::lowConfidenceWordsHighlighted()
{
    gp::OCRMode mode;
    QList<MergedOcrWord> words;
    words << makeWord("The", 95) << makeWord("do1or", 42)
          << makeWord("sit", 91)  << makeWord("amet", 55)
          << makeWord("here", 70); // boundary: exactly 70 is NOT low-conf
    mode.setOcrResults(words);

    QCOMPARE(mode.lowConfidenceWordCount(), 2);
    QCOMPARE(mode.uncertainHighlightCount(), 2);
}

void TestOcrVerify::toggleHidesAndRestoresHighlights()
{
    gp::OCRMode mode;
    QList<MergedOcrWord> words;
    words << makeWord("alpha", 30) << makeWord("beta", 90) << makeWord("gamma", 10);
    mode.setOcrResults(words);
    QCOMPARE(mode.uncertainHighlightCount(), 2);

    auto* toggle = mode.findChild<QToolButton*>(QStringLiteral("ocrBtnUncertainToggle"));
    QVERIFY2(toggle, "Uncertain toggle button not found by objectName");

    toggle->setChecked(false);
    QCOMPARE(mode.uncertainHighlightCount(), 0);
    // Word index survives the toggle so it can be restored.
    QCOMPARE(mode.lowConfidenceWordCount(), 2);

    toggle->setChecked(true);
    QCOMPARE(mode.uncertainHighlightCount(), 2);
}

void TestOcrVerify::rejectClearsHighlightState()
{
    gp::OCRMode mode;
    QList<MergedOcrWord> words;
    words << makeWord("muddy", 20);
    mode.setOcrResults(words);
    QCOMPARE(mode.uncertainHighlightCount(), 1);

    mode.findChild<QToolButton*>(QStringLiteral("ocrBtnReject"))->click();
    QCOMPARE(mode.lowConfidenceWordCount(), 0);
    QCOMPARE(mode.uncertainHighlightCount(), 0);
    QVERIFY(mode.textPane()->toPlainText().isEmpty());
}

QTEST_MAIN(TestOcrVerify)
#include "TestOcrVerify.moc"
