// SPDX-License-Identifier: Apache-2.0
// TestOcrRegionReocr — PARITY-SCORECARD-2026-09-30 §4 row 12 pins.
//
// Row 12: "Re-OCR region scoping — Honesty fix shipped; the feature (map
// click → LayoutRegion bbox) is in-code future work."
//
// The feature: drag a rectangle on the OCR review surface's source-image
// canvas → the bbox (pageImage pixel space — the SAME coordinate system
// LayoutRegion::bbox uses) is stored on OCRMode → "Re-OCR this region"
// dispatches reOcrRegionRequested(bbox) → EditController::runOcr(region)
// re-runs the EXISTING pipeline over the region crop only and delivers the
// same review records the whole-page path produces (boxes mapped back into
// pageImage space). Honest failure handling: a garbage region (degenerate,
// outside the page image, or yielding no text) is a typed refusal, never a
// silent no-op and never a dishonest whole-page fallback.
//
// R7 discipline: the pins FAIL at runtime against the seams-only state
// (inert stubs, committed first — the honest fail-before), the negative
// control (scoped revert of the implementation commit) must reproduce that
// failure set exactly once, and the implementation commit must pass ×3
// consecutive serial runs.
//
// findings-tests 2026-10-02 extension (testing-specialist wave-2b §4.4): the
// nothing-recognized leg of the row's honesty contract — "a scoped run that
// recognizes NOTHING is a typed ocrRunFailed, review intact" — is now driven
// END-TO-END once: a real GpMainWindow + real viewer + real Tesseract over a
// blank region crop, asserting through the shipped host wiring (EditController
// → OCRMode::notifyOcrFailed). The pre-dispatch refusals above stay seam-level;
// this pin converts the completion-lambda decision
// (`isRegionRun && mergedWords.isEmpty()`) from unpinned to red-able.
#include <QtTest>
#include <QAbstractButton>
#include <QFileInfo>
#include <QMouseEvent>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <memory>

#include "modes/OCRMode.h"
#include "modes/OcrReviewSession.h"
#include "engines/ocr/ILayoutDetector.h"   // LayoutRegion (row 12 bbox system)
#include "engines/ocr/OcrPipeline.h"       // MergedOcrWord
#include "shell/controllers/EditController.h"
#include "ui/OcrScanCanvas.h"
#include "ui/PdfViewerWidget.h"
#include "GpMainWindow.h"
#include "app/Bootstrapper.h"
#include "core/AppContext.h"
#include "core/Capability.h"

using gp::EditController;
using gp::OCRMode;
using gp::OcrReviewSession;
using gp::OcrScanCanvas;
// LayoutRegion / RegionType / MergedOcrWord are global-namespace types
// (engines/ocr/ILayoutDetector.h, engines/ocr/OcrPipeline.h).
using ::LayoutRegion;
using ::MergedOcrWord;
using ::RegionType;

