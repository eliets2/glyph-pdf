// SPDX-License-Identifier: Apache-2.0
// sweep-legacy (2026-09-19) — the /Rotate + offset-MediaBox origin class at the
// annotation and form-field engine boundaries.
//
// The SEP13 L5/L8 repair established ONE viewer→user page-space law
// (core/PageSpaceTransform.h) and F1 (R14 review) fixed the REDACTION consumers
// of annotation rects. This suite pins the REMAINING legacy consumers of the
// same law at their shared boundary — PoDoFoBackend::applyAnnotationsToDoc /
// extractAnnotations (annotation move/resize/appearance commit + read-back)
// and FormManager field creation / updateFieldRect (FormBuilder placement):
//
//   AnnotationItem::rect / FormManager rect parameters are DISPLAY-space
//   values (top-left origin, Y down, dimensions = the page's displayed size,
//   MediaBox W/H swapped for /Rotate 90/270 — exactly the viewer space the
//   PageSpace header defines). The legacy writers flip Y with the MediaBox
//   HEIGHT alone (a) dropping the MediaBox lower-left origin and (b) ignoring
//   /Rotate, so on any rotated or offset-origin page the saved /Rect (and the
//   /InkList geometry) lands somewhere the user did not draw, while GlyphPDF's
//   own overlay (fed by the inverse-wrong read-back) keeps showing the mark in
//   the right place — the exact silent-misplacement class SEP13 L5/L8
//   documented for redaction marks.
//
// Independent read paths: the raw /Rect array via PoDoFo's dictionary walk AND
// PDFium's FPDFAnnot_GetRect (page user space). Expected values are computed
// through gp::PageSpace::viewerToUser (the ledger-verified law) with the
// fixture constants hardcoded as literals for the decisive pages.
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <cmath>

#include "engines/podofo/PoDoFoBackend.h"
#include "engines/FormManager.h"
#include "core/AnnotationTypes.h"
#include "core/PageSpaceTransform.h"
#include "core/ItemSpaceTransform.h"
#include <podofo/podofo.h>

#include <fpdfview.h>
#include <fpdf_annot.h>
#include "engines/pdfium/PdfiumEnvironment.h"

