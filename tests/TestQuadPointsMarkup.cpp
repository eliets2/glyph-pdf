// SPDX-License-Identifier: Apache-2.0
// PARITY-SCORECARD-2026-09-30 §4 row 1 (July audit §9.3 P1 row 15): QuadPoints
// text-anchored markup. A free-rectangle highlight silently covers blank space
// and misses wrapped lines; text-anchored Highlight/Underline/Strikeout/
// Squiggly carry one 8-value quad PER TEXT LINE (ISO 32000 §12.5.6.10: corners
// in the order lower-left, lower-right, upper-right, upper-left; quads in
// reading order — top line first) so the markup hugs the actual glyphs, in our
// own saves AND in Acrobat-style documents from other viewers.
//
// Pins (TestShapeInkPersistence pattern: hand-crafted seed PDF, embed,
// extract, assert on the saved artifact):
//   1. writer        — a 2-line selection serializes TWO quads matching the
//                      line rects (top line first); a rect-only item writes NO
//                      QuadPoints key (free-rect markup works unchanged).
//   2. round-trip    — save→reload preserves the quads, both directions,
//                      including an externally-crafted Acrobat-shaped fixture.
//   3. rendering     — paintShape draws the quads over the actual line rects,
//                      NOT over the union rect's blank corners.
//   4. seam          — TextMatchFinder reports per-line rects for a match that
//                      spans a wrapped line (the placement seam).
//   5. sidecar       — the .ann sidecar round-trips quads (pending-embed work
//                      survives an app restart before the PDF is saved).
#include <QtTest/QtTest>
#include <QImage>
#include <QPainter>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <podofo/podofo.h>

#include "core/AnnotationTypes.h"
#include "core/AnnotationSerializer.h"
#include "engines/TextMatchFinder.h"
#include "engines/podofo/PoDoFoBackend.h"
#include "ui/AnnotationLayer.h"

namespace {

// A Letter seed page (MediaBox 0 0 612 792, /Rotate 0) — same pattern as
// TestShapeInkPersistence, so viewer space == user space flipped at y=792.
void writeSeedPdf(const QTemporaryDir& tmp, const QString& name, QString& path)
{
    path = tmp.filePath(name);
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(
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
        "startxref\n183\n%%EOF\n");
    f.close();
}

// The two display-space line rects of a wrapped 2-line selection (top first).
QList<QRectF> twoLineSelection()
{
    return { QRectF(50, 100, 200, 14), QRectF(50, 140, 200, 14) };
}

AnnotationItem highlightItem(int pageIndex, const QList<QRectF>& quads)
{
    AnnotationItem item;
    item.mode = ToolMode::Highlight;
    item.pageIndex = pageIndex;
    item.quads = quads;
    QRectF u;
    for (const QRectF& q : quads)
        u = u.isNull() ? q : u.united(q);
    item.rect = u;
    item.color = Qt::yellow;
    return item;
}

// ISO 32000 §12.5.6.10 quad corner order per line: lower-left, lower-right,
// upper-right, upper-left. On the /Rotate 0 seed page the display flip is
// y' = 792 - y, so display line y∈[100,114] becomes user y∈[678,692].
QVector<double> expectedTwoLineQuadPoints()
{
    return {
        50, 678, 250, 678, 250, 692, 50, 692,   // line 1 (top line first)
        50, 638, 250, 638, 250, 652, 50, 652    // line 2
    };
}

bool sameRect(const QRectF& a, const QRectF& b, double eps = 1e-6)
{
    return qAbs(a.x() - b.x()) < eps && qAbs(a.y() - b.y()) < eps
        && qAbs(a.width() - b.width()) < eps && qAbs(a.height() - b.height()) < eps;
}

// Assemble a raw PDF whose xref offsets are computed from the object bodies —
// the externally-crafted (Acrobat-shaped) fixture must not depend on our own
// writer to exist.
void writeForeignHighlightPdf(const QTemporaryDir& tmp, const QString& name,
                              QString& path)
{
    QByteArray body =
        "1 0 obj<</Type/Catalog/Pages 2 0 R>>endobj\n"
        "2 0 obj<</Type/Pages/Kids[3 0 R]/Count 1>>endobj\n"
        "3 0 obj<</Type/Page/Parent 2 0 R/MediaBox[0 0 612 792]/Annots[4 0 R]>>endobj\n"
        "4 0 obj<</Type/Annot/Subtype/Highlight/Rect[50 638 250 692]"
        "/QuadPoints[50 678 250 678 250 692 50 692 50 638 250 638 250 652 50 652]"
        "/C[1 0 0]>>endobj\n";
    QByteArray pdf = "%PDF-1.4\n" + body;
    QList<int> offsets;
    for (int obj = 1; obj <= 4; ++obj) {
        const QByteArray key = QByteArray::number(obj) + " 0 obj";
        const int off = pdf.indexOf(key);
        QVERIFY2(off > 0, "fixture object not found");
        offsets.append(off);
    }
    const int xrefOffset = pdf.size();
    QByteArray xref = "xref\n0 5\n0000000000 65535 f \n";
    for (int off : offsets)
        xref += QString("%1 00000 n \n").arg(off, 10, 10, QChar('0')).toLatin1();
    xref += "trailer<</Size 5/Root 1 0 R>>\nstartxref\n"
            + QByteArray::number(xrefOffset) + "\n%%EOF\n";
    pdf += xref;

    path = tmp.filePath(name);
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(pdf);
    f.close();
}

} // namespace

