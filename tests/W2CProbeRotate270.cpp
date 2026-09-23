// SPDX-License-Identifier: Apache-2.0
// SWEEP-W2C (guarantee-verification-engine, 2026-09-20) — INDEPENDENT probe
// for the W2B-1 rotate-270 page-space fix re-submission (SL1 re-verify).
//
// This probe does NOT re-run the fix lane's assertions. Everything here is
// driven through the production engine APIs a user drives, with MY OWN
// hand-computed literals, MY OWN rect values (never the committed suites'),
// MY OWN raw-dictionary walker (page dict → /Annots, /AcroForm → /Fields —
// not PoDoFo's annotation/field APIs), and a raw-bytes byte scan.
//
// The law (core/PageSpaceTransform.h viewerToUser), display rect
// (vx0,vy0,w,h) with raw page (x0, y0, W, H):
//   rot   0: ux = x0 + vx          uy = y0 + H - vy
//   rot  90: ux = x0 + vy          uy = y0 + vx
//   rot 180: ux = x0 + W - vx      uy = y0 + vy
//   rot 270: ux = x0 + W - vy      uy = y0 + H - vx
//
// Fixtures: Letter [0 0 612 792] and offset [0 200 612 1042]
// (PoDoFo::Rect(0, 200, 612, 842) — X, Y, W, H). Display sizes 612x792 /
// 792x612 / 842x612.
//
// Slots:
//   1. annotation move/resize through the production embed path (two model
//      states), all six shapes, raw dict + PDFium + read-back + byte scan.
//   2. form field move through FormManager::updateFieldRect (in-place move),
//      rot270 Letter + rot270 offset.
//   3. F5 containment honesty on the offset 270 shape: correct anchors
//      accepted at the law literal; edge-crossing and just-off-page anchors
//      refused; refused runs are pure reads (source SHA-256 unchanged).
//   4. SEP13 L5/L8 on MY OWN offset-270 raw-bytes fixture: the secret is
//      excised, the proof PASSES naming only the secret, and a wrong-region
//      mark leaves the secret surviving with the proof FAILING (the
//      false-success class W2B-1 broke stays dead).
//   5. signature-field verbatim /Rect store (the CreateField corruption fix):
//      field dict + widget dict + PDFium + raw bytes all carry the law
//      literal on rot 90 and rot 270 + offset.
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QCryptographicHash>
#include <cmath>
#include <vector>

#include "engines/podofo/PoDoFoBackend.h"
#include "engines/FormManager.h"
#include "engines/SignatureFieldCreator.h"
#include "engines/RedactOperation.h"
#include "engines/pdfium/PdfiumBackend.h"
#include "core/AnnotationTypes.h"
#include "core/RedactionProof.h"
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

QByteArray fileSha(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    return QCryptographicHash::hash(f.readAll(), QCryptographicHash::Sha256);
}

// ── fixture ──────────────────────────────────────────────────────────────────
// p0 rot0 Letter | p1 rot90 Letter | p2 rot180 Letter | p3 rot270 Letter
// p4 rot90 offset [0 200 612 1042] | p5 rot270 offset (THE W2B-1 shape)
struct Shape {
    int rotation;
    double x0, y0, W, H;   // raw MediaBox
};

QList<Shape> shapes()
{
    return {
        { 0,   0,   0,   612, 792 },
        { 90,  0,   0,   612, 792 },
        { 180, 0,   0,   612, 792 },
        { 270, 0,   0,   612, 792 },
        { 90,  0, 200,   612, 842 },
        { 270, 0, 200,   612, 842 },
    };
}

QString buildFixture(const QString& path)
{
    try {
        PoDoFo::PdfMemDocument doc;
        for (const Shape& s : shapes()) {
            PoDoFo::PdfPage& page = doc.GetPages().CreatePage(
                PoDoFo::Rect(s.x0, s.y0, s.W, s.H));
            page.SetRotation(s.rotation);
        }
        doc.Save(path.toUtf8().constData());
        return path;
    } catch (const std::exception& e) {
        qWarning() << "buildFixture failed:" << e.what();
        return QString();
    }
}

// ── MY OWN raw-dict readers (no PoDoFo annotation/field convenience APIs) ───

QRectF rectFromArray(const PoDoFo::PdfObject* obj)
{
    if (!obj || !obj->IsArray()) return QRectF();
    const auto& arr = obj->GetArray();
    if (arr.size() != 4) return QRectF();
    const double v[4] = { arr[0].GetReal(), arr[1].GetReal(),
                          arr[2].GetReal(), arr[3].GetReal() };
    return QRectF(QPointF(qMin(v[0], v[2]), qMin(v[1], v[3])),
                  QPointF(qMax(v[0], v[2]), qMax(v[1], v[3])));
}

