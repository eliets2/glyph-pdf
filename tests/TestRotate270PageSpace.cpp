// SPDX-License-Identifier: Apache-2.0
// W2B-1 (sweep-w2b-verify, 2026-09-20) — the /Rotate page-space law re-pinned
// on ALL FOUR rotations (+ offset origins), the committed blind spot closed.
//
// FINDING W2B-1: vendored PoDoFo 1.1.0 PdfPage::GetMediaBox() is
// rotation-NORMALIZED (W/H-swapped on /Rotate 90/270) while gp::PageSpace::
// pageGeometry() documented "rotation-independent" dimensions. /Rotate 0/180
// and the /Rotate 90 formula shape never read W/H, so every committed fixture
// (rot {0,90}+offset) was green while /Rotate 270 pages stored TRANSPOSED
// annotation and form-field rects. The fix derives the geometry from the RAW
// /MediaBox (GetMediaBoxRaw) and lets the law apply /Rotate itself.
//
// Every value below is a hand-computed HARDCODED literal from the law's
// published table (core/PageSpaceTransform.h) — nothing is computed through
// the code under test. Independent read paths: the raw /Rect dictionary
// arrays (PoDoFo) AND PDFium (FPDFAnnot_GetRect + FPDF_GetPageWidthF/HeightF,
// a second engine that independently confirms both the rects and the
// displayed page size). Consumers pinned: PoDoFoBackend::embedAnnotations /
// extractAnnotations, FormManager::addTextField,
// SignatureFieldCreator::createSignatureFields (SWEEP-W1 F5 containment
// clean path), FormManager::autoDetectFields (display-bounds containment),
// and the GetMediaBox-vs-file-bytes root-cause pin.
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <cmath>

#include "engines/podofo/PoDoFoBackend.h"
#include "engines/FormManager.h"
#include "engines/SignatureFieldCreator.h"
#include "engines/SignatureManager.h"
#include "core/AnnotationTypes.h"
#include "core/PageSpaceTransform.h"
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

// One page shape = (raw MediaBox, /Rotate) + the hand-computed law literals
// for the three pinned consumer rects and the displayed size.
//   embed rect: display (60, 80, 100 x 50)
//   field rect: display (40, 50, 120 x 40)
//   signature anchor: display (200, 300, 150 x 50)
struct Shape {
    PoDoFo::Rect media;
    int rotation;
    QSizeF display;      // the size a viewer shows (W/H swapped for odd rot)
    QRectF embedUser;    // expected RAW /Rect for the embed rect
    QRectF fieldUser;    // expected RAW /Rect for the form field
    QRectF sigUser;      // expected RAW /Rect for the signature field
};

QList<Shape> shapes()
{
    return {
        // rot 0 Letter: uy = 792-vy1..792-vy0
        { PoDoFo::Rect(0, 0, 612, 792), 0, QSizeF(612, 792),
          QRectF(QPointF(60, 662), QPointF(160, 712)),
          QRectF(QPointF(40, 702), QPointF(160, 742)),
          QRectF(QPointF(200, 442), QPointF(350, 492)) },
        // rot 90 Letter: ux = vy0..vy1, uy = vx0..vx1 (formula never reads W/H)
        { PoDoFo::Rect(0, 0, 612, 792), 90, QSizeF(792, 612),
          QRectF(QPointF(80, 60), QPointF(130, 160)),
          QRectF(QPointF(50, 40), QPointF(90, 160)),
          QRectF(QPointF(300, 200), QPointF(350, 350)) },
        // rot 180 Letter: ux = 612-vx1..612-vx0, uy = vy0..vy1 (direct)
        { PoDoFo::Rect(0, 0, 612, 792), 180, QSizeF(612, 792),
          QRectF(QPointF(452, 80), QPointF(552, 130)),
          QRectF(QPointF(452, 50), QPointF(572, 90)),
          QRectF(QPointF(262, 300), QPointF(412, 350)) },
        // rot 270 Letter (THE W2B-1 shape): ux = 612-vy1..612-vy0,
        // uy = 792-vx1..792-vx0 — reads BOTH W and H.
        { PoDoFo::Rect(0, 0, 612, 792), 270, QSizeF(792, 612),
          QRectF(QPointF(482, 632), QPointF(532, 732)),
          QRectF(QPointF(522, 632), QPointF(562, 752)),
          QRectF(QPointF(262, 442), QPointF(312, 592)) },
        // rot 90 + offset origin [0 200 612 1042]
        { PoDoFo::Rect(0, 200, 612, 842), 90, QSizeF(842, 612),
          QRectF(QPointF(80, 260), QPointF(130, 360)),
          QRectF(QPointF(50, 240), QPointF(90, 360)),
          QRectF(QPointF(300, 400), QPointF(350, 550)) },
        // rot 270 + offset origin (the committed fixtures' blind spot);
        // raw H is 842: uy = 200+(842-vx1)..200+(842-vx0)
        { PoDoFo::Rect(0, 200, 612, 842), 270, QSizeF(842, 612),
          QRectF(QPointF(482, 882), QPointF(532, 982)),
          QRectF(QPointF(522, 882), QPointF(562, 1002)),
          QRectF(QPointF(262, 692), QPointF(312, 842)) },
    };
}

