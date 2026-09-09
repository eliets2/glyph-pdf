// SPDX-License-Identifier: Apache-2.0
// T1 measurement toolset — saved-artifact persistence contract.
//
// create → calibrate → measure → save → reopen, asserted through THREE
// independent read paths:
//   1. GlyphPDF's own extractAnnotations (what the viewer shows on reopen),
//   2. a RAW PoDoFo dictionary inspection of the saved bytes (the ISO 32000-1
//      /Measure schema itself, independent of our mapping),
//   3. PDFium's annotation API (a genuinely different engine reading geometry
//      and confirming the /Measure key exists).
// Plus the negative control: an uncalibrated measurement persists the truthful
// 1 pt scale, and a plain DrawLine still round-trips as DrawLine (no /Measure).
//
// Gate G21/G22 regressions live here too:
//   G21 — /Rect of a points-carried annotation is the REAL bounding box (plus
//         a half-stroke-width appearance margin), never the 0×0 last point a
//         QRectF union of point-sized rects produces; verified in raw bytes
//         AND through PDFium's embedder-visible rect (what PDFium-based
//         viewers hit-test and cull against), including the shared ink branch.
//   G22 — the perimeter tool measures a CLOSED boundary; the serialized
//         /PolyLine /Vertices therefore include the closing segment (first
//         vertex repeated last), so a reader that walks the stored path
//         measures exactly the displayed label. The path length is verified
//         FROM THE SERIALIZED BYTES and through PDFium, not just our helper.
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <cmath>
#include <vector>
#include "engines/podofo/PoDoFoBackend.h"
#include "core/MeasureCore.h"
#include <podofo/podofo.h>

#ifdef HAS_PDFIUM
#include <fpdfview.h>
#include <fpdf_annot.h>
#include "engines/pdfium/PdfiumEnvironment.h"
#endif

using namespace gp::measure;

