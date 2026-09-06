// SPDX-License-Identifier: Apache-2.0
// U03 regression tests: the OCR verify screen is a real source-and-correction
// workflow.
//
// What these tests pin (plan U03 + research Q-U03):
//   1. ONE confidence classifier (gp::OcrConfidence::bandFor, 90/70) drives
//      legend text, colors, the low-confidence count and navigation. The
//      boundary matrix 49/50/69/70/79/80/89/90 must classify
//      Low Low Low Medium Medium Medium Medium High.
//   2. Uncertain-word navigation is a wrap-around iterator over the reviewed
//      records (Low band ∧ not deleted ∧ has text); corrections do NOT turn a
//      model confidence estimate into a high one (provenance stays separate).
//   3. The scan pane is the REAL source page image with positioned word boxes;
//      clicking a box selects the same stable word record the links select
//      (one selection funnel: OCRMode::selectWord).
//   4. The zoom pane shows a magnified CROP of the selected word's source
//      pixels with a computed ×N magnification (no static "4×" claims).
//   5. The review session (including the 2.0× page image) is delivered to
//      OCRMode without a re-render or per-widget copy (QImage implicit
//      sharing — data pointers must be identical).
#include <QtTest>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QSignalSpy>
#include <QToolButton>

#include "modes/OCRMode.h"
#include "modes/OcrConfidence.h"
#include "modes/OcrReviewSession.h"
#include "modes/ModeController.h"
#include "ui/OcrScanCanvas.h"
#include "ui/OcrWordMagnifier.h"

using ::MergedOcrWord;   // engines/ocr/OcrPipeline.h — global-scope struct
using gp::OCRMode;
using gp::OcrConfidence;
using gp::OcrReviewedWord;
using gp::OcrReviewSession;
using gp::OcrScanCanvas;
using gp::OcrWordMagnifier;

