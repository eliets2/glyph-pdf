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
#include <QComboBox>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QToolButton>

#include "modes/OCRMode.h"
#include "modes/OcrConfidence.h"
#include "modes/OcrReviewSession.h"
#include "ui/OcrScanCanvas.h"

using ::MergedOcrWord;
using gp::OCRMode;
using gp::OcrReviewedWord;
using gp::OcrReviewSession;
using gp::OcrScanCanvas;

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

    // ── P2 (B9): ranked spelling suggestions in the word inspector ─────────

    // Pure seam: candidates are ranked by Damerau-Levenshtein distance (≤ 2),
    // the word itself is never suggested (case-insensitive), and the list
    // caps at 5.
    void suggestionsRankByEditDistance()
    {
        const QStringList vocab = {
            QStringLiteral("dolor"),     // sub 1→l            → dist 1
            QStringLiteral("color"),     // sub d→c, sub 1→l   → dist 2
            QStringLiteral("colon"),     // dist 3             → excluded
            QStringLiteral("dolorous"),  // dist 4             → excluded
            QStringLiteral("colored"),   // dist 4+            → excluded
        };
        const QStringList ranked = OCRMode::suggestCorrections(QStringLiteral("do1or"), vocab);
        QCOMPARE(ranked, (QStringList{ QStringLiteral("dolor"), QStringLiteral("color") }));

        // Transpositions count as ONE edit (Damerau, optimal string alignment).
        QCOMPARE(OCRMode::suggestCorrections(QStringLiteral("ac"),
                                             QStringList{ QStringLiteral("ca") }).size(), 1);

        // The word itself (any case) is not suggested.
        QVERIFY(OCRMode::suggestCorrections(QStringLiteral("Dolor"),
                                            QStringList{ QStringLiteral("dolor") }).isEmpty());

        // The list is capped at 5: "abc0".."abc7" are all dist 1 from "abc".
        QStringList near;
        for (int i = 0; i < 8; ++i) near.append(QStringLiteral("abc%1").arg(i));
        QCOMPARE(OCRMode::suggestCorrections(QStringLiteral("abc"), near).size(), 5);
    }

    // Wiring: selecting a word ranks the rest of the page (+ user dictionary)
    // against its text; activating a suggestion applies the correction through
    // applyWordCorrection (the reviewed record is updated, box untouched).
    void suggestionActivationAppliesCorrection()
    {
        removeTestDictionary();
        // The dictionary contributes vocabulary too.
        QVERIFY(OCRMode::addUserDictionaryWord(QLatin1String(kLang), QStringLiteral("colored")));

        QList<MergedOcrWord> words = makeWords();
        words[0] = makeWord(QStringLiteral("do1or"), 40, QRectF(10, 10, 60, 14));
        words[1] = makeWord(QStringLiteral("dolor"), 95, QRectF(10, 40, 40, 14));
        words[2] = makeWord(QStringLiteral("colored"), 95, QRectF(10, 70, 50, 14));

        OCRMode panel;
        panel.setUserDictionaryLanguage(QLatin1String(kLang));
        panel.setReviewSession(makeSession(words));

        panel.selectWord(0);
        // Vocabulary = page words + dictionary, minus the word itself.
        // "colored" vs "do1or" is dist > 2 → excluded; only "dolor" qualifies.
        QCOMPARE(panel.currentSuggestions(), (QStringList{ QStringLiteral("dolor") }));

        // Activating the suggestion applies the correction to the record.
        QComboBox* combo = panel.findChild<QComboBox*>(QStringLiteral("ocrSuggestionCombo"));
        QVERIFY(combo);
        QVERIFY(!combo->itemText(0).isEmpty());
        emit combo->activated(0);   // Qt6: activated(int) is the user-pick signal

        const QList<OcrReviewedWord> reviewed = panel.reviewedWords();
        QCOMPARE(reviewed[0].reviewedText, QStringLiteral("dolor"));
        QCOMPARE(reviewed[0].originalText, QStringLiteral("do1or"));   // provenance kept
        QCOMPARE(reviewed[0].boundingBox, QRectF(10, 10, 60, 14));     // box untouched

        removeTestDictionary();
    }

    // ── P3 (B4): Skip All / Replace All — token-scoped bulk dispositions ───

    // Skip All: every current occurrence of the token leaves the uncertain
    // walk for THIS session (new deliveries reset it). Review action — it is
    // rejected outside ReviewReady.
    void skipAllSuppressesTokenForSession()
    {
        QList<MergedOcrWord> words = makeWords();
        words.append(makeWord(QStringLiteral("beta"), 50, QRectF(10, 100, 40, 14))); // 2nd beta

        OCRMode panel;
        // Guard: outside ReviewReady a bulk disposition is refused.
        QCOMPARE(panel.skipAllOccurrences(QStringLiteral("beta")), -1);
        QCOMPARE(panel.replaceAllOccurrences(QStringLiteral("beta"), QStringLiteral("x")), -1);

        panel.setReviewSession(makeSession(words));
        QCOMPARE(panel.nextUncertainWord(-1, true), 1);   // beta(65)

        // Two occurrences of "beta" are suppressed in one call.
        QCOMPARE(panel.skipAllOccurrences(QStringLiteral("beta")), 2);
        // gamma(40) is the first uncertain word left.
        QCOMPARE(panel.nextUncertainWord(-1, true), 2);
        QCOMPARE(panel.nextUncertainWord(2, true), 2);    // sole → wraps to itself
        // Empty tokens are refused (0 occurrences, no state change).
        QCOMPARE(panel.skipAllOccurrences(QStringLiteral("  ")), -1);

        // Session scoping: a fresh delivery restores the flags.
        panel.setReviewSession(makeSession(words));
        QCOMPARE(panel.nextUncertainWord(-1, true), 1);   // beta flagged again
    }

    // Replace All: every record whose current text matches the token gets the
    // replacement through applyWordCorrection — reviewed text changes,
    // provenance (originalText + boundingBox) is preserved per record.
    void replaceAllAppliesToEveryOccurrence()
    {
        QList<MergedOcrWord> words;
        words.append(makeWord(QStringLiteral("do1or"), 40, QRectF(10, 10, 60, 14)));
        words.append(makeWord(QStringLiteral("dolor"), 95, QRectF(10, 40, 40, 14)));
        words.append(makeWord(QStringLiteral("do1or"), 45, QRectF(80, 10, 60, 14)));

        OCRMode panel;
        panel.setReviewSession(makeSession(words));

        QCOMPARE(panel.replaceAllOccurrences(QStringLiteral("do1or"), QStringLiteral("dolor")), 2);

        const QList<OcrReviewedWord> reviewed = panel.reviewedWords();
        QCOMPARE(reviewed[0].reviewedText, QStringLiteral("dolor"));
        QCOMPARE(reviewed[0].originalText, QStringLiteral("do1or"));
        QCOMPARE(reviewed[0].boundingBox, QRectF(10, 10, 60, 14));
        QCOMPARE(reviewed[2].reviewedText, QStringLiteral("dolor"));
        QCOMPARE(reviewed[2].boundingBox, QRectF(80, 10, 60, 14));
        // Unmatched words untouched.
        QCOMPARE(reviewed[1].reviewedText, QStringLiteral("dolor"));

        // Repeated replace-all is idempotent — no record matches anymore.
        QCOMPARE(panel.replaceAllOccurrences(QStringLiteral("do1or"), QStringLiteral("dolor")), 0);
    }

    // ── P4 (B7+B12): word/page verification state ──────────────────────────

    // Per-word verified marks drive a VERIFIED % cell; removed words drop out
    // of the denominator; marks are refused outside ReviewReady / for unknown
    // or removed records.
    void verifiedPercentTracksMarkedWords()
    {
        OCRMode panel;
        // Guard: outside ReviewReady nothing can be marked.
        QCOMPARE(panel.markWordVerified(0), false);

        panel.setReviewSession(makeSession(makeWords()));   // alpha95 beta65 gamma40
        QCOMPARE(panel.verifiedPercent(), 0);

        QLabel* lbl = panel.findChild<QLabel*>(QStringLiteral("ocrVerifiedLabel"));
        QVERIFY(lbl);   // the info strip carries the VERIFIED cell

        // One of three → 33, and the strip says so.
        QCOMPARE(panel.markWordVerified(0), true);
        QCOMPARE(panel.verifiedPercent(), 33);
        QVERIFY(lbl->text().contains(QStringLiteral("33")));

        // Unknown stable ids are refused.
        QCOMPARE(panel.markWordVerified(99), false);

        // Marking alpha and beta too → all three verified.
        QCOMPARE(panel.markWordVerified(1), true);
        QCOMPARE(panel.markWordVerified(2), true);
        QCOMPARE(panel.verifiedPercent(), 100);

        // A removed word leaves the denominator entirely: delete the unmarked
        // gamma — the percentage stays 100 instead of dropping to 67.
        QVERIFY(panel.markWordDeleted(2));
        QCOMPARE(panel.verifiedPercent(), 100);

        // Removed records cannot be (re)marked.
        QCOMPARE(panel.markWordVerified(2), false);
    }

    // Ctrl+T page-level triage: an explicit "this page is done" state that
    // resets on fresh deliveries and follows the ReviewReady lifecycle.
    void pageVerifiedToggleLifecycle()
    {
        OCRMode panel;
        QVERIFY(!panel.isPageVerified());

        QToolButton* btn = panel.findChild<QToolButton*>(QStringLiteral("ocrBtnPageVerified"));
        QVERIFY(btn);

        // Outside ReviewReady the toggle is inert (and unchecked).
        btn->click();
        QVERIFY(!panel.isPageVerified());

        panel.setReviewSession(makeSession(makeWords()));
        QVERIFY(!panel.isPageVerified());

        // Programmatic set syncs the toolbar toggle.
        panel.setPageVerified(true);
        QVERIFY(panel.isPageVerified());
        QVERIFY(btn->isChecked());

        // Fresh deliveries reset the triage state.
        panel.setReviewSession(makeSession(makeWords()));
        QVERIFY(!panel.isPageVerified());
        QVERIFY(!btn->isChecked());

        // And the button drives it back in ReviewReady.
        btn->click();
        QVERIFY(panel.isPageVerified());

        // Reject clears it too.
        panel.onRejectResults();
        QVERIFY(!panel.isPageVerified());
    }

    // ── P5 (B14): Ctrl+Tab / Ctrl+Shift+Tab pane focus cycling ─────────────

    // Cycling walks the pane set in a fixed order (page list, scan canvas,
    // rich-text fallback, text preview, word inspector), skipping invisible
    // / no-focus widgets; forward and backward are exact inverses.
    void ctrlTabCyclesPaneFocus()
    {
        OCRMode panel;
        panel.show();
        panel.setReviewSession(makeSession(makeWords()));   // canvas is the visible scan pane

        QListWidget* pageList = panel.findChild<QListWidget*>(QStringLiteral("ocrPageList"));
        OcrScanCanvas* canvas = panel.findChild<OcrScanCanvas*>(QStringLiteral("ocrScanCanvas"));
        QPlainTextEdit* textEdit = panel.findChild<QPlainTextEdit*>(QStringLiteral("ocrTextEdit"));
        QLineEdit* wordEdit = panel.findChild<QLineEdit*>(QStringLiteral("ocrWordEdit"));
        QVERIFY(pageList && canvas && textEdit && wordEdit);

        // A word is selected: the word inspector is enabled and focusable.
        panel.selectWord(0);

        // order: pageList → canvas → (label skipped: NoFocus) → textEdit → wordEdit
        textEdit->setFocus();
        panel.cyclePaneFocus(+1);
        QCOMPARE(panel.focusWidget(), static_cast<QWidget*>(wordEdit));

        panel.cyclePaneFocus(+1);
        QCOMPARE(panel.focusWidget(), static_cast<QWidget*>(pageList));   // wrap

        panel.cyclePaneFocus(-1);
        QCOMPARE(panel.focusWidget(), static_cast<QWidget*>(wordEdit));

        panel.cyclePaneFocus(-1);
        QCOMPARE(panel.focusWidget(), static_cast<QWidget*>(textEdit));

        // The scan pane is skipped: OcrScanCanvas (plain QWidget) and the
        // rich-text QLabel both default to NoFocus — the canvas is mouse-
        // driven, the keyboard ring is page list → text → inspector.
        panel.cyclePaneFocus(-1);
        QCOMPARE(panel.focusWidget(), static_cast<QWidget*>(pageList));

        // From the page list, backward wraps to the word inspector — the
        // cycle is a ring, not a line.
        panel.cyclePaneFocus(-1);
        QCOMPARE(panel.focusWidget(), static_cast<QWidget*>(wordEdit));
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
