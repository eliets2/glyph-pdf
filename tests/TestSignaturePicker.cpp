// SPDX-License-Identifier: Apache-2.0
// §9.7 P0 regression test (audit 2026-07-01): the signature tool is draw-only
// today. The Draw/Type/Upload picker must feed Type and Upload signatures into
// the SAME annotation persistence as Draw (no second source of truth):
//   - Type: typed text rendered to a real ink image (non-blank pixels), blank
//     text rejected
//   - Upload: an image file becomes the signature image; unreadable files
//     rejected with an error, huge images capped
//   - both persist as real PDF annotations (/Stamp + image appearance stream)
//     that survive a PoDoFo save/reload round-trip and map back to their own
//     ToolMode instead of degrading to comment notes
//   - Draw freehand is unchanged (/Ink round-trip)
//   - AnnotationLayer paints the signature image (not a placeholder)
//   - dialog cancel produces nothing
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include "core/AnnotationSerializer.h"
#include "engines/podofo/PoDoFoBackend.h"
#include "ui/AnnotationLayer.h"
#include "ui/SignaturePicker.h"

// The §9.7 ToolMode ordinals are appended AFTER FormAddCalculated (=34) so the
// AnnotationSerializer sidecar ordinals stay stable. The persistence cases
// below deliberately use the raw ordinals so they compile — and fail — against
// a reverted src tree, where extractAnnotations maps unknown subtypes to
// AddComment and the serializer drops out-of-range ordinals.
constexpr int kAddSignatureTypedOrdinal  = 35;
constexpr int kAddSignatureUploadOrdinal = 36;

namespace {

// True when the image carries at least one visible, non-white-ish pixel —
// distinguishes rendered ink from a blank canvas.
bool hasInkPixel(const QImage &img)
{
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x) {
            const QColor c = img.pixelColor(x, y);
            if (c.alpha() > 40 && (c.red() < 200 || c.green() < 200 || c.blue() < 200))
                return true;
        }
    return false;
}

const AnnotationItem *findByMode(const QList<AnnotationItem> &annos, int modeOrdinal)
{
    for (const auto &a : annos)
        if (static_cast<int>(a.mode) == modeOrdinal)
            return &a;
    return nullptr;
}

// Minimal one-page PDF (same seed TestShapeInkPersistence uses).
QString makeSeedPdf(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return {};
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
    return path;
}

} // namespace

class TestSignaturePicker : public QObject {
    Q_OBJECT
private slots:
    void typedSignatureRendersInkAndPersists();
    void uploadedSignatureSurvivesRoundTrip();
    void drawFreehandPathUnchanged();
    void typedAndUploadOrdinalsSurviveSidecar();
    void placedImagePaintsThroughAnnotationLayer();
    void pickerDialogGatesEmptyTextAndCancel();
    void hugeUploadedImageIsCapped();
    // §9.7 P1: initials variant — a 4th picker kind that derives the ink from
    // the user's full name ("John Hancock" → "JH") at a smaller default size.
    void initialsVariantRendersCompactInk();
    void initialsTabAndPlacementUseTypedPath();
};