namespace {

MergedOcrWord makeWord(const QString& text, int confidence, QRectF box)
{
    MergedOcrWord w;
    w.text = text;
    w.confidence = confidence;
    w.boundingBox = box;
    w.sourceEngine = QStringLiteral("Tesseract");
    return w;
}

// {95, 65, 80, 10}: certain, uncertain, medium, uncertain.
QList<MergedOcrWord> makeMixedWords()
{
    return {
        makeWord(QStringLiteral("alpha"), 95, QRectF(10, 10, 60, 14)),
        makeWord(QStringLiteral("beta"),  65, QRectF(10, 40, 40, 14)),
        makeWord(QStringLiteral("gamma"), 80, QRectF(10, 70, 50, 14)),
        makeWord(QStringLiteral("delta"), 10, QRectF(10, 100, 55, 14)),
    };
}

// A review session as EditController builds it: identity + revision + the
// rendered page image the word boxes live in (2.0× pixel space).
OcrReviewSession makeImageSession()
{
    OcrReviewSession s;
    s.generation = 7;
    s.sourcePath = QStringLiteral("C:/scans/src.pdf");
    s.sourcePage = 2;
    s.sourcePageCount = 40;

    QImage img(400, 300, QImage::Format_RGB32);
    img.fill(QColor(245, 245, 240));
    s.pageImage = img;

    const QList<MergedOcrWord> words = {
        makeWord(QStringLiteral("alpha"), 95, QRectF(10, 10, 60, 14)),
        makeWord(QStringLiteral("beta"),  65, QRectF(10, 40, 40, 14)),
        makeWord(QStringLiteral("gamma"), 95, QRectF(10, 70, 50, 14)),
    };
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

void pressAt(QWidget* w, const QPointF& pos)
{
    QMouseEvent press(QEvent::MouseButtonPress, pos, pos,
                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(w, &press);
}

QToolButton* nextUncertainButton(const OCRMode& panel)
{
    return panel.findChild<QToolButton*>(QStringLiteral("ocrBtnNextUncertain"));
}
QToolButton* prevUncertainButton(const OCRMode& panel)
{
    return panel.findChild<QToolButton*>(QStringLiteral("ocrBtnPrevUncertain"));
}

} // namespace

class TestOcrVerifyNavigation : public QObject
{
    Q_OBJECT

private slots:

    // ── 1. ONE classifier: the boundary matrix pins 90/70 for every consumer ─
    void confidenceBoundaryMatrix()
    {
        using B = OcrConfidence::Band;
        // 49, 50, 69, 70, 79, 80, 89, 90 → Low Low Low Medium Medium Medium Medium High
        QHash<int, OcrConfidence::Band> expected = {
            { 49, B::Low }, { 50, B::Low },  { 69, B::Low },  { 70, B::Medium },
            { 79, B::Medium }, { 80, B::Medium }, { 89, B::Medium }, { 90, B::High },
        };
        for (auto it = expected.cbegin(); it != expected.cend(); ++it)
            QCOMPARE(OcrConfidence::bandFor(it.key()), it.value());

        // Clamping: out-of-range estimates classify, never crash or invert.
        QCOMPARE(OcrConfidence::bandFor(-5), B::Low);
        QCOMPARE(OcrConfidence::bandFor(0), B::Low);
        QCOMPARE(OcrConfidence::bandFor(100), B::High);
        QCOMPARE(OcrConfidence::bandFor(150), B::High);
    }

    void classifierColorsAndRangeText()
    {
        using B = OcrConfidence::Band;
        // The overlay palette the scan pane has always drawn (90/70 bands).
        QCOMPARE(OcrConfidence::bandColor(B::High).name(),   QStringLiteral("#22c55e"));
        QCOMPARE(OcrConfidence::bandColor(B::Medium).name(), QStringLiteral("#eab308"));
        QCOMPARE(OcrConfidence::bandColor(B::Low).name(),    QStringLiteral("#ef4444"));
        // Legend/tooltip text — meaning must not depend on red/green alone.
        QCOMPARE(OcrConfidence::bandRangeText(B::High),   QStringLiteral("≥ 90%"));
        QCOMPARE(OcrConfidence::bandRangeText(B::Medium), QStringLiteral("70–89%"));
        QCOMPARE(OcrConfidence::bandRangeText(B::Low),    QStringLiteral("< 70%"));
    }

    // ── The legend is built FROM the classifier (80/50 outlier is gone) ──────
    void legendMatchesClassifierThresholds()
    {
        OCRMode panel;
        auto* high   = panel.findChild<QLabel*>(QStringLiteral("ocrLegendHigh"));
        auto* medium = panel.findChild<QLabel*>(QStringLiteral("ocrLegendMedium"));
        auto* low    = panel.findChild<QLabel*>(QStringLiteral("ocrLegendLow"));
        QVERIFY(high && medium && low);
        QCOMPARE(high->text(),   QStringLiteral("HIGH (%1)").arg(OcrConfidence::bandRangeText(OcrConfidence::Band::High)));
        QCOMPARE(medium->text(), QStringLiteral("MEDIUM (%1)").arg(OcrConfidence::bandRangeText(OcrConfidence::Band::Medium)));
        QCOMPARE(low->text(),    QStringLiteral("LOW (%1)").arg(OcrConfidence::bandRangeText(OcrConfidence::Band::Low)));
        // No stale "80"/"50" claims anywhere in the legend.
        QVERIFY(!high->text().contains(QStringLiteral("80")));
        QVERIFY(!low->text().contains(QStringLiteral("50")));
    }

    // ── 2. Uncertain-word walk: order, wrap-around, skip-deleted ─────────────
    void uncertainWalkOrderWrapAroundSkipsDeleted()
    {
        OCRMode panel;
        panel.setOcrResults(makeMixedWords());   // confidences {95, 65, 80, 10}

        // Uncertain = Low band ∧ not deleted ∧ has text → {1, 3}.
        QVERIFY(!OCRMode::isUncertain(panel.reviewedWords().at(0)));
        QVERIFY(OCRMode::isUncertain(panel.reviewedWords().at(1)));
        QVERIFY(!OCRMode::isUncertain(panel.reviewedWords().at(2)));
        QVERIFY(OCRMode::isUncertain(panel.reviewedWords().at(3)));

        // No selection yet → first/last uncertain in list order.
        QCOMPARE(panel.nextUncertainWord(-1, true), 1);
        QCOMPARE(panel.nextUncertainWord(-1, false), 3);

        // Forward: 1 → 3; wrap: 3 → 1.
        QCOMPARE(panel.nextUncertainWord(1, true), 3);
        QCOMPARE(panel.nextUncertainWord(3, true), 1);

        // Backward: 1 → wraps to 3; 3 → 1 (plain previous).
        QCOMPARE(panel.nextUncertainWord(1, false), 3);
        QCOMPARE(panel.nextUncertainWord(3, false), 1);

        // Deleted words are skipped: remove delta (3) → only beta (1) remains.
        QVERIFY(panel.markWordDeleted(3));
        QCOMPARE(panel.nextUncertainWord(-1, true), 1);
        QCOMPARE(panel.nextUncertainWord(1, true), 1);   // wraps around to itself
        QCOMPARE(panel.nextUncertainWord(1, false), 1);

        // A CORRECTED word keeps its model confidence (provenance is separate
        // from review status): correcting beta does not make it certain.
        QVERIFY(panel.applyWordCorrection(1, QStringLiteral("beta corrected")));
        QCOMPARE(panel.reviewedWords().at(1).confidence, 65);
        QVERIFY(OCRMode::isUncertain(panel.reviewedWords().at(1)));
        QCOMPARE(panel.nextUncertainWord(2, true), 1);
    }

    void allHighListHasNoUncertainWord()
    {
        OCRMode panel;
        panel.setOcrResults({
            makeWord(QStringLiteral("one"), 95, QRectF(10, 10, 60, 14)),
            makeWord(QStringLiteral("two"), 90, QRectF(10, 40, 40, 14)),
        });
        QCOMPARE(panel.nextUncertainWord(-1, true), -1);
        QCOMPARE(panel.nextUncertainWord(-1, false), -1);
        QCOMPARE(panel.nextUncertainWord(0, true), -1);
        QCOMPARE(panel.nextUncertainWord(0, false), -1);
    }

    // ── 3. One selection funnel: links, navigation and canvas clicks agree ───
    void selectWordFunnelSyncsInspectorCanvasMagnifier()
    {
        OCRMode panel;
        const OcrReviewSession session = makeImageSession();
        panel.setReviewSession(session);
        QCOMPARE(panel.reviewState(), OCRMode::ReviewState::ReviewReady);
        QCOMPARE(panel.reviewedWords().size(), 3);

        QSignalSpy spy(&panel, &OCRMode::selectedWordChanged);

        panel.selectWord(1);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().toInt(), 1);
        auto* edit = panel.findChild<QLineEdit*>(QStringLiteral("ocrWordEdit"));
        QVERIFY(edit);
        QCOMPARE(edit->text(), QStringLiteral("beta"));
        auto* canvas = panel.findChild<OcrScanCanvas*>(QStringLiteral("ocrScanCanvas"));
        QVERIFY(canvas);
        QCOMPARE(canvas->selectedWord(), 1);
        auto* magnifier = panel.findChild<OcrWordMagnifier*>(QStringLiteral("ocrWordMagnifier"));
        QVERIFY(magnifier);
        // The magnifier now shows the selected word's source region (padded,
        // clamped) — the word box must be inside the drawn crop.
        QVERIFY(magnifier->currentSourceRect().isValid());
        QVERIFY(magnifier->currentSourceRect().contains(QRect(10, 40, 40, 14)));

        // The scan-pane word link funnels through the same selection.
        panel.activateWordLink(QStringLiteral("word:0"));
        QCOMPARE(spy.count(), 2);
        QCOMPARE(spy.at(1).first().toInt(), 0);
        QCOMPARE(edit->text(), QStringLiteral("alpha"));
        QCOMPARE(canvas->selectedWord(), 0);
        QVERIFY(magnifier->currentSourceRect().contains(QRect(10, 10, 60, 14)));

        // A canvas click funnels through the same selection: click inside
        // alpha's box (10,10,60,14). Boxes are in pageImage pixel space — no
        // second DPR multiply anywhere in the mapping.
        canvas->resize(400, 300);
        pressAt(canvas, QPointF(40, 17));
        QCOMPARE(spy.count(), 3);
        QCOMPARE(spy.at(2).first().toInt(), 0);
        QCOMPARE(canvas->selectedWord(), 0);
        QCOMPARE(edit->text(), QStringLiteral("alpha"));

        // Unknown/stale ids are ignored (no signal, selection unchanged).
        panel.selectWord(42);
        QCOMPARE(spy.count(), 3);
        QCOMPARE(canvas->selectedWord(), 0);
    }

    void canvasClickSelectsRightWordRecord()
    {
        OcrScanCanvas canvas;
        QSignalSpy spy(&canvas, &OcrScanCanvas::wordClicked);

        QImage img(400, 300, QImage::Format_RGB32);
        img.fill(QColor(245, 245, 240));
        canvas.setPageImage(img);
        canvas.setWords({
            makeReviewed(QStringLiteral("alpha"), 95, QRectF(100, 120, 60, 20), 0),
            makeReviewed(QStringLiteral("beta"),  65, QRectF(200, 40, 80, 30), 1),
        });
        canvas.resize(400, 300);   // same aspect as the image → exact fit

        // Click inside alpha's box → word 0.
        pressAt(&canvas, QPointF(110, 130));
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().toInt(), 0);
        QCOMPARE(canvas.selectedWord(), 0);

        // Click inside beta's box → word 1.
        pressAt(&canvas, QPointF(210, 50));
        QCOMPARE(spy.count(), 2);
        QCOMPARE(spy.at(1).first().toInt(), 1);
        QCOMPARE(canvas.selectedWord(), 1);

        // Click on empty page space → nothing changes.
        pressAt(&canvas, QPointF(5, 5));
        QCOMPARE(spy.count(), 2);
        QCOMPARE(canvas.selectedWord(), 1);

        // Programmatic selection highlights the right record.
        canvas.setSelectedWord(0);
        QCOMPARE(canvas.selectedWord(), 0);
    }