// All annotation /Rects on a page, walked straight off the page dictionary's
// /Annots array (resolving indirect refs myself).
QList<QRectF> rawAnnotRects(const QString& path, int pageIndex)
{
    QList<QRectF> out;
    try {
        PoDoFo::PdfMemDocument doc;
        doc.Load(path.toUtf8().constData());
        auto& page = doc.GetPages().GetPageAt(pageIndex);
        const auto* annos = page.GetDictionary().FindKey("Annots");
        if (!annos) return out;
        if (annos->IsReference())
            annos = &doc.GetObjects().MustGetObject(annos->GetReference());
        if (!annos || !annos->IsArray()) return out;
        for (const auto& ref : annos->GetArray()) {
            const PoDoFo::PdfObject* obj = &ref;
            if (obj->IsReference())
                obj = &doc.GetObjects().MustGetObject(obj->GetReference());
            out.append(rectFromArray(obj->GetDictionary().FindKey("Rect")));
        }
    } catch (const std::exception& e) {
        qWarning() << "rawAnnotRects failed:" << e.what();
    }
    out.removeAll(QRectF());
    return out;
}

// The /Rect of the annotation whose /NM (the AnnotationItem id the engine
// stores) equals `nm` — walked straight off the page dictionary.
QList<QRectF> rawRectsForNm(const QString& path, int pageIndex, const QString& nm)
{
    QList<QRectF> out;
    try {
        PoDoFo::PdfMemDocument doc;
        doc.Load(path.toUtf8().constData());
        auto& page = doc.GetPages().GetPageAt(pageIndex);
        const auto* annos = page.GetDictionary().FindKey("Annots");
        if (!annos) return out;
        if (annos->IsReference())
            annos = &doc.GetObjects().MustGetObject(annos->GetReference());
        if (!annos || !annos->IsArray()) return out;
        for (const auto& ref : annos->GetArray()) {
            const PoDoFo::PdfObject* obj = &ref;
            if (obj->IsReference())
                obj = &doc.GetObjects().MustGetObject(obj->GetReference());
            const auto* nmObj = obj->GetDictionary().FindKey("NM");
            const bool nmMatches =
                nmObj && nmObj->IsString()
                && nm == QString::fromStdString(
                       std::string(nmObj->GetString().GetString()));
            if (nmMatches)
                out.append(rectFromArray(obj->GetDictionary().FindKey("Rect")));
        }
    } catch (const std::exception& e) {
        qWarning() << "rawRectsForNm failed:" << e.what();
    }
    return out;
}

// Field-dict and widget-dict /Rects for a signature field, resolved from
// /AcroForm /Fields and the page's /Annots independently.
struct SigRects {
    QList<QRectF> fromAcroFormFields;
    QList<QRectF> fromPageAnnots;
};

SigRects sigFieldRects(const QString& path, int pageIndex)
{
    SigRects out;
    try {
        PoDoFo::PdfMemDocument doc;
        doc.Load(path.toUtf8().constData());
        auto* catalog = doc.GetCatalog().GetDictionary().FindKey("AcroForm");
        if (catalog && catalog->IsReference())
            catalog = &doc.GetObjects().MustGetObject(catalog->GetReference());
        if (catalog) {
            const auto* fields = catalog->GetDictionary().FindKey("Fields");
            if (fields && fields->IsReference())
                fields = &doc.GetObjects().MustGetObject(fields->GetReference());
            if (fields && fields->IsArray()) {
                for (const auto& ref : fields->GetArray()) {
                    const PoDoFo::PdfObject* obj = &ref;
                    if (obj->IsReference())
                        obj = &doc.GetObjects().MustGetObject(obj->GetReference());
                    out.fromAcroFormFields.append(
                        rectFromArray(obj->GetDictionary().FindKey("Rect")));
                }
            }
        }
        auto& page = doc.GetPages().GetPageAt(pageIndex);
        const auto* annos = page.GetDictionary().FindKey("Annots");
        if (annos && annos->IsReference())
            annos = &doc.GetObjects().MustGetObject(annos->GetReference());
        if (annos && annos->IsArray()) {
            for (const auto& ref : annos->GetArray()) {
                const PoDoFo::PdfObject* obj = &ref;
                if (obj->IsReference())
                    obj = &doc.GetObjects().MustGetObject(obj->GetReference());
                out.fromPageAnnots.append(
                    rectFromArray(obj->GetDictionary().FindKey("Rect")));
            }
        }
    } catch (const std::exception& e) {
        qWarning() << "sigFieldRects failed:" << e.what();
    }
    out.fromAcroFormFields.removeAll(QRectF());
    out.fromPageAnnots.removeAll(QRectF());
    return out;
}

