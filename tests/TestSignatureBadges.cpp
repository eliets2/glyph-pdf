// SPDX-License-Identifier: Apache-2.0
// §9.7 P0 audit item: on-page signature validity badges.
//
// The badge layer is a Qt VIEW-LAYER overlay ONLY — it must never be written
// into the PDF (ISO 32000-2 forbids validation status inside the field
// appearance; Acrobat's ribbon is viewer-drawn). These tests pin:
//   * the four badge states and their distinct colors (green trusted check,
//     amber untrusted "?", red modified X, gray unknown),
//   * the set/clear data API on PdfViewerWidget,
//   * that a badge fed with {pageIndex, fieldRect, state, tooltip} is painted
//     at the field rect's TOP-RIGHT corner on the page (pixel scan of the
//     overlay grab), single-page mode AND two-page composite mode,
//   * that an empty badge list clears the paint,
//   * the tooltip payload (signer name + status detail),
//   * the SignaturesPanel → PdfViewerWidget badge push: after the panel's
//     validateSignatures() run, per-signature states derived from the binding
//     mapping reach the viewer (stub-driven: MockSignatureManager; the
//     production connect() lives in GpMainWindow, which is outside this
//     test's file-ownership lane).
#include <QtTest/QtTest>
#include <QAbstractScrollArea>
#include <QFileInfo>
#include <QImage>
#include <QLabel>
#include <QScrollBar>

#include "ui/PdfViewerWidget.h"
#include "modes/SignaturesPanel.h"
#include "mocks/MockSignatureManager.h"
#include "engines/SignatureManager.h"

#ifdef SOURCE_DIR
static const QString kFixtureDir = QStringLiteral(SOURCE_DIR "/tests/fixtures/signing");
#else
static const QString kFixtureDir = QStringLiteral("tests/fixtures/signing");
#endif
static const QString kInputPdf = kFixtureDir + QStringLiteral("/test_input.pdf");