    // ── 4. Magnifier crop math: padding + clamping + pixel identity ──────────
    void magnifierSourceRectPadsAndClamps()
    {
        // A word box in the middle: 25% padding each side, no clamping needed.
        QCOMPARE(OcrWordMagnifier::sourceRectFor(QRectF(100, 120, 60, 20), QSize(400, 300)),
                 QRect(85, 115, 90, 30));
        // A box in the corner: padded rect is clamped to the image bounds.
        QCOMPARE(OcrWordMagnifier::sourceRectFor(QRectF(0, 0, 60, 20), QSize(400, 300)),
                 QRect(0, 0, 75, 25));
        // An oversized box never leaves the image.
        QCOMPARE(OcrWordMagnifier::sourceRectFor(QRectF(-50, -50, 600, 500), QSize(400, 300)),
                 QRect(0, 0, 400, 300));
        // Degenerate inputs produce no crop.
        QVERIFY(OcrWordMagnifier::sourceRectFor(QRectF(), QSize(400, 300)).isEmpty());
        QVERIFY(OcrWordMagnifier::sourceRectFor(QRectF(0, 0, 10, 10), QSize()).isEmpty());
    }

    void magnifierPixelIdentityAtOneToOne()
    {
        // Synthetic page with a unique color per pixel so any mis-mapped crop
        // is detectable.
        QImage img(400, 300, QImage::Format_RGB32);
        for (int y = 0; y < img.height(); ++y)
            for (int x = 0; x < img.width(); ++x)
                img.setPixel(x, y, qRgb((x * 7) % 256, (y * 11) % 256, 77));

        OcrWordMagnifier magnifier;
        magnifier.setPageImage(img);
        magnifier.setCropRect(QRectF(100, 120, 60, 20));
        // Widget sized so the padded crop (90×30) fits 1:1 inside the 8px
        // margins → magnification is exactly 1.0 and pixels must be identical.
        magnifier.resize(90 + 16, 30 + 16);
        QVERIFY(!magnifier.currentSourceRect().isEmpty());
        QCOMPARE(magnifier.currentSourceRect(), QRect(85, 115, 90, 30));
        QVERIFY(qAbs(magnifier.currentMagnification() - 1.0) < 1e-6);

        const QImage out = magnifier.grab().toImage();
        QCOMPARE(out.size(), QSize(90 + 16, 30 + 16));   // offscreen DPR is 1

        const QRect src = magnifier.currentSourceRect();
        // The word-box ring is drawn over the crop; skip its 2px band.
        const QRect ringBand = QRect(8 + 100 - 85, 8 + 120 - 115, 60, 20).adjusted(-3, -3, 3, 3);
        for (int dy = 0; dy < src.height(); ++dy) {
            for (int dx = 0; dx < src.width(); ++dx) {
                const QPoint screen(8 + dx, 8 + dy);
                if (ringBand.contains(screen)) continue;
                const QPoint source(src.x() + dx, src.y() + dy);
                if (out.pixel(screen) != img.pixel(source)) {
                    QString fail = QStringLiteral(
                        "pixel mismatch at screen %1,%2 vs source %3,%4")
                        .arg(screen.x()).arg(screen.y())
                        .arg(source.x()).arg(source.y());
                    QFAIL(fail.toLatin1().constData());
                }
            }
        }

        // Double the pane → exactly 2× magnification (computed, not "4×").
        magnifier.resize(90 * 2 + 16, 30 * 2 + 16);
        QVERIFY(qAbs(magnifier.currentMagnification() - 2.0) < 1e-6);
    }