QString buildFixture(const QString& path)
{
    try {
        PoDoFo::PdfMemDocument doc;
        for (const Shape& s : shapes()) {
            PoDoFo::PdfPage& page = doc.GetPages().CreatePage(s.media);
            page.SetRotation(s.rotation);
        }
        doc.Save(path.toUtf8().constData());
        return path;
    } catch (const std::exception& e) {
        qWarning() << "buildFixture failed:" << e.what();
        return QString();
    }
}

// Raw /Rect arrays straight off the page's annotation dictionaries.
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

QList<QRectF> pdfiumAnnotRects(const QString& path, int pageIndex, QSizeF* pageSizeOut = nullptr)
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

// Letter-page PDF with one text line, carrying /Rotate on the page dict
// (raw bytes; content user space is rotation-independent).
bool buildLabelPdf(const QString& path, int rotation, int x, int y, const QByteArray& label)
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
    addObj("<</Type/Page/Parent 2 0 R/MediaBox[0 0 612 792]/Rotate " +
           QByteArray::number(rotation) + "/Contents 4 0 R" +
           "/Resources<</Font<</F1 5 0 R>>>>>>");
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

class TestRotate270PageSpace : public QObject {
    Q_OBJECT

    QTemporaryDir m_dir;

    QString fixturePath() const { return m_dir.filePath("rot-fixture.pdf"); }
    QString outPath(const char* name) const { return m_dir.filePath(QLatin1String(name)); }

private slots:
    void init()
    {
        QVERIFY(m_dir.isValid());
        QVERIFY(!buildFixture(fixturePath()).isEmpty());
    }

    // ── the root-cause pin: the adapter's geometry == the FILE BYTES ────────
    //
    // GetMediaBoxRaw() is PoDoFo's raw inheritable-/MediaBox accessor; the
    // file bytes here are the literals buildFixture wrote. The W2B-1 defect
    // was pageGeometry() reporting the NORMALIZED (swapped) numbers instead.
    void pageGeometryMatchesTheFileBytesOnEveryRotation()
    {
        const QList<Shape> all = shapes();
        try {
            PoDoFo::PdfMemDocument doc;
            doc.Load(fixturePath().toUtf8().constData());
            for (int p = 0; p < all.size(); ++p) {
                auto& page = doc.GetPages().GetPageAt(p);
                const auto raw = page.GetMediaBoxRaw();
                const Shape& s = all[p];
                QCOMPARE(raw.X1, s.media.X);
                QCOMPARE(raw.Y1, s.media.Y);
                QCOMPARE(raw.X2, s.media.X + s.media.Width);
                QCOMPARE(raw.Y2, s.media.Y + s.media.Height);

                const gp::PageSpace::PageGeometry geo = gp::PageSpace::pageGeometry(page);
                QCOMPARE(geo.x0, s.media.X);
                QCOMPARE(geo.y0, s.media.Y);
                QCOMPARE(geo.width, s.media.Width);
                QCOMPARE(geo.height, s.media.Height);
                QCOMPARE(geo.rotation, s.rotation);

                // PDFium independently confirms the DISPLAYED size (the
                // numbers FPDF_GetPageWidthF/HeightF report).
                QSizeF pdfiumSize;
                pdfiumAnnotRects(fixturePath(), p, &pdfiumSize);
                QVERIFY(std::fabs(pdfiumSize.width() - s.display.width()) < 0.5
                        && std::fabs(pdfiumSize.height() - s.display.height()) < 0.5);
            }
        } catch (const std::exception& e) {
            QFAIL(qPrintable(QString("raw geometry walk failed: %1").arg(e.what())));
        }
    }

