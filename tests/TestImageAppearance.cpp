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
#include "commands/MoveImageCommand.h"
#include "commands/RotateImageCommand.h"
#include "commands/CheckedHistory.h"

using gp::content::EditResult;

namespace {

// Two overlapping images: ImA (red) at x100..300 y400..600 and ImB (blue) at
// x150..350 y450..650 in PDF user space — they overlap at x150..300 y450..600.
const char *kTwoImages =
    "q 200 0 0 200 100 400 cm /ImA Do Q\n"
    "q 200 0 0 200 150 450 cm /ImB Do Q\n";

QString makeTwoImagePdf(const QString &dir, const QString &name, const QByteArray &content,
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
    const std::string data(content.constData(), static_cast<size_t>(content.size()));
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

// Rendered page 0 at 1 px = 1 pt (A4), for whole-image comparisons.
QImage renderPage(const QString &path)
{
    QPdfDocument pdf;
    if (pdf.load(path) != QPdfDocument::Error::None) return {};
    return pdf.render(0, QSize(595, 842));
}

// The six numbers of the image's own cm — the bytes move/resize/rotate
// actually rewrite — read back from the page content with the gp::content
// lexer, under the same rule replaceImageMatrix uses (the last cm at the Do's
// own depth, inside its own q..Q block). Empty when the placement sets no cm.
QList<double> localCmNumbers(const QString &path, const QByteArray &name)
{
    const QByteArray content = pageContent(path);
    QList<gp::content::Token> toks;
    if (!gp::content::lex(content, &toks)) return {};
    int doIdx = -1;
    for (int i = 1; i < toks.size(); ++i) {
        if (toks[i].kind == gp::content::Token::Kind::Operator && toks[i].text == "Do"
            && toks[i - 1].kind == gp::content::Token::Kind::Name
            && toks[i - 1].text == name) { doIdx = i; break; }
    }
    if (doIdx < 0) return {};
    int closed = 0;   // Q..q pairs between the Do and the current token
    for (int i = doIdx - 1; i >= 0; --i) {
        const auto &t = toks[i];
        if (t.kind != gp::content::Token::Kind::Operator) continue;
        if (t.text == "Q") { ++closed; continue; }
        if (t.text == "q") {
            if (closed > 0) { --closed; continue; }
            break;      // the Do's own block opens here: no cm of its own
        }
        if (t.text != "cm" || closed > 0 || t.depth != toks[doIdx].depth) continue;
        QList<double> v;
        for (int k = i - 6; k < i; ++k) {
            if (k < 0 || toks[k].kind != gp::content::Token::Kind::Number) return {};
            bool ok = false;
            v.append(content.mid(toks[k].start, toks[k].end - toks[k].start)
                         .toDouble(&ok));
            if (!ok) return {};
        }
        return v;
    }
    return {};
}

bool closeTo(double a, double b, double eps = 1e-6)
{
    return qAbs(a - b) < eps;
}

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

    // ── Part 3: CX-02 — edits under outer transforms keep the full matrix ──
    void moveUnderOuterScaleDoesNotDouble();
    void movePlusUndoRestoresAllSixCoefficients();
    void zeroMoveKeepsSkewAndReflectionPixelIdentical();
    void moveKeepsAPreRotatedPlacement();
    void resizeUnderOuterScaleSetsTheExactSize();
    void rotateUnderOuterScaleKeepsTheDrawnRect();

    // ── Part 4: CX-12 — deleteImage removes the parsed, isolated span ──────
    void removeImagePlacementSpanRules();
    void removeImagePlacementByOccurrence();
    void deleteImageRemovesTheOffsetZeroPlacement();
    void deleteImageKeepsNeighboursOverShapesAndEndings();
    void deleteImageRefusalLeavesTheFileUntouched();

    // ── Part 5: CX-08 — the inline image data extent is exact ──────────────
    void inlineImageRidesOverAFakeEi();
    void inlineImageLengthAndEodExtents();
    void inlineImageRefusesUnknownExtents();
    void opacityReachesTheRealImagePastInlineData();

    // ── Part 6: CX-11 — matrix replacement refuses shared blocks ───────────
    void replaceImageMatrixRefusesSharedBlocks();
    void moveImageRefusesASharedBlockKeepsNeighbours();

    // ── Part 7: CX-10 — a gs-only opacity wrapper is part of the own block ─
    void gsWrapperIsPartOfTheOwnBlock();
    void opacityThenEditsComposeAndUndo();
};

// ── Part 1 ─────────────────────────────────────────────────────────────────

void TestImageAppearance::lexerSkipsStringsCommentsAndInlineImages()
{
    // "q"/"Q" inside a string, a comment and inline-image data are not
    // operators; the only real pair is the outer q..Q. The inline data is
    // 4 bytes ("\x01Q\x02q"), so H must declare 4 rows of 1 byte (CX-08:
    // the extent comes from W×H, not from the first EI-shaped bytes).
    const QByteArray s = "q (a (q) Q \\) q) Tj % Q q comment\n"
                         "BI /W 1 /H 4 /BPC 8 /CS /G ID \x01Q\x02q EI\n"
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

// ── Part 3: CX-02 — the edit family must keep the full six-coefficient
// matrix ────────────────────────────────────────────────────────────────────
// move/resize/rotate used to rebuild the image's local cm from the page-space
// rect, so any enclosing cm was applied twice (Codex's nested-scale probe:
// move +10 turned (20,40,200,200) into (60,80,400,400) and the inverse move
// made it worse again), and the (w, h, rotation) breakdown dropped skew and
// reflection. The fix: listImages reports the full effective matrix and the
// base CTM before the image's own cm; the new local cm is desired × base⁻¹.

// Codex's probe, verbatim: an outer 2× scale around the image's own cm.
void TestImageAppearance::moveUnderOuterScaleDoesNotDouble()
{
    QTemporaryDir dir;
    const QString f = makeTwoImagePdf(dir.path(), "nested.pdf",
        "q 2 0 0 2 0 0 cm q 100 0 0 100 10 20 cm /ImA Do Q Q\n");
    PdfEditorEngine engine;
    QVERIFY(engine.loadDocumentForEditing(f));
    auto imageA = [&]() {
        for (const auto &img : engine.listImages(0))
            if (img.xobjectName == QLatin1String("ImA")) return img;
        return PdfImageInfo{};
    };
    QVERIFY(!imageA().xobjectName.isEmpty());
    QCOMPARE(imageA().placement, QRectF(20, 40, 200, 200));

    QVERIFY(engine.moveImage(0, QStringLiteral("ImA"), 10, 0));
    QVERIFY2(imageA().placement == QRectF(30, 40, 200, 200),
             qPrintable(QStringLiteral("move +10 under a 2x outer scale gave %1 %2 %3x%4")
                            .arg(imageA().placement.x())
                            .arg(imageA().placement.y())
                            .arg(imageA().placement.width())
                            .arg(imageA().placement.height())));
    QVERIFY(isRed(pixelAt(f, 35, 45)));     // on disk: 30..230 x 40..240

    // The inverse move (what MoveImageCommand's undo does) must restore the
    // original placement exactly — not compound the error again.
    QVERIFY(engine.moveImage(0, QStringLiteral("ImA"), -10, 0));
    QCOMPARE(imageA().placement, QRectF(20, 40, 200, 200));
}

// Move plus undo restores all six coefficients of the placement matrix.
void TestImageAppearance::movePlusUndoRestoresAllSixCoefficients()
{
    QTemporaryDir dir;
    const QString f = makeTwoImagePdf(dir.path(), "undomatrix.pdf",
        "q 2 0 0 2 0 0 cm q 100 0 0 100 10 20 cm /ImA Do Q Q\n");
    PdfEditorEngine engine;
    QVERIFY(engine.loadDocumentForEditing(f));
    DocumentSession doc;
    doc.beginDocument(f);
    QUndoStack stack;

    stack.push(new MoveImageCommand(&engine, &doc, 0, QStringLiteral("ImA"), 25, -15));
    QCOMPARE(stack.count(), 1);
    QVERIFY(CheckedHistory::undo(&stack));

    const QList<double> local = localCmNumbers(f, "ImA");
    QCOMPARE(local.size(), 6);
    const double expected[6] = { 100, 0, 0, 100, 10, 20 };
    for (int k = 0; k < 6; ++k)
        QVERIFY2(closeTo(local[k], expected[k]),
                 qPrintable(QStringLiteral("local cm coefficient %1: %2 != %3")
                                .arg(k).arg(local[k]).arg(expected[k])));
    // And the effective placement is the original one again.
    PdfEditorEngine reload;
    QVERIFY(reload.loadDocumentForEditing(f));
    for (const auto &img : reload.listImages(0))
        if (img.xobjectName == QLatin1String("ImA"))
            QCOMPARE(img.placement, QRectF(20, 40, 200, 200));
}

// A zero-distance move on a skewed, reflected placement must be pixel-
// identical and change no coefficient beyond 1e-6 — the old (w, h, rotation)
// rebuild destroyed both the skew and the reflection.
void TestImageAppearance::zeroMoveKeepsSkewAndReflectionPixelIdentical()
{
    QTemporaryDir dir;
    // Skew (b = 20) and vertical reflection (d < 0) under an outer anisotropic
    // scale — six distinct coefficients, none axis-aligned.
    const QString f = makeTwoImagePdf(dir.path(), "skewflip.pdf",
        "q 1.5 0 0 0.5 0 0 cm q 80 20 0 -100 30 200 cm /ImA Do Q Q\n");
    PdfEditorEngine engine;
    QVERIFY(engine.loadDocumentForEditing(f));
    const QImage before = renderPage(f);
    QVERIFY(!before.isNull());

    QVERIFY(engine.moveImage(0, QStringLiteral("ImA"), 0, 0));

    const QList<double> local = localCmNumbers(f, "ImA");
    QCOMPARE(local.size(), 6);
    const double expected[6] = { 80, 20, 0, -100, 30, 200 };
    for (int k = 0; k < 6; ++k)
        QVERIFY2(closeTo(local[k], expected[k]),
                 qPrintable(QStringLiteral("local cm coefficient %1: %2 != %3")
                                .arg(k).arg(local[k]).arg(expected[k])));
    QCOMPARE(renderPage(f), before);        // pixel-identical
}

// An image placed pre-rotated (30 degrees) under an outer scale: a move must
// shift the position by exactly (dx, dy) and keep the rotation.
void TestImageAppearance::moveKeepsAPreRotatedPlacement()
{
    QTemporaryDir dir;
    // 100-unit axes rotated 30 degrees, under a 2x outer scale.
    const QString f = makeTwoImagePdf(dir.path(), "prerotated.pdf",
        "q 2 0 0 2 0 0 cm q 86.60254037844387 50 -50 86.60254037844387 10 20 cm"
        " /ImA Do Q Q\n");
    PdfEditorEngine engine;
    QVERIFY(engine.loadDocumentForEditing(f));
    auto imageA = [&]() {
        for (const auto &img : engine.listImages(0))
            if (img.xobjectName == QLatin1String("ImA")) return img;
        return PdfImageInfo{};
    };
    QVERIFY(closeTo(imageA().rotation, 30.0));
    QVERIFY(closeTo(imageA().placement.width(), 200.0));

    QVERIFY(engine.moveImage(0, QStringLiteral("ImA"), 10, 0));
    QVERIFY2(closeTo(imageA().placement.x(), 30.0)
                 && closeTo(imageA().placement.y(), 40.0),
             qPrintable(QStringLiteral("position %1,%2 after +10,0")
                            .arg(imageA().placement.x()).arg(imageA().placement.y())));
    QVERIFY2(closeTo(imageA().placement.width(), 200.0, 1e-5)
                 && closeTo(imageA().placement.height(), 200.0, 1e-5),
             "the outer scale must not be applied a second time");
    QVERIFY(closeTo(imageA().rotation, 30.0, 1e-5));
}

// A resize under an outer scale must set the REPORTED size exactly — the old
// code resized the local cm and let the outer scale double it.
void TestImageAppearance::resizeUnderOuterScaleSetsTheExactSize()
{
    QTemporaryDir dir;
    const QString f = makeTwoImagePdf(dir.path(), "resize.pdf",
        "q 2 0 0 2 0 0 cm q 100 0 0 100 10 20 cm /ImA Do Q Q\n");
    PdfEditorEngine engine;
    QVERIFY(engine.loadDocumentForEditing(f));

    QVERIFY(engine.resizeImage(0, QStringLiteral("ImA"), 100, 50));
    bool found = false;
    for (const auto &img : engine.listImages(0)) {
        if (img.xobjectName != QLatin1String("ImA")) continue;
        found = true;
        QVERIFY2(img.placement == QRectF(20, 40, 100, 50),
                 qPrintable(QStringLiteral("resized to %1 %2 %3x%4")
                                .arg(img.placement.x()).arg(img.placement.y())
                                .arg(img.placement.width()).arg(img.placement.height())));
    }
    QVERIFY(found);
    QVERIFY(isRed(pixelAt(f, 25, 45)));     // 20..120 x 40..90 on disk
    QVERIFY(!isRed(pixelAt(f, 125, 45)));
}

// A rotation under an outer scale must turn the image about its own centre —
// the drawn square keeps its rect (20,40)-(220,240).
void TestImageAppearance::rotateUnderOuterScaleKeepsTheDrawnRect()
{
    QTemporaryDir dir;
    const QString f = makeTwoImagePdf(dir.path(), "rotate.pdf",
        "q 2 0 0 2 0 0 cm q 100 0 0 100 10 20 cm /ImA Do Q Q\n");
    PdfEditorEngine engine;
    QVERIFY(engine.loadDocumentForEditing(f));

    QVERIFY(engine.rotateImage(0, QStringLiteral("ImA"), 90));
    bool found = false;
    for (const auto &img : engine.listImages(0)) {
        if (img.xobjectName != QLatin1String("ImA")) continue;
        found = true;
        QVERIFY(closeTo(img.rotation, 90.0, 1e-5));
    }
    QVERIFY(found);
    // The square still covers exactly 20..220 x 40..240 on disk.
    QVERIFY(isRed(pixelAt(f, 25, 45)));
    QVERIFY(isRed(pixelAt(f, 215, 45)));
    QVERIFY(isRed(pixelAt(f, 120, 140)));   // the centre stays put
    QVERIFY(!isRed(pixelAt(f, 410, 470)));  // the doubled image must not appear
}

// ── Part 4: CX-12 — deleteImage removes the parsed, isolated span ──────────

// The pure gp::content contract of the removal the engine delete now goes
// through: stream start/end, CRLF, escaped names, neighbouring content,
// strings containing "/ImA Do", nested blocks, shared blocks and marked
// content that does not close inside the span.
void TestImageAppearance::removeImagePlacementSpanRules()
{
    QByteArray out;

    // The Codex shape: the block starts at offset 0 (no "\nq" before it).
    const QByteArray s0 = "q 100 0 0 100 20 20 cm /ImA Do Q";
    QCOMPARE(gp::content::removeImagePlacement(s0, "ImA", 0, &out),
             EditResult::Changed);
    QVERIFY2(QString::fromLatin1(out).trimmed().isEmpty(),
             qPrintable(QStringLiteral("the whole block goes, got <%1>").arg(QString::fromLatin1(out))));

    // Block at the very end of the stream: everything before it survives.
    const QByteArray se = "1 js\nq 1 0 0 1 5 5 cm /ImA Do Q";
    QCOMPARE(gp::content::removeImagePlacement(se, "ImA", 0, &out),
             EditResult::Changed);
    QCOMPARE(out, QByteArray("1 js\n"));

    // CRLF endings: the removal is by parsed offsets, not line text.
    const QByteArray crlf = "1 js\r\nq 1 0 0 1 5 5 cm\r\n/ImA Do\r\nQ";
    QCOMPARE(gp::content::removeImagePlacement(crlf, "ImA", 0, &out),
             EditResult::Changed);
    QCOMPARE(out, QByteArray("1 js\r\n"));

    // An # escaped name is the same placement.
    const QByteArray esc = "q 1 0 0 1 5 5 cm /Im#41 Do Q";
    QCOMPARE(gp::content::removeImagePlacement(esc, "ImA", 0, &out),
             EditResult::Changed);
    QVERIFY(out != esc && !out.contains("Do"));

    // A neighbour block survives byte-exact, in order.
    const QByteArray nb = "0 js\nq 1 0 0 1 5 5 cm /ImA Do Q\n"
                          "q 2 0 0 2 1 1 cm /ImB Do Q\n";
    QCOMPARE(gp::content::removeImagePlacement(nb, "ImA", 0, &out),
             EditResult::Changed);
    QCOMPARE(out, QByteArray("0 js\n\nq 2 0 0 2 1 1 cm /ImB Do Q\n"));

    // A nested block: the innermost span around the Do goes, the rest stays.
    const QByteArray nested = "q q cm /ImA Do Q Q 1 js";
    QCOMPARE(gp::content::removeImagePlacement(nested, "ImA", 0, &out),
             EditResult::Changed);
    QVERIFY2(!out.contains("/ImA Do") && out.contains("1 js"),
             qPrintable(QStringLiteral("got <%1>").arg(QString::fromLatin1(out))));

    // "/ImA Do" inside a literal string is not a placement.
    out.clear();
    QCOMPARE(gp::content::removeImagePlacement("(x /ImA Do y) Tj", "ImA", 0, &out),
             EditResult::NotFound);
    QVERIFY(out.isEmpty());

    // A block that paints another image must not go with this one…
    out.clear();
    QCOMPARE(gp::content::removeImagePlacement(
                 "q 100 0 0 100 20 20 cm /ImA Do /ImB Do Q", "ImA", 0, &out),
             EditResult::SharedBlock);
    QVERIFY(out.isEmpty());
    // …nor one that paints text.
    out.clear();
    QCOMPARE(gp::content::removeImagePlacement("q /ImA Do (t) Tj Q", "ImA", 0, &out),
             EditResult::SharedBlock);
    QVERIFY(out.isEmpty());

    // A placement outside every q..Q: refused (its cm would stay in force).
    out.clear();
    QCOMPARE(gp::content::removeImagePlacement("1 js /ImA Do", "ImA", 0, &out),
             EditResult::NotIsolated);
    QVERIFY(out.isEmpty());

    // A marked-content region opened inside the span must close inside it.
    out.clear();
    QCOMPARE(gp::content::removeImagePlacement("q /P BDC /ImA Do Q", "ImA", 0, &out),
             EditResult::StateInTheWay);
    QVERIFY(out.isEmpty());
    // …and a stray EMC inside the span is refused as well.
    out.clear();
    QCOMPARE(gp::content::removeImagePlacement("q EMC /ImA Do Q", "ImA", 0, &out),
             EditResult::StateInTheWay);
    QVERIFY(out.isEmpty());
    // A balanced region that the span owns moves out with it.
    const QByteArray owned = "0 js\nq /P << /MCID 0 >> BDC /ImA Do EMC Q";
    QCOMPARE(gp::content::removeImagePlacement(owned, "ImA", 0, &out),
             EditResult::Changed);
    QCOMPARE(out, QByteArray("0 js\n"));

    // Unknown name and absent occurrence (refusals leave `out` untouched —
    // it is cleared first so "still empty" proves exactly that).
    out.clear();
    QCOMPARE(gp::content::removeImagePlacement("q /ImB Do Q", "ImA", 0, &out),
             EditResult::NotFound);
    QCOMPARE(gp::content::removeImagePlacement("q /ImA Do Q\nq /ImA Do Q", "ImA", 2, &out),
             EditResult::NotFound);
    QVERIFY(out.isEmpty());

    // Unparseable content is refused, never silently edited.
    QCOMPARE(gp::content::removeImagePlacement("q /ImA Do", "ImA", 0, &out),
             EditResult::Malformed);
    QVERIFY(out.isEmpty());
}

// Two placements of one image: the occurrence picks which span goes (the
// byte-level foundation of N1's placement addressing).
void TestImageAppearance::removeImagePlacementByOccurrence()
{
    QByteArray out;
    const QByteArray two = "q 1 0 0 1 5 5 cm /ImA Do Q\nq 1 0 0 1 7 7 cm /ImA Do Q\n";

    QCOMPARE(gp::content::removeImagePlacement(two, "ImA", 0, &out),
             EditResult::Changed);
    QVERIFY2(!out.contains("5 5 cm") && out.contains("7 7 cm"),
             qPrintable(QStringLiteral("occurrence 0 must remove the first span, got <%1>")
                            .arg(QString::fromLatin1(out))));

    out.clear();
    QCOMPARE(gp::content::removeImagePlacement(two, "ImA", 1, &out),
             EditResult::Changed);
    QVERIFY2(out.contains("5 5 cm") && !out.contains("7 7 cm"),
             qPrintable(QStringLiteral("occurrence 1 must remove the second span, got <%1>")
                            .arg(QString::fromLatin1(out))));
}

// The Codex repro: the image's q..Q block starts at offset 0, so the raw
// substring delete ("rfind \nq" before the Do, "find \nQ" after it) found
// neither anchor, erased nothing — and still returned true.
void TestImageAppearance::deleteImageRemovesTheOffsetZeroPlacement()
{
    QTemporaryDir dir;
    const QString f = makeTwoImagePdf(
        dir.path(), "del0.pdf",
        "q 200 0 0 200 100 400 cm /ImA Do Q\n"
        "q 200 0 0 200 150 450 cm /ImB Do Q\n");
    PdfEditorEngine engine;
    QVERIFY(engine.loadDocumentForEditing(f));

    QVERIFY(engine.deleteImage(0, QStringLiteral("ImA")));
    const QByteArray content = pageContent(f);
    QVERIFY2(!content.contains("/ImA Do"), "the deleted placement must be gone from the content");
    QVERIFY2(content.contains("/ImB Do"), "the neighbour placement must survive");
    for (const auto &img : engine.listImages(0))
        QVERIFY2(img.xobjectName != QLatin1String("ImA"),
                 "listImages must no longer report the deleted image");
    QVERIFY(isBlue(pixelAt(f, kOverlap.x(), kOverlap.y())));   // ImB untouched
    QVERIFY(!isRed(pixelAt(f, kOnlyA.x(), kOnlyA.y())));       // ImA really gone
}

// The image block at the very end of the stream, CRLF endings, a nested block
// and an # escaped name: exactly the image's own span goes, every neighbouring
// byte survives verbatim.
void TestImageAppearance::deleteImageKeepsNeighboursOverShapesAndEndings()
{
    // Block at the very end; the neighbour (and its CRLF) sits before it.
    {
        QTemporaryDir dir;
        const QString f = makeTwoImagePdf(
            dir.path(), "delEnd.pdf",
            "q 200 0 0 200 150 450 cm /ImB Do Q\r\n"
            "q 200 0 0 200 100 400 cm /ImA Do Q");
        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(f));
        QVERIFY(engine.deleteImage(0, QStringLiteral("ImA")));
        const QByteArray content = pageContent(f);
        QVERIFY2(!content.contains("/ImA Do"), "the deleted placement must be gone");
        QVERIFY2(content.startsWith("q 200 0 0 200 150 450 cm /ImB Do Q\r\n"),
                 "the neighbour's bytes must survive verbatim");
    }
    // A nested block: only the innermost span around the Do is removed.
    {
        QTemporaryDir dir;
        const QString f = makeTwoImagePdf(
            dir.path(), "delNested.pdf",
            "q q 200 0 0 200 100 400 cm /ImA Do Q Q\n"
            "q 200 0 0 200 150 450 cm /ImB Do Q\n");
        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(f));
        QVERIFY(engine.deleteImage(0, QStringLiteral("ImA")));
        const QByteArray content = pageContent(f);
        QVERIFY2(!content.contains("/ImA Do"), "the deleted placement must be gone");
        QVERIFY2(content.contains("/ImB Do"), "the neighbour placement must survive");
        QVERIFY(isBlue(pixelAt(f, kOverlap.x(), kOverlap.y())));
    }
    // "/Im#41 Do" is the placement of ImA — the parsed search decodes it.
    {
        QTemporaryDir dir;
        const QString f = makeTwoImagePdf(
            dir.path(), "delEsc.pdf",
            "q 200 0 0 200 100 400 cm /Im#41 Do Q\n");
        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(f));
        QVERIFY(engine.deleteImage(0, QStringLiteral("ImA")));
        QVERIFY2(!pageContent(f).contains("/Im#41"),
                 "the escaped-name placement must be removed from the content");
    }
}

