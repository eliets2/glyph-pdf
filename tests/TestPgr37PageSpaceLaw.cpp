// SPDX-License-Identifier: Apache-2.0
// TestPgr37PageSpaceLaw.cpp
//
// PGR-37 (D2 delta review 2026-09-23) — the SEP13 L8 viewer-space law was
// applied at the excision boundary while its PDFium-side PRODUCERS still
// emitted pre-L8 rectangles:
//
//   * PatternRedactor emitted "raw-user X + display-flip Y" — viewer space
//     ONLY on /Rotate 0 origin-0 pages. applyPatternRedactions[Multi] (the
//     batch / preset redaction seam) fed those rects through
//     PageSpace::viewerToUser, which on /Rotate 90/270 pages SWAPS the axes
//     (transposed excision — the matched content SURVIVES while the
//     operation reports success) and on offset-origin MediaBoxes ADDS the
//     origin twice.
//
//   * TextMatchFinder + PoDoFoBackend::replaceTextRegions re-derived the
//     forbidden local flip on BOTH sides; the two flips cancelled only while
//     PoDoFo's rotation-normalized MediaBox height equals PDFium's display
//     height — a CropBox≠MediaBox document shifted the excision and the
//     replacement draw by the height difference (replace reported success;
//     the matched text survived).
//
// Fix under test: the producers emit RAW PDF USER space (FPDFText_GetCharBox
// verbatim, y-up); the excision consumes it through
// PoDoFoBackend::applyRedactionsUserSpace / replaceTextRegions verbatim; the
// viewer-mark entry (applyRedactions, the L8 law) is untouched.
//
// Property-oracle discipline (fuzz-harness doctrine §F): every test asserts
// the END STATE of the artifact — the matched text must NOT survive in the
// saved output (re-parsed with PDFium through PatternRedactor::findMatches),
// never just the call's return code. Fixtures are hand-built raw PDFs whose
// user-space glyph positions are known literals.
//
// Repro (each test IS the minimized seed — the fixture bytes are written by
// buildShapedPdf below):
//   QT_QPA_PLATFORM=offscreen ctest -R TestPgr37PageSpaceLaw --output-on-failure

#include <QtTest/QtTest>
#include <QFile>
#include <QRegularExpression>
#include <QTemporaryDir>

#include "engines/PatternRedactor.h"
#include "engines/TextMatchFinder.h"
#include "engines/PdfEditorEngine.h"

namespace {

constexpr const char* kSecret = "SECRET-ALPHA-42";
constexpr const char* kPublic = "PUBLIC-OMEGA-99";

// Hand-built one-page PDF: raw /MediaBox (+ optional /CropBox) + /Rotate +
// one Helvetica text line at a KNOWN user-space position. The content stream
// is user space — /Rotate never enters it.
bool buildShapedPdf(const QString& path,
                    int rotation,
                    const QRectF& mediaBox,
                    const QRectF& cropBox,   // null = absent
                    int x, int y,
                    const QByteArray& text)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return false;
    QByteArray out = "%PDF-1.4\n";
    QList<qint64> off;
    auto addObj = [&out, &off](const QByteArray& body) {
        off.append(out.size());
        out += QByteArray::number(off.size()) + " 0 obj\n" + body + "\nendobj\n";
    };
    const QByteArray media = "/MediaBox[" +
        QByteArray::number(int(mediaBox.x())) + " " +
        QByteArray::number(int(mediaBox.y())) + " " +
        QByteArray::number(int(mediaBox.x() + mediaBox.width())) + " " +
        QByteArray::number(int(mediaBox.y() + mediaBox.height())) + "]";
    const QByteArray crop = cropBox.isNull() ? QByteArray() :
        "/CropBox[" +
        QByteArray::number(int(cropBox.x())) + " " +
        QByteArray::number(int(cropBox.y())) + " " +
        QByteArray::number(int(cropBox.x() + cropBox.width())) + " " +
        QByteArray::number(int(cropBox.y() + cropBox.height())) + "]";
    addObj("<</Type/Catalog/Pages 2 0 R>>");
    addObj("<</Type/Pages/Kids[3 0 R]/Count 1>>");
    addObj("<</Type/Page/Parent 2 0 R" + media + crop + "/Rotate " +
           QByteArray::number(rotation) + "/Contents 4 0 R"
           "/Resources<</Font<</F1 5 0 R>>>>>>");
    const QByteArray stream = "BT /F1 12 Tf " + QByteArray::number(x) + " "
        + QByteArray::number(y) + " Td (" + text + ") Tj ET\n";
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

int matchCount(const QString& pdfPath, const char* text)
{
    const QRegularExpression rx(QRegularExpression::escape(QString::fromLatin1(text)));
    const QList<QRectF> found = PatternRedactor::findMatches(pdfPath, 0, rx);
    return static_cast<int>(found.size());
}

} // namespace