// PDFium annotation rects (raw user space, the convention the committed pins
// establish for FPDFAnnot_GetRect on this vendored build).
QList<QRectF> pdfiumAnnotRects(const QString& path, int pageIndex,
                               QSizeF* pageSizeOut = nullptr)
{
    QList<QRectF> out;
    PdfiumEnvironment env;
    FPDF_DOCUMENT doc = FPDF_LoadDocument(path.toUtf8().constData(), nullptr);
    if (!doc) return out;
    FPDF_PAGE page = FPDF_LoadPage(doc, pageIndex);
    if (page) {
        if (pageSizeOut)
            *pageSizeOut = QSizeF(FPDF_GetPageWidthF(page), FPDF_GetPageHeightF(page));
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

// ── MY raw-bytes redaction fixture (offset MediaBox + /Rotate 270) ──────────
//
// One page: /MediaBox [0 200 612 1042] /Rotate 270, Helvetica 12pt:
//   secret  "Offset270Secret hidden note"   Td (100, 700)
//   public1 "KeepThisVisible public record" Td (100, 650)
//   public2 "SecondLine public record"      Td (100, 300)
//
// Display position of a glyph band at Td(x,y) (dy = y - 200, H = 842, W = 612;
// display = (H - dy, W - dx), band dy..dy+9, run extends +x by L):
//   secret  dy=500: display x [333,342]; y [612-L-100, 612-100] ~ [334,512]
//   line2   dy=450: display x [383,392]  (same y span)
//   public  dy=100: display x [733,742]  (same y span)
//
// TIGHT mark over the secret (the fair analog of the committed fixtures'
// hand-placed marks, and of the redact tool's text-snapped boxes):
//   M = (329, 320) 17x215 -> covers [333,342]x[334,512] with ~4pt x-pads.
//   User extents: ux=[612-535,612-320]=[77,292], uy=[1042-346,1042-329]=[696,713].
//   Secret glyphs x[100,265] y[700,709] sit inside; the line-2 glyphs
//   y[650,659] clear markLo=696 by 10pt beyond the 3*fs attribution headroom
//   (RedactionProof's runIntersects reaches 36pt above a 650 baseline). A
//   LOOSER mark over-attributes the neighbor line and false-alarms a correct
//   redaction (demonstrated in probe-w2c-loose.txt; recorded as a design
//   observation in SWEEP-W2C — rotation-independent, precision-only).
QRectF secretMark() { return QRectF(329, 320, 17, 215); }

// buildSecretPdf(path, duplicateSecret):
//   duplicateSecret=false — one secret line at (100,700) + two public lines.
//   duplicateSecret=true  — the r/pdf missed-spot scenario on rot270+offset:
//       the SAME secret string occurs AGAIN at (100,650), unmarked. The mark
//       removes the first occurrence; the second survives and the survival
//       sweep must catch it (sweep is per-STRING across the whole document).
QString buildSecretPdf(const QString& path, bool duplicateSecret = false)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return QString();
    QByteArray out = "%PDF-1.4\n";
    QList<qint64> off;
    auto addObj = [&out, &off](const QByteArray& body) {
        off.append(out.size());
        out += QByteArray::number(off.size()) + " 0 obj\n" + body + "\nendobj\n";
    };
    addObj("<</Type/Catalog/Pages 2 0 R>>");
    addObj("<</Type/Pages/Kids[3 0 R]/Count 1>>");
    addObj("<</Type/Page/Parent 2 0 R/MediaBox[0 200 612 1042]/Rotate 270"
           "/Contents 4 0 R/Resources<</Font<</F1 5 0 R>>>>>>");
    QByteArray stream = "BT /F1 12 Tf\n";
    stream += "100 700 Td (Offset270Secret hidden note) Tj ET\n";
    stream += "BT /F1 12 Tf\n";
    if (duplicateSecret)
        stream += "100 650 Td (Offset270Secret hidden note) Tj ET\n";
    else
        stream += "100 650 Td (KeepThisVisible public record) Tj ET\n";
    stream += "BT /F1 12 Tf\n";
    stream += "100 300 Td (SecondLine public record) Tj ET\n";
    addObj("<</Length " + QByteArray::number(stream.size()) + ">>stream\n"
           + stream + "endstream");
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
    return path;
}

QString joinedProofFailures(const gp::RedactResult& r)
{
    return r.proofFailures.join(QStringLiteral("; "));
}

} // namespace

class W2CProbeRotate270 : public QObject {
    Q_OBJECT