namespace {

// Same minimal single-page seed TestShapeInkPersistence pins (612×792 page).
QByteArray seedPdf()
{
    return
        "%PDF-1.4\n"
        "1 0 obj<</Type/Catalog/Pages 2 0 R>>endobj\n"
        "2 0 obj<</Type/Pages/Kids[3 0 R]/Count 1>>endobj\n"
        "3 0 obj<</Type/Page/Parent 2 0 R/MediaBox[0 0 612 792]>>endobj\n"
        "xref\n0 4\n"
        "0000000000 65535 f \n"
        "0000000009 00000 n \n"
        "0000000058 00000 n \n"
        "0000000115 00000 n \n"
        "trailer<</Size 4/Root 1 0 R>>\n"
        "startxref\n183\n%%EOF\n";
}

bool writeSeed(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return false;
    f.write(seedPdf());
    return true;
}

// Calibrated 1 mm = 2 pt (unitsPerPt 0.5): a 72 pt line reads 36 mm.
AnnotationItem calibratedDistance()
{
    AnnotationItem a;
    a.mode = ToolMode::MeasureDistance;
    a.pageIndex = 0;
    a.points = { QPointF(10, 10), QPointF(82, 10) };  // top-left page space
    a.rect = QRectF(10, 10, 72, 0);
    a.color = Qt::red;
    a.measureCalibrated = true;
    a.measureUnitsPerPt = 0.5;
    a.measureUnit = QStringLiteral("mm");
    a.measureAreaUnit = QStringLiteral("mm\u00B2");
    a.measureRatio = QStringLiteral("1 mm = 2 pt");
    a.text = QStringLiteral("36.00 mm");   // /Contents snapshot at creation
    return a;
}

AnnotationItem calibratedPerimeter()
{
    AnnotationItem a;
    a.mode = ToolMode::MeasurePerimeter;
    a.pageIndex = 0;
    a.points = { QPointF(10, 10), QPointF(82, 10), QPointF(82, 82), QPointF(10, 82) };
    a.rect = QRectF(10, 10, 72, 72);
    a.measureCalibrated = true;
    a.measureUnitsPerPt = 0.5;
    a.measureUnit = QStringLiteral("mm");
    a.measureAreaUnit = QStringLiteral("mm\u00B2");
    a.measureRatio = QStringLiteral("1 mm = 2 pt");
    a.text = QStringLiteral("144.00 mm");
    return a;
}

// Non-convex L-shape, 1/10 of the TestMeasureCore polygon: 64 pt² → 16 mm² at 0.5.
AnnotationItem calibratedArea()
{
    AnnotationItem a;
    a.mode = ToolMode::MeasureArea;
    a.pageIndex = 0;
    a.points = { QPointF(0, 0), QPointF(10, 0), QPointF(10, 4),
                 QPointF(4, 4),  QPointF(4, 10), QPointF(0, 10) };
    a.rect = QRectF(0, 0, 10, 10);
    a.measureCalibrated = true;
    a.measureUnitsPerPt = 0.5;
    a.measureUnit = QStringLiteral("mm");
    a.measureAreaUnit = QStringLiteral("mm\u00B2");
    a.measureRatio = QStringLiteral("1 mm = 2 pt");
    a.text = QStringLiteral("16.00 mm\u00B2");
    return a;
}

const AnnotationItem* findByMode(const QList<AnnotationItem>& list, ToolMode mode)
{
    for (const auto& a : list)
        if (a.mode == mode) return &a;
    return nullptr;
}

// G21 helper: read the RAW /Rect array of an annotation dictionary.
bool rawRect(const PoDoFo::PdfDictionary& dict, double out[4])
{
    const PoDoFo::PdfObject* r = dict.FindKey("Rect");
    if (!r || !r->IsArray() || r->GetArray().size() != 4) return false;
    const auto& arr = r->GetArray();
    for (int i = 0; i < 4; ++i)
        if (!arr[size_t(i)].IsNumberOrReal()) return false;
    for (int i = 0; i < 4; ++i)
        out[i] = arr[size_t(i)].GetReal();
    return true;
}

// Open-path traversal of a serialized [x0 y0 x1 y1 …] vertex list — the
// length ANY reader measures by walking the stored path (G22 contract).
double serializedPathLength(const QList<QPointF>& pts)
{
    return gp::measure::polylineLength(pts);
}

} // namespace

class TestMeasureRoundTrip : public QObject {
    Q_OBJECT
private slots:
    void distanceRoundTripsWithValues();
    void rawDictCarriesVerifiedMeasureSchema();
    void perimeterAndAreaRoundTrip();
    void savedRectEnclosesGeometryAndHitTests();
    void serializedPerimeterPathLengthMatchesLabel();
    void inkStrokePersistsUsableRect();
    void pdfiumReadPathSeesMeasureAnnots();
    void uncalibratedMeasurePersistsTruthfulPt();
    void plainLineWithoutMeasureStaysDrawLine();

private:
    // Saves the three calibrated measurements ONCE and returns the output path.
    QString saveCalibratedDoc();
    QTemporaryDir m_tmpDir;
    QString m_savedPath;
};

QString TestMeasureRoundTrip::saveCalibratedDoc()
{
    if (!m_savedPath.isEmpty()) return m_savedPath;
    if (!m_tmpDir.isValid()) {
        QTest::qFail("temp dir invalid", __FILE__, __LINE__);
        return QString();
    }
    const QString seed = m_tmpDir.filePath("seed.pdf");
    if (!writeSeed(seed)) {
        QTest::qFail("could not write seed PDF", __FILE__, __LINE__);
        return QString();
    }
    PoDoFoBackend backend;
    const QString out = m_tmpDir.filePath("measured.pdf");
    if (!backend.embedAnnotations(seed, out,
        { calibratedDistance(), calibratedPerimeter(), calibratedArea() })) {
        QTest::qFail("embedAnnotations failed", __FILE__, __LINE__);
        return QString();
    }
    m_savedPath = out;
    return out;
}