class TestSignatureBadges : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() {
        // Isolate QSettings (same idiom as TestOcrPreprocessPrefs).
        QCoreApplication::setOrganizationName(QStringLiteral("GlyphPDFTests"));
        QCoreApplication::setApplicationName(QStringLiteral("TestSignatureBadges"));
    }

    // ── State → color: the four states must be visually distinct ────────────
    void stateColorsAreDistinct() {
        const QColor green = PdfViewerWidget::signatureBadgeColor(SignatureBadgeState::ValidTrusted);
        const QColor amber = PdfViewerWidget::signatureBadgeColor(SignatureBadgeState::UntrustedChain);
        const QColor red   = PdfViewerWidget::signatureBadgeColor(SignatureBadgeState::ModifiedAfterSigning);
        const QColor gray  = PdfViewerWidget::signatureBadgeColor(SignatureBadgeState::Unknown);
        QVERIFY(green != amber);
        QVERIFY(green != red);
        QVERIFY(green != gray);
        QVERIFY(amber != red);
        QVERIFY(amber != gray);
        QVERIFY(red != gray);
        // Design: green trusted / amber untrusted / red modified / gray unknown.
        QVERIFY2(green.green() > green.red() && green.green() > green.blue(),
                 "ValidTrusted must be a green-dominant color");
        QVERIFY2(red.red() > red.green() && red.red() > red.blue(),
                 "ModifiedAfterSigning must be a red-dominant color");
        QVERIFY2(amber.red() > amber.blue() && amber.green() > amber.blue(),
                 "UntrustedChain must be an amber (red+green, low blue) color");
        QVERIFY2(qAbs(gray.red() - gray.green()) < 24 && qAbs(gray.green() - gray.blue()) < 24,
                 "Unknown must be a neutral gray");
    }

    // ── Data API: store, query, clear on empty list ─────────────────────────
    void setAndClearBadges() {
        PdfViewerWidget w;
        QVERIFY(w.signatureBadges().isEmpty());

        SignatureBadgeSpec spec;
        spec.pageIndex = 0;
        spec.fieldRect = QRectF(60, 60, 200, 50);
        spec.state = SignatureBadgeState::ValidTrusted;
        spec.tooltip = QStringLiteral("Alice — VALID (trust: Valid)");
        w.setSignatureBadges({spec});

        QCOMPARE(w.signatureBadges().size(), 1);
        QCOMPARE(w.signatureBadges().first().pageIndex, 0);
        QCOMPARE(w.signatureBadges().first().fieldRect, QRectF(60, 60, 200, 50));
        QVERIFY(w.signatureBadges().first().state == SignatureBadgeState::ValidTrusted);
        QCOMPARE(w.signatureBadges().first().tooltip, QStringLiteral("Alice — VALID (trust: Valid)"));

        w.setSignatureBadges({});   // empty list must clear
        QVERIFY2(w.signatureBadges().isEmpty(), "an empty badge list must clear all badges");
    }

    // ── Painted at the field rect's top-right corner (single-page mode) ─────
    void badgePaintedAtFieldCorner_singlePageMode() {
        if (!QFileInfo::exists(kInputPdf))
            QSKIP("test_input.pdf missing — run tests/fixtures/signing/generate_test_input.py");

        PdfViewerWidget w;
        w.resize(900, 1000);
        QVERIFY(w.loadDocument(kInputPdf));
        QCOMPARE(w.pageCount(), 1);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        QApplication::processEvents();

        // Field well inside the blank 612x792 page so corner + control windows
        // are unambiguous (the fixture page has no content streams).
        const QRectF field(200, 200, 300, 100);
        w.setSignatureBadges({makeSpec(0, field, SignatureBadgeState::ValidTrusted,
                                       QStringLiteral("Alice — VALID"))});
        QApplication::processEvents();

        QWidget *overlay = w.findChild<QWidget *>(QStringLiteral("signatureBadgeOverlay"));
        QVERIFY2(overlay, "PdfViewerWidget must own a signatureBadgeOverlay child");

        // INDEPENDENT recomputation of the documented viewport mapping (the
        // same convention as PdfViewerWidget::handleLinkClick, top-left origin
        // page space): viewport pos = pageOrigin + pagePoint * zoom,
        // pageOrigin = (viewportSize - pageSize*zoom)/2 - scrollbar values.
        QWidget *pdfView = w.findChild<QWidget *>(QStringLiteral("pdfView"));
        QVERIFY(pdfView);
        QWidget *vp = pdfView->findChild<QWidget *>(QStringLiteral("qt_scrollarea_viewport"));
        QVERIFY(vp);
        auto *area = qobject_cast<QAbstractScrollArea *>(pdfView);
        QVERIFY(area);
        const QPointF vpTopLeft = vp->mapTo(&w, QPoint(0, 0)) - overlay->mapTo(&w, QPoint(0, 0));
        const QSizeF page = w.document()->pagePointSize(0);
        const qreal zoom = w.zoomLevel();
        const qreal originX = qMax<qreal>(0, (vp->width() - page.width() * zoom) / 2.0)
                            - area->horizontalScrollBar()->value();
        const qreal originY = qMax<qreal>(0, (vp->height() - page.height() * zoom) / 2.0)
                            - area->verticalScrollBar()->value();
        const QPointF corner = vpTopLeft
                             + QPointF(originX + field.right() * zoom, originY + field.top() * zoom);

        const QImage img = overlay->grab().toImage();
        QVERIFY(!img.isNull());
        const QColor green = PdfViewerWidget::signatureBadgeColor(SignatureBadgeState::ValidTrusted);
        const QRect cornerWin((corner - QPointF(20, 20)).toPoint(), QSize(40, 40));
        QVERIFY2(windowHasColor(img, cornerWin, green, 40, 8),
                 "the green trusted badge must be painted at the field rect's "
                 "top-right corner on the page");

        // Control regions: the field's top-LEFT corner (mirrored across the
        // field) and the page interior must carry no badge color.
        const QPointF ctrlCorner = corner - QPointF(field.width() * zoom, 0);
        const QRect ctrlWin((ctrlCorner - QPointF(20, 20)).toPoint(), QSize(40, 40));
        QVERIFY2(!windowHasColor(img, ctrlWin, green, 40, 1),
                 "no badge color may appear away from the field's top-right corner");
        const QPointF fieldCenter = vpTopLeft
                                  + QPointF(originX + field.center().x() * zoom,
                                            originY + field.center().y() * zoom);
        const QRect centerWin((fieldCenter - QPointF(20, 20)).toPoint(), QSize(40, 40));
        QVERIFY2(!windowHasColor(img, centerWin, green, 40, 1),
                 "no badge color may appear in the middle of the field rect");
    }

    // ── Empty list clears the paint ─────────────────────────────────────────
    void clearingBadgesRemovesPaint_singlePageMode() {
        if (!QFileInfo::exists(kInputPdf))
            QSKIP("test_input.pdf missing — run tests/fixtures/signing/generate_test_input.py");

        PdfViewerWidget w;
        w.resize(900, 1000);
        QVERIFY(w.loadDocument(kInputPdf));
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        QApplication::processEvents();

        const QRectF field(200, 200, 300, 100);
        w.setSignatureBadges({makeSpec(0, field, SignatureBadgeState::ModifiedAfterSigning,
                                       QStringLiteral("Bob — MODIFIED"))});
        QApplication::processEvents();
        QWidget *overlay = w.findChild<QWidget *>(QStringLiteral("signatureBadgeOverlay"));
        QVERIFY(overlay);
        const QColor red = PdfViewerWidget::signatureBadgeColor(SignatureBadgeState::ModifiedAfterSigning);

        // With the badge set, SOME corner window holds the red — compute it the
        // same independent way as badgePaintedAtFieldCorner_singlePageMode.
        const QPointF corner = singlePageOverlayCorner(w, overlay, field);
        const QRect cornerWin((corner - QPointF(20, 20)).toPoint(), QSize(40, 40));
        QVERIFY2(windowHasColor(overlay->grab().toImage(), cornerWin, red, 40, 8),
                 "precondition: the modified badge is painted at the corner");

        w.setSignatureBadges({});
        QApplication::processEvents();
        QVERIFY2(!windowHasColor(overlay->grab().toImage(), cornerWin, red, 40, 1),
                 "an empty badge list must remove the on-page badge");
    }

    // ── Painted into the two-page composite too ─────────────────────────────
    void badgePaintedInTwoPageMode() {
        if (!QFileInfo::exists(kInputPdf))
            QSKIP("test_input.pdf missing — run tests/fixtures/signing/generate_test_input.py");

        PdfViewerWidget w;
        w.resize(1200, 900);
        QVERIFY(w.loadDocument(kInputPdf));
        w.setTwoPageMode(true);
        QApplication::processEvents();

        const QRectF field(200, 200, 300, 100);
        w.setSignatureBadges({makeSpec(0, field, SignatureBadgeState::UntrustedChain,
                                       QStringLiteral("Carol — UNTRUSTED"))});
        QApplication::processEvents();

        QLabel *left = w.findChild<QLabel *>(QStringLiteral("twoPageLeftLabel"));
        QVERIFY(left);
        const QImage img = left->pixmap().toImage();
        QVERIFY(!img.isNull());
        // updateTwoPageView renders at renderScale = zoom * 2 and paintTwoPage-
        // Overlays maps top-left-origin page points directly by renderScale
        // (documented §9.1 convention, same as search highlights).
        const qreal renderScale = w.zoomLevel() * 2.0;
        const QPointF corner(field.right() * renderScale, field.top() * renderScale);
        const QColor amber = PdfViewerWidget::signatureBadgeColor(SignatureBadgeState::UntrustedChain);
        const QRect cornerWin((corner - QPointF(24, 24)).toPoint(), QSize(48, 48));
        QVERIFY2(windowHasColor(img, cornerWin, amber, 40, 8),
                 "the amber untrusted badge must appear in the two-page composite "
                 "at the field rect's top-right corner");

        const QPointF center(field.center().x() * renderScale, field.center().y() * renderScale);
        const QRect centerWin((center - QPointF(24, 24)).toPoint(), QSize(48, 48));
        QVERIFY2(!windowHasColor(img, centerWin, amber, 40, 1),
                 "no badge color may appear in the middle of the field rect in "
                 "the two-page composite");
    }

    // ── Tooltip payload: signer name + status detail, hit-testable ──────────
    void badgeTooltipPayloadAndHitTest() {
        if (!QFileInfo::exists(kInputPdf))
            QSKIP("test_input.pdf missing — run tests/fixtures/signing/generate_test_input.py");

        PdfViewerWidget w;
        w.resize(900, 1000);
        QVERIFY(w.loadDocument(kInputPdf));
        const QString tip = QStringLiteral("Alice — VALID (trust: Valid)");
        w.setSignatureBadges({makeSpec(0, QRectF(200, 200, 300, 100),
                                       SignatureBadgeState::ValidTrusted, tip)});
        QCOMPARE(w.signatureBadges().first().tooltip, tip);

        // Hit-test seam: the tooltip must be retrievable at the badge center in
        // viewport coordinates and nowhere else. (The real QToolTip popup is
        // driven from the viewport's ToolTip event and is not headless-assertable.)
        QWidget *pdfView = w.findChild<QWidget *>(QStringLiteral("pdfView"));
        QVERIFY(pdfView);
        QWidget *vp = pdfView->findChild<QWidget *>(QStringLiteral("qt_scrollarea_viewport"));
        QVERIFY(vp);
        auto *area = qobject_cast<QAbstractScrollArea *>(pdfView);
        QVERIFY(area);
        const QSizeF page = w.document()->pagePointSize(0);
        const qreal zoom = w.zoomLevel();
        const qreal originX = qMax<qreal>(0, (vp->width() - page.width() * zoom) / 2.0)
                            - area->horizontalScrollBar()->value();
        const qreal originY = qMax<qreal>(0, (vp->height() - page.height() * zoom) / 2.0)
                            - area->verticalScrollBar()->value();
        const QPointF center(originX + 500 * zoom, originY + 200 * zoom); // field.topRight()
        QCOMPARE(w.signatureBadgeTooltipAt(center.toPoint()), tip);
        QVERIFY2(w.signatureBadgeTooltipAt(QPoint(1, 1)).isEmpty(),
                 "no tooltip away from the badge");
    }

    // ── Panel → viewer push: SignatureInfo → the four badge states ──────────
    // Binding mapping: integrityIntact==false → ModifiedAfterSigning;
    // integrityIntact && isValid && trusted → ValidTrusted; integrityIntact &&
    // !isValid (or untrusted chain) → UntrustedChain; no data → Unknown.
    void panelPushMapsValidTrustedSignature() {
        PanelPush p;
        SignatureInfo s;
        s.fieldName = QStringLiteral("Sig1");
        s.signerName = QStringLiteral("Alice");
        s.integrityIntact = true;
        s.isValid = true;
        s.trustStatus = QStringLiteral("Valid");
        p.mock.m_signatures = {s};

        p.panel.setDocument(QStringLiteral("mock.pdf"), &p.mock);
        QCOMPARE(p.viewer.signatureBadges().size(), 1);
        const SignatureBadgeSpec b = p.viewer.signatureBadges().first(); // copy: QList temporary
        QVERIFY2(b.state == SignatureBadgeState::ValidTrusted,
                 "integrity intact + valid + trusted must map to the green state");
        QVERIFY2(b.tooltip.contains(QStringLiteral("Alice")),
                 "the tooltip must carry the signer name");
        QVERIFY2(b.tooltip.contains(QStringLiteral("VALID")),
                 "the tooltip must carry the status detail");
        // ValidWithDSS is a trusted status too.
        s.trustStatus = QStringLiteral("ValidWithDSS");
        p.mock.m_signatures = {s};
        p.panel.setDocument(QStringLiteral("mock.pdf"), &p.mock);
        QCOMPARE(p.viewer.signatureBadges().size(), 1);
        QVERIFY(p.viewer.signatureBadges().first().state == SignatureBadgeState::ValidTrusted);
    }

    void panelPushMapsModifiedSignature() {
        PanelPush p;
        SignatureInfo s;
        s.fieldName = QStringLiteral("Sig1");
        s.signerName = QStringLiteral("Bob");
        s.integrityIntact = false;   // byte range broke → document modified
        s.isValid = false;
        s.trustStatus = QStringLiteral("ByteRangeMismatch");
        p.mock.m_signatures = {s};

        p.panel.setDocument(QStringLiteral("mock.pdf"), &p.mock);
        QCOMPARE(p.viewer.signatureBadges().size(), 1);
        const SignatureBadgeSpec b = p.viewer.signatureBadges().first(); // copy: QList temporary
        QVERIFY2(b.state == SignatureBadgeState::ModifiedAfterSigning,
                 "integrityIntact==false must map to the red modified state");
        QVERIFY2(b.tooltip.contains(QStringLiteral("Bob")),
                 "the tooltip must carry the signer name");
        QVERIFY2(b.tooltip.contains(QStringLiteral("MODIFIED")),
                 "the tooltip must carry the status detail");
    }

    void panelPushMapsUntrustedChainSignature() {
        PanelPush p;
        SignatureInfo s;
        s.fieldName = QStringLiteral("Sig1");
        s.signerName = QStringLiteral("Carol");
        s.integrityIntact = true;
        s.isValid = false;
        s.trustStatus = QStringLiteral("UntrustedChain");
        p.mock.m_signatures = {s};

        p.panel.setDocument(QStringLiteral("mock.pdf"), &p.mock);
        QCOMPARE(p.viewer.signatureBadges().size(), 1);
        QVERIFY2(p.viewer.signatureBadges().first().state == SignatureBadgeState::UntrustedChain,
                 "integrity intact + not valid must map to the amber state");

        // Integrity ok and cryptographically valid, but the chain is untrusted
        // (cert expired) — still the amber "integrity-ok-but-untrusted" state.
        s.isValid = true;
        s.trustStatus = QStringLiteral("CertExpired");
        p.mock.m_signatures = {s};
        p.panel.setDocument(QStringLiteral("mock.pdf"), &p.mock);
        QCOMPARE(p.viewer.signatureBadges().size(), 1);
        QVERIFY2(p.viewer.signatureBadges().first().state == SignatureBadgeState::UntrustedChain,
                 "integrity intact + untrusted chain must map to the amber state, "
                 "even when isValid is true");
        QVERIFY2(p.viewer.signatureBadges().first().tooltip.contains(QStringLiteral("Carol")),
                 "the tooltip must carry the signer name");
    }

    void panelPushMapsUnknownSignature() {
        PanelPush p;
        // No data at all: validation produced an empty shell → gray.
        p.mock.m_signatures = {SignatureInfo{}};
        p.panel.setDocument(QStringLiteral("mock.pdf"), &p.mock);
        QCOMPARE(p.viewer.signatureBadges().size(), 1);
        QVERIFY2(p.viewer.signatureBadges().first().state == SignatureBadgeState::Unknown,
                 "a signature with no validation data must map to the gray state");
    }

    void panelPushClearsOnUnsignedDocument() {
        PanelPush p;
        SignatureInfo s;
        s.signerName = QStringLiteral("Alice");
        s.integrityIntact = true;
        s.isValid = true;
        s.trustStatus = QStringLiteral("Valid");
        p.mock.m_signatures = {s};
        p.panel.setDocument(QStringLiteral("mock.pdf"), &p.mock);
        QCOMPARE(p.viewer.signatureBadges().size(), 1);

        p.mock.m_signatures = {};
        p.panel.setDocument(QStringLiteral("unsigned.pdf"), &p.mock);
        QVERIFY2(p.viewer.signatureBadges().isEmpty(),
                 "an unsigned document must push an EMPTY badge list (clears)");
    }

