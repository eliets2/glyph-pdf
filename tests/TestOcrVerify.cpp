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
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QSettings>
#include <QSignalSpy>
#include <QStandardPaths>
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
    /** Give QSettings a real scope so language persistence is testable. */
    void initTestCase()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("glyphpdf-tests"));
        QCoreApplication::setApplicationName(QStringLiteral("ocr-verify-tests"));
    }

    /** High-confidence-only input produces zero highlights. */
    void noHighlightsWhenAllConfident();

    /** Words below 70 are counted and highlighted in the text pane. */
    void lowConfidenceWordsHighlighted();

    /** The Uncertain toggle hides/reapplies the highlights. */
    void toggleHidesAndRestoresHighlights();

    /** Rejecting results clears counts and highlights. */
    void rejectClearsHighlightState();

    /** B5: clicking a scan-pane word selects it in the text pane + zoom. */
    void scanWordClickSyncsTextAndZoom();

    /** B5: moving the text caret into a word emits wordSelected. */
    void textCaretMoveEmitsWordSelected();

    /** B2: Next/Prev walk the low-confidence index in order, wrapping. */
    void uncertainNavigationWalksIndexInOrder();

    /** B7: page progress + language cells render in the info strip. */
    void statusStripShowsPageAndLanguage();

    /** B7: verified % tracks markWordVerified and resets on new results. */
    void verifiedPercentTracksVerification();

    /** B6: zoom pane shows a magnified crop when a page image is present. */
    void zoomPaneRendersMagnifiedCrop();

    /** B3/B4: Verify dialog walks flagged words; Confirm applies edits. */
    void verifyDialogWalksAndAppliesCorrections();

    /** B12: page-verified toggle state and its reset on new results. */
    void pageVerifiedToggleAndReset();

    /** B10: user dictionary suppresses flagging and persists to disk. */
    void userDictionarySuppressesFlagging();

    /** B9: ranked suggestions appear in the dialog and fill the edit field. */
    void suggestionsRankedAndApplicable();
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

void TestOcrVerify::scanWordClickSyncsTextAndZoom()
{
    gp::OCRMode mode;
    QList<MergedOcrWord> words;
    words << makeWord("alpha", 95) << makeWord("b3ta", 40);
    mode.setOcrResults(words);

    QSignalSpy spy(&mode, &gp::OCRMode::wordSelected);

    // Simulate clicking word 1 in the scan pane (QLabel link activation).
    emit mode.findChild<QLabel*>(QStringLiteral("ocrScanContent"))->linkActivated(
        QStringLiteral("ocrword:1"));

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.first().at(0).toInt(), 1);
    // Text pane selected the corresponding range.
    QVERIFY(mode.textPane()->textCursor().hasSelection());
    QCOMPARE(mode.textPane()->textCursor().selectedText(), QStringLiteral("b3ta"));
    // Zoom pane shows the recognized word.
    bool zoomPopulated = false;
    const QList<QLabel*> labels = mode.findChildren<QLabel*>();
    for (const QLabel* l : labels)
        if (l->text().contains(QStringLiteral("conf 40%"))) zoomPopulated = true;
    QVERIFY2(zoomPopulated, "zoom meta label not populated with word confidence");
}

void TestOcrVerify::textCaretMoveEmitsWordSelected()
{
    gp::OCRMode mode;
    QList<MergedOcrWord> words;
    words << makeWord("alpha", 95) << makeWord("b3ta", 40) << makeWord("gamma", 88);
    mode.setOcrResults(words);

    QSignalSpy spy(&mode, &gp::OCRMode::wordSelected);

    // Move the caret into the second word ("b3ta" starts at char 6).
    QTextCursor c = mode.textPane()->textCursor();
    c.setPosition(7);
    mode.textPane()->setTextCursor(c);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.first().at(0).toInt(), 1);
}

void TestOcrVerify::uncertainNavigationWalksIndexInOrder()
{
    gp::OCRMode mode;
    QList<MergedOcrWord> words;
    words << makeWord("alpha", 95) << makeWord("b3ta", 40)
          << makeWord("gamma", 88) << makeWord("d1ta", 10);
    mode.setOcrResults(words);

    QSignalSpy spy(&mode, &gp::OCRMode::wordSelected);

    // Next from "not started" lands on the first low-confidence word (index 1).
    mode.findChild<QToolButton*>(QStringLiteral("ocrBtnNextUncertain"))->click();
    QCOMPARE(spy.last().at(0).toInt(), 1);

    // Next again reaches the second low-confidence word (index 3).
    mode.findChild<QToolButton*>(QStringLiteral("ocrBtnNextUncertain"))->click();
    QCOMPARE(spy.last().at(0).toInt(), 3);

    // Next wraps around to the first low-confidence word.
    mode.findChild<QToolButton*>(QStringLiteral("ocrBtnNextUncertain"))->click();
    QCOMPARE(spy.last().at(0).toInt(), 1);

    // Prev from "not started" starts at the last low-confidence word.
    gp::OCRMode mode2;
    mode2.setOcrResults(words);
    QSignalSpy spy2(&mode2, &gp::OCRMode::wordSelected);
    mode2.findChild<QToolButton*>(QStringLiteral("ocrBtnPrevUncertain"))->click();
    QCOMPARE(spy2.last().at(0).toInt(), 3);
}