void TestMeasureRoundTrip::distanceRoundTripsWithValues()
{
    const QString out = saveCalibratedDoc();
    PoDoFoBackend backend;
    const QList<AnnotationItem> back = backend.extractAnnotations(out);
    QCOMPARE(back.size(), 3);

    const AnnotationItem* d = findByMode(back, ToolMode::MeasureDistance);
    QVERIFY2(d, "distance measurement must survive as MeasureDistance");
    QCOMPARE(d->points.size(), 2);
    // Y-flip symmetry: top-left page space in, same coordinates back out.
    QVERIFY(std::fabs(d->points.first().x() - 10.0) < 1e-6);
    QVERIFY(std::fabs(d->points.last().x() - 82.0) < 1e-6);
    QVERIFY(std::fabs(d->points.first().y() - 10.0) < 1e-6);
    // Calibration round-trips.
    QVERIFY(d->measureCalibrated);
    QVERIFY(std::fabs(d->measureUnitsPerPt - 0.5) < 1e-9);
    QCOMPARE(d->measureUnit, QStringLiteral("mm"));
    QCOMPARE(d->measureRatio, QStringLiteral("1 mm = 2 pt"));
    // Computed value from RESTORED geometry × RESTORED scale.
    QVERIFY(std::fabs(convertLengthPt(polylineLength(d->points),
                      scaleFrom(d->measureUnitsPerPt, d->measureUnit, d->measureCalibrated, d->measureRatio)) - 36.0) < 1e-6);
    // /Contents snapshot survives.
    QCOMPARE(d->text, QStringLiteral("36.00 mm"));
}