// A block that paints another image, and a "/ImA Do" that only appears inside
// a literal string: both must be refused — byte-identical file, image intact.
void TestImageAppearance::deleteImageRefusalLeavesTheFileUntouched()
{
    {
        QTemporaryDir dir;
        const QString f = makeTwoImagePdf(
            dir.path(), "delShared.pdf",
            "q 200 0 0 200 100 400 cm /ImA Do /ImB Do Q\n");
        const QByteArray before = fileBytes(f);
        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(f));
        QVERIFY2(!engine.deleteImage(0, QStringLiteral("ImA")),
                 "a shared block must not be deletable as one span");
        QCOMPARE(fileBytes(f), before);
        QCOMPARE(engine.listImages(0).size(), 2);   // both placements still listed
        QVERIFY(isBlue(pixelAt(f, kOnlyA.x(), kOnlyA.y())));   // both still paint: ImB covers ImA
    }
    {
        QTemporaryDir dir;
        const QString f = makeTwoImagePdf(
            dir.path(), "delString.pdf",
            "q (x /ImA Do y) Tj Q\nq 200 0 0 200 100 400 cm /ImB Do Q\n");
        const QByteArray before = fileBytes(f);
        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(f));
        QVERIFY2(!engine.deleteImage(0, QStringLiteral("ImA")),
                 "a /ImA Do inside a string is not a placement");
        QCOMPARE(fileBytes(f), before);
    }
}