namespace {

QList<MergedOcrWord> makeWords()
{
    QList<MergedOcrWord> words;
    MergedOcrWord w;
    w.text = QStringLiteral("alpha");
    w.boundingBox = QRectF(10, 10, 60, 14);
    w.confidence = 95;
    w.sourceEngine = QStringLiteral("Tesseract");
    words.append(w);
    MergedOcrWord w2 = w;
    w2.text = QStringLiteral("beta");
    w2.boundingBox = QRectF(10, 40, 40, 14);
    w2.confidence = 65;
    words.append(w2);
    return words;
}

OcrReviewSession makeSession()
{
    OcrReviewSession s;
    s.generation = 7;
    s.sourcePath = QStringLiteral("C:/scans/src.pdf");
    s.sourcePage = 2;
    s.sourcePageCount = 40;
    QImage img(200, 100, QImage::Format_RGB32);
    img.fill(QColor(245, 245, 240));
    s.pageImage = img;
    const QList<MergedOcrWord> words = makeWords();
    for (int i = 0; i < words.size(); ++i) {
        gp::OcrReviewedWord rec;
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

// Drive a private slot through the moc exactly as the context-menu action
// does (the TestSep13LeadOcrGuards pattern).
bool invokeReocrEntry(OCRMode& panel, const char* slot)
{
    return QMetaObject::invokeMethod(&panel, slot, Qt::DirectConnection);
}

// Deterministic headless drag: press → move → release with explicit
// QMouseEvents (QTest::mouseMove is a no-op on the offscreen platform).
void dragWidget(QWidget* w, const QPointF& from, const QPointF& to)
{
    QMouseEvent press(QEvent::MouseButtonPress, from,
                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(w, &press);
    QMouseEvent move(QEvent::MouseMove, to,
                     Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(w, &move);
    QMouseEvent release(QEvent::MouseButtonRelease, to,
                        Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(w, &release);
}

} // namespace

class TestOcrRegionReocr : public QObject {
    Q_OBJECT

    std::unique_ptr<gp::MainWindow> m_win;

private slots:

    void initTestCase()
    {
        // Isolate QSettings (TestOcrPreprocessPrefs idiom) AND pin the shipped
        // defaults for the live-OCR pin below: engine auto → Tesseract,
        // language EN, preprocessing all off (F5-F2). The suite's other pins
        // construct their own panels and read no persisted state.
        QCoreApplication::setOrganizationName(QStringLiteral("GlyphPDFTests"));
        QCoreApplication::setApplicationName(QStringLiteral("TestOcrRegionReocr"));
        QSettings().remove(QStringLiteral("ocr/engine"));
        QSettings().remove(QStringLiteral("ocr/language"));
        QSettings().remove(QStringLiteral("ocr/preprocessDeskew"));
        QSettings().remove(QStringLiteral("ocr/preprocessBinarize"));
        QSettings().remove(QStringLiteral("ocr/preprocessDenoise"));
        QSettings().remove(QStringLiteral("ocr/orientDetect"));
    }

    void cleanup() { m_win.reset(); }

    // ── Harness / control pins (must PASS before and after) ────────────────

    // Control: a delivered session is ReviewReady — the review surface the
    // region actions act on.
    void deliveredSessionIsReviewReady()
    {
        OCRMode panel;
        panel.setOcrResults(makeWords());
        QCOMPARE(int(panel.reviewState()), int(OCRMode::ReviewState::ReviewReady));
    }

    // Control (SEP13 lead 11, kept green): the region entry stays guarded —
    // outside ReviewReady it never dispatches, selection or not.
    void regionEntryOutsideReviewReadyStaysGuarded()
    {
        OCRMode panel;   // Idle
        QSignalSpy reOcrSpy(&panel, &OCRMode::reOcrRegionRequested);
        QVERIFY(invokeReocrEntry(panel, "onReOcrRegion"));
        QCOMPARE(reOcrSpy.count(), 0);
    }

    // Control: the whole-page entry carries the SEP13 guard too and always
    // dispatches the EMPTY bbox (whole page) in ReviewReady.
    void wholePageEntryGuardedAndDispatchesEmptyBbox()
    {
        OCRMode panel;   // Idle — guard must hold
        QSignalSpy reOcrSpy(&panel, &OCRMode::reOcrRegionRequested);
        QVERIFY(invokeReocrEntry(panel, "onReOcrWholePage"));
        QCOMPARE(reOcrSpy.count(), 0);

        panel.setOcrResults(makeWords());
        QVERIFY(invokeReocrEntry(panel, "onReOcrWholePage"));
        QCOMPARE(reOcrSpy.count(), 1);
        QCOMPARE(reOcrSpy.at(0).at(0).toRectF(), QRectF());
    }

    // ── OCRMode region store + scoped dispatch (RED on seams-only state) ───

    // "Re-OCR this region" WITHOUT a selection must be a typed, visible
    // refusal — never a silent no-op and never a dishonest whole-page run.
    void regionReocrWithoutSelectionIsTypedNoop()
    {
        OCRMode panel;
        panel.setOcrResults(makeWords());   // ReviewReady
        QSignalSpy reOcrSpy(&panel, &OCRMode::reOcrRegionRequested);
        QVERIFY(invokeReocrEntry(panel, "onReOcrRegion"));
        QVERIFY2(reOcrSpy.count() == 0,
                 "region entry dispatched with NO selection — the empty bbox "
                 "means WHOLE PAGE downstream, so this silently re-ran the "
                 "entire page instead of refusing");
        QVERIFY2(panel.lastLifecycleMessage().contains(QStringLiteral("no region selected"),
                                                        Qt::CaseInsensitive),
                 QStringLiteral("typed refusal missing — lastLifecycleMessage was '%1'")
                     .arg(panel.lastLifecycleMessage()).toUtf8().constData());
    }

    // The canvas's committed drag-select is stored verbatim (LayoutRegion
    // bbox coordinate system) and dispatched verbatim by the region entry.
    void selectedRegionIsStoredAndDispatchedVerbatim()
    {
        OCRMode panel;
        panel.setReviewSession(makeSession());   // ReviewReady + source image
        const QRectF region(5, 6, 20, 10);
        QVERIFY(QMetaObject::invokeMethod(&panel, "onScanRegionSelected",
                                          Q_ARG(QRectF, region)));
        QCOMPARE(panel.contextRegionBbox(), region);

        QSignalSpy reOcrSpy(&panel, &OCRMode::reOcrRegionRequested);
        QVERIFY(invokeReocrEntry(panel, "onReOcrRegion"));
        QCOMPARE(reOcrSpy.count(), 1);
        QCOMPARE(reOcrSpy.at(0).at(0).toRectF(), region);
    }

    // An EMPTY regionSelected (drag missed the image) clears any stored
    // selection — a stale region must never scope the next run.
    void emptySelectionClearsStoredRegion()
    {
        OCRMode panel;
        panel.setReviewSession(makeSession());
        QVERIFY(QMetaObject::invokeMethod(&panel, "onScanRegionSelected",
                                          Q_ARG(QRectF, QRectF(5, 6, 20, 10))));
        QVERIFY(QMetaObject::invokeMethod(&panel, "onScanRegionSelected",
                                          Q_ARG(QRectF, QRectF())));
        QVERIFY(panel.contextRegionBbox().isEmpty());
    }

    // A FRESH recognition invalidates the stored region: the new review
    // records describe a new run; the old bbox is stale garbage.
    void freshDeliveryClearsStoredRegion()
    {
        OCRMode panel;
        panel.setReviewSession(makeSession());
        QVERIFY(QMetaObject::invokeMethod(&panel, "onScanRegionSelected",
                                          Q_ARG(QRectF, QRectF(5, 6, 20, 10))));
        panel.setReviewSession(makeSession());   // fresh delivery
        QVERIFY2(panel.contextRegionBbox().isEmpty(),
                 "stored region survived a fresh recognition delivery — the "
                 "next 'Re-OCR this region' would scope to a stale bbox");
    }

    // The whole-page entry never consumes nor dispatches a stored region.
    void wholePageEntryDoesNotDisturbStoredRegion()
    {
        OCRMode panel;
        panel.setReviewSession(makeSession());
        const QRectF region(5, 6, 20, 10);
        QVERIFY(QMetaObject::invokeMethod(&panel, "onScanRegionSelected",
                                          Q_ARG(QRectF, region)));
        QSignalSpy reOcrSpy(&panel, &OCRMode::reOcrRegionRequested);
        QVERIFY(invokeReocrEntry(panel, "onReOcrWholePage"));
        QCOMPARE(reOcrSpy.count(), 1);
        QCOMPARE(reOcrSpy.at(0).at(0).toRectF(), QRectF());
        QCOMPARE(panel.contextRegionBbox(), region);
    }

    // ── OcrScanCanvas: widget-space drag → pageImage pixel space (RED) ─────

    // 200×100 image letterboxed in a 400×100 pane: imgRect = (100,0,200,100),
    // scale 1.0 — the mapping must be the exact inverse of wordIdAt()'s.
    void imageRegionForMapsWidgetDragIntoImageSpace()
    {
        const QImage img(200, 100, QImage::Format_RGB32);
        const QRectF pane(0, 0, 400, 100);
        QCOMPARE(OcrScanCanvas::imageRegionFor(QRectF(150, 10, 100, 50), img, pane),
                 QRectF(50, 10, 100, 50));
        // Right-to-left drags arrive unnormalized — same region.
        QCOMPARE(OcrScanCanvas::imageRegionFor(QRectF(250, 10, -100, 50), img, pane),
                 QRectF(50, 10, 100, 50));
    }

    // Drags that run off the image are CLAMPED to it, not widened or dropped.
    void imageRegionForClampsToImage()
    {
        const QImage img(200, 100, QImage::Format_RGB32);
        const QRectF pane(0, 0, 400, 100);
        QCOMPARE(OcrScanCanvas::imageRegionFor(QRectF(250, -50, 100, 80), img, pane),
                 QRectF(150, 0, 50, 30));
    }

    // A drag that misses the image entirely maps to an EMPTY region — the
    // garbage selection must stay a refusal, never a whole-page fallback.
    void imageRegionForEmptyWhenDragMissesImage()
    {
        const QImage img(200, 100, QImage::Format_RGB32);
        const QRectF pane(0, 0, 400, 100);
        QVERIFY(OcrScanCanvas::imageRegionFor(QRectF(500, 50, 100, 30), img, pane)
                    .isEmpty());
        QVERIFY(OcrScanCanvas::imageRegionFor(QRectF(), img, pane).isEmpty());
    }

    // Wiring: a real drag on the shown canvas commits the selection and emits
    // it in image space; the committed region is queryable; a new page image
    // invalidates it.
    void canvasDragCommitsRegionInImageSpace()
    {
        OcrScanCanvas canvas;
        canvas.setObjectName(QStringLiteral("regionReocrCanvas"));
        canvas.resize(400, 100);
        QImage img(200, 100, QImage::Format_RGB32);
        img.fill(Qt::white);
        canvas.setPageImage(img);
        canvas.show();

        QSignalSpy regionSpy(&canvas, &OcrScanCanvas::regionSelected);
        dragWidget(&canvas, QPointF(150, 10), QPointF(250, 60));

        QVERIFY2(regionSpy.count() == 1,
                 "a drag-select on the source canvas emitted no region — "
                 "the row-12 click→bbox mapping is not wired");
        if (regionSpy.count() == 1)
            QCOMPARE(regionSpy.at(0).at(0).toRectF(), QRectF(50, 10, 100, 50));
        QCOMPARE(canvas.selectedRegion(), QRectF(50, 10, 100, 50));

        // A plain click (press+release, no move) is NOT a region selection.
        QSignalSpy clickSpy(&canvas, &OcrScanCanvas::regionSelected);
        dragWidget(&canvas, QPointF(20, 20), QPointF(20, 20));
        QCOMPARE(clickSpy.count(), 0);
        QCOMPARE(canvas.selectedRegion(), QRectF(50, 10, 100, 50));

        // A new page image invalidates the selection (stale pixel space).
        QImage img2(300, 200, QImage::Format_RGB32);
        img2.fill(Qt::black);
        canvas.setPageImage(img2);
        QVERIFY(canvas.selectedRegion().isEmpty());
    }

    // ── EditController seams: crop + box mapping (RED) ──────────────────────

    // The user selection IS a LayoutRegion bbox (same pixel space): the crop
    // seam takes region.bbox verbatim.
    void regionCropRectMapsInteriorRegion()
    {
        LayoutRegion userRegion;               // click/drag → LayoutRegion bbox
        userRegion.bbox = QRectF(10, 20, 30, 40);
        userRegion.type = RegionType::Other;
        userRegion.confidence = 1.0;
        QString reason = QStringLiteral("unset");
        const QRect crop = EditController::ocrRegionCropRect(
            userRegion.bbox, QSize(100, 200), &reason);
        QVERIFY2(!crop.isNull(),
                 "interior region produced no crop — scoped re-OCR cannot run");
        QCOMPARE(crop, QRect(10, 20, 30, 40));
        QVERIFY2(reason.isEmpty(),
                 QStringLiteral("interior region must not be refused, reason was '%1'")
                     .arg(reason).toUtf8().constData());
    }

    void regionCropRectNormalizesNegativeExtent()
    {
        QString reason;
        QCOMPARE(EditController::ocrRegionCropRect(QRectF(90, 10, -30, 20),
                                                   QSize(100, 200), &reason),
                 QRect(60, 10, 30, 20));
        QVERIFY(reason.isEmpty());
    }

    void regionCropRectClampsToPage()
    {
        QString reason;
        QCOMPARE(EditController::ocrRegionCropRect(QRectF(80, 190, 40, 30),
                                                   QSize(100, 200), &reason),
                 QRect(80, 190, 20, 10));
        QVERIFY(reason.isEmpty());
    }

    // Garbage regions are REFUSED with a typed reason — empty crop + message,
    // so the scoped run can fail honestly instead of re-OCR-ing the page.
    void regionCropRectRejectsGarbageWithTypedReason()
    {
        const QSize pageSize(100, 200);
        const char* const what[] = {"outside", "degenerate"};
        const QRectF garbage[] = {
            QRectF(150, 0, 30, 40),   // entirely right of the page
            QRectF(10, 10, 30, 0),    // zero height
        };
        for (int i = 0; i < 2; ++i) {
            QString reason;
            const QRect crop = EditController::ocrRegionCropRect(
                garbage[i], pageSize, &reason);
            QVERIFY2(crop.isNull(),
                     QStringLiteral("%1 region must yield no crop").arg(what[i])
                         .toUtf8().constData());
            QVERIFY2(!reason.trimmed().isEmpty(),
                     QStringLiteral("%1 region was silently accepted — no typed "
                                    "refusal reason").arg(what[i])
                         .toUtf8().constData());
        }
    }

    // A scoped run recognizes the crop: word boxes land in crop space and
    // MUST be mapped back into pageImage space so the delivered review
    // records are identical in shape/coordinates to the whole-page path's.
    void regionWordsAreMappedBackIntoPageSpace()
    {
        QList<MergedOcrWord> words = makeWords();   // boxes (10,10,60,14)/(10,40,40,14)
        words = EditController::ocrRegionWordsToPageSpace(words, QPoint(100, 50));
        QCOMPARE(words.size(), 2);
        QCOMPARE(words[0].boundingBox, QRectF(110, 60, 60, 14));
        QCOMPARE(words[1].boundingBox, QRectF(110, 90, 40, 14));
        QCOMPARE(words[0].text, QStringLiteral("alpha"));

        // Empty crop result stays empty (nothing invented).
        QVERIFY(EditController::ocrRegionWordsToPageSpace({}, QPoint(7, 9)).isEmpty());
    }

    // ── Row 12, recognition half (findings-tests 2026-10-02): a scoped run
    //    that recognizes NOTHING is a typed ocrRunFailed, review intact ────
    // Driven END-TO-END once (real window, real viewer render, real
    // Tesseract over a genuinely blank region crop) and asserted through the
    // SHIPPED host wiring (EditController::ocrRunFailed → the hosted OCRMode
    // panel). The completion-lambda decision this pins:
    //   if (isRegionRun && mergedWords.isEmpty()) emit ocrRunFailed(...)
    // — deleting it routes the empty payload into the success path, which
    // would deliver a ZERO-word session and WIPE the user's review records
    // (both halves of this pin go red on exactly that).
    void regionRunRecognizingNothingIsTypedFailureAndReviewStaysIntact()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        // A genuinely BLANK image-only scan: every region crop recognizes
        // nothing, so the typed-failure path involves no recognition luck.
        const QString blankPdf = dir.filePath(QStringLiteral("blank-scan.pdf"));
        {
            QImage img(794, 1123, QImage::Format_RGB32);
            img.fill(QColor(245, 245, 240));
            QPdfWriter w(blankPdf);
            w.setResolution(96);
            w.setPageSize(QPageSize(QPageSize::A4));
            QPainter pw(&w);
            pw.drawImage(QRect(0, 0, w.width(), w.height()), img);
            pw.end();
        }
        QVERIFY(QFileInfo::exists(blankPdf));

        // Real window + real route seams (TestSweepW3UxFlows idiom).
        m_win = std::make_unique<gp::MainWindow>(Bootstrapper::createContext());
        m_win->show();

        // Real pipelines only: like flow5, the live leg QSKIPs honestly where
        // no OCR engine is staged (it asserts nothing there).
        auto* caps = m_win->appContext()
                         ? m_win->appContext()->capabilities.get() : nullptr;
        if (!caps || !(caps->available(gp::CapId::OcrTesseract)
                       || caps->available(gp::CapId::OcrRapidModels)))
            QSKIP("no OCR engine available next to the test binary — the "
                  "nothing-recognized contract needs a real recognition run");

        m_win->openDocument(blankPdf);
        QTRY_COMPARE_WITH_TIMEOUT(m_win->pdfViewer()->pageCount(), 1, 20000);
        m_win->activateScreen(QStringLiteral("ocr"));
        auto* panel = m_win->findChild<gp::OCRMode*>();
        QVERIFY2(panel, "the OCR verify screen must be hostable");
        auto* ctrl = m_win->findChild<gp::EditController*>();
        QVERIFY2(ctrl, "the window must host the EditController");

        // The user's EXISTING review — the thing that must stay intact when
        // the scoped run recognizes nothing.
        const OcrReviewSession existing = makeSession();
        panel->setReviewSession(existing);
        QCOMPARE(int(panel->reviewState()), int(OCRMode::ReviewState::ReviewReady));
        QCOMPARE(panel->reviewSession().words.size(), 2);

        // The region is a LayoutRegion-style bbox in pageImage pixel space —
        // the EXACT space runOcrRegion documents: the rendered page at the
        // run's own scale (renderPage(page, 2.0)). Bottom half of a blank
        // page.
        const QImage pageImg = m_win->pdfViewer()->renderPage(0, 2.0);
        QVERIFY2(!pageImg.isNull(), "the viewer must render the opened scan");
        const QRectF region(0, pageImg.height() / 2.0,
                            static_cast<qreal>(pageImg.width()),
                            pageImg.height() / 2.0);

        QSignalSpy failedSpy(ctrl, &EditController::ocrRunFailed);
        QSignalSpy readySpy(ctrl, &EditController::ocrResultsReady);
        ctrl->runOcrRegion(region);

        // Typed failure through the REAL pipeline. Budget: first-use engine
        // init (tessdata copy + session) is disk/CPU-bound and can be slow on
        // a cold/contended host — flow5's narrated-wait lesson.
        QTRY_VERIFY_WITH_TIMEOUT(failedSpy.count() == 1, 300000);
        // Guarded read: on a RED run (typed failure never fires) the spy is
        // empty — the message assertions below must fail honestly, not UB.
        const QString message = failedSpy.isEmpty()
                                    ? QString() : failedSpy.at(0).at(0).toString();
        QVERIFY2(message.contains(
                     QStringLiteral("no text recognized in the selected region"),
                     Qt::CaseInsensitive),
                 qPrintable(QStringLiteral("the nothing-recognized failure must be "
                              "TYPED (never a silent no-op), got: '%1'")
                                .arg(message.left(240))));
        // No silent empty delivery: the success signal must never fire.
        QCOMPARE(readySpy.count(), 0);

        // Review intact: the panel's records survive verbatim (notifyOcrFailed
        // keeps them), the typed failure reached the panel through the real
        // host wiring, the state is the retryable RecoverableError, and Run is
        // re-armed (no dead end).
        QCOMPARE(panel->reviewSession().words.size(), 2);
        QCOMPARE(panel->reviewSession().words.at(0).originalText,
                 QStringLiteral("alpha"));
        QCOMPARE(panel->reviewSession().words.at(1).originalText,
                 QStringLiteral("beta"));
        QCOMPARE(int(panel->reviewState()),
                 int(OCRMode::ReviewState::RecoverableError));
        QVERIFY2(panel->lastLifecycleMessage().contains(
                     QStringLiteral("no text recognized"), Qt::CaseInsensitive),
                 qPrintable(QStringLiteral("the panel must surface the typed failure, "
                              "got: '%1'")
                                .arg(panel->lastLifecycleMessage().left(240))));
        auto* runBtn = panel->findChild<QAbstractButton*>(
            QStringLiteral("ocrBtnRun"));
        QVERIFY2(runBtn && runBtn->isEnabled(),
                 "Run must be re-armed after the typed failure (no dead end)");
    }
};

#include "TestOcrRegionReocr.moc"
QTEST_MAIN(TestOcrRegionReocr)