    // ── annotation embed + extract, every rotation, hardcoded literals ──────
    void annotationEmbedAndExtractFollowTheLawOnEveryRotation()
    {
        const QRectF drawn(60, 80, 100, 50); // display space, every page
        PoDoFoBackend engine;
        QVERIFY(engine.loadDocument(fixturePath()));

        QList<AnnotationItem> items;
        const QList<Shape> all = shapes();
        for (int p = 0; p < all.size(); ++p) {
            AnnotationItem item;
            item.pageIndex = p;
            item.mode = ToolMode::DrawRectangle;
            item.rect = drawn;
            item.text = QStringLiteral("w2b1-rot-%1").arg(p);
            items.append(item);
        }
        const QString out = outPath("embed-all.pdf");
        QVERIFY(engine.embedAnnotations(fixturePath(), out, items));

        PoDoFoBackend reader;
        const QList<AnnotationItem> back = reader.extractAnnotations(out);
        QCOMPARE(back.size(), items.size());

        for (int p = 0; p < all.size(); ++p) {
            const Shape& s = all[p];
            const QList<QRectF> raw = rawAnnotRects(out, p);
            QVERIFY2(raw.size() == 1,
                     qPrintable(QString("page %1 (rot %2): %3 annots")
                                    .arg(p).arg(s.rotation).arg(raw.size())));
            QVERIFY2(rectClose(raw.first(), s.embedUser),
                     qPrintable(QString("W2B-1: page %1 (rot %2) raw /Rect %3 != law literal %4")
                                    .arg(p).arg(s.rotation)
                                    .arg(rectStr(raw.first()), rectStr(s.embedUser))));

            const QList<QRectF> viaPdfium = pdfiumAnnotRects(out, p);
            QVERIFY2(!viaPdfium.isEmpty(),
                     qPrintable(QString("page %1: PDFium saw no annot").arg(p)));
            QVERIFY2(rectClose(viaPdfium.first(), s.embedUser),
                     qPrintable(QString("page %1 (rot %2) PDFium %3 != %4")
                                    .arg(p).arg(s.rotation)
                                    .arg(rectStr(viaPdfium.first()), rectStr(s.embedUser))));

            // Read-back surfaces the drawn spot on every rotation.
            const AnnotationItem* item = nullptr;
            for (const AnnotationItem& b : back)
                if (b.pageIndex == p) item = &b;
            QVERIFY2(item != nullptr, qPrintable(QString("page %1 missing from read-back").arg(p)));
            QVERIFY2(rectClose(item->rect, drawn),
                     qPrintable(QString("page %1 (rot %2) read-back %3 != drawn %4")
                                    .arg(p).arg(s.rotation)
                                    .arg(rectStr(item->rect), rectStr(drawn))));
        }
    }