// ── Part 5: CX-08 — the inline image data extent is exact ──────────────────

namespace {

// Codex's hostile inline image: a well-formed unfiltered RGB strip (32×1×8
// bits = exactly 96 bytes) whose pixel bytes embed a whitespace-delimited
// "EI" followed by operator-looking text (a balanced q..Q, as raw image
// bytes may legitimately contain). Used by the lexer tests and by the
// engine test below.
QByteArray hostileInlineData()
{
    QByteArray data(96, '\x01');
    data.replace(40, 16, " EI q /ImA Do Q ");
    return data;
}

} // namespace

// The first EI-shaped byte pair is data: the lexer must ride over it and the
// only Do is the real one after the data.
void TestImageAppearance::inlineImageRidesOverAFakeEi()
{
    const QByteArray data = hostileInlineData();
    const QByteArray s = "q 200 0 0 8 20 700 cm BI /W 32 /H 1 /BPC 8 /CS /RGB ID "
                         + data + " EI /ImA Do Q";
    QList<gp::content::Token> toks;
    QVERIFY2(gp::content::lex(s, &toks),
             "the exact 96-byte extent must reach the real EI");
    int inlineCount = 0, doCount = 0, qCount = 0, QCount = 0;
    qsizetype inlineEnd = -1;
    for (const auto &t : toks) {
        if (t.kind == gp::content::Token::Kind::InlineImage) {
            ++inlineCount;
            inlineEnd = t.end;
        }
        if (t.kind == gp::content::Token::Kind::Operator && t.text == "Do") ++doCount;
        if (t.kind == gp::content::Token::Kind::Operator && t.text == "q") ++qCount;
        if (t.kind == gp::content::Token::Kind::Operator && t.text == "Q") ++QCount;
    }
    QCOMPARE(inlineCount, 1);
    QCOMPARE(doCount, 1);       // the fake Do inside the data is not a token
    QCOMPARE(qCount, 1);        // the fake q inside the data is not a token
    QCOMPARE(QCount, 1);
    QVERIFY2(inlineEnd > s.indexOf(" EI q /ImA Do Q "),
             "the inline token must cover the fake EI inside the data");
}