namespace {

QString rectStr(const QRectF& r)
{
    return QString("[%1 %2 %3 %4]").arg(r.left()).arg(r.top())
        .arg(r.right()).arg(r.bottom());
}

constexpr bool rectClose(const QRectF& a, const QRectF& b)
{
    return std::fabs(a.x() - b.x()) < 0.01 && std::fabs(a.y() - b.y()) < 0.01
        && std::fabs(a.right() - b.right()) < 0.01
        && std::fabs(a.bottom() - b.bottom()) < 0.01;
}

// The four fixture pages: (mediaBox, rotation). Same order the builder writes.
struct PageSpec {
    PoDoFo::Rect media;
    int rotation;
};

QList<PageSpec> pageSpecs()
{
    return {
        { PoDoFo::Rect(0, 0, 612, 792), 0 },    // page 0: plain control
        { PoDoFo::Rect(0, 200, 612, 842), 0 },  // page 1: offset origin
        { PoDoFo::Rect(0, 0, 612, 792), 90 },   // page 2: rotated
        { PoDoFo::Rect(0, 200, 612, 842), 90 }, // page 3: rotated + offset
        // W2B-1 (sweep-w2b-verify 2026-09-20): the original fixture covered
        // rot {0, 90} + offset only — exactly the shapes where the law's
        // formula never reads the MediaBox W/H, so the rotation-normalized
        // GetMediaBox defect was INVISIBLE here while /Rotate 270 pages
        // stored transposed rects. Page 4 is the blind spot, pinned with
        // hand-computed literals below.
        { PoDoFo::Rect(0, 200, 612, 842), 270 }, // page 4: rotated + offset, opposite handedness
    };
}

// Build a 4-page fixture (no content — annotations/fields don't need ink).
QString buildFixture(const QString& path)
{
    try {
        PoDoFo::PdfMemDocument doc;
        for (const PageSpec& spec : pageSpecs()) {
            PoDoFo::PdfPage& page = doc.GetPages().CreatePage(spec.media);
            page.SetRotation(spec.rotation);
        }
        doc.Save(path.toUtf8().constData());
        return path;
    } catch (const std::exception& e) {
        qWarning() << "buildFixture failed:" << e.what();
        return QString();
    }
}

// Add a foreign /Square annotation at an explicit USER-space rect (what a
// spec-compliant external writer produces). PoDoFo's CreateAnnot rect
// parameter is /Rotate-View-space and gets transformed (probe: the stored
// /Rect differs from the input on rotated pages), so the raw user rect is
// stored verbatim via SetRectRaw — the same discipline the writer fix uses.
bool addForeignSquare(const QString& path, int pageIndex, const PoDoFo::Rect& userRect)
{
    try {
        PoDoFo::PdfMemDocument doc;
        doc.Load(path.toUtf8().constData());
        PoDoFo::PdfPage& page = doc.GetPages().GetPageAt(pageIndex);
        auto& annot = page.GetAnnotations().CreateAnnot(
            PoDoFo::PdfAnnotationType::Square,
            PoDoFo::Rect(userRect.X, userRect.Y, userRect.Width, userRect.Height));
        annot.SetRectRaw(PoDoFo::Corners(userRect.X, userRect.Y,
                                         userRect.X + userRect.Width,
                                         userRect.Y + userRect.Height));
        doc.Save(path.toUtf8().constData());
        return true;
    } catch (const std::exception& e) {
        qWarning() << "addForeignSquare failed:" << e.what();
        return false;
    }
}

// Read every annotation /Rect on `pageIndex` from the RAW dictionary array —
// PoDoFo's GetRect() folds /Rotate at read time (the F1 lesson), so the pin
// walks the array itself. Form widgets created via page.CreateField appear in
// the same page annotation array, so one reader covers both surfaces.
QList<QRectF> rawAnnotRects(const QString& path, int pageIndex)
{
    QList<QRectF> out;
    try {
        PoDoFo::PdfMemDocument doc;
        doc.Load(path.toUtf8().constData());
        auto& page = doc.GetPages().GetPageAt(pageIndex);
        auto& annos = page.GetAnnotations();
        for (unsigned a = 0; a < annos.GetCount(); ++a) {
            auto* rectObj = annos.GetAnnotAt(a).GetDictionary().FindKey("Rect");
            if (!rectObj || !rectObj->IsArray()) continue;
            const auto& arr = rectObj->GetArray();
            if (arr.size() != 4) continue;
            const double x0 = arr[0].GetReal(), y0 = arr[1].GetReal();
            const double x1 = arr[2].GetReal(), y1 = arr[3].GetReal();
            out.append(QRectF(QPointF(qMin(x0, x1), qMin(y0, y1)),
                              QPointF(qMax(x0, x1), qMax(y0, y1))));
        }
    } catch (const std::exception& e) {
        qWarning() << "rawAnnotRects failed:" << e.what();
    }
    return out;
}

// PDFium's view of the annotation rects on a page (page user space, y-up).
QList<QRectF> pdfiumAnnotRects(const QString& path, int pageIndex)
{
    QList<QRectF> out;
    PdfiumEnvironment env;
    FPDF_DOCUMENT doc = FPDF_LoadDocument(path.toUtf8().constData(), nullptr);
    if (!doc) return out;
    FPDF_PAGE page = FPDF_LoadPage(doc, pageIndex);
    if (page) {
        const int count = FPDFPage_GetAnnotCount(page);
        for (int i = 0; i < count; ++i) {
            FPDF_ANNOTATION annot = FPDFPage_GetAnnot(page, i);
            FS_RECTF box;
            if (annot && FPDFAnnot_GetRect(annot, &box)) {
                out.append(QRectF(QPointF(box.left, box.bottom),
                                  QPointF(box.right, box.top)));
            }
            if (annot) FPDFPage_CloseAnnot(annot);
        }
        FPDF_ClosePage(page);
    }
    FPDF_CloseDocument(doc);
    return out;
}

QRectF firstRect(const QList<QRectF>& rects)
{
    return rects.isEmpty() ? QRectF() : rects.first();
}

} // namespace

class TestLegacyOriginSpace : public QObject {
    Q_OBJECT

    QTemporaryDir m_dir;

    QString fixturePath() const { return m_dir.filePath("origin-fixture.pdf"); }
    QString outPath(const char* name) const { return m_dir.filePath(QLatin1String(name)); }

private slots:

