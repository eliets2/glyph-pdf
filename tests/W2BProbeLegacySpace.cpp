// SPDX-License-Identifier: MIT
// W2BProbeLegacySpace.cpp — SWEEP-W2B INDEPENDENT verifier probe
//                          (SL1 page-space law + SL3 suggestion placement).
//
// Written by the W2B guarantee-verification-engine against the SAVED tip
// (feat/sweep-w2-verify-b @ 2d29a16). The brief mandates a fixture of my own
// through the REAL edit path (PoDoFoBackend::embedAnnotations/extractAnnotations
// and FormManager::addTextField/updateFieldRect) — different page shapes,
// different display rects and DIFFERENT hardcoded literals than the fix lane's
// TestLegacyOriginSpace, computed by hand from the law's published table
// (PageSpaceTransform.h): viewer→user for /Rotate 90 is ux=x0+vy, uy=y0+vx.
//
// My fixture: 3 pages
//   page 0: Letter [0 0 612 792] /Rotate 0    (plain control)
//   page 1: [0 200 612 842]     /Rotate 90    (offset origin + rotated)
//   page 2: Letter [0 0 612 792] /Rotate 270  (opposite rotation)
// drawn display rect (60, 80, 100 x 50) on every page. Expected RAW /Rect:
//   page 0: [60 662 160 712]   (ux=60..160, uy=792-130..792-80)
//   page 1: [80 260 130 360]   (ux=80..130,  uy=200+60..200+160)
//   page 2: [482 632 532 732]  (ux=612-130..612-80, uy=792-160..792-60)
// Every value is verified through BOTH independent read paths: raw /Rect
// dictionary arrays (PoDoFo, raw accessor semantics) and PDFium
// FPDFAnnot_GetRect (second engine, page user space).
//
// SL3: a "Amount: " label at user (90, 640) on a Letter page must yield a
// suggestion in DISPLAY space at y≈135.2 (under the label, y-down) — not
// mirrored — and placing it must store the widget /Rect at user y≈640..656.8.
//
// NC bases: SL1 → db18f5e^ (PoDoFoBackend.cpp + FormManager.cpp pre-law)
//           SL3 → 82e661c^ (FormManager.cpp auto-detect pre-map).

#include <QtTest/QtTest>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <cmath>

#include "engines/podofo/PoDoFoBackend.h"
#include "engines/FormManager.h"
#include "core/AnnotationTypes.h"
#include <podofo/podofo.h>

#include <fpdfview.h>
#include <fpdf_annot.h>
#include "engines/pdfium/PdfiumEnvironment.h"

namespace {

constexpr bool rectClose(const QRectF& a, const QRectF& b)
{
    return std::fabs(a.x() - b.x()) < 0.01 && std::fabs(a.y() - b.y()) < 0.01
        && std::fabs(a.right() - b.right()) < 0.01
        && std::fabs(a.bottom() - b.bottom()) < 0.01;
}

QString rectStr(const QRectF& r)
{
    return QString("[%1 %2 %3 %4]").arg(r.left()).arg(r.top())
        .arg(r.right()).arg(r.bottom());
}

// Build MY 3-page fixture (no content; annotations/fields need none).
bool buildFixture(const QString& path)
{
    try {
        PoDoFo::PdfMemDocument doc;
        auto& pages = doc.GetPages();
        PoDoFo::PdfPage& p0 = pages.CreatePage(PoDoFo::Rect(0, 0, 612, 792));
        p0.SetRotation(0);
        PoDoFo::PdfPage& p1 = pages.CreatePage(PoDoFo::Rect(0, 200, 612, 842));
        p1.SetRotation(90);
        PoDoFo::PdfPage& p2 = pages.CreatePage(PoDoFo::Rect(0, 0, 612, 792));
        p2.SetRotation(180);
        PoDoFo::PdfPage& p3 = pages.CreatePage(PoDoFo::Rect(0, 0, 612, 792));
        p3.SetRotation(270);
        doc.Save(path.toUtf8().constData());
        return true;
    } catch (const std::exception& e) {
        qWarning() << "buildFixture failed:" << e.what();
        return false;
    }
}

QList<QRectF> rawAnnotRects(const QString& path, int pageIndex)
{
    QList<QRectF> out;
    try {
        PoDoFo::PdfMemDocument doc;
        doc.Load(path.toUtf8().constData());
        auto& annos = doc.GetPages().GetPageAt(pageIndex).GetAnnotations();
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
            if (annot && FPDFAnnot_GetRect(annot, &box))
                out.append(QRectF(QPointF(box.left, box.bottom),
                                  QPointF(box.right, box.top)));
            if (annot) FPDFPage_CloseAnnot(annot);
        }
        FPDF_ClosePage(page);
    }
    FPDF_CloseDocument(doc);
    return out;
}