void TestMeasureRoundTrip::rawDictCarriesVerifiedMeasureSchema()
{
    const QString out = saveCalibratedDoc();

    PoDoFo::PdfMemDocument doc;
    doc.Load(out.toUtf8().constData());
    auto& page = doc.GetPages().GetPageAt(0);
    const double pageHeight = page.GetMediaBox().Height;   // 792
    auto& annos = page.GetAnnotations();

    // Find the Line-subtype measurement annotation.
    PoDoFo::PdfDictionary* line = nullptr;
    for (unsigned i = 0; i < annos.GetCount(); ++i) {
        auto& a = annos.GetAnnotAt(i);
        const PoDoFo::PdfObject* sub = a.GetDictionary().FindKey("Subtype");
        if (sub && sub->IsName() && std::string(sub->GetName().GetString()) == "Line")
            line = &a.GetDictionary();
    }
    QVERIFY2(line, "no /Line measurement annotation in saved file");

    // /IT dimension intent (PDF 1.7, Table 175).
    const PoDoFo::PdfObject* it = line->FindKey("IT");
    QVERIFY(it && it->IsName());
    QCOMPARE(std::string(it->GetName().GetString()), std::string("LineDimension"));

    // /Measure: the VERIFIED ISO 32000-1 schema (NOT /Subtype /R, NOT a /C array).
    const PoDoFo::PdfObject* m = line->FindKey("Measure");
    QVERIFY2(m && m->IsDictionary(), "/Measure dictionary missing");
    const PoDoFo::PdfDictionary& md = m->GetDictionary();
    const PoDoFo::PdfObject* mSub = md.FindKey("Subtype");
    QVERIFY(mSub && mSub->IsName());
    QCOMPARE(std::string(mSub->GetName().GetString()), std::string("RL"));

    const PoDoFo::PdfObject* r = md.FindKey("R");
    QVERIFY(r && r->IsString());
    QCOMPARE(QString::fromUtf8(r->GetString().GetString().data(),
                               int(r->GetString().GetString().size())),
             QStringLiteral("1 mm = 2 pt"));

    // /X[0]: /U (mm), /C 0.5 (real mm per user-space unit), /D 100 precision.
    const PoDoFo::PdfObject* xArr = md.FindKey("X");
    QVERIFY(xArr && xArr->IsArray());
    QCOMPARE(xArr->GetArray().size(), size_t(1));
    QVERIFY(xArr->GetArray()[0].IsDictionary());
    const PoDoFo::PdfDictionary& x0 = xArr->GetArray()[0].GetDictionary();
    const PoDoFo::PdfObject* u = x0.FindKey("U");
    QVERIFY(u && u->IsString());
    QCOMPARE(QString::fromUtf8(u->GetString().GetString().data(),
                               int(u->GetString().GetString().size())),
             QStringLiteral("mm"));
    const PoDoFo::PdfObject* c = x0.FindKey("C");
    QVERIFY(c && c->IsNumberOrReal());
    const double cval = c->GetReal();
    QVERIFY(std::fabs(cval - 0.5) < 1e-9);
    const PoDoFo::PdfObject* prec = x0.FindKey("D");
    QVERIFY(prec && prec->IsNumber());
    QCOMPARE(prec->GetNumber(), int64_t(100));

    // /D and /A arrays exist (required by Table 262), factors 1 in-family.
    for (const char* key : { "D", "A" }) {
        const PoDoFo::PdfObject* arr = md.FindKey(key);
        QVERIFY2(arr && arr->IsArray(), key);
        QCOMPARE(arr->GetArray().size(), size_t(1));
        QVERIFY(arr->GetArray()[0].IsDictionary());
        const PoDoFo::PdfObject* fc = arr->GetArray()[0].GetDictionary().FindKey("C");
        QVERIFY(fc && fc->IsNumberOrReal());
        const double v = fc->GetReal();
        QVERIFY(std::fabs(v - 1.0) < 1e-12);
    }
    const PoDoFo::PdfObject* a0u = md.FindKey("A");
    QVERIFY(a0u && a0u->GetArray()[0].IsDictionary());
    const PoDoFo::PdfObject* au = a0u->GetArray()[0].GetDictionary().FindKey("U");
    QVERIFY(au && au->IsString());
    QCOMPARE(QString::fromUtf8(au->GetString().GetString().data(),
                               int(au->GetString().GetString().size())),
             QStringLiteral("mm\u00B2"));

    // /L geometry in PDF user space (y flipped: 792 − 10 = 782).
    const PoDoFo::PdfObject* l = line->FindKey("L");
    QVERIFY(l && l->IsArray() && l->GetArray().size() == 4);
    QVERIFY(std::fabs(l->GetArray()[0].GetReal() - 10.0) < 1e-6);
    QVERIFY(std::fabs(l->GetArray()[1].GetReal() - (pageHeight - 10.0)) < 1e-6);
    QVERIFY(std::fabs(l->GetArray()[2].GetReal() - 82.0) < 1e-6);
    Q_UNUSED(pageHeight);
}

void TestMeasureRoundTrip::perimeterAndAreaRoundTrip()
{
    const QString out = saveCalibratedDoc();
    PoDoFoBackend backend;
    const QList<AnnotationItem> back = backend.extractAnnotations(out);

    const AnnotationItem* p = findByMode(back, ToolMode::MeasurePerimeter);
    QVERIFY2(p, "perimeter measurement must survive as MeasurePerimeter");
    QCOMPARE(p->points.size(), 4);
    QVERIFY(std::fabs(p->measureUnitsPerPt - 0.5) < 1e-9);
    // 4 × 72 pt = 288 pt → 144 mm at 0.5 mm/pt.
    QVERIFY(std::fabs(convertLengthPt(closedPerimeter(p->points),
                      scaleFrom(p->measureUnitsPerPt, p->measureUnit, p->measureCalibrated, p->measureRatio)) - 144.0) < 1e-6);

    const AnnotationItem* a = findByMode(back, ToolMode::MeasureArea);
    QVERIFY2(a, "area measurement must survive as MeasureArea");
    QCOMPARE(a->points.size(), 6);
    // Non-convex L-shape: 64 pt² → 16 mm² (squared factor!). A linear mistake
    // would report 32; a convexified geometry would report more.
    QVERIFY(std::fabs(polygonArea(a->points) - 64.0) < 1e-6);
    QVERIFY(std::fabs(convertAreaPt2(polygonArea(a->points),
                     scaleFrom(a->measureUnitsPerPt, a->measureUnit, a->measureCalibrated, a->measureRatio)) - 16.0) < 1e-6);
}