    // The inverse in ItemSpaceTransform.h must be the EXACT inverse of the
    // shared forward law, for every rotation × MediaBox offset × rect shape —
    // this is what lets the read boundary mirror the write boundary without
    // touching the W1-owned PageSpaceTransform.h.
    void userToViewerIsTheExactInverseOfTheForwardLaw()
    {
        const QList<PageSpec> specs = pageSpecs();
        const QList<QRectF> rects = {
            QRectF(100, 150, 80, 40),
            QRectF(0.5, 0.25, 611.5, 791.5),
            QRectF(20, 30, 572, 732),
        };
        for (const PageSpec& spec : specs) {
            const gp::PageSpace::PageGeometry geo = gp::PageSpace::pageGeometryFromMediaBox(
                spec.media.X, spec.media.Y, spec.media.Width, spec.media.Height,
                spec.rotation);
            for (const QRectF& viewer : rects) {
                const QRectF back = gp::ItemSpace::userToViewer(
                    gp::PageSpace::viewerToUser(viewer, geo), geo);
                QVERIFY2(rectClose(back, viewer),
                         qPrintable(QString("rot %1: viewer %2 -> user -> viewer %3")
                                        .arg(spec.rotation).arg(rectStr(viewer), rectStr(back))));
                const QRectF backFwd = gp::PageSpace::viewerToUser(
                    gp::ItemSpace::userToViewer(viewer, geo), geo);
                QVERIFY2(rectClose(backFwd, viewer),
                         qPrintable(QString("rot %1: viewer %2 -> user^-1 -> user %3")
                                        .arg(spec.rotation).arg(rectStr(viewer), rectStr(backFwd))));
            }
        }
    }

    void init()
    {
        QVERIFY(m_dir.isValid());
        QVERIFY(!buildFixture(fixturePath()).isEmpty());
    }

    // ── the decisive anchors: annotation embed must follow the page-space law ──
    void annotationEmbedWritesTheDrawnPositionOnEveryPageShape()
    {
        PoDoFoBackend engine;
        QVERIFY(engine.loadDocument(fixturePath()));

        const QRectF drawn(100, 150, 80, 40); // display space, inside every page
        QList<AnnotationItem> items;
        for (int p = 0; p < pageSpecs().size(); ++p) {
            AnnotationItem item;
            item.pageIndex = p;
            item.mode = ToolMode::DrawRectangle;
            item.rect = drawn;
            item.text = QStringLiteral("origin-probe-%1").arg(p);
            items.append(item);
        }
        const QString out = outPath("embed-all-pages.pdf");
        QVERIFY(engine.embedAnnotations(fixturePath(), out, items));

        const QList<PageSpec> specs = pageSpecs();
        for (int p = 0; p < specs.size(); ++p) {
            const gp::PageSpace::PageGeometry geo =
                gp::PageSpace::pageGeometryFromMediaBox(
                    specs[p].media.X, specs[p].media.Y,
                    specs[p].media.Width, specs[p].media.Height, specs[p].rotation);
            const QRectF expected = gp::PageSpace::viewerToUser(drawn, geo);

            // Independent read path 1: raw /Rect arrays.
            const QList<QRectF> raw = rawAnnotRects(out, p);
            QVERIFY2(raw.size() == 1, qPrintable(QString("page %1: %2 annots").arg(p).arg(raw.size())));
            QVERIFY2(rectClose(raw.first(), expected),
                     qPrintable(QString("page %1 raw /Rect %2 != law rect %3")
                                    .arg(p).arg(rectStr(raw.first()), rectStr(expected))));

            // Independent read path 2: PDFium (a second engine reports the box).
            const QRectF viaPdfium = firstRect(pdfiumAnnotRects(out, p));
            QVERIFY2(!viaPdfium.isNull(), qPrintable(QString("page %1: PDFium saw no annot").arg(p)));
            QVERIFY2(rectClose(viaPdfium, expected),
                     qPrintable(QString("page %1 PDFium rect %2 != law rect %3")
                                    .arg(p).arg(rectStr(viaPdfium), rectStr(expected))));
        }

        // Hardcoded literals for the decisive shapes (independent arithmetic,
        // not computed through the law): page 1 (offset, /Rotate 0),
        // page 3 (/Rotate 90 + offset) and page 4 (/Rotate 270 + offset, the
        // W2B-1 blind spot) for drawn (100,150,80,40).
        {
            const QList<QRectF> raw1 = rawAnnotRects(out, 1);
            const QRectF expect1(QPointF(100, 852), QPointF(180, 892));
            QVERIFY2(rectClose(raw1.first(), expect1),
                     qPrintable(QString("page1 %1 != [100 852 180 892]").arg(rectStr(raw1.first()))));
        }
        {
            const QList<QRectF> raw3 = rawAnnotRects(out, 3);
            const QRectF expect3(QPointF(150, 300), QPointF(190, 380));
            QVERIFY2(rectClose(raw3.first(), expect3),
                     qPrintable(QString("page3 %1 != [150 300 190 380]").arg(rectStr(raw3.first()))));
        }
        {
            // Page 4: raw box (0,200,612,842), /Rotate 270. vx 100..180,
            // vy 150..190 → ux = 612-190..612-150 = 422..462;
            // uy = 200+842-180..200+842-100 = 862..942. The user rect is
            // 40 wide x 80 tall (the display 80x40 swapped) — a transposed
            // (doubly-rotated) store would be 80x40.
            const QList<QRectF> raw4 = rawAnnotRects(out, 4);
            const QRectF expect4(QPointF(422, 862), QPointF(462, 942));
            QVERIFY2(rectClose(raw4.first(), expect4),
                     qPrintable(QString("W2B-1: page4 (rot 270+offset) %1 != [422 862 462 942]")
                                    .arg(rectStr(raw4.first()))));
        }
    }