class TestQuadPointsMarkup : public QObject {
    Q_OBJECT

private slots:
    // ── Pin 1a: the writer ─────────────────────────────────────────────────
    // A 2-line wrapped selection produces /QuadPoints with 2 quads whose
    // coordinates match the line rects (top line first). RED before the
    // writer: the annotation serializes rect-only (no QuadPoints key).
    void writerEmitsQuadPointsPerLine();

    // ── Guard: free-rect markup unchanged ──────────────────────────────────
    // An item WITHOUT line quads must keep serializing exactly as before —
    // no QuadPoints key.
    void rectOnlyMarkupWritesNoQuadPoints();

    // ── Pin 1b: round-trip ─────────────────────────────────────────────────
    void quadPointsRoundTripPreservesLines();
    void foreignAcrobatQuadPointsFixtureLoads();
    void doubleRoundTripIsStable();

    // ── Pin 1c: rendering ──────────────────────────────────────────────────
    // The quads render over the actual glyphs' lines — the union rect's blank
    // band between the two lines stays unpainted.
    void paintDrawsQuadsNotUnionBlank();

    // ── Placement seam: per-line rects from the text layer ─────────────────
    void wrappedMatchReportsPerLineRects();

    // ── Sidecar (.ann) round-trip ──────────────────────────────────────────
    void sidecarRoundTripPreservesQuads();
};

void TestQuadPointsMarkup::writerEmitsQuadPointsPerLine()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QString seed; writeSeedPdf(tmp, "seed.pdf", seed);
    const QString out = tmp.filePath("quad.pdf");

    PoDoFoBackend backend;
    QVERIFY(backend.embedAnnotations(seed, out,
                                     { highlightItem(0, twoLineSelection()) }));

    // Inspect the SAVED artifact directly (the pin, not our own model).
    PoDoFo::PdfMemDocument doc;
    doc.Load(out.toUtf8().constData());
    auto& page = doc.GetPages().GetPageAt(0);
    QCOMPARE(page.GetAnnotations().GetCount(), 1u);
    auto& annot = page.GetAnnotations().GetAnnotAt(0);
    const PoDoFo::PdfDictionary& dict = annot.GetDictionary();

    const auto* qp = dict.FindKey("QuadPoints");
    QVERIFY2(qp != nullptr, "writer must serialize /QuadPoints for a "
                            "text-anchored highlight (got rect-only)");
    QVERIFY(qp->IsArray());
    const auto& arr = qp->GetArray();
    QCOMPARE(static_cast<int>(arr.size()), 16);

    const QVector<double> expected = expectedTwoLineQuadPoints();
    for (int i = 0; i < 16; ++i) {
        QVERIFY2(arr[static_cast<size_t>(i)].IsNumberOrReal(),
                 qPrintable(QStringLiteral("QuadPoints[%1] not a number").arg(i)));
        QVERIFY2(qAbs(arr[static_cast<size_t>(i)].GetReal() - expected[i]) < 1e-6,
                 qPrintable(QStringLiteral("QuadPoints[%1]=%2 expected %3")
                                .arg(i)
                                .arg(arr[static_cast<size_t>(i)].GetReal())
                                .arg(expected[i])));
    }
}