    QTemporaryDir m_dir;

    QString fixturePath() const { return m_dir.filePath("w2c-fixture.pdf"); }
    QString outPath(const char* name) const { return m_dir.filePath(QLatin1String(name)); }

private slots:
    void init()
    {
        QVERIFY(m_dir.isValid());
        QVERIFY(!buildFixture(fixturePath()).isEmpty());
    }

    // ── Slot 1: MY OWN non-symmetric rects, move + resize through the REAL
    // production edit path (embedAnnotations carries the full model list —
    // exactly what HomeController's save drives). Two model states:
    //   A = (137, 221) 130x47   (wide)
    //   B = (315,  97)  74x158  (tall — the move+resize)
    // Hand-computed RAW /Rect per shape (see law above):
    void moveResizeThroughProductionEmbedFollowsTheLaw()
    {
        const QRectF rectA(137, 221, 130, 47);
        const QRectF rectB(315, 97, 74, 158);
        const QRectF expectedA[6] = {
            QRectF(QPointF(137, 524), QPointF(267, 571)), // rot0
            QRectF(QPointF(221, 137), QPointF(268, 267)), // rot90
            QRectF(QPointF(345, 221), QPointF(475, 268)), // rot180
            QRectF(QPointF(344, 525), QPointF(391, 655)), // rot270
            QRectF(QPointF(221, 337), QPointF(268, 467)), // rot90 + offset
            QRectF(QPointF(344, 775), QPointF(391, 905)), // rot270 + offset
        };
        const QRectF expectedB[6] = {
            QRectF(QPointF(315, 537), QPointF(389, 695)),
            QRectF(QPointF(97, 315), QPointF(255, 389)),
            QRectF(QPointF(223, 97), QPointF(297, 255)),
            QRectF(QPointF(357, 403), QPointF(515, 477)),
            QRectF(QPointF(97, 515), QPointF(255, 589)),
            QRectF(QPointF(357, 653), QPointF(515, 727)),
        };

        const QList<Shape> all = shapes();
        for (int p = 0; p < all.size(); ++p) {
            const Shape& s = all[p];
            const QString tag = QString("p%1 rot%2").arg(p).arg(s.rotation);

            // Stage A: place.
            const QString outA = outPath(QString("move-a-p%1.pdf").arg(p).toUtf8().constData());
            {
                PoDoFoBackend engine;
                QList<AnnotationItem> items;
                AnnotationItem item;
                item.pageIndex = p;
                item.mode = ToolMode::DrawRectangle;
                item.id = QStringLiteral("w2c-move-%1").arg(p);
                item.rect = rectA;
                item.text = QStringLiteral("w2c-a");
                items.append(item);
                QVERIFY2(engine.embedAnnotations(fixturePath(), outA, items),
                         qPrintable(tag + ": stage-A embed failed"));
            }
            // Stage B: the user MOVES + RESIZES the mark in the model; the app
            // persists by re-embedding the current model state (chained file —
            // the HomeController save flow's input/output are the same
            // document lineage).
            const QString outB = outPath(QString("move-b-p%1.pdf").arg(p).toUtf8().constData());
            {
                PoDoFoBackend engine;
                QList<AnnotationItem> items;
                AnnotationItem item;
                item.pageIndex = p;
                item.mode = ToolMode::DrawRectangle;
                item.id = QStringLiteral("w2c-move-%1").arg(p);
                item.rect = rectB;
                item.text = QStringLiteral("w2c-b");
                items.append(item);
                QVERIFY2(engine.embedAnnotations(outA, outB, items),
                         qPrintable(tag + ": stage-B (moved) embed failed"));
            }

            // The moved state is what the committed bytes must carry.
            const QList<QRectF> nmRects =
                rawRectsForNm(outB, p, QStringLiteral("w2c-move-%1").arg(p));
            QVERIFY2(!nmRects.isEmpty(), qPrintable(tag + ": no /NM annot after move"));
            bool movedAtLawLiteral = false;
            for (const QRectF& r : nmRects)
                movedAtLawLiteral = movedAtLawLiteral || rectClose(r, expectedB[p]);
            QVERIFY2(movedAtLawLiteral,
                     qPrintable(QString("W2C: %1 moved raw /Rect %2 != law literal %3")
                                    .arg(tag, rectStr(nmRects.first()),
                                         rectStr(expectedB[p]))));

            // PDFium must see the moved shape on the page.
            QSizeF pdfiumSize;
            const QList<QRectF> viaPdfium = pdfiumAnnotRects(outB, p, &pdfiumSize);
            QVERIFY2(!viaPdfium.isEmpty(), qPrintable(tag + ": PDFium saw no annot"));
            const QSizeF wantSize = (s.rotation == 90 || s.rotation == 270)
                                        ? QSizeF(s.H, s.W) : QSizeF(s.W, s.H);
            QVERIFY2(std::fabs(pdfiumSize.width() - wantSize.width()) < 0.5
                     && std::fabs(pdfiumSize.height() - wantSize.height()) < 0.5,
                     qPrintable(QString("W2C: %1 PDFium page %2x%3 != display %4x%5")
                                    .arg(tag).arg(pdfiumSize.width())
                                    .arg(pdfiumSize.height())
                                    .arg(wantSize.width()).arg(wantSize.height())));
            bool pdfiumSeesMoved = false;
            for (const QRectF& r : viaPdfium)
                pdfiumSeesMoved = pdfiumSeesMoved || rectClose(r, expectedB[p]);
            QVERIFY2(pdfiumSeesMoved,
                     qPrintable(QString("W2C: %1 PDFium annots %2 miss moved literal %3")
                                    .arg(tag).arg(rectStr(viaPdfium.first()),
                                         rectStr(expectedB[p]))));

            // Read-back must surface the moved DISPLAY rect.
            PoDoFoBackend reader;
            const QList<AnnotationItem> back = reader.extractAnnotations(outB);
            bool readBackMoved = false;
            for (const AnnotationItem& b : back)
                if (b.pageIndex == p && rectClose(b.rect, rectB))
                    readBackMoved = true;
            QVERIFY2(readBackMoved,
                     qPrintable(QString("W2C: %1 read-back does not surface moved display %2")
                                    .arg(tag, rectStr(rectB))));

            if (p == 5) {
                // Honest observation: the chained embed appends (the engine
                // has no /NM dedup) — the stale stage-A dict may still exist.
                // The LAW claim is only that the moved state is stored at the
                // law literal and nothing is stored at a TRANSPOSED shape.
                for (const QRectF& r : nmRects) {
                    QVERIFY2(rectClose(r, expectedB[5]) || rectClose(r, expectedA[5]),
                             qPrintable(QString("W2C: p5 unexpected annot shape %1 "
                                                "(neither the moved nor the stale literal)")
                                            .arg(rectStr(r))));
                }
                // Byte scan: the moved literal appears verbatim in the file.
                QFile bf(outB);
                QVERIFY(bf.open(QIODevice::ReadOnly));
                const QByteArray bytes = bf.readAll();
                const QByteArray lit = QString("357 653 515 727").toLatin1();
                QVERIFY2(bytes.contains(lit),
                         "W2C: p5 moved literal [357 653 515 727] not found in raw bytes");
            }
        }
    }