// Letter-page PDF with one text line: "BT /F1 12 Tf <x> <y> Td (<label>) Tj ET".
bool buildLabelPdf(const QString& path, int x, int y, const QByteArray& label)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return false;
    QByteArray out = "%PDF-1.4\n";
    QList<qint64> off;
    auto addObj = [&out, &off](const QByteArray& body) {
        off.append(out.size());
        out += QByteArray::number(off.size()) + " 0 obj\n" + body + "\nendobj\n";
    };
    addObj("<</Type/Catalog/Pages 2 0 R>>");
    addObj("<</Type/Pages/Kids[3 0 R]/Count 1>>");
    addObj("<</Type/Page/Parent 2 0 R/MediaBox[0 0 612 792]/Contents 4 0 R/Resources<</Font<</F1 5 0 R>>>>>>");
    const QByteArray stream = "BT /F1 12 Tf " + QByteArray::number(x) + " "
        + QByteArray::number(y) + " Td (" + label + ") Tj ET\n";
    addObj("<</Length " + QByteArray::number(stream.size()) + ">>stream\n" + stream + "endstream");
    addObj("<</Type/Font/Subtype/Type1/BaseFont/Helvetica>>");
    const qint64 xref = out.size();
    out += QString("xref\n0 %1\n").arg(off.size() + 1).toLatin1();
    out += "0000000000 65535 f \n";
    for (qint64 o : off)
        out += QString("%1 00000 n \n").arg(o, 10, 10, QChar('0')).toLatin1();
    out += QString("trailer<</Size %1/Root 1 0 R>>\nstartxref\n%2\n%%EOF\n")
               .arg(off.size() + 1).arg(xref).toLatin1();
    f.write(out);
    f.close();
    return true;
}

} // namespace

class W2BProbeLegacySpace : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<QTemporaryDir> m_dir;