    // Annotation geometry arrays (/InkList for freehand) must follow the same
    // law point-by-point — the /Rect alone does not carry a freehand stroke.
    void freehandInkListFollowsTheSameLaw()
    {
        // W2B-1: parameterized over the rotated+offset pages of both
        // handednesses — page 3 (rot 90) and page 4 (rot 270). The per-point
        // expectation is computed through the shared law (0-size rects are
        // corner-exact for 90-degree multiples).
        const struct { int page; int rotation; } cases[] = { { 3, 90 }, { 4, 270 } };
        for (const auto& c : cases) {
            PoDoFoBackend engine;
            QVERIFY(engine.loadDocument(fixturePath()));

            AnnotationItem item;
            item.pageIndex = c.page; // rotated + offset
            item.mode = ToolMode::DrawFreehand;
            item.points = QList<QPointF>{ QPointF(100, 150), QPointF(180, 150),
                                          QPointF(180, 190), QPointF(100, 190) };
            item.rect = QRectF(100, 150, 80, 40);
            const QString out = outPath(c.page == 3 ? "embed-ink.pdf" : "embed-ink-270.pdf");
            QVERIFY(engine.embedAnnotations(fixturePath(), out, { item }));

            const gp::PageSpace::PageGeometry geo = gp::PageSpace::pageGeometryFromMediaBox(
                0, 200, 612, 842, c.rotation);
            QList<QPointF> expectedPts;
            for (const QPointF& pt : item.points) {
                expectedPts.append(gp::PageSpace::viewerToUser(QRectF(pt, pt), geo).topLeft());
            }

            // Raw /InkList walk.
            try {
                PoDoFo::PdfMemDocument doc;
                doc.Load(out.toUtf8().constData());
                auto& annos = doc.GetPages().GetPageAt(c.page).GetAnnotations();
                QVERIFY(annos.GetCount() >= 1);
                auto* ink = annos.GetAnnotAt(0).GetDictionary().FindKey("InkList");
                QVERIFY(ink && ink->IsArray() && !ink->GetArray().IsEmpty());
                const auto& stroke = ink->GetArray()[0].GetArray();
                QCOMPARE(stroke.size(), static_cast<size_t>(item.points.size() * 2));
                for (size_t k = 0; k + 1 < stroke.size(); k += 2) {
                    const QPointF got(stroke[k].GetReal(), stroke[k + 1].GetReal());
                    const QPointF want = expectedPts[k / 2];
                    QVERIFY2(std::fabs(got.x() - want.x()) < 0.01
                                 && std::fabs(got.y() - want.y()) < 0.01,
                             qPrintable(QString("rot %1 ink point %2: got (%3,%4) want (%5,%6)")
                                            .arg(c.rotation)
                                            .arg(static_cast<int>(k / 2))
                                            .arg(got.x()).arg(got.y())
                                            .arg(want.x()).arg(want.y())));
                }
            } catch (const std::exception& e) {
                QFAIL(qPrintable(QString("PoDoFo walk failed: %1").arg(e.what())));
            }
        }
    }