    // ── Slot 2: in-place form-field move through updateFieldRect ────────────
    void formMoveViaUpdateFieldRectFollowsTheLaw()
    {
        struct Leg { int page; QRectF first; QRectF moved; QRectF litFirst; QRectF litMoved; };
        // p5 (rot270 + offset, y0=200, H=842):
        //   (71,303,96x37):  ux=[612-340,612-303]=[272,309] uy=[1042-167,1042-71]=[875,971]
        //   (380,402,55x121):ux=[612-523,612-402]=[89,210]  uy=[1042-435,1042-380]=[607,662]
        // p3 (rot270 Letter):
        //   (29,411,83x24):  ux=[612-435,612-411]=[177,201] uy=[792-112,792-29]=[680,763]
        //   (500,55,40x333): ux=[612-388,612-55]=[224,557]  uy=[792-540,792-500]=[252,292]
        const QList<Leg> legs = {
            { 5, QRectF(71, 303, 96, 37), QRectF(380, 402, 55, 121),
              QRectF(QPointF(272, 875), QPointF(309, 971)),
              QRectF(QPointF(89, 607), QPointF(210, 662)) },
            { 3, QRectF(29, 411, 83, 24), QRectF(500, 55, 40, 333),
              QRectF(QPointF(177, 680), QPointF(201, 763)),
              QRectF(QPointF(224, 252), QPointF(557, 292)) },
        };
        for (const Leg& leg : legs) {
            FormManager forms;
            const QString out = outPath(
                QString("form-%1.pdf").arg(leg.page).toUtf8().constData());
            QVERIFY2(forms.addTextField(fixturePath(), leg.page, leg.first,
                                        QStringLiteral("W2CForm%1").arg(leg.page), out),
                     qPrintable(QString("p%1: addTextField failed").arg(leg.page)));
            QVERIFY2(forms.updateFieldRect(out, QStringLiteral("W2CForm%1").arg(leg.page),
                                           leg.page, leg.moved, out),
                     qPrintable(QString("p%1: updateFieldRect (the move) failed").arg(leg.page)));

            const QList<QRectF> raw = rawAnnotRects(out, leg.page);
            QVERIFY2(raw.size() == 1,
                     qPrintable(QString("p%1: %2 annots after move").arg(leg.page).arg(raw.size())));
            QVERIFY2(rectClose(raw.first(), leg.litMoved),
                     qPrintable(QString("W2C: p%1 MOVED field raw /Rect %2 != law literal %3")
                                    .arg(leg.page).arg(rectStr(raw.first()),
                                                       rectStr(leg.litMoved))));
            const QList<QRectF> viaPdfium = pdfiumAnnotRects(out, leg.page);
            QVERIFY2(!viaPdfium.isEmpty() && rectClose(viaPdfium.first(), leg.litMoved),
                     qPrintable(QString("W2C: p%1 PDFium field %2 != %3")
                                    .arg(leg.page).arg(rectStr(viaPdfium.first()),
                                                       rectStr(leg.litMoved))));
        }
    }