private:
    // Stub seam harness: the panel→viewer connect() that production does in
    // GpMainWindow; here it IS the contract under test (direct connection).
    struct PanelPush {
        gp::SignaturesPanel panel;
        PdfViewerWidget viewer;
        MockSignatureManager mock;
        PanelPush() {
            QObject::connect(&panel, &gp::SignaturesPanel::signatureBadgesChanged,
                             &viewer, &PdfViewerWidget::setSignatureBadges);
        }
    };

    static SignatureBadgeSpec makeSpec(int page, const QRectF &rect,
                                       SignatureBadgeState state, const QString &tip) {
        SignatureBadgeSpec s;
        s.pageIndex = page;
        s.fieldRect = rect;
        s.state = state;
        s.tooltip = tip;
        return s;
    }

    static bool windowHasColor(const QImage &img, const QRect &window,
                               const QColor &target, int tol, int minCount) {
        if (img.isNull()) return false;
        int hits = 0;
        for (int y = window.top(); y <= window.bottom(); ++y) {
            for (int x = window.left(); x <= window.right(); ++x) {
                if (x < 0 || y < 0 || x >= img.width() || y >= img.height()) continue;
                const QColor px = img.pixelColor(x, y);
                if (qAbs(px.red() - target.red()) <= tol
                    && qAbs(px.green() - target.green()) <= tol
                    && qAbs(px.blue() - target.blue()) <= tol) {
                    if (++hits >= minCount) return true;
                }
            }
        }
        return false;
    }

    // Independent recomputation of the single-page overlay corner (documented
    // viewport mapping; see badgePaintedAtFieldCorner_singlePageMode).
    static QPointF singlePageOverlayCorner(PdfViewerWidget &w, QWidget *overlay, const QRectF &field) {
        QWidget *pdfView = w.findChild<QWidget *>(QStringLiteral("pdfView"));
        QWidget *vp = pdfView->findChild<QWidget *>(QStringLiteral("qt_scrollarea_viewport"));
        auto *area = qobject_cast<QAbstractScrollArea *>(pdfView);
        const QPointF vpTopLeft = vp->mapTo(&w, QPoint(0, 0)) - overlay->mapTo(&w, QPoint(0, 0));
        const QSizeF page = w.document()->pagePointSize(0);
        const qreal zoom = w.zoomLevel();
        const qreal originX = qMax<qreal>(0, (vp->width() - page.width() * zoom) / 2.0)
                            - area->horizontalScrollBar()->value();
        const qreal originY = qMax<qreal>(0, (vp->height() - page.height() * zoom) / 2.0)
                            - area->verticalScrollBar()->value();
        return vpTopLeft + QPointF(originX + field.right() * zoom, originY + field.top() * zoom);
    }