void TestOcrVerify::statusStripShowsPageAndLanguage()
{
    // Persist a language so the strip has a real value to show.
    // sync() makes the buffered write visible to OCRMode's own QSettings
    // instance (offscreen tests share the default store).
    QSettings settings;
    settings.setValue(QStringLiteral("ocr/language"), QStringLiteral("DE"));
    settings.sync();

    gp::OCRMode mode;
    QList<MergedOcrWord> words;
    words << makeWord("alpha", 95) << makeWord("b3ta", 40);
    mode.setOcrResults(words);
    mode.setPageProgress(2, 14);

    QString pageCell, langCell, uncertainCell, verifiedCell;
    const QList<QLabel*> labels = mode.findChildren<QLabel*>();
    for (const QLabel* l : labels) {
        const QString t = l->text();
        if (t.startsWith(QStringLiteral("PAGE")))       pageCell = t;
        if (t.startsWith(QStringLiteral("LANGUAGE")))   langCell = t;
        if (t.startsWith(QStringLiteral("UNCERTAIN")))  uncertainCell = t;
        if (t.startsWith(QStringLiteral("VERIFIED")))   verifiedCell = t;
    }
    QCOMPARE(pageCell, QStringLiteral("PAGE 2 OF 14"));
    QCOMPARE(langCell, QStringLiteral("LANGUAGE DE"));
    QCOMPARE(uncertainCell, QStringLiteral("UNCERTAIN 1 REMAINING"));
    QCOMPARE(verifiedCell, QStringLiteral("VERIFIED 0%"));
}

void TestOcrVerify::verifiedPercentTracksVerification()
{
    gp::OCRMode mode;
    QList<MergedOcrWord> words;
    words << makeWord("a", 95) << makeWord("b", 40) << makeWord("c", 95) << makeWord("d", 10);
    mode.setOcrResults(words);
    QCOMPARE(mode.verifiedPercent(), 0);

    mode.markWordVerified(0);
    mode.markWordVerified(1);
    QCOMPARE(mode.verifiedPercent(), 50); // 2 of 4

    mode.markWordVerified(3);
    QCOMPARE(mode.verifiedPercent(), 75);

    // Out-of-range index is ignored.
    mode.markWordVerified(99);
    QCOMPARE(mode.verifiedPercent(), 75);

    // Loading new results resets verification state.
    mode.setOcrResults(words);
    QCOMPARE(mode.verifiedPercent(), 0);
}

void TestOcrVerify::zoomPaneRendersMagnifiedCrop()
{
    gp::OCRMode mode;
    QList<MergedOcrWord> words;
    MergedOcrWord w = makeWord("do1or", 42);
    w.boundingBox = QRectF(10, 10, 30, 12);
    words << w;
    mode.setOcrResults(words);

    // Without a page image the zoom pane falls back to text.
    mode.findChild<QToolButton*>(QStringLiteral("ocrBtnNextUncertain"))->click();

    // Provide a page raster: selecting a word now renders a magnified crop.
    QImage page(100, 40, QImage::Format_RGB32);
    page.fill(Qt::white);
    mode.setPageImage(page);

    bool foundPixmap = false;
    const QList<QLabel*> labels = mode.findChildren<QLabel*>();
    for (QLabel* l : labels)
        if (!l->pixmap().isNull()) foundPixmap = true;
    QVERIFY2(foundPixmap, "zoom pane did not render a magnified crop pixmap");
}

void TestOcrVerify::verifyDialogWalksAndAppliesCorrections()
{
    gp::OCRMode mode;
    QList<MergedOcrWord> words;
    words << makeWord("alpha", 95) << makeWord("do1or", 42)
          << makeWord("gamma", 88);
    mode.setOcrResults(words);

    // Open the Verify Text dialog from the toolbar.
    auto* btnVerify = mode.findChild<QToolButton*>(QStringLiteral("ocrBtnVerify"));
    QVERIFY2(btnVerify, "Verify Text toolbar button not found");
    btnVerify->click();

    auto* dlg = mode.findChild<gp::OcrVerifyDialog*>();
    QVERIFY2(dlg, "OcrVerifyDialog was not created");
    QCOMPARE(dlg->remaining(), 1);           // one low-confidence word
    auto* edit = dlg->findChild<QLineEdit*>(QStringLiteral("ocrVerifyEdit"));
    QVERIFY(edit);
    QCOMPARE(edit->text(), QStringLiteral("do1or"));

    // Confirm a correction: the text pane must be updated in place.
    edit->setText(QStringLiteral("dolor"));
    dlg->findChild<QToolButton*>(QStringLiteral("ocrVerifyConfirm"))->click();
    QCOMPARE(mode.textPane()->toPlainText(),
             QStringLiteral("alpha dolor gamma"));
    QCOMPARE(mode.verifiedPercent(), 33);    // 1 of 3 words verified
    QCOMPARE(dlg->remaining(), 0);           // queue exhausted
}