    void magnifierClearsSelection()
    {
        OcrWordMagnifier magnifier;
        magnifier.setPageImage(makeImageSession().pageImage);
        magnifier.setCropRect(QRectF(100, 120, 60, 20));
        QVERIFY(!magnifier.currentSourceRect().isEmpty());

        magnifier.clearSelection();
        QVERIFY(magnifier.currentSourceRect().isEmpty());
        QCOMPARE(magnifier.currentMagnification(), 0.0);
    }

    // ── 5. Session delivery: image travels without a re-render or copy ───────
    void sessionDeliveredToOcrModeWithoutRerender()
    {
        gp::ModeController modes;
        modes.setScreen(QStringLiteral("ocr"));
        auto* panel = modes.findChild<OCRMode*>();
        QVERIFY(panel);

        OcrReviewSession session = makeImageSession();
        // A marker pixel that only the delivered image can carry.
        session.pageImage.setPixel(7, 7, qRgb(250, 10, 10));

        // Production order: words first, then the session (with the image).
        modes.deliverOcrResults(makeImageWords());
        QCOMPARE(panel->reviewState(), OCRMode::ReviewState::ReviewReady);

        modes.deliverOcrReview(session);
        QCOMPARE(panel->reviewState(), OCRMode::ReviewState::ReviewReady);
        QCOMPARE(panel->reviewedWords().size(), 3);
        QCOMPARE(panel->reviewedWords().at(1).reviewedText, QStringLiteral("beta"));
        // The reviewed page identity is now known and real.
        auto* pageLabel = panel->findChild<QLabel*>(QStringLiteral("ocrPageLabel"));
        QVERIFY(pageLabel);
        QVERIFY(pageLabel->text().contains(QStringLiteral("3")));
        QVERIFY(pageLabel->text().contains(QStringLiteral("40")));

        // The scan canvas holds the SAME image buffer — implicit sharing, no
        // re-render, no 7 MB copy per widget.
        auto* canvas = panel->findChild<OcrScanCanvas*>(QStringLiteral("ocrScanCanvas"));
        QVERIFY(canvas);
        QVERIFY(!canvas->pageImage().isNull());
        QCOMPARE(reinterpret_cast<const uchar*>(canvas->pageImage().constBits()),
                 session.pageImage.constBits());
        QCOMPARE(canvas->pageImage().cacheKey(), session.pageImage.cacheKey());
        QCOMPARE(canvas->pageImage().pixel(7, 7), QRgb(session.pageImage.pixel(7, 7)));

        auto* magnifier = panel->findChild<OcrWordMagnifier*>(QStringLiteral("ocrWordMagnifier"));
        QVERIFY(magnifier);
        QCOMPARE(reinterpret_cast<const uchar*>(magnifier->pageImage().constBits()),
                 session.pageImage.constBits());

        // The zoom header never claims a static "4×".
        auto* zoomHeader = panel->findChild<QLabel*>(QStringLiteral("ocrZoomHeader"));
        QVERIFY(zoomHeader);
        QVERIFY(!zoomHeader->text().contains(QStringLiteral("4×")));
    }