    // ── form field creation, every rotation, hardcoded literals ─────────────
    void formFieldCreationFollowsTheLawOnEveryRotation()
    {
        const QList<Shape> all = shapes();
        for (int p = 0; p < all.size(); ++p) {
            const Shape& s = all[p];
            FormManager forms;
            const QString out = outPath(QString("field-p%1.pdf").arg(p).toUtf8().constData());
            QVERIFY2(forms.addTextField(fixturePath(), p, QRectF(40, 50, 120, 40),
                                        QStringLiteral("Field%1").arg(p), out),
                     qPrintable(QString("page %1 (rot %2): addTextField failed")
                                    .arg(p).arg(s.rotation)));
            const QList<QRectF> raw = rawAnnotRects(out, p);
            QVERIFY(raw.size() == 1);
            QVERIFY2(rectClose(raw.first(), s.fieldUser),
                     qPrintable(QString("W2B-1: page %1 (rot %2) field /Rect %3 != law literal %4")
                                    .arg(p).arg(s.rotation)
                                    .arg(rectStr(raw.first()), rectStr(s.fieldUser))));

            const QList<QRectF> viaPdfium = pdfiumAnnotRects(out, p);
            QVERIFY(!viaPdfium.isEmpty());
            QVERIFY2(rectClose(viaPdfium.first(), s.fieldUser),
                     qPrintable(QString("page %1 (rot %2) field PDFium %3 != %4")
                                    .arg(p).arg(s.rotation)
                                    .arg(rectStr(viaPdfium.first()), rectStr(s.fieldUser))));
        }
    }

    // ── SWEEP-W1 F5 clean path, every rotation ───────────────────────────────
    //
    // The containment must judge the converted USER rect against the RAW
    // MediaBox (the same box viewerToUser maps into). With the pre-W2B-1-fix
    // pairing (raw-space rect vs normalized box) a perfectly placed /Rotate
    // 270 anchor is false-refused; with the pre-law base the transposed rect
    // sailed through containment while landing in the wrong place.
    void signatureFieldPlacementFollowsTheLawOnEveryRotation()
    {
        const QList<Shape> all = shapes();
        for (int p = 0; p < all.size(); ++p) {
            const Shape& s = all[p];
            const QRectF anchor(200, 300, 150, 50); // on-page on every shape

            // Positive: accepted, stored at the law literal.
            {
                const QString out = outPath(QString("sig-p%1.pdf").arg(p).toUtf8().constData());
                QVector<gp::SignatureFieldCreator::Spec> specs;
                gp::SignatureFieldCreator::Spec spec;
                spec.fieldName = QStringLiteral("SigField%1").arg(p);
                spec.pageIndex = p;
                spec.viewerRect = anchor;
                specs.append(spec);
                QString err;
                QVERIFY2(gp::SignatureFieldCreator::createSignatureFields(
                             fixturePath(), specs, out, &err),
                         qPrintable(QString("page %1 (rot %2): on-page anchor refused: %3")
                                        .arg(p).arg(s.rotation).arg(err)));
                const QList<QRectF> raw = rawAnnotRects(out, p);
                QVERIFY(raw.size() == 1);
                QVERIFY2(rectClose(raw.first(), s.sigUser),
                         qPrintable(QString("W2B-1: page %1 (rot %2) sig /Rect %3 != law literal %4")
                                        .arg(p).arg(s.rotation)
                                        .arg(rectStr(raw.first()), rectStr(s.sigUser))));
            }
            // Negative control: the off-page anchor is refused on EVERY
            // rotation (fail-loud, nothing written).
            {
                const QString out = outPath(QString("sig-off-p%1.pdf").arg(p).toUtf8().constData());
                QVector<gp::SignatureFieldCreator::Spec> specs;
                gp::SignatureFieldCreator::Spec spec;
                spec.fieldName = QStringLiteral("SigOff%1").arg(p);
                spec.pageIndex = p;
                spec.viewerRect = QRectF(200000, 200000, 150, 50);
                specs.append(spec);
                QString err;
                QVERIFY2(!gp::SignatureFieldCreator::createSignatureFields(
                             fixturePath(), specs, out, &err),
                         qPrintable(QString("page %1 (rot %2): off-page anchor ACCEPTED")
                                        .arg(p).arg(s.rotation)));
                QVERIFY2(err.contains(QStringLiteral("outside page")),
                         qPrintable(QString("page %1 (rot %2): refusal wording: %3")
                                        .arg(p).arg(s.rotation).arg(err)));
                QVERIFY2(!QFile::exists(out),
                         "a refused placement must not write the destination");
            }
        }
    }