// /L counts bytes wherever " EI " hides; AHx ends at '>', A85 at '~>'.
void TestImageAppearance::inlineImageLengthAndEodExtents()
{
    QList<gp::content::Token> toks;
    const auto counts = [&toks](int *inlineCount, int *doCount) {
        *inlineCount = *doCount = 0;
        for (const auto &t : toks) {
            if (t.kind == gp::content::Token::Kind::InlineImage) ++*inlineCount;
            if (t.kind == gp::content::Token::Kind::Operator && t.text == "Do") ++*doCount;
        }
    };

    // Flate-filtered bytes with /L: the byte count decides.
    QByteArray data(20, '\x13');
    data[8] = ' '; data[9] = 'E'; data[10] = 'I'; data[11] = ' ';
    QVERIFY(gp::content::lex("q BI /W 4 /H 1 /F /Fl /L 20 ID " + data
                             + " EI /ImA Do Q", &toks));
    int inlineCount = 0, doCount = 0;
    counts(&inlineCount, &doCount);
    QCOMPARE(inlineCount, 1);
    QCOMPARE(doCount, 1);

    // ASCIIHexDecode ends at the '>' EOD.
    QVERIFY(gp::content::lex(
        "q BI /W 2 /H 1 /BPC 8 /CS /G /F /AHx ID 4142> EI /ImA Do Q", &toks));
    counts(&inlineCount, &doCount);
    QCOMPARE(inlineCount, 1);
    QCOMPARE(doCount, 1);

    // ASCII85Decode ends at "~>" — even when the data embeds " EI ".
    QVERIFY(gp::content::lex(
        "q BI /W 2 /H 1 /BPC 8 /CS /G /F /A85 ID s EI x~> EI /ImA Do Q", &toks));
    counts(&inlineCount, &doCount);
    QCOMPARE(inlineCount, 1);
    QCOMPARE(doCount, 1);

    // A zero-length data section stays lexable.
    QVERIFY(gp::content::lex("0 0 1 1 ID\nEI\n1 js", &toks));

    // /IM true: one 1-bit component (9 bits → 2 packed bytes per row). The
    // data bytes are built explicitly: a \x00 inside a C literal would
    // truncate the QByteArray at it.
    QByteArray imCase = QByteArray("q BI /W 9 /H 1 /IM true ID \xA0", 28);
    imCase += char(0x00);
    imCase += " EI /ImA Do Q";
    QVERIFY(gp::content::lex(imCase, &toks));
    counts(&inlineCount, &doCount);
    QCOMPARE(inlineCount, 1);
    QCOMPARE(doCount, 1);
}