    // ── Slot 3: F5 containment honesty on the offset 270 shape ──────────────
    void f5ContainmentHonestOnOffset270()
    {
        const int p = 5;
        // Accept: on-page anchor -> stored at the hand literal
        //   (486,173,121x83): ux=[612-256,612-173]=[356,439]
        //                     uy=[1042-607,1042-486]=[435,556]
        {
            const QString out = outPath("f5-ok.pdf");
            QVector<gp::SignatureFieldCreator::Spec> specs;
            gp::SignatureFieldCreator::Spec spec;
            spec.fieldName = QStringLiteral("W2CSigOk");
            spec.pageIndex = p;
            spec.viewerRect = QRectF(486, 173, 121, 83);
            specs.append(spec);
            QString err;
            QVERIFY2(gp::SignatureFieldCreator::createSignatureFields(
                         fixturePath(), specs, out, &err),
                     qPrintable(QString("W2C: correct offset-270 anchor refused: %1").arg(err)));
            const QList<QRectF> raw = rawAnnotRects(out, p);
            QVERIFY2(raw.size() == 1 && rectClose(raw.first(),
                                                  QRectF(QPointF(356, 435), QPointF(439, 556))),
                     qPrintable(QString("W2C: sig raw /Rect %1 != [356 435 439 556]")
                                    .arg(raw.isEmpty() ? QString("none")
                                                       : rectStr(raw.first()))));
            const QList<QRectF> viaPdfium = pdfiumAnnotRects(out, p);
            QVERIFY2(!viaPdfium.isEmpty()
                     && rectClose(viaPdfium.first(),
                                  QRectF(QPointF(356, 435), QPointF(439, 556))),
                     "W2C: PDFium does not confirm the signature field rect");
        }
        // Refuse: an anchor crossing the displayed right edge
        //   (790,100,80x40): display width is 842, vx1=870 -> the law maps it
        //   to uy=[172,252], breaching the raw origin y0=200.
        // Refuse: an anchor JUST off the right edge (1pt clear)
        //   (843,0,50x50): vx0=843>842 -> uy=[149,199], entirely below y0=200.
        for (const auto& bad : std::vector<std::pair<const char*, QRectF>>{
                 {"edge-crossing", QRectF(790, 100, 80, 40)},
                 {"just-off-edge", QRectF(843, 0, 50, 50)}}) {
            const QString out = outPath("f5-bad.pdf");
            if (QFile::exists(out)) QFile::remove(out);
            const QByteArray shaBefore = fileSha(fixturePath());
            QVector<gp::SignatureFieldCreator::Spec> specs;
            gp::SignatureFieldCreator::Spec spec;
            spec.fieldName = QStringLiteral("W2CSigBad");
            spec.pageIndex = p;
            spec.viewerRect = bad.second;
            specs.append(spec);
            QString err;
            const bool accepted = gp::SignatureFieldCreator::createSignatureFields(
                fixturePath(), specs, out, &err);
            QVERIFY2(!accepted,
                     qPrintable(QString("W2C F5: %1 anchor %2 ACCEPTED on offset 270")
                                    .arg(bad.first, rectStr(bad.second))));
            QVERIFY2(err.contains(QStringLiteral("outside page")),
                     qPrintable(QString("W2C F5: %1 refusal wording: %2")
                                    .arg(bad.first, err)));
            QVERIFY2(!QFile::exists(out),
                     "W2C F5: a refused placement must not write the destination");
            QVERIFY2(fileSha(fixturePath()) == shaBefore,
                     "W2C F5: a refused placement must be a pure read (source changed)");
        }
    }