private slots:
    void init()
    {
        m_dir = std::make_unique<QTemporaryDir>(
            QDir::tempPath() + QStringLiteral("/w2bprobe-legspace-XXXXXX"));
        QVERIFY(m_dir->isValid());
        QVERIFY(buildFixture(fixturePath()));
    }
    void cleanup() { m_dir.reset(); }

    QString fixturePath() const { return m_dir->filePath("w2b-origin.pdf"); }
    QString outPath(const char* n) const { return m_dir->filePath(QLatin1String(n)); }

    // ── SL1: the real annotation edit path stores the drawn position ────────
    void annotationEmbedFollowsTheLawOnMyFixture()
    {
        PoDoFoBackend engine;
        QVERIFY(engine.loadDocument(fixturePath()));

        const QRectF drawn(60, 80, 100, 50);    // display space, my numbers
        QList<AnnotationItem> items;
        for (int p = 0; p < 4; ++p) {
            AnnotationItem item;
            item.pageIndex = p;
            item.mode = ToolMode::DrawRectangle;
            item.rect = drawn;
            item.text = QStringLiteral("w2b-probe-%1").arg(p);
            items.append(item);
        }
        const QString out = outPath("embed.pdf");
        QVERIFY(engine.embedAnnotations(fixturePath(), out, items));

        // MY hardcoded literals (hand-computed, NOT via any project header),
        // from the law's published table (PageSpaceTransform.h):
        const QList<QRectF> expected = {
            QRectF(QPointF(60, 662), QPointF(160, 712)),    // page 0: rot 0
            QRectF(QPointF(80, 260), QPointF(130, 360)),    // page 1: rot 90 + offset
            QRectF(QPointF(452, 80), QPointF(552, 130)),    // page 2: rot 180 (law: uy = y0 + vy direct)
            QRectF(QPointF(482, 632), QPointF(532, 732)),   // page 3: rot 270
        };
        // Diagnostics: what does the SAVED page report for box + rotation,
        // what do the FILE BYTES carry, and what does PDFium (independent
        // engine) see for the page geometry?
        {
            QFile f(out);
            if (f.open(QIODevice::ReadOnly)) {
                const QByteArray bytes = f.readAll();
                f.close();
                int idx = 0, pageIdx = 0;
                while ((idx = bytes.indexOf("MediaBox", idx)) >= 0) {
                    qWarning() << "w2b-diag file MediaBox @ page-obj" << pageIdx++
                               << bytes.mid(idx, 40);
                    idx += 8;
                }
                idx = 0;
                int rotIdx = 0;
                while ((idx = bytes.indexOf("/Rotate", idx)) >= 0) {
                    qWarning() << "w2b-diag file /Rotate #" << rotIdx++
                               << bytes.mid(idx, 16);
                    idx += 7;
                }
            }
        }
        try {
            PoDoFo::PdfMemDocument d;
            d.Load(out.toUtf8().constData());
            for (unsigned p = 0; p < d.GetPages().GetCount(); ++p) {
                auto& pg = d.GetPages().GetPageAt(p);
                const PoDoFo::Rect m = pg.GetMediaBox();
                qWarning() << "w2b-diag GetMediaBox page" << p << ":"
                           << m.X << m.Y << m.Width << m.Height
                           << "rotation" << pg.GetRotation();
            }
        } catch (const std::exception& e) {
            qWarning() << "w2b-diag load failed:" << e.what();
        }
        {
            const QList<QList<QRectF>> perPage = { pdfiumAnnotRects(out, 0),
                                                   pdfiumAnnotRects(out, 1),
                                                   pdfiumAnnotRects(out, 2),
                                                   pdfiumAnnotRects(out, 3) };
            PdfiumEnvironment env;
            FPDF_DOCUMENT doc = FPDF_LoadDocument(out.toUtf8().constData(), nullptr);
            if (doc) {
                for (int p = 0; p < 4; ++p) {
                    FPDF_PAGE pg = FPDF_LoadPage(doc, p);
                    if (pg) {
                        qWarning() << "w2b-diag PDFium page" << p << "size"
                                   << FPDF_GetPageWidthF(pg) << FPDF_GetPageHeightF(pg)
                                   << "annotRects" << perPage[p];
                        FPDF_ClosePage(pg);
                    }
                }
                FPDF_CloseDocument(doc);
            }
        }
        for (int p = 0; p < 4; ++p) {
            const QList<QRectF> raw = rawAnnotRects(out, p);
            QVERIFY2(raw.size() == 1,
                     qPrintable(QString("page %1: %2 annots").arg(p).arg(raw.size())));
            QVERIFY2(rectClose(raw.first(), expected[p]),
                     qPrintable(QString("SL1 REGRESSION: page %1 raw /Rect %2 != my law "
                                        "literal %3 — the drawn mark is not where the "
                                        "user drew it").arg(p).arg(rectStr(raw.first()),
                                                               rectStr(expected[p]))));

            const QList<QRectF> viaPdfium = pdfiumAnnotRects(out, p);
            QVERIFY2(!viaPdfium.isEmpty(), qPrintable(QString("page %1: PDFium saw no annot").arg(p)));
            QVERIFY2(rectClose(viaPdfium.first(), expected[p]),
                     qPrintable(QString("page %1 PDFium rect %2 != %3 (second engine "
                                        "disagrees with the law)").arg(p)
                                   .arg(rectStr(viaPdfium.first()), rectStr(expected[p]))));
        }
    }

    // SL1 blast-radius on /Rotate 270 (found by THIS probe; the committed
    // suite's fixture only covers rot {0, 90}): does the FORM path and the
    // READ path break on 270 the way the embed path does?
    void formFieldOnRotate270FollowsTheLaw()
    {
        FormManager forms;
        // Page 3 (Letter, /Rotate 270): display (40, 50, 120x40) -> law user
        // rect: ux = W - vy1..W - vy0 = 612-90..612-50 = 522..552;
        // uy = H - vx1..H - vx0 = 792-160..792-40 = 632..752.
        const QString out = outPath("field-270.pdf");
        QVERIFY(forms.addTextField(fixturePath(), 3, QRectF(40, 50, 120, 40),
                                   QStringLiteral("Field270"), out));
        const QList<QRectF> raw = rawAnnotRects(out, 3);
        QVERIFY(raw.size() == 1);
        const QRectF expect(QPointF(522, 632), QPointF(552, 752));
        QVERIFY2(rectClose(raw.first(), expect),
                 qPrintable(QString("SL1-270 REGRESSION: field /Rect %1 != law %2 "
                                    "on a /Rotate 270 page")
                                .arg(rectStr(raw.first()), rectStr(expect))));
    }

    void extractMapsForeignRectOnRotate270()
    {
        // Foreign spec-correct writer stores the user rect for display
        // (60,80,100x50) on a /Rotate 270 page: [482 632 50 100] (w/h swap).
        try {
            PoDoFo::PdfMemDocument doc;
            doc.Load(fixturePath().toUtf8().constData());
            auto& page = doc.GetPages().GetPageAt(3);
            auto& annot = page.GetAnnotations().CreateAnnot(
                PoDoFo::PdfAnnotationType::Square, PoDoFo::Rect(482, 632, 50, 100));
            annot.SetRectRaw(PoDoFo::Corners(482, 632, 532, 732));
            doc.Save(fixturePath().toUtf8().constData());
        } catch (const std::exception& e) {
            QFAIL(qPrintable(QString("foreign-annot plant (270) failed: %1").arg(e.what())));
        }
        PoDoFoBackend reader;
        const QList<AnnotationItem> items = reader.extractAnnotations(fixturePath());
        QCOMPARE(items.size(), 1);
        QCOMPARE(items.first().pageIndex, 3);
        const QRectF want(60, 80, 100, 50);
        QVERIFY2(rectClose(items.first().rect, want),
                 qPrintable(QString("SL1-270 REGRESSION: foreign rect surfaced at "
                                    "display %1, expected the drawn spot (60,80,100x50)")
                                .arg(rectStr(items.first().rect))));
    }

    // Read-back: a foreign writer's user-space rect surfaces at the drawn spot.
    void extractMapsForeignRectOnMyFixture()
    {
        // The user rect that a spec-compliant writer would store for display
        // (60,80,100x50) on page 1 ([0 200 612 842] /Rotate 90): [80 260 50 100]
        // — note width/height are SWAPPED relative to the display rect.
        try {
            PoDoFo::PdfMemDocument doc;
            doc.Load(fixturePath().toUtf8().constData());
            auto& page = doc.GetPages().GetPageAt(1);
            auto& annot = page.GetAnnotations().CreateAnnot(
                PoDoFo::PdfAnnotationType::Square, PoDoFo::Rect(80, 260, 50, 100));
            annot.SetRectRaw(PoDoFo::Corners(80, 260, 130, 360));
            doc.Save(fixturePath().toUtf8().constData());
        } catch (const std::exception& e) {
            QFAIL(qPrintable(QString("foreign-annot plant failed: %1").arg(e.what())));
        }

        PoDoFoBackend reader;
        const QList<AnnotationItem> items = reader.extractAnnotations(fixturePath());
        QCOMPARE(items.size(), 1);
        QCOMPARE(items.first().pageIndex, 1);
        const QRectF want(60, 80, 100, 50);
        QVERIFY2(rectClose(items.first().rect, want),
                 qPrintable(QString("SL1 REGRESSION: foreign rect surfaced at display %1, "
                                    "expected the drawn spot (60,80,100x50)")
                                .arg(rectStr(items.first().rect))));
    }

    // ── SL1 (forms): field creation + rect update through the same law ──────
    void formFieldCreationFollowsTheLawOnMyFixture()
    {
        FormManager forms;
        // Page 1 ([0 200 612 842] rot 90): display (40, 50, 120x40) → user
        // ux=50..90, uy=200+40..200+160 → [50 240 90 360].
        const QString out = outPath("field.pdf");
        QVERIFY(forms.addTextField(fixturePath(), 1, QRectF(40, 50, 120, 40),
                                   QStringLiteral("W2BField"), out));
        const QList<QRectF> raw = rawAnnotRects(out, 1);
        QVERIFY(raw.size() == 1);
        const QRectF expect(QPointF(50, 240), QPointF(90, 360));
        QVERIFY2(rectClose(raw.first(), expect),
                 qPrintable(QString("SL1 REGRESSION: field /Rect %1 != my literal %2")
                                .arg(rectStr(raw.first()), rectStr(expect))));

        const QList<QRectF> viaPdfium = pdfiumAnnotRects(out, 1);
        QVERIFY(!viaPdfium.isEmpty());
        QVERIFY2(rectClose(viaPdfium.first(), expect),
                 qPrintable(QString("PDFium %1 != %2")
                                .arg(rectStr(viaPdfium.first()), rectStr(expect))));

        // Move the field through updateFieldRect: display (20, 30, 60x24) →
        // user ux=30..54, uy=200+20..200+80 → [30 220 54 280].
        QVERIFY(forms.updateFieldRect(out, QStringLiteral("W2BField"), 1,
                                      QRectF(20, 30, 60, 24), out));
        const QList<QRectF> moved = rawAnnotRects(out, 1);
        QVERIFY(moved.size() == 1);
        const QRectF expect2(QPointF(30, 220), QPointF(54, 280));
        QVERIFY2(rectClose(moved.first(), expect2),
                 qPrintable(QString("SL1 REGRESSION: moved /Rect %1 != %2")
                                .arg(rectStr(moved.first()), rectStr(expect2))));
    }

    // ── SL3: auto-detected suggestions sit under the label, not mirrored ────
    void suggestionSitsUnderMyLabelNotMirrored()
    {
        // MY label: "Amount: " at user (90, 640) on Letter — baseline display
        // y = 792-640 = 152; field height 1.4*12 = 16.8 → display y = 135.2;
        // display x = 90 + 8 chars * 6 = 138.
        const QString pdf = m_dir->filePath("label.pdf");
        QVERIFY(buildLabelPdf(pdf, 90, 640, "Amount: "));
        FormManager fm;
        const auto suggestions = fm.autoDetectFields(pdf, 0);
        QVERIFY2(!suggestions.isEmpty(),
                 "an 'Amount:' label must produce a suggestion");
        const auto& s = suggestions.first();
        QVERIFY2(std::fabs(s.rect.x() - 138.0) < 0.5,
                 qPrintable(QString("suggestion x %1 != 138").arg(s.rect.x())));
        QVERIFY2(std::fabs(s.rect.y() - 135.2) < 0.5,
                 qPrintable(QString("SL3 REGRESSION: suggestion y %1 != 135.2 — the "
                                    "field is mirrored to the opposite side of the page")
                                .arg(s.rect.y())));

        // Placing it must store the widget /Rect under the label in USER
        // space (y ≈ 640..656.8), read back RAW.
        const QString out = m_dir->filePath("placed.pdf");
        QVERIFY(fm.addTextField(pdf, 0, s.rect, s.suggestedName, out));
        const QList<QRectF> raw = rawAnnotRects(out, 0);
        QVERIFY(raw.size() == 1);
        QVERIFY2(std::fabs(raw.first().top() - 640.0) < 0.5
                     && std::fabs(raw.first().bottom() - 656.8) < 0.5,
                 qPrintable(QString("SL3 REGRESSION: stored /Rect %1 != user y "
                                    "640..656.8 under the label (mirrored)")
                                .arg(rectStr(raw.first()))));
    }
};

QTEST_MAIN(W2BProbeLegacySpace)
#include "W2BProbeLegacySpace.moc"