void TestSignaturePicker::typedSignatureRendersInkAndPersists()
{
    // Seam contract: blank typed text is rejected exactly like a cancel.
    QImage blank = SignatureContent::renderTyped(QStringLiteral("   "), QStringLiteral("Arial"),
                                                 36, Qt::darkBlue);
    QVERIFY2(blank.isNull(), "blank typed text must produce a null image");

    // Type mode renders a non-null image with the typed text baked in.
    const QImage ink = SignatureContent::renderTyped(QStringLiteral("John Hancock"),
                                                     QStringLiteral("Arial"), 48, Qt::darkBlue);
    QVERIFY2(!ink.isNull(), "typed signature must render to a real image");
    QVERIFY2(hasInkPixel(ink), "typed signature canvas must contain visible ink pixels");

    AnnotationItem anno = SignatureContent::makeAnnotation(
        SignatureContent::Kind::Typed, 0, QRectF(60, 500, 220, 80), ink,
        QStringLiteral("John Hancock"));
    QCOMPARE(static_cast<int>(anno.mode), kAddSignatureTypedOrdinal);
    QVERIFY(!anno.image.isNull());

    // Persistence: real annotation, survives save/reload, text kept as /Contents.
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString seed = makeSeedPdf(tmp.filePath("seed.pdf"));
    QVERIFY(!seed.isEmpty());
    PoDoFoBackend backend;
    const QString out = tmp.filePath("typed.pdf");
    QVERIFY(backend.embedAnnotations(seed, out, { anno }));

    // Hold the list: findByMode returns a pointer into it — binding the
    // temporary directly would leave `back` dangling (use-after-free).
    const QList<AnnotationItem> restoredAnnos = backend.extractAnnotations(out);
    const AnnotationItem *back = findByMode(restoredAnnos, kAddSignatureTypedOrdinal);
    QVERIFY2(back, "typed signature must survive as its own ToolMode, not degrade "
                   "to a comment note");
    QVERIFY(qAbs(back->rect.x() - 60.0) < 0.01);
    QVERIFY(qAbs(back->rect.width() - 220.0) < 0.01);
    QCOMPARE(back->text, QStringLiteral("John Hancock"));
    QVERIFY2(!back->image.isNull(), "the signature image must be persisted inside "
                                    "the PDF and restored on reload");
    QCOMPARE(back->image.size(), ink.size());
    QVERIFY2(hasInkPixel(back->image), "restored signature image must still carry ink");
}

void TestSignaturePicker::uploadedSignatureSurvivesRoundTrip()
{
    // Asymmetric pattern: opaque red top-left quadrant, transparent elsewhere —
    // catches color-space AND write/read orientation bugs.
    QImage src(41, 23, QImage::Format_RGBA8888);
    src.fill(Qt::transparent);
    for (int y = 0; y < 12; ++y)
        for (int x = 0; x < 21; ++x)
            src.setPixelColor(x, y, QColor(220, 20, 20, 255));

    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString png = tmp.filePath("sig.png");
    QVERIFY(src.save(png));

    QString err;
    const QImage up = SignatureContent::loadUploaded(png, &err);
    QVERIFY2(err.isEmpty(), qPrintable(QStringLiteral("valid upload must not error: %1").arg(err)));
    QCOMPARE(up.size(), src.size());

    // Edge case: unreadable/missing file → null image + error, no crash.
    QImage bad = SignatureContent::loadUploaded(tmp.filePath("missing.png"), &err);
    QVERIFY(bad.isNull());
    QVERIFY2(!err.isEmpty(), "unreadable upload must report an error message");

    AnnotationItem anno = SignatureContent::makeAnnotation(
        SignatureContent::Kind::Upload, 0, QRectF(300, 600, 90, 50), up);
    QCOMPARE(static_cast<int>(anno.mode), kAddSignatureUploadOrdinal);

    const QString seed = makeSeedPdf(tmp.filePath("seed2.pdf"));
    QVERIFY(!seed.isEmpty());
    PoDoFoBackend backend;
    const QString out = tmp.filePath("upload.pdf");
    QVERIFY(backend.embedAnnotations(seed, out, { anno }));

    const QList<AnnotationItem> restoredUpload = backend.extractAnnotations(out);
    const AnnotationItem *back = findByMode(restoredUpload, kAddSignatureUploadOrdinal);
    QVERIFY2(back, "uploaded signature must survive a save/reload round-trip as a "
                   "real PDF annotation");
    QVERIFY2(!back->image.isNull(), "uploaded image must be persisted inside the PDF");
    QCOMPARE(back->image.size(), src.size());
    const QColor tl = back->image.pixelColor(3, 3);
    QVERIFY2(tl.alpha() > 180 && tl.red() > 180 && tl.green() < 80 && tl.blue() < 80,
             qPrintable(QStringLiteral("opaque ink quadrant must round-trip, got "
                                      "rgba(%1,%2,%3,%4)")
                           .arg(tl.red()).arg(tl.green()).arg(tl.blue()).arg(tl.alpha())));
    const QColor br = back->image.pixelColor(back->image.width() - 4, back->image.height() - 4);
    QVERIFY2(br.alpha() < 60,
             qPrintable(QStringLiteral("transparent quadrant must stay transparent via "
                                       "/SMask, got rgba(%1,%2,%3,%4)")
                           .arg(br.red()).arg(br.green()).arg(br.blue()).arg(br.alpha())));
}