    // ── Slot 4: SEP13 L5/L8 on MY offset-270 fixture ─────────────────────────
    void sep13SecretExcisedAndProofHonestOnOffset270()
    {
        const QString src = buildSecretPdf(outPath("w2c-secret-src.pdf"));
        QVERIFY(!src.isEmpty());
        const int p = 0;

        // L8 (correct region): the secret must be excised; proof PASSES.
        {
            const QString dest = outPath("w2c-secret-red.pdf");
            gp::RedactRequest req;
            req.sourcePath = src;
            req.destinationPath = dest;
            req.redactionsByPage[p].append(secretMark());
            req.produceProof = true;
            gp::RedactOperation op(req);
            gp::RedactResult captured;
            QObject::connect(&op, &gp::RedactOperation::finished, &op,
                             [&captured](const gp::RedactResult& r) { captured = r; });
            op.run();
            QCOMPARE(captured.outcome, gp::RedactOutcome::Completed);
            QVERIFY(captured.proofRan);
            QVERIFY2(captured.proofPassed,
                     qPrintable(QString("W2C L8: proof should PASS on offset-270: %1")
                                    .arg(joinedProofFailures(captured))));

            // Independent extractor: secret gone, both public lines alive.
            PdfiumBackend backend;
            QVERIFY(backend.loadDocument(dest));
            const QString text = backend.extractText(p);
            QVERIFY2(!text.contains(QStringLiteral("Offset270Secret")),
                     "W2C L8: the secret SURVIVED the offset-270 excision");
            QVERIFY2(text.contains(QStringLiteral("KeepThisVisible")),
                     "W2C L8: public line 1 was collateral damage");
            QVERIFY2(text.contains(QStringLiteral("SecondLine")),
                     "W2C L8: public line 2 was collateral damage");

            // Raw bytes too (decode-independent surface).
            QFile df(dest);
            QVERIFY(df.open(QIODevice::ReadOnly));
            QVERIFY2(!df.readAll().contains(QByteArray("Offset270Secret")),
                     "W2C L8: secret bytes found in the committed file");

            // L5 attribution honesty: the proof's own attribution must name
            // ONLY the secret for this mark.
            gp::RedactionProof::Request vreq;
            vreq.sourcePath = src;
            vreq.outputPath = dest;
            vreq.redactionsByPage[p].append(secretMark());
            const gp::RedactionProof::Result vr = gp::RedactionProof::verify(vreq);
            QVERIFY(vr.proofRan && vr.proofPassed);
            QVERIFY2(!vr.entries.isEmpty(), "W2C L5: no excision entries");
            bool namesSecret = false, namesPublic = false;
            for (const auto& e : vr.entries) {
                for (const QString& s : e.removedStrings) {
                    if (s.contains(QStringLiteral("Offset270Secret"))) namesSecret = true;
                    if (s.contains(QStringLiteral("KeepThisVisible"))
                        || s.contains(QStringLiteral("SecondLine"))) namesPublic = true;
                }
            }
            QVERIFY2(namesSecret, "W2C L5: the proof did not attribute the secret");
            QVERIFY2(!namesPublic, "W2C L5: the proof attributed public text to the mark");
        }

        // Honesty (the false-success class): the r/pdf missed-spot scenario
        // on rot270+offset - a SECOND occurrence of the secret sits on an
        // unmarked line. The mark removes occurrence #1; the survival sweep
        // (per-STRING, whole document) must catch occurrence #2 and FAIL the
        // proof naming the secret. (A mark that never touches the secret is
        // the wrong probe shape: the proof certifies the marks that were
        // applied, not the operator's intent.)
        {
            const QString src2 = buildSecretPdf(outPath("w2c-secret-src2.pdf"), true);
            QVERIFY(!src2.isEmpty());
            const QString dest = outPath("w2c-secret-missed.pdf");
            gp::RedactRequest req;
            req.sourcePath = src2;
            req.destinationPath = dest;
            req.redactionsByPage[p].append(secretMark());
            req.produceProof = true;
            gp::RedactOperation op(req);
            gp::RedactResult captured;
            QObject::connect(&op, &gp::RedactOperation::finished, &op,
                             [&captured](const gp::RedactResult& r) { captured = r; });
            op.run();
            QCOMPARE(captured.outcome, gp::RedactOutcome::Completed);
            QVERIFY(captured.proofRan);
            QVERIFY2(!captured.proofPassed,
                     qPrintable(QString("W2C honesty: proof PASSED although a second "
                                        "secret occurrence survives (missed spot): %1")
                                    .arg(joinedProofFailures(captured))));
            QVERIFY2(joinedProofFailures(captured)
                         .contains(QStringLiteral("Offset270Secret")),
                     "W2C honesty: the failed proof does not name the surviving secret");

            // Independent extractor: occurrence #2 is alive in the output.
            PdfiumBackend backend;
            QVERIFY(backend.loadDocument(dest));
            QVERIFY2(backend.extractText(p).contains(QStringLiteral("Offset270Secret")),
                     "W2C honesty: the second occurrence vanished - fixture drifted");
        }
    }