void TestOcrVerify::pageVerifiedToggleAndReset()
{
    gp::OCRMode mode;
    QList<MergedOcrWord> words;
    words << makeWord("alpha", 95);
    mode.setOcrResults(words);
    QVERIFY(!mode.isPageVerified());

    auto* btn = mode.findChild<QToolButton*>(QStringLiteral("ocrBtnPageVerified"));
    QVERIFY2(btn, "Page Verified toolbar button not found");
    btn->click();
    QVERIFY(mode.isPageVerified());

    // Programmatic reset also syncs the button.
    mode.setPageVerified(false);
    QVERIFY(!btn->isChecked());

    // Loading new results resets the state again.
    btn->click();
    QVERIFY(mode.isPageVerified());
    mode.setOcrResults(words);
    QVERIFY(!mode.isPageVerified());
}

void TestOcrVerify::userDictionarySuppressesFlagging()
{
    // Isolate the on-disk dictionary for this test process.
    QCoreApplication::setApplicationName(
        QStringLiteral("ocr-verify-dict-%1").arg(QCoreApplication::applicationPid()));
    QStandardPaths::setTestModeEnabled(true);

    const QString lang = QStringLiteral("EN");
    QVERIFY(gp::OCRMode::loadUserDictionary(lang).isEmpty());
    QVERIFY(gp::OCRMode::addUserDictionaryWord(lang, QStringLiteral("do1or")));
    // Duplicate add is a no-op but still succeeds.
    QVERIFY(gp::OCRMode::addUserDictionaryWord(lang, QStringLiteral("do1or")));
    QCOMPARE(gp::OCRMode::loadUserDictionary(lang).size(), 1);

    gp::OCRMode mode;
    QList<MergedOcrWord> words;
    words << makeWord("do1or", 42) << makeWord("b3ta", 10);
    mode.setOcrResults(words);

    // 'do1or' is in the dictionary → only 'b3ta' stays flagged.
    QCOMPARE(mode.lowConfidenceWordCount(), 1);
    QCOMPARE(mode.uncertainHighlightCount(), 1);
}

void TestOcrVerify::suggestionsRankedAndApplicable()
{
    // Isolate from the B10 dictionary test running earlier in this process:
    // a leftover 'do1or' entry would suppress flagging entirely.
    QCoreApplication::setApplicationName(
        QStringLiteral("ocr-verify-sugg-%1").arg(QCoreApplication::applicationPid()));
    QStandardPaths::setTestModeEnabled(true);

    // Unit level: distance-1 candidate ranks first; far words are excluded.
    const QStringList vocab = {"dolor", "dollar", "gamma", "zebra"};
    const QStringList sugg = gp::OCRMode::suggestCorrections(
        QStringLiteral("do1or"), vocab);
    QVERIFY2(!sugg.isEmpty(), "expected at least one suggestion");
    QCOMPARE(sugg.first(), QStringLiteral("dolor"));
    QVERIFY(!sugg.contains(QStringLiteral("zebra")));

    // Dialog level: suggestions populate the list; clicking fills the edit.
    gp::OCRMode mode;
    QList<MergedOcrWord> words;
    words << makeWord("do1or", 42) << makeWord("dolor", 95) << makeWord("dollar", 90);
    mode.setOcrResults(words);
    mode.findChild<QToolButton*>(QStringLiteral("ocrBtnVerify"))->click();

    auto* dlg = mode.findChild<gp::OcrVerifyDialog*>();
    QVERIFY(dlg);
    auto* list = dlg->findChild<QListWidget*>(QStringLiteral("ocrVerifySuggestions"));
    QVERIFY2(list, "suggestions list not found");
    QVERIFY(list->count() >= 1);
    QCOMPARE(list->item(0)->text(), QStringLiteral("dolor"));

    list->item(0)->setSelected(true);
    emit list->itemClicked(list->item(0));
    auto* edit = dlg->findChild<QLineEdit*>(QStringLiteral("ocrVerifyEdit"));
    QCOMPARE(edit->text(), QStringLiteral("dolor"));
}

QTEST_MAIN(TestOcrVerify)
#include "TestOcrVerify.moc"