// An extent that cannot be known refuses the edit — never a guess.
void TestImageAppearance::inlineImageRefusesUnknownExtents()
{
    QList<gp::content::Token> toks;
    // A binary filter without /L.
    QVERIFY2(!gp::content::lex(
                 "q BI /W 2 /H 1 /F /Fl ID \x13\x13 EI /ImA Do Q", &toks),
             "Flate without /L must refuse");
    // An unknown colour space without /L.
    QVERIFY2(!gp::content::lex(
                 "q BI /W 2 /H 1 /CS /Pattern ID \x01\x01 EI /ImA Do Q", &toks),
             "an unknown colour space must refuse");
    // Unfiltered data shorter than W×H declares.
    QVERIFY2(!gp::content::lex(
                 "q BI /W 4 /H 1 /CS /G ID \x01\x02 EI /ImA Do Q", &toks),
             "short unfiltered data must refuse");
    // An "ID" without a BI is undefined PDF; it keeps the lexer's historical
    // first-EI read (pinned by the CX-15 sanitizer gate). CX-08's exact
    // extent governs real inline images, which always carry BI.
    QVERIFY2(gp::content::lex(
                 "q 0 0 1 1 ID \x01 EI /ImA Do Q", &toks),
             "a BI-less ID keeps the historical first-EI read");
}