void TestSignaturePicker::drawFreehandPathUnchanged()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString seed = makeSeedPdf(tmp.filePath("seed3.pdf"));
    QVERIFY(!seed.isEmpty());
    PoDoFoBackend backend;

    AnnotationItem ink;
    ink.mode = ToolMode::AddSignature;      // the pre-§9.7 draw flow, untouched
    ink.pageIndex = 0;
    ink.rect = QRectF(20, 20, 200, 100);
    ink.points << QPointF(20, 30) << QPointF(60, 80) << QPointF(120, 40) << QPointF(220, 90);

    const QString out = tmp.filePath("freehand.pdf");
    QVERIFY(backend.embedAnnotations(seed, out, { ink }));

    const QList<AnnotationItem> back = backend.extractAnnotations(out);
    QCOMPARE(back.size(), 1);
    // Draw keeps the existing freehand path: /Ink → DrawFreehand with points.
    QCOMPARE(back.first().mode, ToolMode::DrawFreehand);
    QCOMPARE(back.first().points.size(), 4);
    QVERIFY(qAbs(back.first().points.first().x() - 20.0) < 0.01);
    // The new modes must not hijack the ink path.
    QVERIFY(!findByMode(back, kAddSignatureTypedOrdinal));
    QVERIFY(!findByMode(back, kAddSignatureUploadOrdinal));
}

void TestSignaturePicker::typedAndUploadOrdinalsSurviveSidecar()
{
    // The .ann sidecar is the viewer overlay's source of truth between
    // sessions, so the new ordinals must pass AnnotationSerializer's range
    // gate instead of being silently dropped as "invalid ToolMode".
    AnnotationItem typed;
    typed.mode = static_cast<ToolMode>(kAddSignatureTypedOrdinal);
    typed.pageIndex = 0;
    typed.rect = QRectF(60, 500, 220, 80);
    typed.text = QStringLiteral("John Hancock");

    AnnotationItem up;
    up.mode = static_cast<ToolMode>(kAddSignatureUploadOrdinal);
    up.pageIndex = 0;
    up.rect = QRectF(300, 600, 90, 50);

    const QList<AnnotationItem> back =
        AnnotationSerializer::fromJson(AnnotationSerializer::toJson({ typed, up }));
    QCOMPARE(back.size(), 2);
    QCOMPARE(static_cast<int>(back.first().mode), kAddSignatureTypedOrdinal);
    QCOMPARE(static_cast<int>(back.last().mode), kAddSignatureUploadOrdinal);
    QVERIFY(qAbs(back.first().rect.width() - 220.0) < 0.01);
}

void TestSignaturePicker::placedImagePaintsThroughAnnotationLayer()
{
    QImage img(40, 24, QImage::Format_RGBA8888);
    img.fill(QColor(220, 20, 20, 255));     // solid opaque red ink

    AnnotationItem anno = SignatureContent::makeAnnotation(
        SignatureContent::Kind::Upload, 0, QRectF(40, 40, 160, 90), img);

    AnnotationLayer layer;
    layer.resize(400, 300);
    layer.setAnnotations({ anno });
    const QImage shot = layer.grab().toImage();
    QVERIFY(!shot.isNull());
    const QRect placed = anno.rect.toAlignedRect().intersected(shot.rect());
    QVERIFY2(hasInkPixel(shot.copy(placed)),
             "AnnotationLayer must paint the signature image itself, not a "
             "placeholder box");
}

