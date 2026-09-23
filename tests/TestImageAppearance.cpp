// SPDX-License-Identifier: Apache-2.0
// Image stacking order and opacity (Wave 2B/2C port from the editing-parity
// line, rebuilt on gp::content byte-exact edits).
//
// Part 1 — gp::content (ContentSpans.h), pure byte-level contracts:
//   the lexer skips strings, comments and inline-image data and rejects an
//   unbalanced q/Q; restacking moves ONLY the image's own q..Q block, keeps
//   every other byte, and refuses when the move could change how the image
//   draws; the opacity wrap is inserted once and recognised afterwards.
//
// Part 2 — the real engine (PdfEditorEngine over PoDoFo), judged by RENDERED
//   pixels at the overlap of two images, not by call counts:
//   bring-to-front / send-to-back change which image is visible and persist
//   on disk; a refused edit leaves the file byte-identical; opacity blends
//   and a second opacity edit updates the same ExtGState instead of nesting;
//   a page whose /Resources are inherited keeps its images visible; undo of
//   the command restores the previous stacking.
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QUndoStack>
#include <QPdfDocument>
#include <podofo/podofo.h>
#include "engines/podofo/ContentSpans.h"
#include "engines/PdfEditorEngine.h"
#include "engines/DocumentSession.h"
#include "commands/ImageAppearanceCommand.h"
#include "commands/RotateImageCommand.h"
#include "commands/CheckedHistory.h"

using gp::content::EditResult;