private slots:
    // ── §9.7 badge anchoring: real field page + rect from the engine ────────
    void engineResolvesFieldAnchorsOnSignedFixture() {
        if (!QFileInfo::exists(kInputPdf) || !QFileInfo::exists(kFixtureDir + "/test_signer.p12"))
            QSKIP("signing fixtures missing");

        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString signedPdf = tmp.filePath("anchored.pdf");
        SignatureManager mgr;
        QVERIFY2(mgr.signDocument(kInputPdf, signedPdf, kFixtureDir + "/test_signer.p12",
                                  QStringLiteral("test"), QStringLiteral("AnchorTest"),
                                  QString()) == SignOutcome::Success,
                 "signDocument must succeed with the test P12");

        const auto anchors = mgr.signatureFieldAnchors(signedPdf);
        QVERIFY2(anchors.size() == 1,
                 qPrintable(QStringLiteral("signed fixture must expose exactly 1 anchor, got %1")
                                .arg(anchors.size())));
        const auto &a = anchors.first();
        QCOMPARE(a.pageIndex, 0);
        QVERIFY2(!a.fieldName.isEmpty(), "the anchor must carry the field name");
        QVERIFY2(a.rect.width() > 50 && a.rect.height() > 20,
                 qPrintable(QStringLiteral("anchor rect must be the real widget rect: %1x%2")
                                .arg(a.rect.width()).arg(a.rect.height())));

        // The validator's fieldName and the anchor must describe the SAME field —
        // this equality is what the panel matches on.
        const auto sigs = mgr.validateSignatures(signedPdf);
        QVERIFY2(!sigs.isEmpty() && sigs.first().fieldName == a.fieldName,
                 qPrintable(QStringLiteral("fieldName mismatch: validator='%1' anchor='%2'")
                                .arg(sigs.isEmpty() ? QStringLiteral("<none>") : sigs.first().fieldName,
                                        a.fieldName)));
    }

    void panelAnchorsSpecsThroughMockAnchors() {
        PanelPush p;
        SignatureInfo s;
        s.fieldName = QStringLiteral("Sig1");
        s.signerName = QStringLiteral("Alice");
        s.integrityIntact = true;
        s.isValid = true;
        s.trustStatus = QStringLiteral("Valid");
        p.mock.m_signatures = {s};
        // The engine side supplies the on-page anchor for THAT field name.
        ISignatureManager::SignatureFieldAnchor a;
        a.fieldName = QStringLiteral("Sig1");
        a.pageIndex = 2;
        a.rect = QRectF(100, 650, 200, 100);
        p.mock.m_anchors = {a};

        p.panel.setDocument(QStringLiteral("mock.pdf"), &p.mock);
        QCOMPARE(p.viewer.signatureBadges().size(), 1);
        const SignatureBadgeSpec b = p.viewer.signatureBadges().first();
        QCOMPARE(b.pageIndex, 2);
        QCOMPARE(b.fieldRect, QRectF(100, 650, 200, 100));
        QVERIFY(b.state == SignatureBadgeState::ValidTrusted); // state mapping unaffected
    }

};

QTEST_MAIN(TestSignatureBadges)
#include "TestSignatureBadges.moc"