void TestMeasureRoundTrip::savedRectEnclosesGeometryAndHitTests()
{
    // G21: /Rect in the SAVED BYTES is a real bounding box (plus a half-stroke
    // appearance pad), never the 0×0 last-point rect the old QRectF union of
    // point-sized rects produced ("/Rect [10 710 10 710]" for a 72×72 square).
    const QString out = saveCalibratedDoc();
    PoDoFo::PdfMemDocument doc;
    doc.Load(out.toUtf8().constData());
    auto& page = doc.GetPages().GetPageAt(0);
    const double H = page.GetMediaBox().Height;   // 792
    auto& annos = page.GetAnnotations();

    struct Expect {
        const char* subtype;
        QList<QPointF> pdfPts;   // serialized geometry in PDF user space
        double maxW, maxH;       // logical extent + stroke pad slack
    };
    const Expect expects[] = {
        { "Line",     { {10, H - 10}, {82, H - 10} },                       74.0,  4.0 },
        { "PolyLine", { {10, H - 10}, {82, H - 10}, {82, H - 82}, {10, H - 82} }, 74.0, 74.0 },
        { "Polygon",  { {0, H}, {10, H}, {10, H - 4},
                        {4, H - 4}, {4, H - 10}, {0, H - 10} },             12.0, 12.0 },
    };
    for (const auto& e : expects) {
        bool found = false;
        for (unsigned i = 0; i < annos.GetCount(); ++i) {
            const PoDoFo::PdfDictionary& d = annos.GetAnnotAt(i).GetDictionary();
            const PoDoFo::PdfObject* sub = d.FindKey("Subtype");
            if (!sub || !sub->IsName()
                || std::string(sub->GetName().GetString()) != e.subtype)
                continue;
            found = true;
            double r[4];
            QVERIFY2(rawRect(d, r), "saved annotation has a usable /Rect");
            const double x0 = r[0], y0 = r[1], x1 = r[2], y1 = r[3];
            // Nonzero area — a zero-area rect makes the annot uncullable,
            // unhit-testable and invisible to bounds-based selection.
            QVERIFY2(x1 - x0 > 0.0, "G21: /Rect width must be positive");
            QVERIFY2(y1 - y0 > 0.0, "G21: /Rect height must be positive");
            // Every serialized point lies INSIDE the rect …
            for (const auto& p : e.pdfPts) {
                QVERIFY2(p.x() >= x0 - 1e-6 && p.x() <= x1 + 1e-6, "G21: /Rect must contain geometry (x)");
                QVERIFY2(p.y() >= y0 - 1e-6 && p.y() <= y1 + 1e-6, "G21: /Rect must contain geometry (y)");
            }
            // … without the rect being absurdly oversized (≤ extent + pad).
            QVERIFY(x1 - x0 <= e.maxW + 1e-6);
            QVERIFY(y1 - y0 <= e.maxH + 1e-6);
        }
        QVERIFY2(found, e.subtype);
    }
}