namespace {

// Two overlapping images: ImA (red) at x100..300 y400..600 and ImB (blue) at
// x150..350 y450..650 in PDF user space — they overlap at x150..300 y450..600.
const char *kTwoImages =
    "q 200 0 0 200 100 400 cm /ImA Do Q\n"
    "q 200 0 0 200 150 450 cm /ImB Do Q\n";

QString makeTwoImagePdf(const QString &dir, const QString &name, const char *content,
                        bool inheritResources = false)
{
    const QString path = dir + QLatin1Char('/') + name;
    PoDoFo::PdfMemDocument doc;
    auto makeImage = [&](char r, char g, char b) -> PoDoFo::PdfObject & {
        std::string px;
        for (int i = 0; i < 16; ++i) { px.push_back(r); px.push_back(g); px.push_back(b); }
        auto &img = doc.GetObjects().CreateDictionaryObject();
        img.GetDictionary().AddKey("Type", PoDoFo::PdfName("XObject"));
        img.GetDictionary().AddKey("Subtype", PoDoFo::PdfName("Image"));
        img.GetDictionary().AddKey("Width", static_cast<int64_t>(4));
        img.GetDictionary().AddKey("Height", static_cast<int64_t>(4));
        img.GetDictionary().AddKey("ColorSpace", PoDoFo::PdfName("DeviceRGB"));
        img.GetDictionary().AddKey("BitsPerComponent", static_cast<int64_t>(8));
        img.GetOrCreateStream().SetData(PoDoFo::bufferview(px));
        return img;
    };
    auto &imA = makeImage(char(0xC8), char(0x1E), char(0x1E));
    auto &imB = makeImage(char(0x1E), char(0x1E), char(0xC8));
    auto &font = doc.GetObjects().CreateDictionaryObject();
    font.GetDictionary().AddKey("Type", PoDoFo::PdfName("Font"));
    font.GetDictionary().AddKey("Subtype", PoDoFo::PdfName("Type1"));
    font.GetDictionary().AddKey("BaseFont", PoDoFo::PdfName("Helvetica"));

    auto &page = doc.GetPages().CreatePage(
        PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
    PoDoFo::PdfDictionary xobjects;
    xobjects.AddKey(PoDoFo::PdfName("ImA"), PoDoFo::PdfObject(imA.GetIndirectReference()));
    xobjects.AddKey(PoDoFo::PdfName("ImB"), PoDoFo::PdfObject(imB.GetIndirectReference()));
    PoDoFo::PdfDictionary fonts;
    fonts.AddKey(PoDoFo::PdfName("F1"), PoDoFo::PdfObject(font.GetIndirectReference()));
    PoDoFo::PdfDictionary res;
    res.AddKey("XObject", PoDoFo::PdfObject(xobjects));
    res.AddKey("Font", PoDoFo::PdfObject(fonts));
    if (inheritResources) {
        // Resources live on the page-tree root; the page inherits them.
        auto *parent = page.GetDictionary().FindKey("Parent");
        parent->GetDictionary().AddKey("Resources", PoDoFo::PdfObject(res));
        page.GetDictionary().RemoveKey("Resources");
    } else {
        page.GetDictionary().AddKey("Resources", PoDoFo::PdfObject(res));
    }
    const std::string data(content);
    page.GetOrCreateContents().CreateStreamForAppending().SetData(PoDoFo::bufferview(data));
    doc.Save(path.toStdString());
    return path;
}

QByteArray fileBytes(const QString &path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

QByteArray pageContent(const QString &path)
{
    PoDoFo::PdfMemDocument doc;
    doc.Load(path.toStdString());
    PoDoFo::charbuff buf;
    doc.GetPages().GetPageAt(0).GetContents()->CopyTo(buf);
    return QByteArray(buf.data(), static_cast<int>(buf.size()));
}

// Rendered colour at PDF user-space point (x, y) on page 0 (A4, 1 px = 1 pt).
QColor pixelAt(const QString &path, double x, double y)
{
    QPdfDocument pdf;
    if (pdf.load(path) != QPdfDocument::Error::None) return {};
    const QImage img = pdf.render(0, QSize(595, 842));
    return img.pixelColor(int(x), int(842 - y));
}

bool isRed(const QColor &c) { return c.red() > 150 && c.blue() < 80; }
bool isBlue(const QColor &c) { return c.blue() > 150 && c.red() < 80; }

const QPointF kOverlap(225, 525);   // inside both images
const QPointF kOnlyA(120, 420);     // ImA only

} // namespace

class TestImageAppearance : public QObject {
    Q_OBJECT
private slots:
    // ── Part 1: gp::content ────────────────────────────────────────────────
    void lexerSkipsStringsCommentsAndInlineImages();
    void lexerRejectsUnbalancedOrUnterminatedContent();
    void restackMovesOnlyTheImageBlock();
    void restackWithinTheParentBlock();
    void restackRefusesWhenTheImageWouldChange();
    void restackIsUnchangedWhenAlreadyInPlace();
    void wrapIsInsertedOnceAndRecognised();
    void escapedNamesMatch();

    // ── Part 2: real engine, rendered result ───────────────────────────────
    void bringToFrontChangesTheVisibleImageAndPersists();
    void sendToBackChangesTheVisibleImage();
    void refusedRestackLeavesTheFileUntouched();
    void opacityBlendsAndUpdatesInPlace();
    void opacityOnInheritedResourcesKeepsImagesVisible();
    void commandUndoRestoresTheStacking();
    void customAngleRotationUndoesExactly();
    void moveResizeRotateReachTheRealImage();
    void listImagesReportsThePlacementMatrix();
    void matrixRewriteKeepsTheBlockBalanced();
};

// ── Part 1 ─────────────────────────────────────────────────────────────────

void TestImageAppearance::lexerSkipsStringsCommentsAndInlineImages()
{
    // "q"/"Q" inside a string, a comment and inline-image data are not
    // operators; the only real pair is the outer q..Q.
    const QByteArray s = "q (a (q) Q \\) q) Tj % Q q comment\n"
                         "BI /W 1 /H 1 /BPC 8 /CS /G ID \x01Q\x02q EI\n"
                         "/Im#41 Do Q";
    QList<gp::content::Token> toks;
    QVERIFY(gp::content::lex(s, &toks));
    int q = 0, Q = 0, inlineImages = 0;
    for (const auto &t : toks) {
        if (t.kind == gp::content::Token::Kind::Operator && t.text == "q") ++q;
        if (t.kind == gp::content::Token::Kind::Operator && t.text == "Q") ++Q;
        if (t.kind == gp::content::Token::Kind::InlineImage) ++inlineImages;
    }
    QCOMPARE(q, 1);
    QCOMPARE(Q, 1);
    QCOMPARE(inlineImages, 1);
}

void TestImageAppearance::lexerRejectsUnbalancedOrUnterminatedContent()
{
    QList<gp::content::Token> toks;
    QVERIFY(!gp::content::lex("q q Q", &toks));          // q left open
    QVERIFY(!gp::content::lex("Q", &toks));              // Q without q
    QVERIFY(!gp::content::lex("(never closed", &toks));  // string
    QVERIFY(!gp::content::lex("BI /W 1 ID xyz", &toks)); // inline image without EI
}

void TestImageAppearance::restackMovesOnlyTheImageBlock()
{
    const QByteArray a = "q 200 0 0 200 100 400 cm /ImA Do Q";
    const QByteArray b = "q 200 0 0 200 150 450 cm /ImB Do Q";
    const QByteArray text = "BT /F1 12 Tf (keep) Tj ET";
    const QByteArray s = a + "\n" + text + "\n" + b + "\n";

    QByteArray out;
    QCOMPARE(gp::content::restackImage(s, "ImA", true, &out), EditResult::Changed);
    QVERIFY(out.indexOf(b) < out.indexOf(a));             // A now paints last
    QVERIFY(out.indexOf(text) < out.indexOf(a));
    QCOMPARE(out.count(a), 1);                            // moved, not copied
    QVERIFY(out.contains(text) && out.contains(b));       // other bytes verbatim

    QCOMPARE(gp::content::restackImage(s, "ImB", false, &out), EditResult::Changed);
    QVERIFY(out.indexOf(b) < out.indexOf(a));             // B now paints first
    QVERIFY(out.indexOf(b) < out.indexOf(text));
}

void TestImageAppearance::restackWithinTheParentBlock()
{
    // A whole-page wrapper: the image moves to the end of the wrapper's body,
    // never outside it (that would drop the wrapper's cm).
    const QByteArray s = "q 1 0 0 1 0 0 cm q /ImA Do Q q /ImB Do Q Q\n";
    QByteArray out;
    QCOMPARE(gp::content::restackImage(s, "ImA", true, &out), EditResult::Changed);
    QVERIFY(out.indexOf("/ImB Do") < out.indexOf("/ImA Do"));
    QVERIFY(out.indexOf("/ImA Do") < out.lastIndexOf('Q'));
    QVERIFY(out.trimmed().endsWith('Q'));
    QList<gp::content::Token> toks;
    QVERIFY(gp::content::lex(out, &toks));                // still balanced

    // A non-identity cm at the parent level before the image: sending it to
    // the back of the parent would move it before that cm.
    const QByteArray flipped = "q 1 0 0 -1 0 842 cm q /ImA Do Q q /ImB Do Q Q\n";
    QCOMPARE(gp::content::restackImage(flipped, "ImB", false, &out), EditResult::StateInTheWay);
    QCOMPARE(gp::content::restackImage(flipped, "ImA", true, &out), EditResult::Changed);
}

void TestImageAppearance::restackRefusesWhenTheImageWouldChange()
{
    QByteArray out = "untouched";
    // Drawn at top level: its cm would travel without it.
    QCOMPARE(gp::content::restackImage("200 0 0 200 0 0 cm /ImA Do\nq /ImB Do Q\n",
                                       "ImA", true, &out), EditResult::NotIsolated);
    // Its q..Q also paints text.
    QCOMPARE(gp::content::restackImage("q /ImA Do BT /F1 9 Tf (x) Tj ET Q\nq /ImB Do Q\n",
                                       "ImA", true, &out), EditResult::SharedBlock);
    // A cm / gs / clip between old and new position at the parent level.
    QCOMPARE(gp::content::restackImage("q /ImA Do Q 2 0 0 2 0 0 cm q /ImB Do Q\n",
                                       "ImA", true, &out), EditResult::StateInTheWay);
    QCOMPARE(gp::content::restackImage("q /ImA Do Q /GS1 gs q /ImB Do Q\n",
                                       "ImA", true, &out), EditResult::StateInTheWay);
    QCOMPARE(gp::content::restackImage("q /ImA Do Q 0 0 10 10 re W n q /ImB Do Q\n",
                                       "ImA", true, &out), EditResult::StateInTheWay);
    // An identity cm is harmless.
    QCOMPARE(gp::content::restackImage("q /ImA Do Q 1 0 0 1 0 0 cm q /ImB Do Q\n",
                                       "ImA", true, &out), EditResult::Changed);
    // A stencil mask paints with the fill colour: a colour change is in the way.
    QCOMPARE(gp::content::restackImage("q /ImA Do Q 1 0 0 rg q /ImB Do Q\n",
                                       "ImA", true, &out, /*colorSensitive=*/true),
             EditResult::StateInTheWay);
    QCOMPARE(gp::content::restackImage("q /ImA Do Q 1 0 0 rg q /ImB Do Q\n",
                                       "ImA", true, &out), EditResult::Changed);
    // Missing image, broken stream.
    QCOMPARE(gp::content::restackImage(kTwoImages, "ImZ", true, &out), EditResult::NotFound);
    QCOMPARE(gp::content::restackImage("q /ImA Do", "ImA", true, &out), EditResult::Malformed);
}

void TestImageAppearance::restackIsUnchangedWhenAlreadyInPlace()
{
    QByteArray out = "untouched";
    QCOMPARE(gp::content::restackImage(kTwoImages, "ImB", true, &out), EditResult::Unchanged);
    QCOMPARE(gp::content::restackImage(kTwoImages, "ImA", false, &out), EditResult::Unchanged);
    QCOMPARE(out, QByteArray("untouched"));
}

void TestImageAppearance::wrapIsInsertedOnceAndRecognised()
{
    QByteArray out;
    QCOMPARE(gp::content::wrapImageInExtGState(kTwoImages, "ImB", "GSop1", &out),
             EditResult::Changed);
    QVERIFY(out.contains("q\n/GSop1 gs\n/ImB Do\nQ"));
    QVERIFY(out.contains("q 200 0 0 200 100 400 cm /ImA Do Q"));   // ImA untouched
    QByteArray again = "untouched";
    QCOMPARE(gp::content::wrapImageInExtGState(out, "ImB", "GSop1", &again),
             EditResult::Unchanged);
    QCOMPARE(again, QByteArray("untouched"));
    QCOMPARE(gp::content::wrapImageInExtGState(kTwoImages, "ImZ", "GSop1", &again),
             EditResult::NotFound);
}

void TestImageAppearance::escapedNamesMatch()
{
    QByteArray out;
    QCOMPARE(gp::content::restackImage("q /Im#41 Do Q\nq /ImB Do Q\n", "ImA", true, &out),
             EditResult::Changed);
}

// ── Part 2 ─────────────────────────────────────────────────────────────────

void TestImageAppearance::bringToFrontChangesTheVisibleImageAndPersists()
{
    QTemporaryDir dir;
    const QString f = makeTwoImagePdf(dir.path(), "front.pdf", kTwoImages);
    QVERIFY(isBlue(pixelAt(f, kOverlap.x(), kOverlap.y())));   // ImB paints last

    PdfEditorEngine engine;
    QVERIFY(engine.loadDocumentForEditing(f));
    QVERIFY(engine.setImageZOrder(0, QStringLiteral("ImA"), true));

    QVERIFY2(isRed(pixelAt(f, kOverlap.x(), kOverlap.y())), "ImA must now be on top, on disk");
    QVERIFY(isRed(pixelAt(f, kOnlyA.x(), kOnlyA.y())));         // not moved
    const auto images = engine.listImages(0);
    QCOMPARE(images.size(), 2);
    QCOMPARE(images.last().xobjectName, QStringLiteral("ImA"));
}

void TestImageAppearance::sendToBackChangesTheVisibleImage()
{
    QTemporaryDir dir;
    const QString f = makeTwoImagePdf(dir.path(), "back.pdf", kTwoImages);
    PdfEditorEngine engine;
    QVERIFY(engine.loadDocumentForEditing(f));
    QVERIFY(engine.setImageZOrder(0, QStringLiteral("ImB"), false));
    QVERIFY(isRed(pixelAt(f, kOverlap.x(), kOverlap.y())));
}

void TestImageAppearance::refusedRestackLeavesTheFileUntouched()
{
    QTemporaryDir dir;
    const QString f = makeTwoImagePdf(
        dir.path(), "shared.pdf",
        "q 200 0 0 200 100 400 cm /ImA Do 0.005 0 0 0.005 0 0 cm "
        "BT /F1 12 Tf (x) Tj ET Q\n"
        "q 200 0 0 200 150 450 cm /ImB Do Q\n");
    const QByteArray before = fileBytes(f);
    PdfEditorEngine engine;
    QVERIFY(engine.loadDocumentForEditing(f));
    QVERIFY(!engine.setImageZOrder(0, QStringLiteral("ImA"), true));
    QVERIFY(!engine.lastError().userMessage.isEmpty());
    QCOMPARE(fileBytes(f), before);
    QVERIFY(isBlue(pixelAt(f, kOverlap.x(), kOverlap.y())));
}

void TestImageAppearance::opacityBlendsAndUpdatesInPlace()
{
    QTemporaryDir dir;
    const QString f = makeTwoImagePdf(dir.path(), "opacity.pdf", kTwoImages);
    PdfEditorEngine engine;
    QVERIFY(engine.loadDocumentForEditing(f));

    QVERIFY(engine.setImageOpacity(0, QStringLiteral("ImB"), 0.5));
    const QColor half = pixelAt(f, kOverlap.x(), kOverlap.y());
    QVERIFY2(half.red() > 80 && half.blue() > 80,
             qPrintable(QStringLiteral("50% blue over red must blend, got %1").arg(half.name())));
    QCOMPARE(pageContent(f).count(" gs\n"), 1);

    QVERIFY(engine.setImageOpacity(0, QStringLiteral("ImB"), 0.25));
    QCOMPARE(pageContent(f).count(" gs\n"), 1);          // updated, not nested
    const QColor quarter = pixelAt(f, kOverlap.x(), kOverlap.y());
    QVERIFY2(quarter.red() > half.red(),
             "a lower opacity must let more of the red image through");
}

void TestImageAppearance::opacityOnInheritedResourcesKeepsImagesVisible()
{
    QTemporaryDir dir;
    const QString f = makeTwoImagePdf(dir.path(), "inherited.pdf", kTwoImages,
                                      /*inheritResources=*/true);
    QVERIFY(isBlue(pixelAt(f, kOverlap.x(), kOverlap.y())));
    PdfEditorEngine engine;
    QVERIFY(engine.loadDocumentForEditing(f));
    QVERIFY(engine.setImageOpacity(0, QStringLiteral("ImB"), 0.5));
    QVERIFY2(isRed(pixelAt(f, kOnlyA.x(), kOnlyA.y())),
             "the inherited XObjects must stay reachable once the page gets its own /Resources");
    const QColor blended = pixelAt(f, kOverlap.x(), kOverlap.y());
    QVERIFY(blended.red() > 80 && blended.blue() > 80);
}

void TestImageAppearance::commandUndoRestoresTheStacking()
{
    QTemporaryDir dir;
    const QString f = makeTwoImagePdf(dir.path(), "undo.pdf", kTwoImages);
    PdfEditorEngine engine;
    QVERIFY(engine.loadDocumentForEditing(f));
    DocumentSession doc;
    doc.beginDocument(f);
    QUndoStack stack;

    const QByteArray backup = engine.extractPageAsBytes(f, 0);
    QVERIFY(!backup.isEmpty());
    stack.push(new ImageAppearanceCommand(&engine, &doc, 0, QStringLiteral("ImA"),
                                          ImageAppearanceCommand::Kind::BringToFront,
                                          1.0, backup));
    QCOMPARE(stack.count(), 1);
    QVERIFY(isRed(pixelAt(f, kOverlap.x(), kOverlap.y())));

    QVERIFY(CheckedHistory::undo(&stack));
    QVERIFY2(isBlue(pixelAt(f, kOverlap.x(), kOverlap.y())), "undo must restore the stacking");

    QVERIFY(CheckedHistory::redo(&stack));
    QCOMPARE(stack.index(), 1);
    QVERIFY(isRed(pixelAt(f, kOverlap.x(), kOverlap.y())));
}

// "Rotate by Angle…" pushes RotateImageCommand, whose undo is the inverse
// rotation (no page backup) — so an arbitrary angle must invert exactly.
void TestImageAppearance::customAngleRotationUndoesExactly()
{
    QTemporaryDir dir;
    const QString f = makeTwoImagePdf(dir.path(), "angle.pdf", kTwoImages);
    PdfEditorEngine engine;
    QVERIFY(engine.loadDocumentForEditing(f));
    DocumentSession doc;
    doc.beginDocument(f);
    QUndoStack stack;

    auto imageA = [&]() {
        for (const auto &img : engine.listImages(0))
            if (img.xobjectName == QLatin1String("ImA")) return img;
        return PdfImageInfo{};
    };
    const PdfImageInfo before = imageA();
    QVERIFY(!before.xobjectName.isEmpty());

    stack.push(new RotateImageCommand(&engine, &doc, 0, QStringLiteral("ImA"), 30.0));
    QVERIFY2(qAbs(imageA().rotation - before.rotation) > 1.0, "the rotation must take effect");

    stack.undo();
    const PdfImageInfo after = imageA();
    QVERIFY2(qAbs(after.rotation - before.rotation) < 0.01,
             qPrintable(QStringLiteral("rotation %1 -> %2").arg(before.rotation).arg(after.rotation)));
    QVERIFY2(qAbs(after.placement.x() - before.placement.x()) < 0.5
                 && qAbs(after.placement.y() - before.placement.y()) < 0.5
                 && qAbs(after.placement.width() - before.placement.width()) < 0.5
                 && qAbs(after.placement.height() - before.placement.height()) < 0.5,
             "undo of a 30-degree rotation must restore the placement");
}

// rewriteImageMatrix used to match the image's Do as a plain operator —
// PoDoFo 1.x reports it as DoXObject — so move/resize/rotate returned false
// on every real image and the commands pushed no-op history entries.
void TestImageAppearance::moveResizeRotateReachTheRealImage()
{
    QTemporaryDir dir;
    const QString f = makeTwoImagePdf(dir.path(), "matrix.pdf", kTwoImages);
    PdfEditorEngine engine;
    QVERIFY(engine.loadDocumentForEditing(f));
    auto imageA = [&]() {
        for (const auto &img : engine.listImages(0))
            if (img.xobjectName == QLatin1String("ImA")) return img;
        return PdfImageInfo{};
    };

    QVERIFY(engine.moveImage(0, QStringLiteral("ImA"), 20, -30));
    QCOMPARE(imageA().placement.topLeft(), QPointF(120, 370));
    QVERIFY(isRed(pixelAt(f, 130, 380)));                       // on disk too

    QVERIFY(engine.resizeImage(0, QStringLiteral("ImA"), 100, 50));
    QCOMPARE(imageA().placement.size(), QSizeF(100, 50));

    QVERIFY(engine.rotateImage(0, QStringLiteral("ImA"), 90));
    QVERIFY2(qAbs(imageA().rotation - 90.0) < 0.01,
             qPrintable(QStringLiteral("rotation %1").arg(imageA().rotation)));
    // Rotation is about the image centre: before it the 100x50 image's centre
    // was (170, 395); the rotated image must be centred there too — on disk.
    QVERIFY(isRed(pixelAt(f, 170, 395)));
    QVERIFY(isRed(pixelAt(f, 170, 395 + 40)));                  // now 50 wide, 100 tall
    QVERIFY(!isRed(pixelAt(f, 120 + 90, 380)));                 // old right end: gone
}

// PdfVariantStack indexes from the top (stack[0] = last operand); listImages
// read the six cm operands forwards and reported every non-symmetric
// placement wrong — the base every image edit computes from.
void TestImageAppearance::listImagesReportsThePlacementMatrix()
{
    QTemporaryDir dir;
    const QString f = makeTwoImagePdf(dir.path(), "placement.pdf", kTwoImages);
    PdfEditorEngine engine;
    QVERIFY(engine.loadDocumentForEditing(f));
    const auto images = engine.listImages(0);
    QCOMPARE(images.size(), 2);
    QCOMPARE(images[0].xobjectName, QStringLiteral("ImA"));
    QCOMPARE(images[0].placement, QRectF(100, 400, 200, 200));
    QCOMPARE(images[0].rotation, 0.0);
    QCOMPARE(images[1].placement, QRectF(150, 450, 200, 200));
}

// The matrix rewrite replaces exactly the six cm operands: a "q" on the same
// line (the old line-based replace deleted it) and a " cm" inside a string
// survive, and the stream stays balanced.
void TestImageAppearance::matrixRewriteKeepsTheBlockBalanced()
{
    const QByteArray s = "BT /F1 9 Tf (5 cm) Tj ET q 200 0 0 200 100 400 cm /ImA Do Q\n";
    QByteArray out;
    QCOMPARE(gp::content::replaceImageMatrix(s, "ImA", "1 0 0 1 5 6", &out), EditResult::Changed);
    QCOMPARE(out, QByteArray("BT /F1 9 Tf (5 cm) Tj ET q 1 0 0 1 5 6 cm /ImA Do Q\n"));
    QCOMPARE(gp::content::replaceImageMatrix("200 0 0 200 0 0 cm /ImA Do\n", "ImA", "1 0 0 1 0 0", &out),
             EditResult::NotIsolated);                           // no block of its own
    QCOMPARE(gp::content::replaceImageMatrix("q /ImA Do Q\n", "ImA", "1 0 0 1 0 0", &out),
             EditResult::NotIsolated);                           // block sets no cm
}

QTEST_MAIN(TestImageAppearance)
#include "TestImageAppearance.moc"