// The engine-level CX-08: the opacity edit must wrap the REAL placement
// after the data, never the operator look-alike inside the pixel bytes.
void TestImageAppearance::opacityReachesTheRealImagePastInlineData()
{
    QTemporaryDir dir;
    const QByteArray data = hostileInlineData();
    const QString f = makeTwoImagePdf(
        dir.path(), "inline.pdf",
        "q 200 0 0 8 20 700 cm BI /W 32 /H 1 /BPC 8 /CS /RGB ID " + data + " EI Q\n"
        "q 200 0 0 200 100 400 cm /ImA Do Q\n"
        "q 200 0 0 200 150 450 cm /ImB Do Q\n");
    PdfEditorEngine engine;
    QVERIFY(engine.loadDocumentForEditing(f));

    const QColor stripBefore = pixelAt(f, 30, 704);
    const QColor imageBefore = pixelAt(f, kOnlyA.x(), kOnlyA.y());
    QVERIFY(isRed(imageBefore));

    QVERIFY(engine.setImageOpacity(0, QStringLiteral("ImA"), 0.5));

    const QByteArray content = pageContent(f);
    QVERIFY2(content.contains(data), "the inline pixel bytes must survive verbatim");
    const qsizetype dataAt = content.indexOf(data);
    const qsizetype wrapAt = content.indexOf("\nq\n/GSop");
    QVERIFY2(wrapAt >= 0, "the real placement must be wrapped");
    QVERIFY2(wrapAt > dataAt + data.size(),
             "the wrap must sit after the inline data, never inside it");

    const QColor stripAfter = pixelAt(f, 30, 704);
    QCOMPARE(stripAfter, stripBefore);   // the strip renders exactly as before
    // PDFium keeps the page's transparency in the rendered QImage: the real
    // image now paints at half alpha (the strip stays fully opaque).
    const QColor blended = pixelAt(f, kOnlyA.x(), kOnlyA.y());
    QVERIFY2(blended.alphaF() > 0.4 && blended.alphaF() < 0.6
                 && blended.red() > blended.green() + 40,
             qPrintable(QStringLiteral("the real image must blend at 50%%, got %1")
                            .arg(blended.name())));
}