void TestQuadPointsMarkup::rectOnlyMarkupWritesNoQuadPoints()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QString seed; writeSeedPdf(tmp, "seed.pdf", seed);
    const QString out = tmp.filePath("rectonly.pdf");

    PoDoFoBackend backend;
    QVERIFY(backend.embedAnnotations(seed, out,
                                     { highlightItem(0, {}) }));

    PoDoFo::PdfMemDocument doc;
    doc.Load(out.toUtf8().constData());
    auto& page = doc.GetPages().GetPageAt(0);
    QCOMPARE(page.GetAnnotations().GetCount(), 1u);
    const PoDoFo::PdfDictionary& dict =
        page.GetAnnotations().GetAnnotAt(0).GetDictionary();
    QVERIFY2(dict.FindKey("QuadPoints") == nullptr,
             "a rect-only (drag) markup must NOT gain a QuadPoints key — "
             "free-rect markup works unchanged");
}

void TestQuadPointsMarkup::quadPointsRoundTripPreservesLines()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QString seed; writeSeedPdf(tmp, "seed.pdf", seed);
    const QString out = tmp.filePath("roundtrip.pdf");

    PoDoFoBackend backend;
    QVERIFY(backend.embedAnnotations(seed, out,
                                     { highlightItem(0, twoLineSelection()) }));

    const QList<AnnotationItem> back = backend.extractAnnotations(out);
    QCOMPARE(back.size(), 1);
    QCOMPARE(back.first().mode, ToolMode::Highlight);
    // QVERIFY (returns on failure), not QCOMPARE (records and CONTINUES) —
    // the indexing below must stay in-bounds on a RED run.
    QVERIFY2(back.first().quads.size() == 2,
             "round-trip must preserve BOTH line quads");
    QVERIFY(sameRect(back.first().quads[0], twoLineSelection()[0]));
    QVERIFY(sameRect(back.first().quads[1], twoLineSelection()[1]));
}

void TestQuadPointsMarkup::foreignAcrobatQuadPointsFixtureLoads()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QString path; writeForeignHighlightPdf(tmp, "foreign.pdf", path);

    PoDoFoBackend backend;
    const QList<AnnotationItem> back = backend.extractAnnotations(path);
    QCOMPARE(back.size(), 1);
    QCOMPARE(back.first().mode, ToolMode::Highlight);
    // QVERIFY (returns on failure), not QCOMPARE — the indexing below must
    // stay in-bounds on a RED run.
    QVERIFY2(back.first().quads.size() == 2,
             "the two fixture quads must load as line quads, not a union rect");
    // The two fixture quads map back to display lines y∈[100,114] and
    // y∈[140,154] — NOT a single union rect [100,154].
    QVERIFY(sameRect(back.first().quads[0], QRectF(50, 100, 200, 14)));
    QVERIFY(sameRect(back.first().quads[1], QRectF(50, 140, 200, 14)));
}

void TestQuadPointsMarkup::doubleRoundTripIsStable()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QString seed; writeSeedPdf(tmp, "seed.pdf", seed);
    const QString out1 = tmp.filePath("rt1.pdf");
    const QString out2 = tmp.filePath("rt2.pdf");

    PoDoFoBackend backend;
    QVERIFY(backend.embedAnnotations(seed, out1,
                                     { highlightItem(0, twoLineSelection()) }));
    const QList<AnnotationItem> first = backend.extractAnnotations(out1);
    QCOMPARE(first.size(), 1);
    // Second generation: the RELOADED items saved again. applyAnnotationsToDoc
    // is additive (the annots already inside out1 must not be duplicated), so
    // generation 2 embeds onto a fresh seed — save→reload→save→reload, both
    // directions, starting from the reloaded model.
    QString seed2; writeSeedPdf(tmp, "seed2.pdf", seed2);
    PoDoFoBackend backend2;   // AR-4 D2: one backend instance per lineage
    QVERIFY(backend2.embedAnnotations(seed2, out2, first));
    const QList<AnnotationItem> second = backend2.extractAnnotations(out2);
    QCOMPARE(second.size(), 1);

    // QVERIFY (returns on failure), not QCOMPARE — the indexing below must
    // stay in-bounds on a RED run (writer reverted ⇒ quads empty).
    QVERIFY2(second.first().quads.size() == 2,
             "second generation must still carry BOTH line quads");
    QCOMPARE(second.first().quads.size(), first.first().quads.size());
    for (int i = 0; i < first.first().quads.size(); ++i)
        QVERIFY(sameRect(second.first().quads[i], first.first().quads[i]));
    // And still the ORIGINAL selection, not a drifted approximation.
    QVERIFY(sameRect(second.first().quads[0], twoLineSelection()[0]));
    QVERIFY(sameRect(second.first().quads[1], twoLineSelection()[1]));
}