    // ── Slot 5: signature-field verbatim /Rect (CreateField corruption fix) ──
    void signatureFieldStoresVerbatimRawRect()
    {
        // rot90 Letter (211,147,131x61): ux=[147,208] uy=[211,342]
        // rot270 offset (486,173,121x83): ux=[356,439] uy=[435,556]
        struct Leg { int page; QRectF viewer; QRectF lit; const char* tag; };
        const QList<Leg> legs = {
            { 1, QRectF(211, 147, 131, 61),
              QRectF(QPointF(147, 211), QPointF(208, 342)), "rot90 Letter" },
            { 5, QRectF(486, 173, 121, 83),
              QRectF(QPointF(356, 435), QPointF(439, 556)), "rot270 offset" },
        };
        for (const Leg& leg : legs) {
            const QString out = outPath(
                QString("sigver-%1.pdf").arg(leg.page).toUtf8().constData());
            QVector<gp::SignatureFieldCreator::Spec> specs;
            gp::SignatureFieldCreator::Spec spec;
            spec.fieldName = QStringLiteral("W2CVerbatim%1").arg(leg.page);
            spec.pageIndex = leg.page;
            spec.viewerRect = leg.viewer;
            specs.append(spec);
            QString err;
            QVERIFY2(gp::SignatureFieldCreator::createSignatureFields(
                         fixturePath(), specs, out, &err),
                     qPrintable(QString("%1: create refused: %2").arg(leg.tag, err)));

            // Field dict AND widget dict (page /Annots) carry the literal.
            const SigRects rects = sigFieldRects(out, leg.page);
            QVERIFY2(!rects.fromAcroFormFields.isEmpty(),
                     qPrintable(QString("%1: no /AcroForm /Fields /Rect").arg(leg.tag)));
            QVERIFY2(!rects.fromPageAnnots.isEmpty(),
                     qPrintable(QString("%1: no page /Annots /Rect").arg(leg.tag)));
            bool fieldOk = false, widgetOk = false;
            for (const QRectF& r : rects.fromAcroFormFields)
                fieldOk = fieldOk || rectClose(r, leg.lit);
            for (const QRectF& r : rects.fromPageAnnots)
                widgetOk = widgetOk || rectClose(r, leg.lit);
            QVERIFY2(fieldOk,
                     qPrintable(QString("W2C verbatim: %1 FIELD dict /Rect %2 != %3")
                                    .arg(leg.tag, rectStr(rects.fromAcroFormFields.first()),
                                         rectStr(leg.lit))));
            QVERIFY2(widgetOk,
                     qPrintable(QString("W2C verbatim: %1 WIDGET dict /Rect %2 != %3")
                                    .arg(leg.tag, rectStr(rects.fromPageAnnots.first()),
                                         rectStr(leg.lit))));

            // PDFium confirms.
            const QList<QRectF> viaPdfium = pdfiumAnnotRects(out, leg.page);
            bool pdfiumOk = false;
            for (const QRectF& r : viaPdfium)
                pdfiumOk = pdfiumOk || rectClose(r, leg.lit);
            QVERIFY2(pdfiumOk,
                     qPrintable(QString("W2C verbatim: %1 PDFium %2 != %3")
                                    .arg(leg.tag).arg(viaPdfium.isEmpty()
                                                          ? QString("none")
                                                          : rectStr(viaPdfium.first()))
                                    .arg(rectStr(leg.lit))));

            // The literal must be findable in the RAW FILE BYTES.
            QFile bf(out);
            QVERIFY(bf.open(QIODevice::ReadOnly));
            const QByteArray bytes = bf.readAll();
            const QString litStr = QString("%1 %2 %3 %4")
                                       .arg(leg.lit.left()).arg(leg.lit.top())
                                       .arg(leg.lit.right()).arg(leg.lit.bottom());
            QVERIFY2(bytes.contains(litStr.toLatin1()),
                     qPrintable(QString("W2C verbatim: %1 literal '%2' not in raw bytes")
                                    .arg(leg.tag, litStr)));
        }
    }
};

QTEST_MAIN(W2CProbeRotate270)
#include "W2CProbeRotate270.moc"