// ── Part 6: CX-11 — matrix replacement refuses shared blocks ───────────────

// The target Do must be the only painting operator in its own q..Q block —
// the same isolation rule restack applies — or the shared cm would carry the
// neighbours along (Codex: q 100 0 0 100 20 20 cm /ImA Do /ImB Do Q moved
// BOTH images).
void TestImageAppearance::replaceImageMatrixRefusesSharedBlocks()
{
    QByteArray out;

    // Image + image, target first…
    out.clear();
    QCOMPARE(gp::content::replaceImageMatrix(
                 "q 100 0 0 100 20 20 cm /ImA Do /ImB Do Q", "ImA",
                 "1 0 0 1 50 20", &out),
             EditResult::SharedBlock);
    QVERIFY(out.isEmpty());
    // …and second: no neighbour is moved by proxy either.
    out.clear();
    QCOMPARE(gp::content::replaceImageMatrix(
                 "q 100 0 0 100 20 20 cm /ImB Do /ImA Do Q", "ImA",
                 "1 0 0 1 50 20", &out),
             EditResult::SharedBlock);
    QVERIFY(out.isEmpty());

    // Image + text painting in one block.
    out.clear();
    QCOMPARE(gp::content::replaceImageMatrix(
                 "q /ImA Do (hello) Tj Q", "ImA", "1 0 0 1 0 0", &out),
             EditResult::SharedBlock);
    QVERIFY(out.isEmpty());

    // An isolated placement still rewrites.
    const QByteArray alone = "q 100 0 0 100 20 20 cm /ImA Do Q";
    QCOMPARE(gp::content::replaceImageMatrix(alone, "ImA", "1 0 0 1 50 20", &out),
             EditResult::Changed);
    QVERIFY(out.contains("1 0 0 1 50 20 cm /ImA Do"));
}

// The engine-level contract: a move inside a shared block is refused with a
// byte-identical file — the neighbours' bytes and rendered pixels stay put.
void TestImageAppearance::moveImageRefusesASharedBlockKeepsNeighbours()
{
    // Image + image sharing one q..Q (both at the same rect: ImB covers ImA).
    {
        QTemporaryDir dir;
        const QString f = makeTwoImagePdf(
            dir.path(), "sharedII.pdf",
            "q 200 0 0 200 100 400 cm /ImA Do /ImB Do Q\n");
        const QByteArray before = fileBytes(f);
        const QImage renderBefore = renderPage(f);
        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(f));

        QVERIFY2(!engine.moveImage(0, QStringLiteral("ImA"), 20, 0),
                 "a shared cm must not be rewritten");
        QCOMPARE(fileBytes(f), before);
        QCOMPARE(renderPage(f), renderBefore);
        QCOMPARE(engine.listImages(0).size(), 2);   // both placements intact
    }
    // Image + text sharing one q..Q.
    {
        QTemporaryDir dir;
        const QString f = makeTwoImagePdf(
            dir.path(), "sharedIT.pdf",
            "q 200 0 0 200 100 400 cm /ImA Do BT /F1 12 Tf (x) Tj ET Q\n");
        const QByteArray before = fileBytes(f);
        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(f));

        QVERIFY2(!engine.moveImage(0, QStringLiteral("ImA"), 20, 0),
                 "a block painting text must not be rewritten either");
        QCOMPARE(fileBytes(f), before);
        QCOMPARE(engine.listImages(0).size(), 1);
    }
}

// ── Part 7: CX-10 — a gs-only opacity wrapper is part of the own block ─────

namespace {
// The wrapper setImageOpacity produces: "q /GSop… gs /ImA Do Q" nested
// inside the placement block.
const char *kWrappedBlock =
    "q 100 0 0 100 20 20 cm q /GSop1a2b3c gs /ImA Do Q Q";
} // namespace

void TestImageAppearance::gsWrapperIsPartOfTheOwnBlock()
{
    QByteArray out;

    // The placement cm is reachable through the wrapper: the rewrite hits
    // the block's cm, never the wrapper (which has none).
    QCOMPARE(gp::content::replaceImageMatrix(QByteArray(kWrappedBlock), "ImA",
                                             "1 0 0 1 30 40", &out),
             EditResult::Changed);
    QVERIFY2(out.contains("1 0 0 1 30 40 cm q /GSop1a2b3c gs"),
             qPrintable(QStringLiteral("the block's cm must be rewritten, got <%1>")
                            .arg(QString::fromLatin1(out))));

    // Restacking moves wrapper + placement together: the whole block (cm and
    // wrapper) relocates past /ImB.
    const QByteArray stream =
        "0 js\n" + QByteArray(kWrappedBlock) + "\n"
        + "q 200 0 0 200 150 450 cm /ImB Do Q\n";
    out.clear();
    QCOMPARE(gp::content::restackImage(stream, "ImA", true, &out),
             EditResult::Changed);
    QVERIFY2(out.indexOf("/ImB Do") < out.indexOf("/GSop1a2b3c gs"),
             "the wrapped placement must move as a whole past /ImB");
    QVERIFY2(out.contains("cm q /GSop1a2b3c gs /ImA Do Q Q"),
             "the cm must travel inside the moved span");
    QVERIFY(out.startsWith("0 js\n"));              // the other bytes stay put

    // Already front-most (wrapper and all): Unchanged — honest now.
    out.clear();
    const QByteArray frontmost =
        "q 200 0 0 200 150 450 cm /ImB Do Q\n" + QByteArray(kWrappedBlock) + "\n";
    QCOMPARE(gp::content::restackImage(frontmost, "ImA", true, &out),
             EditResult::Unchanged);
    QVERIFY(out.isEmpty());

    // Deleting a wrapped placement takes the wrapper with it.
    out.clear();
    QCOMPARE(gp::content::removeImagePlacement(QByteArray(kWrappedBlock), "ImA",
                                               0, &out),
             EditResult::Changed);
    QVERIFY2(QString::fromLatin1(out).trimmed().isEmpty(),
             qPrintable(QStringLiteral("wrapper + placement go together, got <%1>")
                            .arg(QString::fromLatin1(out))));

    // A wrapper that carries real content of its own (a cm) is NOT unwrapped:
    // that innermost block IS the image's own placement block — its cm is
    // the one rewritten, the outer cm stays verbatim.
    const QByteArray cmWrapper =
        "q 100 0 0 100 20 20 cm q 200 0 0 200 0 0 cm /ImA Do Q Q";
    out.clear();
    QCOMPARE(gp::content::replaceImageMatrix(cmWrapper, "ImA", "3 0 0 3 0 0", &out),
             EditResult::Changed);
    QVERIFY2(out.contains("3 0 0 3 0 0 cm /ImA Do"),
             qPrintable(QStringLiteral("the innermost placement cm wins, got <%1>")
                            .arg(QString::fromLatin1(out))));
    QVERIFY2(out.contains("q 100 0 0 100 20 20 cm"),
             "the outer cm must stay untouched");
}