void TestQuadPointsMarkup::paintDrawsQuadsNotUnionBlank()
{
    const QList<QRectF> lines = { QRectF(10, 10, 100, 12), QRectF(10, 40, 100, 12) };
    AnnotationItem item = highlightItem(0, lines);   // rect = union [10,10..110,52]

    QImage img(200, 200, QImage::Format_RGB32);
    img.fill(Qt::white);
    QPainter p(&img);
    AnnotationLayer::paintShape(p, item);
    p.end();

    // Inside line 2's quad: highlight yellow (alpha-100 over white kills the
    // blue channel: blended b ≈ 155).
    const QColor onGlyph = img.pixelColor(60, 46);
    QVERIFY2(onGlyph.blue() < 200,
             qPrintable(QStringLiteral("line quad not painted: rgb(%1,%2,%3)")
                            .arg(onGlyph.red()).arg(onGlyph.green())
                            .arg(onGlyph.blue())));
    // Inside the UNION rect but inside NO quad (the blank band between the
    // wrapped lines): must stay background — the old union fill painted it.
    const QColor blank = img.pixelColor(60, 31);
    QVERIFY2(blank.blue() > 240,
             qPrintable(QStringLiteral("union blank band painted: rgb(%1,%2,%3)")
                            .arg(blank.red()).arg(blank.green()).arg(blank.blue())));
}

void TestQuadPointsMarkup::wrappedMatchReportsPerLineRects()
{
    // A match spanning a line break: "Heading" and "Body" on separate text
    // lines; the regex crosses the extractor's newline. The seam reports one
    // rect PER LINE (2), not the union (1).
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = tmp.filePath("wrapped.pdf");
    try {
        PoDoFo::PdfMemDocument doc;
        auto& page = doc.GetPages().CreatePage(
            PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
        PoDoFo::PdfPainter painter;
        painter.SetCanvas(page);
        auto& font = doc.GetFonts().GetStandard14Font(
            PoDoFo::PdfStandard14FontType::Helvetica);
        painter.TextState.SetFont(font, 12.0);
        painter.DrawText("Heading", 50, 700);
        painter.DrawText("Body", 50, 650);
        painter.FinishDrawing();
        doc.Save(pdf.toUtf8().constData());
    } catch (const std::exception& e) {
        QFAIL(qPrintable(QStringLiteral("fixture build failed: %1").arg(e.what())));
    }

    QRegularExpression rx(QStringLiteral("Head[\\s\\S]*Body"));
    rx.setPatternOptions(QRegularExpression::CaseInsensitiveOption
                         | QRegularExpression::DotMatchesEverythingOption);
    const QList<TextMatch> matches =
        TextMatchFinder::findMatches(pdf, { 0 }, rx);
    QCOMPARE(matches.size(), 1);
    // RED before the seam: lineRects is empty (union-only matches).
    QCOMPARE(matches.first().lineRects.size(), 2);
    // The line rects are the real geometry: disjoint vertical bands, both
    // inside the union rect.
    const QRectF l0 = matches.first().lineRects[0];
    const QRectF l1 = matches.first().lineRects[1];
    QVERIFY(!l0.isNull() && !l1.isNull());
    QVERIFY(!l0.intersects(l1));
    QVERIFY(matches.first().rect.contains(l0));
    QVERIFY(matches.first().rect.contains(l1));
}

void TestQuadPointsMarkup::sidecarRoundTripPreservesQuads()
{
    const QList<AnnotationItem> items = { highlightItem(0, twoLineSelection()) };
    const QJsonDocument doc = AnnotationSerializer::toJson(items);
    const QList<AnnotationItem> back = AnnotationSerializer::fromJson(doc);

    QCOMPARE(back.size(), 1);
    QCOMPARE(back.first().quads.size(), 2);
    QVERIFY(sameRect(back.first().quads[0], twoLineSelection()[0]));
    QVERIFY(sameRect(back.first().quads[1], twoLineSelection()[1]));
}

QTEST_MAIN(TestQuadPointsMarkup)
#include "TestQuadPointsMarkup.moc"