void TestMeasureRoundTrip::serializedPerimeterPathLengthMatchesLabel()
{
    // G22: the perimeter tool measures the CLOSED boundary, so the SERIALIZED
    // path — the only thing a second reader can measure — must encode the
    // closing edge (first vertex repeated last). Label, stored vertices and
    // path traversal must agree; verified from the bytes, not just our helper.
    const QString out = saveCalibratedDoc();
    PoDoFo::PdfMemDocument doc;
    doc.Load(out.toUtf8().constData());
    auto& page = doc.GetPages().GetPageAt(0);
    const double H = page.GetMediaBox().Height;
    auto& annos = page.GetAnnotations();

    bool found = false;
    for (unsigned i = 0; i < annos.GetCount(); ++i) {
        const PoDoFo::PdfDictionary& d = annos.GetAnnotAt(i).GetDictionary();
        const PoDoFo::PdfObject* sub = d.FindKey("Subtype");
        if (!sub || !sub->IsName()
            || std::string(sub->GetName().GetString()) != "PolyLine")
            continue;
        found = true;
        const PoDoFo::PdfObject* v = d.FindKey("Vertices");
        QVERIFY2(v && v->IsArray(), "PolyLine carries /Vertices");
        QList<QPointF> serialized;
        const auto& arr = v->GetArray();
        for (size_t k = 0; k + 1 < arr.size(); k += 2)
            serialized.append(QPointF(arr[k].GetReal(), H - arr[k + 1].GetReal()));
        QCOMPARE(serialized.size(), 5);   // 4 corners + the closing vertex
        QVERIFY(std::fabs(serialized.first().x() - serialized.last().x()) < 1e-9);
        QVERIFY(std::fabs(serialized.first().y() - serialized.last().y()) < 1e-9);
        // Walk the stored path: 4 × 72 pt = 288 pt ⇒ ×0.5 mm/pt = 144 mm.
        const double walked = serializedPathLength(serialized);
        QVERIFY(std::fabs(walked - 288.0) < 1e-6);
        QVERIFY(std::fabs(convertLengthPt(walked, scaleFrom(0.5, QStringLiteral("mm"),
                                                            true, QStringLiteral("1 mm = 2 pt")))
                          - 144.0) < 1e-6);
        // The persisted /Contents snapshot states the SAME number.
        const PoDoFo::PdfObject* c = d.FindKey("Contents");
        QVERIFY(c && c->IsString());
        QCOMPARE(QString::fromUtf8(c->GetString().GetString().data(),
                                   int(c->GetString().GetString().size())),
                 QStringLiteral("144.00 mm"));
    }
    QVERIFY2(found, "PolyLine perimeter annotation present");
}

void TestMeasureRoundTrip::inkStrokePersistsUsableRect()
{
    // G21's shared branch: the same bounds expression predates measurements in
    // the /InkList (freehand/signature) path — a stroke must also persist a
    // usable /Rect, not a 0×0 point.
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString seed = tmp.filePath("seed4.pdf");
    QVERIFY(writeSeed(seed));
    AnnotationItem a;
    a.mode = ToolMode::DrawFreehand;
    a.pageIndex = 0;
    a.points = { QPointF(20, 20), QPointF(60, 25), QPointF(100, 60), QPointF(30, 90) };
    a.color = Qt::blue;
    PoDoFoBackend backend;
    const QString out = tmp.filePath("ink.pdf");
    QVERIFY(backend.embedAnnotations(seed, out, { a }));

    const QList<AnnotationItem> back = backend.extractAnnotations(out);
    QCOMPARE(back.size(), 1);
    QCOMPARE(back.first().mode, ToolMode::DrawFreehand);
    QCOMPARE(back.first().points.size(), 4);
    // The read-back rect (parsed from /Rect) covers the stroke's extents.
    QVERIFY(back.first().rect.width() > 78.0);    // x extent 20..100
    QVERIFY(back.first().rect.height() > 68.0);   // y extent 20..90

    // RAW: /Rect nonzero and contains every InkList point (y-flipped).
    PoDoFo::PdfMemDocument doc;
    doc.Load(out.toUtf8().constData());
    const PoDoFo::PdfDictionary& d =
        doc.GetPages().GetPageAt(0).GetAnnotations().GetAnnotAt(0).GetDictionary();
    double r[4];
    QVERIFY2(rawRect(d, r), "ink annotation has a usable /Rect");
    QVERIFY(r[2] - r[0] > 0.0 && r[3] - r[1] > 0.0);
    for (const auto& p : a.points) {
        QVERIFY(p.x() >= r[0] - 1e-6 && p.x() <= r[2] + 1e-6);
        QVERIFY(792.0 - p.y() >= r[1] - 1e-6 && 792.0 - p.y() <= r[3] + 1e-6);
    }
}