// CX-10's engine contract: after an opacity wrap every later image edit
// composes with the wrapped placement, the front command really moves the
// image, and undo restores only the front — judged by rendered pixels.
void TestImageAppearance::opacityThenEditsComposeAndUndo()
{
    QTemporaryDir dir;
    const QString f = makeTwoImagePdf(dir.path(), "opaften.pdf", kTwoImages);
    PdfEditorEngine engine;
    QVERIFY(engine.loadDocumentForEditing(f));
    DocumentSession doc;
    doc.beginDocument(f);
    QUndoStack stack;

    QVERIFY(engine.setImageOpacity(0, QStringLiteral("ImA"), 0.5));

    // MOVE: NotIsolated before CX-10; now the wrapped placement moves.
    QVERIFY(engine.moveImage(0, QStringLiteral("ImA"), 30, 0));
    QVERIFY2(pixelAt(f, 115, 420).alpha() < 60, "the old position must be empty");
    const QColor movedNew = pixelAt(f, 145, 420);
    QVERIFY2(movedNew.alphaF() > 0.4 && movedNew.alphaF() < 0.6
                 && movedNew.red() > movedNew.green() + 40,
             qPrintable(QStringLiteral("the moved image paints at its half opacity, got %1")
                            .arg(movedNew.name())));

    // ROTATE: the wrapper must not stop the rotation either.
    QVERIFY(engine.rotateImage(0, QStringLiteral("ImA"), 90));
    bool found = false;
    for (const auto &img : engine.listImages(0)) {
        if (img.xobjectName != QLatin1String("ImA")) continue;
        found = true;
        QVERIFY(closeTo(img.rotation, 90.0, 1e-5));
    }
    QVERIFY(found);

    // RESIZE: the square shrinks to 100x100 from the fixed corner.
    QVERIFY(engine.resizeImage(0, QStringLiteral("ImA"), 100, 100));
    for (const auto &img : engine.listImages(0)) {
        if (img.xobjectName != QLatin1String("ImA")) continue;
        QVERIFY(closeTo(img.placement.width(), 100.0, 1e-4));
        QVERIFY(closeTo(img.placement.height(), 100.0, 1e-4));
    }

    // FRONT: pushed as the command (redo applies it) — the overlap flips
    // from opaque ImB-blue to a 50/50 ImA/ImB blend. Resizing the 90°-rotated
    // placement scales its matrix columns with the translation fixed, so the
    // drawn rect is (230..330, 400..500); the probe sits inside that and
    // inside ImB (150..350, 450..650). Over an OPAQUE backdrop the composited
    // pixel stays opaque (αout = ½·1 + ½·1): the blend shows in the channels.
    const QPointF probe(300, 470);
    QVERIFY(isBlue(pixelAt(f, probe.x(), probe.y())));
    const QByteArray backup = engine.extractPageAsBytes(f, 0);
    QVERIFY(!backup.isEmpty());
    stack.push(new ImageAppearanceCommand(&engine, &doc, 0, QStringLiteral("ImA"),
                                          ImageAppearanceCommand::Kind::BringToFront,
                                          1.0, backup));
    const QColor over = pixelAt(f, probe.x(), probe.y());
    QVERIFY2(over.red() > 100 && over.red() < 150
                 && over.blue() > 100 && over.blue() < 150
                 && over.green() < 80,
             qPrintable(QStringLiteral("ImA must blend 50/50 with ImB, got %1")
                            .arg(over.name())));

    // UNDO restores the pre-front stacking but keeps the earlier edits.
    QVERIFY(CheckedHistory::undo(&stack));
    QVERIFY(isBlue(pixelAt(f, probe.x(), probe.y())));
    bool kept = false;
    for (const auto &img : engine.listImages(0)) {
        if (img.xobjectName != QLatin1String("ImA")) continue;
        kept = true;
        QVERIFY(closeTo(img.placement.width(), 100.0, 1e-4));
        QVERIFY(closeTo(img.rotation, 90.0, 1e-5));
    }
    QVERIFY(kept);

    QVERIFY(CheckedHistory::redo(&stack));
    const QColor again = pixelAt(f, probe.x(), probe.y());
    QVERIFY2(again.red() > 100 && again.red() < 150
                 && again.blue() > 100 && again.blue() < 150,
             qPrintable(QStringLiteral("redo must re-front the wrapped image, got %1")
                            .arg(again.name())));
}

QTEST_MAIN(TestImageAppearance)
#include "TestImageAppearance.moc"