void TestSignaturePicker::pickerDialogGatesEmptyTextAndCancel()
{
    SignaturePickerDialog dlg;
    dlg.showTab(SignatureContent::Kind::Typed);
    QVERIFY2(!dlg.isAcceptEnabled(),
             "OK must stay disabled while the typed text is blank");
    auto *edit = dlg.findChild<QLineEdit *>(QStringLiteral("signatureTypeEdit"));
    QVERIFY2(edit, "type tab must expose its text field");
    edit->setText(QStringLiteral("John Hancock"));
    QVERIFY2(dlg.isAcceptEnabled(),
             "OK must enable once a non-empty signature text is typed");
    // Accept through the real OK button so the dialog computes its result the
    // same way a user's click does.
    auto *buttons = dlg.findChild<QDialogButtonBox *>();
    QVERIFY2(buttons, "dialog must expose its button box");
    buttons->button(QDialogButtonBox::Ok)->click();
    QCOMPARE(dlg.result(), static_cast<int>(QDialog::Accepted));
    QCOMPARE(dlg.acceptedKind(), SignatureContent::Kind::Typed);
    QVERIFY2(!dlg.acceptedImage().isNull(),
             "an accepted Type tab must carry the rendered signature image");

    // Cancel path: nothing is produced.
    SignaturePickerDialog cancelled;
    cancelled.reject();
    QCOMPARE(cancelled.result(), static_cast<int>(QDialog::Rejected));
    QVERIFY2(cancelled.acceptedImage().isNull(),
             "a cancelled picker must not carry a signature image");
}

void TestSignaturePicker::hugeUploadedImageIsCapped()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QImage huge(10040, 12, QImage::Format_RGB888);
    huge.fill(QColor(220, 20, 20));
    const QString png = tmp.filePath("huge.png");
    QVERIFY(huge.save(png));

    QString err;
    const QImage capped = SignatureContent::loadUploaded(png, &err);
    QVERIFY2(err.isEmpty(), qPrintable(QStringLiteral("huge upload must be capped, "
                                                      "not rejected: %1").arg(err)));
    QVERIFY2(!capped.isNull(), "huge upload must be capped, not rejected");
    QVERIFY2(capped.width() <= SignatureContent::kMaxImageDim &&
                 capped.height() <= SignatureContent::kMaxImageDim,
             qPrintable(QStringLiteral("capped dimensions must respect the %1 limit, "
                                      "got %2x%3")
                           .arg(SignatureContent::kMaxImageDim)
                           .arg(capped.width()).arg(capped.height())));
}

// ── §9.7 P1: the initials variant ───────────────────────────────────────────

void TestSignaturePicker::initialsVariantRendersCompactInk()
{
    // String seam: the first letter of each whitespace-separated token,
    // uppercased. Blank names produce no initials at all.
    QCOMPARE(SignatureContent::initialsForName(QStringLiteral("John Hancock")),
             QStringLiteral("JH"));
    QCOMPARE(SignatureContent::initialsForName(QStringLiteral("jean-luc picard")),
             QStringLiteral("JP"));
    QCOMPARE(SignatureContent::initialsForName(QStringLiteral("   ")), QString());
    QCOMPARE(SignatureContent::initialsForName(QStringLiteral("madonna")), QStringLiteral("M"));

    // Image seam: blank name is rejected exactly like a cancelled dialog;
    // a real name renders actual ink.
    QImage blank = SignatureContent::initialsFromName(QStringLiteral("   "),
                                                      QStringLiteral("Arial"), Qt::darkBlue);
    QVERIFY2(blank.isNull(), "a blank name must produce a null initials image");
    const QImage ink = SignatureContent::initialsFromName(QStringLiteral("John Hancock"),
                                                          QStringLiteral("Arial"),
                                                          Qt::darkBlue);
    QVERIFY2(!ink.isNull(), "a real name must render to a real initials image");
    QVERIFY2(hasInkPixel(ink), "the initials canvas must contain visible ink pixels");

    // The initials default (24pt) is smaller than the Type default (36pt) and
    // the render must actually be derived from the initials at that size.
    QCOMPARE(SignatureContent::kInitialsPointSizeDefault, 24);
    QCOMPARE(SignatureContent::kTypedPointSizeDefault, 36);
    const QImage sameSize = SignatureContent::renderTyped(
        QStringLiteral("JH"), QStringLiteral("Arial"),
        SignatureContent::kInitialsPointSizeDefault, Qt::darkBlue);
    QCOMPARE(ink.size(), sameSize.size());
    const QImage fullName = SignatureContent::renderTyped(
        QStringLiteral("John Hancock"), QStringLiteral("Arial"),
        SignatureContent::kInitialsPointSizeDefault, Qt::darkBlue);
    QVERIFY2(ink.width() < fullName.width(),
             "initials must render the derived monogram, not the full name");
}