    // Read-back: a FOREIGN writer's annotation at a known user rect must
    // surface in the overlay's display space at the drawn spot.
    void extractMapsForeignRectsIntoDisplaySpace()
    {
        const QString path = fixturePath();
        // Foreign spec-correct annotation on the rotated+offset page: the user
        // rect that corresponds to display (100,150,80,40) on page 3.
        QVERIFY(addForeignSquare(path, 3, PoDoFo::Rect(150, 300, 40, 80)));
        // Control on the plain page: user rect for display (100,150,80,40).
        QVERIFY(addForeignSquare(path, 0, PoDoFo::Rect(100, 602, 80, 40)));
        // W2B-1: the same foreign spec-correct rect on the /Rotate 270+offset
        // page — user [422 862 462 942] (40x80, swapped) must surface at the
        // drawn display spot (100,150,80x40), not transposed.
        QVERIFY(addForeignSquare(path, 4, PoDoFo::Rect(422, 862, 40, 80)));

        PoDoFoBackend engine;
        const QList<AnnotationItem> items = engine.extractAnnotations(path);
        QCOMPARE(items.size(), 3);

        for (const AnnotationItem& item : items) {
            const QRectF want(100, 150, 80, 40); // display space, all three pages
            QVERIFY2(rectClose(item.rect, want),
                     qPrintable(QString("page %1 display %2 != (100,150,80x40)")
                                    .arg(item.pageIndex).arg(rectStr(item.rect))));
        }
    }

    // Consistency guard: embed → extract keeps the drawn display position on
    // every page shape (must stay true after the law lands at the boundary).
    void embedExtractRoundTripKeepsTheDrawnPosition()
    {
        PoDoFoBackend engine;
        QVERIFY(engine.loadDocument(fixturePath()));

        QList<AnnotationItem> items;
        for (int p = 0; p < pageSpecs().size(); ++p) {
            AnnotationItem item;
            item.pageIndex = p;
            item.mode = ToolMode::DrawRectangle;
            item.rect = QRectF(100, 150, 80, 40);
            items.append(item);
        }
        const QString out = outPath("roundtrip.pdf");
        QVERIFY(engine.embedAnnotations(fixturePath(), out, items));

        PoDoFoBackend reader;
        const QList<AnnotationItem> back = reader.extractAnnotations(out);
        QCOMPARE(back.size(), items.size());
        for (const AnnotationItem& item : back) {
            QVERIFY2(rectClose(item.rect, QRectF(100, 150, 80, 40)),
                     qPrintable(QString("page %1 round-trip %2 drifted")
                                    .arg(item.pageIndex).arg(rectStr(item.rect))));
        }
    }