    // ── emergence E-3 (SWEEP-W3-EMERGENCE §2d): the badge-anchor read-back ──
    //
    // SignatureManager::signatureFieldAnchors feeds SignaturesPanel's badge
    // painting. A field placed at the display rect (200,300,150x50) — stored
    // RAW at the hand-computed law literal sigUser — must read back as EXACTLY
    // that display rect on every shape. Pre-fix the flip used the rotation-
    // NORMALIZED GetMediaBox().Height (W/H swapped on /Rotate 90/270): on the
    // 612x792 shapes the badge Y was computed from 612 instead of 792 and the
    // MediaBox lower-left origin was dropped, so on every rotated, non-square
    // page the badge painted away from the displayed field.
    void signatureAnchorReadBackMatchesTheDisplayedFieldOnEveryRotation()
    {
        const QList<Shape> all = shapes();
        for (int p = 0; p < all.size(); ++p) {
            const Shape& s = all[p];
            const QRectF anchor(200, 300, 150, 50); // on-page on every shape

            const QString out = outPath(QString("sig-anchor-p%1.pdf").arg(p).toUtf8().constData());
            QVector<gp::SignatureFieldCreator::Spec> specs;
            gp::SignatureFieldCreator::Spec spec;
            spec.fieldName = QStringLiteral("SigAnchor%1").arg(p);
            spec.pageIndex = p;
            spec.viewerRect = anchor;
            specs.append(spec);
            QString err;
            QVERIFY2(gp::SignatureFieldCreator::createSignatureFields(
                         fixturePath(), specs, out, &err),
                     qPrintable(QString("page %1 (rot %2): placement refused: %3")
                                    .arg(p).arg(s.rotation).arg(err)));

            SignatureManager mgr;
            const auto anchors = mgr.signatureFieldAnchors(out);
            const ISignatureManager::SignatureFieldAnchor* a = nullptr;
            for (const auto& cand : anchors) {
                if (cand.pageIndex == p
                        && cand.fieldName == QStringLiteral("SigAnchor%1").arg(p)) {
                    a = &cand;
                    break;
                }
            }
            QVERIFY2(a, qPrintable(QString("page %1 (rot %2): anchor missing")
                                       .arg(p).arg(s.rotation)));
            QVERIFY2(rectClose(a->rect, anchor),
                     qPrintable(QString("W2B-1/E-3: page %1 (rot %2) badge anchor %3 "
                                        "!= displayed field %4")
                                    .arg(p).arg(s.rotation)
                                    .arg(rectStr(a->rect), rectStr(anchor))));
        }
    }

    // ── SL3 suggestions stay inside the DISPLAYED page, every rotation ──────
    //
    // autoDetectFields clamps the suggestion against the page dims in RAW
    // USER space; with the swapped box it clamped against the wrong edge and
    // mapped suggestions off the displayed page on /Rotate 270.
    void autoDetectSuggestionStaysInsideTheDisplayedPageOnEveryRotation()
    {
        for (const int rotation : { 0, 90, 180, 270 }) {
            const QString pdf = m_dir.filePath(QString("label-r%1.pdf").arg(rotation));
            QVERIFY(buildLabelPdf(pdf, rotation, 90, 640, "Amount: "));
            FormManager fm;
            const auto suggestions = fm.autoDetectFields(pdf, 0);
            QVERIFY2(!suggestions.isEmpty(),
                     qPrintable(QString("rot %1: an 'Amount:' label must produce a suggestion")
                                    .arg(rotation)));
            const QSizeF display = (rotation == 90 || rotation == 270)
                                       ? QSizeF(792, 612) : QSizeF(612, 792);
            for (const FieldSuggestion& s : suggestions) {
                QVERIFY2(s.rect.x() >= -0.5 && s.rect.y() >= -0.5
                             && s.rect.right() <= display.width() + 0.5
                             && s.rect.bottom() <= display.height() + 0.5,
                         qPrintable(QString("W2B-1: rot %1 suggestion %2 outside the "
                                            "displayed page %3x%4")
                                        .arg(rotation).arg(rectStr(s.rect))
                                        .arg(display.width()).arg(display.height())));
            }
        }
    }
};

QTEST_MAIN(TestRotate270PageSpace)
#include "TestRotate270PageSpace.moc"