void TestSignaturePicker::initialsTabAndPlacementUseTypedPath()
{
    // Dialog: a 4th tab gates on a non-empty name and produces the initials
    // image (with the full name kept as the searchable /Contents text).
    SignaturePickerDialog dlg;
    QCOMPARE(dlg.findChild<QTabWidget *>()->count(), 4);
    dlg.showTab(SignatureContent::Kind::Initials);
    QCOMPARE(dlg.findChild<QTabWidget *>()->currentIndex(), 3);
    QVERIFY2(!dlg.isAcceptEnabled(),
             "OK must stay disabled while the initials name is blank");
    auto *nameEdit = dlg.findChild<QLineEdit *>(QStringLiteral("signatureInitialsEdit"));
    QVERIFY2(nameEdit, "initials tab must expose its name field");
    nameEdit->setText(QStringLiteral("John Hancock"));
    QVERIFY2(dlg.isAcceptEnabled(),
             "OK must enable once a non-empty name is typed");
    auto *buttons = dlg.findChild<QDialogButtonBox *>();
    buttons->button(QDialogButtonBox::Ok)->click();
    QCOMPARE(dlg.result(), static_cast<int>(QDialog::Accepted));
    QCOMPARE(dlg.acceptedKind(), SignatureContent::Kind::Initials);
    QCOMPARE(dlg.acceptedText(), QStringLiteral("John Hancock"));
    QVERIFY2(!dlg.acceptedImage().isNull(), "an accepted Initials tab must carry the rendered image");
    QVERIFY2(hasInkPixel(dlg.acceptedImage()), "the accepted initials image must carry ink");

    // Placement seam: Initials reuses the EXISTING typed ToolMode (ordinal 35
    // — PdfEnums.h is frozen, so no new ordinal is minted for the variant).
    AnnotationItem anno = SignatureContent::makeAnnotation(
        SignatureContent::Kind::Initials, 0, QRectF(60, 500, 120, 60),
        dlg.acceptedImage(), QStringLiteral("John Hancock"));
    QCOMPARE(static_cast<int>(anno.mode), kAddSignatureTypedOrdinal);
    QVERIFY(!anno.image.isNull());
    QCOMPARE(anno.text, QStringLiteral("John Hancock"));

    // Persistence: the initials stamp survives as the same typed annotation.
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString seed = makeSeedPdf(tmp.filePath("seed4.pdf"));
    QVERIFY(!seed.isEmpty());
    PoDoFoBackend backend;
    const QString out = tmp.filePath("initials.pdf");
    QVERIFY(backend.embedAnnotations(seed, out, { anno }));
    const QList<AnnotationItem> restored = backend.extractAnnotations(out);
    const AnnotationItem *back = findByMode(restored, kAddSignatureTypedOrdinal);
    QVERIFY2(back, "initials signature must survive a save/reload round-trip");
    QCOMPARE(back->text, QStringLiteral("John Hancock"));
    QVERIFY2(!back->image.isNull(), "initials image must be persisted inside the PDF");
}

QTEST_MAIN(TestSignaturePicker)
#include "TestSignaturePicker.moc"
