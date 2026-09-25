// SPDX-License-Identifier: Apache-2.0
// Inline text edit: letter spacing, line spacing and opacity (Wave 2B/2C port
// from the editing-parity line). Judged by the RENDERED page, not by operators:
//   1. line spacing scales the baseline-to-baseline gap (1.5 × the 1.0 gap);
//   2. letter spacing widens the drawn line by about spacing × glyph count;
//   3. opacity applies to the NEW text only — the white cover over the old
//      text stays opaque, so the replaced text can never show through (the
//      original port set the ExtGState before the cover and leaked it);
//   4. the defaults reproduce the pre-port output (no Tc, no gs).
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QPdfDocument>
#include <podofo/podofo.h>
#include "engines/PdfEditorEngine.h"

namespace {

// A4 page with one big dark line at (72, 700), Helvetica 28.
QString makeTextPdf(const QString &dir, const QString &name)
{
    const QString path = dir + QLatin1Char('/') + name;
    PoDoFo::PdfMemDocument doc;
    auto &page = doc.GetPages().CreatePage(
        PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
    PoDoFo::PdfPainter painter;
    painter.SetCanvas(page);
    painter.TextState.SetFont(
        doc.GetFonts().GetStandard14Font(PoDoFo::PdfStandard14FontType::Helvetica), 28);
    painter.DrawText("ORIGINAL SECRET LINE", 72, 700);
    painter.FinishDrawing();
    doc.Save(path.toStdString());
    return path;
}

QImage render(const QString &path)
{
    QPdfDocument pdf;
    if (pdf.load(path) != QPdfDocument::Error::None) return {};
    return pdf.render(0, QSize(595, 842)).convertToFormat(QImage::Format_RGB32);
}

bool dark(QRgb px) { return qGray(px) < 128; }

// Top rows of the dark-pixel bands inside `area` (one band per text line).
QList<int> bandTops(const QImage &img, const QRect &area)
{
    QList<int> tops;
    bool inBand = false;
    for (int y = area.top(); y <= area.bottom(); ++y) {
        bool any = false;
        for (int x = area.left(); x <= area.right() && !any; ++x) any = dark(img.pixel(x, y));
        if (any && !inBand) tops << y;
        inBand = any;
    }
    return tops;
}

// Horizontal extent (px) of the dark pixels inside `area`.
int inkWidth(const QImage &img, const QRect &area)
{
    int left = area.right(), right = area.left();
    for (int y = area.top(); y <= area.bottom(); ++y)
        for (int x = area.left(); x <= area.right(); ++x)
            if (dark(img.pixel(x, y))) { left = qMin(left, x); right = qMax(right, x); }
    return right >= left ? right - left : 0;
}

QByteArray pageContent(const QString &path)
{
    PoDoFo::PdfMemDocument doc;
    doc.Load(path.toStdString());
    PoDoFo::charbuff buf;
    doc.GetPages().GetPageAt(0).GetContents()->CopyTo(buf);
    return QByteArray(buf.data(), static_cast<int>(buf.size()));
}

// The edit rect in Qt top-left user space, covering the original line.
const QRectF kRect(60, 110, 470, 120);
const QRect kRectPx(60, 110, 470, 120);

bool applyEdit(const QString &f, const QString &text, double opacity,
               double letterSpacing, double lineSpacing)
{
    PdfEditorEngine editor;
    if (!editor.loadDocumentForEditing(f)) return false;
    if (!editor.editTextInline(0, kRect, text, QStringLiteral("Helvetica"), 20, Qt::black,
                               false, false, 0, opacity, letterSpacing, lineSpacing))
        return false;
    return editor.writeUpdate(f);
}

} // namespace

class TestTextEditStyle : public QObject {
    Q_OBJECT
private slots:
    void lineSpacingScalesTheBaselineGap();
    void letterSpacingWidensTheLine();
    void opacityNeverLetsTheReplacedTextShowThrough();
    void defaultsKeepThePreviousOutput();
};

void TestTextEditStyle::lineSpacingScalesTheBaselineGap()
{
    QTemporaryDir dir;
    const QString normal = makeTextPdf(dir.path(), "normal.pdf");
    const QString wide = makeTextPdf(dir.path(), "wide.pdf");
    QVERIFY(applyEdit(normal, QStringLiteral("LINE ONE\nLINE TWO"), 1.0, 0.0, 1.0));
    QVERIFY(applyEdit(wide, QStringLiteral("LINE ONE\nLINE TWO"), 1.0, 0.0, 1.5));

    const QList<int> a = bandTops(render(normal), kRectPx);
    const QList<int> b = bandTops(render(wide), kRectPx);
    QCOMPARE(a.size(), 2);
    QCOMPARE(b.size(), 2);
    const double gapNormal = a[1] - a[0];
    const double gapWide = b[1] - b[0];
    QVERIFY2(qAbs(gapWide / gapNormal - 1.5) < 0.1,
             qPrintable(QStringLiteral("gaps %1 → %2").arg(gapNormal).arg(gapWide)));
}

void TestTextEditStyle::letterSpacingWidensTheLine()
{
    QTemporaryDir dir;
    const QString tight = makeTextPdf(dir.path(), "tight.pdf");
    const QString spaced = makeTextPdf(dir.path(), "spaced.pdf");
    const QString text = QStringLiteral("SPACING");            // 7 glyphs
    QVERIFY(applyEdit(tight, text, 1.0, 0.0, 1.0));
    QVERIFY(applyEdit(spaced, text, 1.0, 4.0, 1.0));
    const int w0 = inkWidth(render(tight), kRectPx);
    const int w1 = inkWidth(render(spaced), kRectPx);
    // Tc is added after every glyph: 6 gaps × 4 pt between first and last ink.
    QVERIFY2(qAbs((w1 - w0) - 24) <= 3,
             qPrintable(QStringLiteral("width %1 → %2").arg(w0).arg(w1)));
}

void TestTextEditStyle::opacityNeverLetsTheReplacedTextShowThrough()
{
    QTemporaryDir dir;
    const QString f = makeTextPdf(dir.path(), "opacity.pdf");
    const QImage before = render(f);
    // The original line's ink right of x=250 (where the short new text ends).
    const QRect oldInkOnly(250, 120, 280, 40);
    QVERIFY2(inkWidth(before, oldInkOnly) > 50, "fixture: the original text must be visible");

    QVERIFY(applyEdit(f, QStringLiteral("X"), 0.5, 0.0, 1.0));
    const QImage after = render(f);
    QCOMPARE(inkWidth(after, oldInkOnly), 0);   // cover opaque: nothing leaks

    // The new glyph itself is drawn half-transparent: grey, not black.
    int darkest = 255;
    for (int y = kRectPx.top(); y <= kRectPx.bottom(); ++y)
        for (int x = kRectPx.left(); x < 100; ++x)
            darkest = qMin(darkest, qGray(after.pixel(x, y)));
    QVERIFY2(darkest > 60 && darkest < 200,
             qPrintable(QStringLiteral("darkest new-text pixel %1").arg(darkest)));
}

void TestTextEditStyle::defaultsKeepThePreviousOutput()
{
    QTemporaryDir dir;
    const QString f = makeTextPdf(dir.path(), "defaults.pdf");
    PdfEditorEngine editor;
    QVERIFY(editor.loadDocumentForEditing(f));
    QVERIFY(editor.editTextInline(0, kRect, QStringLiteral("PLAIN"), QStringLiteral("Helvetica"),
                                  20, Qt::black, false, false, 0));
    QVERIFY(editor.writeUpdate(f));
    const QByteArray content = pageContent(f);
    QVERIFY2(!content.contains(" Tc"), "no letter spacing by default");
    QVERIFY2(!content.contains(" gs"), "no transparency state by default");
}

QTEST_MAIN(TestTextEditStyle)
#include "TestTextEditStyle.moc"