void TestMeasureRoundTrip::pdfiumReadPathSeesMeasureAnnots()
{
#ifdef HAS_PDFIUM
    const QString out = saveCalibratedDoc();
    PdfiumEnvironment env;   // FPDF_InitLibrary scoped
    FPDF_DOCUMENT doc = FPDF_LoadDocument(out.toUtf8().constData(), nullptr);
    QVERIFY2(doc, "PDFium could not open the saved document");
    FPDF_PAGE page = FPDF_LoadPage(doc, 0);
    QVERIFY(page);

    QCOMPARE(FPDFPage_GetAnnotCount(page), 3);

    bool sawLine = false, sawPolyline = false, sawPolygon = false;
    for (int i = 0; i < 3; ++i) {
        FPDF_ANNOTATION annot = FPDFPage_GetAnnot(page, i);
        QVERIFY(annot);
        const FPDF_ANNOTATION_SUBTYPE st = FPDFAnnot_GetSubtype(annot);
        if (st == FPDF_ANNOT_LINE) sawLine = true;
        if (st == FPDF_ANNOT_POLYLINE) sawPolyline = true;
        if (st == FPDF_ANNOT_POLYGON) sawPolygon = true;

        // Every measurement annot must carry /Measure (a DIFFERENT engine
        // confirming the dict exists in the saved bytes).
        QCOMPARE(FPDFAnnot_HasKey(annot, "Measure"), 1);

        if (st == FPDF_ANNOT_LINE) {
            FS_POINTF start{}, end{};
            QVERIFY(FPDFAnnot_GetLine(annot, &start, &end));
            QVERIFY(std::fabs(start.x - 10.0) < 1e-3);
            QVERIFY(std::fabs(start.y - 782.0) < 1e-3);   // 792 − 10
            QVERIFY(std::fabs(end.x - 82.0) < 1e-3);
        }
        // G21 through a SECOND engine: the rect PDFium exposes to embedders
        // (what PDFium-based viewers hit-test and cull against) is a real box
        // around the geometry. PDFium has no public hit-test call, so point-in-
        // rect against FPDFAnnot_GetRect IS the embedder hit-test contract.
        FS_RECTF box{};
        QVERIFY(FPDFAnnot_GetRect(annot, &box));
        QVERIFY2(box.right - box.left > 0.0, "G21: embedder rect must have width");
        QVERIFY2(box.top - box.bottom > 0.0, "G21: embedder rect must have height");
        if (st == FPDF_ANNOT_POLYLINE) {
            // The square's center is inside the annot rect (the pre-fix 0×0
            // rect missed every interior point — a dead hit-test).
            QVERIFY(box.left <= 46.0 && box.right >= 46.0);
            QVERIFY(box.bottom <= 746.0 && box.top >= 746.0);

            // G22 through a SECOND engine: 4 logical corners + the serialized
            // closing vertex, and the path IT reports walks out to exactly the
            // displayed perimeter (288 pt ⇒ 144 mm at 0.5 mm/pt).
            const unsigned long n = FPDFAnnot_GetVertices(annot, nullptr, 0);
            QCOMPARE(n, 5ul);
            std::vector<FS_POINTF> v(n);
            QCOMPARE(FPDFAnnot_GetVertices(annot, v.data(), n), n);
            double walked = 0.0;
            for (unsigned long k = 1; k < n; ++k) {
                const double dx = v[k].x - v[k - 1].x;
                const double dy = v[k].y - v[k - 1].y;
                walked += std::sqrt(dx * dx + dy * dy);
            }
            QVERIFY(std::fabs(walked - 288.0) < 1e-3);
            QVERIFY(std::fabs(walked * 0.5 - 144.0) < 1e-3);
        }
        if (st == FPDF_ANNOT_POLYGON)  QCOMPARE(FPDFAnnot_GetVertices(annot, nullptr, 0), 6ul);
        FPDFPage_CloseAnnot(annot);
    }
    QVERIFY(sawLine && sawPolyline && sawPolygon);

    FPDF_ClosePage(page);
    FPDF_CloseDocument(doc);
#else
    QSKIP("built without PDFium");
#endif
}