    // ── form fields: creation and rect update follow the same law ─────────────
    void formFieldCreationLandsAtTheDrawnPosition()
    {
        FormManager forms;

        // Rotated + offset page (page 3): drawn (100,150,200x50) in display
        // space must store /Rect [150 300 200 500] in user space.
        const QString out3 = outPath("field-page3.pdf");
        QVERIFY(forms.addTextField(fixturePath(), 3, QRectF(100, 150, 200, 50),
                                   QStringLiteral("OriginField3"), out3));
        const QList<QRectF> rects3 = rawAnnotRects(out3, 3);
        QVERIFY(rects3.size() == 1);
        const QRectF expect3(QPointF(150, 300), QPointF(200, 500));
        QVERIFY2(rectClose(rects3.first(), expect3),
                 qPrintable(QString("page3 field /Rect %1 != [150 300 200 500]")
                                .arg(rectStr(rects3.first()))));

        // PDFium (form widgets are annotations to a second engine too).
        const QRectF viaPdfium = firstRect(pdfiumAnnotRects(out3, 3));
        QVERIFY(!viaPdfium.isNull());
        QVERIFY2(rectClose(viaPdfium, expect3),
                 qPrintable(QString("page3 field PDFium %1 != law").arg(rectStr(viaPdfium))));

        // W2B-1: /Rotate 270 + offset (page 4): drawn (100,150,200x50) →
        // vx 100..300, vy 150..200 → ux = 612-200..612-150 = 412..462;
        // uy = 200+842-300..200+842-100 = 742..942 (50x200 — swapped).
        const QString out4 = outPath("field-page4.pdf");
        QVERIFY(forms.addTextField(fixturePath(), 4, QRectF(100, 150, 200, 50),
                                   QStringLiteral("OriginField4"), out4));
        const QList<QRectF> rects4 = rawAnnotRects(out4, 4);
        QVERIFY(rects4.size() == 1);
        const QRectF expect4(QPointF(412, 742), QPointF(462, 942));
        QVERIFY2(rectClose(rects4.first(), expect4),
                 qPrintable(QString("W2B-1: page4 (rot 270+offset) field /Rect %1 != [412 742 462 942]")
                                .arg(rectStr(rects4.first()))));

        const QRectF viaPdfium4 = firstRect(pdfiumAnnotRects(out4, 4));
        QVERIFY(!viaPdfium4.isNull());
        QVERIFY2(rectClose(viaPdfium4, expect4),
                 qPrintable(QString("page4 field PDFium %1 != law").arg(rectStr(viaPdfium4))));

        // Offset, unrotated control (page 1): drawn (100,150,200x50) must
        // store /Rect [100 842 300 892] — the dropped y0=200 offset alone.
        const QString out1 = outPath("field-page1.pdf");
        QVERIFY(forms.addTextField(fixturePath(), 1, QRectF(100, 150, 200, 50),
                                   QStringLiteral("OriginField1"), out1));
        const QList<QRectF> rects1 = rawAnnotRects(out1, 1);
        QVERIFY(rects1.size() == 1);
        const QRectF expect1(QPointF(100, 842), QPointF(300, 892));
        QVERIFY2(rectClose(rects1.first(), expect1),
                 qPrintable(QString("page1 field /Rect %1 != [100 842 300 892]")
                                .arg(rectStr(rects1.first()))));
    }

    void formFieldRectUpdateFollowsTheSameLaw()
    {
        FormManager forms;
        const QString out = outPath("field-update.pdf");
        QVERIFY(forms.addTextField(fixturePath(), 3, QRectF(100, 150, 200, 50),
                                   QStringLiteral("UpdField"), out));
        QVERIFY(forms.updateFieldRect(out, QStringLiteral("UpdField"), 3,
                                      QRectF(60, 120, 140, 36), out));
        const QList<QRectF> rects = rawAnnotRects(out, 3);
        QVERIFY(rects.size() == 1);
        // Law expectation for (60,120,140x36) on {0,200,612,842, rot 90}:
        // user = [120 260 156 400].
        const QRectF expect(QPointF(120, 260), QPointF(156, 400));
        QVERIFY2(rectClose(rects.first(), expect),
                 qPrintable(QString("updated /Rect %1 != [120 260 156 400]")
                                .arg(rectStr(rects.first()))));

        // W2B-1: the same update on the /Rotate 270+offset page (page 4):
        // (60,120,140x36) → vx 60..200, vy 120..156 →
        // ux = 612-156..612-120 = 456..492; uy = 200+842-200..200+842-60 =
        // 842..982 (36x140 — swapped).
        const QString out4 = outPath("field-update-270.pdf");
        QVERIFY(forms.addTextField(fixturePath(), 4, QRectF(100, 150, 200, 50),
                                   QStringLiteral("UpdField270"), out4));
        QVERIFY(forms.updateFieldRect(out4, QStringLiteral("UpdField270"), 4,
                                      QRectF(60, 120, 140, 36), out4));
        const QList<QRectF> rects4 = rawAnnotRects(out4, 4);
        QVERIFY(rects4.size() == 1);
        const QRectF expect4(QPointF(456, 842), QPointF(492, 982));
        QVERIFY2(rectClose(rects4.first(), expect4),
                 qPrintable(QString("W2B-1: updated /Rect %1 on rot 270+offset != [456 842 492 982]")
                                .arg(rectStr(rects4.first()))));
    }
};

QTEST_MAIN(TestLegacyOriginSpace)
#include "TestLegacyOriginSpace.moc"