    void uncertainButtonsJumpTheWalk()
    {
        OCRMode panel;
        panel.setReviewSession(makeImageSession());   // one uncertain word: beta (1)
        auto* nextBtn = nextUncertainButton(panel);
        auto* prevBtn = prevUncertainButton(panel);
        QVERIFY(nextBtn && prevBtn);
        QVERIFY(nextBtn->isEnabled());
        QVERIFY(prevBtn->isEnabled());

        auto* edit = panel.findChild<QLineEdit*>(QStringLiteral("ocrWordEdit"));
        QVERIFY(edit);

        // From no selection: next lands on the first uncertain word.
        nextBtn->click();
        QCOMPARE(edit->text(), QStringLiteral("beta"));

        // Wrap-around: next again lands on beta again (it is the only one).
        nextBtn->click();
        QCOMPARE(edit->text(), QStringLiteral("beta"));

        // After deleting it there is nothing left to jump to.
        QVERIFY(panel.markWordDeleted(1));
        QVERIFY(!nextBtn->isEnabled());
        QVERIFY(!prevBtn->isEnabled());
    }

private:
    static OcrReviewedWord makeReviewed(const QString& text, int confidence,
                                        QRectF box, int stableId)
    {
        OcrReviewedWord rec;
        rec.stableId     = stableId;
        rec.originalText = text;
        rec.reviewedText = text;
        rec.deleted      = false;
        rec.boundingBox  = box;
        rec.confidence   = confidence;
        rec.sourceEngine = QStringLiteral("Tesseract");
        return rec;
    }

    static QList<MergedOcrWord> makeImageWords()
    {
        return {
            makeWord(QStringLiteral("alpha"), 95, QRectF(10, 10, 60, 14)),
            makeWord(QStringLiteral("beta"),  65, QRectF(10, 40, 40, 14)),
            makeWord(QStringLiteral("gamma"), 95, QRectF(10, 70, 50, 14)),
        };
    }
};

QTEST_MAIN(TestOcrVerifyNavigation)
#include "TestOcrVerifyNavigation.moc"