class TestPgr37PageSpaceLaw : public QObject {
    Q_OBJECT
    QTemporaryDir m_tmpDir;

    void patternRedactionIsExcised(int rotation, const QRectF& mediaBox,
                                   int x, int y, const char* what)
    {
        const QString src = m_tmpDir.filePath(
            QStringLiteral("pgr37_%1.pdf").arg(what));
        QVERIFY2(buildShapedPdf(src, rotation, mediaBox, QRectF(), x, y, kSecret),
                 qPrintable(QStringLiteral("fixture build failed: %1").arg(what)));
        // Fixture premise: the matcher finds the secret on the shaped page.
        QCOMPARE(matchCount(src, kSecret), 1);

        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(src));
        const QStringList patterns = { QString::fromLatin1(kSecret) };
        const QString out = m_tmpDir.filePath(
            QStringLiteral("pgr37_%1_redacted.pdf").arg(what));
        QVERIFY2(engine.applyPatternRedactionsMulti(patterns, {}, out),
                 "batch pattern redaction must succeed on the shaped page");

        // The property: the secret must NOT survive in the saved artifact.
        QCOMPARE(matchCount(out, kSecret), 0);
    }

private slots:
    void initTestCase() {
        QVERIFY(m_tmpDir.isValid());
    }

    // THE REGRESSION: /Rotate 90 page, batch pattern redaction. Pre-fix the
    // viewer transform swapped the axes and the excision landed transposed —
    // the secret survived while the run reported success.
    void rot90PatternRedactionIsExcised() {
        patternRedactionIsExcised(90, QRectF(0, 0, 612, 792), 72, 700, "rot90");
    }

    // /Rotate 270 — the other swap (same law row family).
    void rot270PatternRedactionIsExcised() {
        patternRedactionIsExcised(270, QRectF(0, 0, 612, 792), 72, 700, "rot270");
    }

    // Offset-origin MediaBox: pre-fix viewerToUser added the (0, 200) origin
    // on top of the producer's already-user-space rect — excision 200pt
    // too high, secret intact, success reported.
    void offsetOriginPatternRedactionIsExcised() {
        patternRedactionIsExcised(0, QRectF(0, 200, 612, 792), 72, 900, "offset");
    }

    // Replace pipeline, /Rotate 90 (refactor pin — the old local flips
    // cancelled here; they must STAY cancelled through the raw-space path).
    void rot90ReplacePipelineExcisesAndRedraws() {
        const QString src = m_tmpDir.filePath(QStringLiteral("pgr37_repl90.pdf"));
        QVERIFY(buildShapedPdf(src, 90, QRectF(0, 0, 612, 792), QRectF(),
                               72, 700, kSecret));
        QCOMPARE(matchCount(src, kSecret), 1);

        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(src));

        const QRegularExpression rx(QRegularExpression::escape(QString::fromLatin1(kSecret)));
        const auto matches = TextMatchFinder::findMatches(src, {0}, rx);
        QCOMPARE(matches.size(), 1);

        QList<TextReplacementSpec> specs;
        TextReplacementSpec s;
        s.pageIndex = matches.first().pageIndex;
        s.rect = matches.first().rect;
        s.text = QString::fromLatin1(kPublic);
        s.fontSize = matches.first().fontSize;
        specs.append(s);
        QList<double> drawnWidths;
        QVERIFY2(engine.replaceTextRegions(specs, &drawnWidths),
                 "replace must succeed on the /Rotate 90 page");
        QCOMPARE(drawnWidths.size(), 1);

        const QString out = m_tmpDir.filePath(QStringLiteral("pgr37_repl90_out.pdf"));
        QVERIFY(engine.saveDocument(out));
        QCOMPARE(matchCount(out, kSecret), 0);
        QCOMPARE(matchCount(out, kPublic), 1);
    }

    // CropBox≠MediaBox document (the silent cancellation breaker): PDFium's
    // display height is the CROP height (792) while PoDoFo's MediaBox height
    // is 992 — the old double-flip miscounted 200pt and the excision missed.
    void cropBoxMismatchReplacePipelineExcises() {
        const QString src = m_tmpDir.filePath(QStringLiteral("pgr37_crop.pdf"));
        QVERIFY(buildShapedPdf(src, 0, QRectF(0, 0, 612, 992),
                               QRectF(0, 100, 612, 792), 72, 500, kSecret));
        QCOMPARE(matchCount(src, kSecret), 1);

        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(src));

        const QRegularExpression rx(QRegularExpression::escape(QString::fromLatin1(kSecret)));
        const auto matches = TextMatchFinder::findMatches(src, {0}, rx);
        QCOMPARE(matches.size(), 1);

        QList<TextReplacementSpec> specs;
        TextReplacementSpec s;
        s.pageIndex = matches.first().pageIndex;
        s.rect = matches.first().rect;
        s.text = QString::fromLatin1(kPublic);
        s.fontSize = matches.first().fontSize;
        specs.append(s);
        QVERIFY(engine.replaceTextRegions(specs, nullptr));

        const QString out = m_tmpDir.filePath(QStringLiteral("pgr37_crop_out.pdf"));
        QVERIFY(engine.saveDocument(out));
        QCOMPARE(matchCount(out, kSecret), 0);
        QCOMPARE(matchCount(out, kPublic), 1);
    }

    // Control: the plain /Rotate 0 origin-0 page keeps redacting (guards
    // against overcorrection of the producer change).
    void plainPagePatternRedactionControl() {
        patternRedactionIsExcised(0, QRectF(0, 0, 612, 792), 72, 700, "plain");
    }

    // Pin: the VIEWER-mark entry (applyRedactions, the SEP13 L8 law) still
    // excises exactly on a /Rotate 90 page — the refactor must not have
    // disturbed the viewer contract (TestRotate270PageSpace pins the law
    // itself; this pins the redaction consumer through the engine API).
    void viewerMarkPathStillExactOnRot90() {
        const QString src = m_tmpDir.filePath(QStringLiteral("pgr37_mark90.pdf"));
        QVERIFY(buildShapedPdf(src, 90, QRectF(0, 0, 612, 792), QRectF(),
                               72, 700, kSecret));
        QCOMPARE(matchCount(src, kSecret), 1);

        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(src));

        // Viewer-space mark for text at user (72..~180, 700..712) on a
        // /Rotate 90 page (law table, rot 90: viewerX = userY, viewerY =
        // userX, origin 0). A generous box around the whole glyph run.
        const QList<QRectF> viewerMarks = { QRectF(690, 55, 40, 140) };
        QVERIFY2(engine.applyRedactions(0, viewerMarks),
                 "viewer-space mark redaction must succeed");
        const QString out = m_tmpDir.filePath(QStringLiteral("pgr37_mark90_out.pdf"));
        QVERIFY(engine.saveDocument(out));
        QCOMPARE(matchCount(out, kSecret), 0);
    }
};

QTEST_MAIN(TestPgr37PageSpaceLaw)

#include "TestPgr37PageSpaceLaw.moc"