void TestMeasureRoundTrip::uncalibratedMeasurePersistsTruthfulPt()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString seed = tmp.filePath("seed2.pdf");
    QVERIFY(writeSeed(seed));

    AnnotationItem a;                       // defaults = uncalibrated pt
    a.mode = ToolMode::MeasureDistance;
    a.pageIndex = 0;
    a.points = { QPointF(5, 5), QPointF(40, 5) };
    a.rect = QRectF(5, 5, 35, 0);

    PoDoFoBackend backend;
    const QString out = tmp.filePath("uncal.pdf");
    QVERIFY(backend.embedAnnotations(seed, out, { a }));

    // 1. GlyphPDF read path: still a measurement, truthfully uncalibrated.
    const QList<AnnotationItem> back = backend.extractAnnotations(out);
    QCOMPARE(back.size(), 1);
    QCOMPARE(back.first().mode, ToolMode::MeasureDistance);
    QVERIFY(!back.first().measureCalibrated);
    QVERIFY(std::fabs(back.first().measureUnitsPerPt - 1.0) < 1e-12);
    QCOMPARE(back.first().measureUnit, QStringLiteral("pt"));
    // The negative-control readout: pt AND it says so.
    const QString readout = formatLengthTruthful(
        polylineLength(back.first().points), ptScale());
    QVERIFY(readout.contains(QStringLiteral("pt")));
    QVERIFY(readout.contains(QStringLiteral("not calibrated")));

    // 2. Raw dict: /Measure exists with the honest C=1 (pt) scale.
    PoDoFo::PdfMemDocument doc;
    doc.Load(out.toUtf8().constData());
    auto& annos = doc.GetPages().GetPageAt(0).GetAnnotations();
    QVERIFY(annos.GetCount() >= 1);
    PoDoFo::PdfDictionary* d = &annos.GetAnnotAt(0).GetDictionary();
    const PoDoFo::PdfObject* m = d->FindKey("Measure");
    QVERIFY(m && m->IsDictionary());
    const PoDoFo::PdfObject* x0 = m->GetDictionary().FindKey("X");
    QVERIFY(x0 && x0->IsArray() && x0->GetArray().size() == 1);
    QVERIFY(x0->GetArray()[0].IsDictionary());
    const PoDoFo::PdfObject* c = x0->GetArray()[0].GetDictionary().FindKey("C");
    QVERIFY(c && c->IsNumberOrReal());
    const double cval = c->GetReal();
    QVERIFY(std::fabs(cval - 1.0) < 1e-12);
}

void TestMeasureRoundTrip::plainLineWithoutMeasureStaysDrawLine()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString seed = tmp.filePath("seed3.pdf");
    QVERIFY(writeSeed(seed));

    AnnotationItem a;                       // §9.3 plain line, no measurement
    a.mode = ToolMode::DrawLine;
    a.pageIndex = 0;
    a.points = { QPointF(1, 1), QPointF(50, 50) };
    a.rect = QRectF(1, 1, 49, 49);

    PoDoFoBackend backend;
    const QString out = tmp.filePath("line.pdf");
    QVERIFY(backend.embedAnnotations(seed, out, { a }));

    const QList<AnnotationItem> back = backend.extractAnnotations(out);
    QCOMPARE(back.size(), 1);
    QCOMPARE(back.first().mode, ToolMode::DrawLine);

    // Regression guard: the writer must not dress a plain line as a measurement.
    PoDoFo::PdfMemDocument doc;
    doc.Load(out.toUtf8().constData());
    auto& d = doc.GetPages().GetPageAt(0).GetAnnotations().GetAnnotAt(0).GetDictionary();
    QVERIFY(d.FindKey("Measure") == nullptr);
    QVERIFY(d.FindKey("IT") == nullptr);
}

QTEST_MAIN(TestMeasureRoundTrip)
#include "TestMeasureRoundTrip.moc"
