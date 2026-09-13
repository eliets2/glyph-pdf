// SPDX-License-Identifier: Apache-2.0
// T2-2: Find & Replace + regex search.
//
// Covers the three real seams the feature is built on:
//   1. TextMatchFinder — flag-aware (exact / match-case / whole-words /
//      regex) matching over the PDFium text layer with per-match geometry,
//      matched text and font size, and page scoping;
//   2. ITextReplacer::replaceTextRegions (PdfEditorEngine + PoDoFoBackend) —
//      matched glyphs excised from the content stream, replacement drawn at
//      the match position, measured drawn widths reported (the moat M8
//      reflow-warning metric), verified in a SAVED, REOPENED artifact;
//   3. FindReplaceDialog — match count shown BEFORE replace, page scoping
//      (current page / range / all), options passed verbatim to the single
//      canonical replace pipeline (injected invoker — no shell dependency).
//
// Deterministic: no sleeps, no modal dialogs (the dialog is modeless and its
// mutation goes through an injectable function).

#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <podofo/podofo.h>
#include "engines/TextMatchFinder.h"
#include "engines/PdfEditorEngine.h"
#include "engines/pdfium/PdfiumBackend.h"
#include "mocks/MockPdfEditorEngine.h"
#include "ui/FindReplaceDialog.h"
#include "ui/PdfViewerWidget.h"
#include "GpMainWindow.h"
#include "app/Bootstrapper.h"
#include "shell/controllers/EditController.h"

#include <QLineEdit>
#include <QCheckBox>
#include <QComboBox>
#include <QFileInfo>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QFontDatabase>

#ifdef HAS_PDFIUM
#include <fpdfview.h>
#include <fpdf_text.h>
#include "engines/pdfium/PdfiumEnvironment.h"
#endif

// The PDFium/windows headers define `DrawText` as a macro (Win32 DrawTextW),
// which collides with PoDoFo::PdfPainter::DrawText. Drop the macro — the
// PoDoFo method is what this fixture needs.
#ifdef DrawText
#undef DrawText
#endif

// ---------------------------------------------------------------------------
// Fixture: a 3-page PDF with known per-page text. Each WORD is drawn as its
// own text operator at its own x offset (like real document text runs), so
// the engine's operator-granularity excision removes exactly the matched
// word's operator and the neighbors survive:
//   page 0: "Alpha"  "beta"  "alphabet"
//   page 1: "ALPHA"  "appears"  "here"
//   page 2: "delta"  "alpha"  "end"
// ---------------------------------------------------------------------------
static QString createThreePagePdf(const QTemporaryDir& tmpDir, const QString& name) {
    struct Word { const char* text; double x; };
    static const Word pages[3][3] = {
        { { "Alpha", 50 }, { "beta", 100 }, { "alphabet", 150 } },
        { { "ALPHA", 50 }, { "appears", 110 }, { "here", 180 } },
        { { "delta", 50 }, { "alpha", 100 }, { "end", 150 } },
    };
    const QString path = tmpDir.filePath(name);
    try {
        PoDoFo::PdfMemDocument doc;
        for (int p = 0; p < 3; ++p) {
            auto& page = doc.GetPages().CreatePage(
                PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
            PoDoFo::PdfPainter painter;
            painter.SetCanvas(page);
            auto& font = doc.GetFonts().GetStandard14Font(
                PoDoFo::PdfStandard14FontType::Helvetica);
            painter.TextState.SetFont(font, 12.0);
            for (const Word& w : pages[p])
                painter.DrawText(w.text, w.x, 700);
            painter.FinishDrawing();
        }
        doc.Save(path.toUtf8().constData());
    } catch (const std::exception& e) {
        qWarning() << "createThreePagePdf failed:" << e.what();
        return {};
    }
    return path;
}

// ---------------------------------------------------------------------------
// Pack A review F2 fixture: a page with SUPPLEMENTARY (astral-plane) text.
//   page 0, one font for every run (y descending, 60pt line gap):
//     y=760: "\u{1F600}\u{1F600}\u{1F600}\u{1F600}"   (4 supplementary chars)
//     y=700: "alpha"                                   (search target)
//     y=640: "omega"                                   (must-survive neighbor)
// ---------------------------------------------------------------------------
// The emoji U+1F600 cannot be encoded by the Standard-14 fonts, so the
// fixture loads a real Windows emoji font (Segoe UI Emoji). When the font is
// unavailable the supplementary tests skip: their premise (a supplementary
// code point in the extracted text layer) cannot be built on that machine.
static QString supplementaryFontPath() {
    static const QString path =
        QStringLiteral("C:/Windows/Fonts/seguiemj.ttf");
    return QFileInfo::exists(path) ? path : QString();
}

static QString createSupplementaryPdf(const QTemporaryDir& tmpDir, const QString& name) {
    const QString fontPath = supplementaryFontPath();
    if (fontPath.isEmpty()) return {};
    const QString path = tmpDir.filePath(name);
    const char32_t emoji[4] = { 0x1F600, 0x1F600, 0x1F600, 0x1F600 };
    try {
        PoDoFo::PdfMemDocument doc;
        auto& page = doc.GetPages().CreatePage(
            PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
        PoDoFo::PdfPainter painter;
        painter.SetCanvas(page);
        auto& font = doc.GetFonts().GetOrCreateFont(
            fontPath.toStdString(), 0);
        painter.TextState.SetFont(font, 12.0);
        const QByteArray emojiLine = QString::fromUcs4(emoji, 4).toUtf8();
        painter.DrawText(emojiLine.constData(), 50, 760);
        painter.DrawText("alpha", 50, 700);
        painter.DrawText("omega", 50, 640);
        painter.FinishDrawing();
        doc.Save(path.toUtf8().constData());
    } catch (const std::exception& e) {
        qWarning() << "createSupplementaryPdf failed:" << e.what();
        return {};
    }
    return path;
}

// F2 scenario B: supplementary char INSIDE the match on one line, with a
// far-away neighbor glyph on the same line that must stay out of the match
// box (x=500 vs the match span at x<=130). Runs are separate text operators
// so operator-granularity excision removes exactly the matched run.
static QString createSupplementaryInlinePdf(const QTemporaryDir& tmpDir, const QString& name) {
    const QString fontPath = supplementaryFontPath();
    if (fontPath.isEmpty()) return {};
    const QString path = tmpDir.filePath(name);
    const char32_t emoji[1] = { 0x1F600 };
    try {
        PoDoFo::PdfMemDocument doc;
        auto& page = doc.GetPages().CreatePage(
            PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
        PoDoFo::PdfPainter painter;
        painter.SetCanvas(page);
        auto& font = doc.GetFonts().GetOrCreateFont(
            fontPath.toStdString(), 0);
        painter.TextState.SetFont(font, 12.0);
        painter.DrawText("x", 50, 700);
        const QString matchRun = QStringLiteral("b")
            + QString::fromUcs4(emoji, 1) + QStringLiteral("c");
        painter.DrawText(matchRun.toUtf8().constData(), 90, 700);
        painter.DrawText("d", 500, 700);
        painter.FinishDrawing();
        doc.Save(path.toUtf8().constData());
    } catch (const std::exception& e) {
        qWarning() << "createSupplementaryInlinePdf failed:" << e.what();
        return {};
    }
    return path;
}

class TestFindReplace : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_tmpDir;

private slots:
    void initTestCase() {
        QVERIFY2(m_tmpDir.isValid(), "Temp directory creation failed");
    }

    // ── TextMatchFinder::buildPattern ────────────────────────────────────

    void patternExactEscapesMetacharacters() {
        // A literal "a.b" must not act as a regex wildcard.
        const QRegularExpression rx = TextMatchFinder::buildPattern(
            QStringLiteral("a.b"), false, false, false);
        QVERIFY(rx.isValid());
        QVERIFY(rx.match(QStringLiteral("a.b")).hasMatch());
        QVERIFY(!rx.match(QStringLiteral("axb")).hasMatch());
    }

    void patternInvalidRegexIsReported() {
        const QRegularExpression rx = TextMatchFinder::buildPattern(
            QStringLiteral("(unclosed"), true, false, true);
        QVERIFY2(!rx.isValid(), "Syntactically bad regex must surface as invalid");
    }

    void patternWholeWordsWrapsLiteral() {
        const QRegularExpression rx = TextMatchFinder::buildPattern(
            QStringLiteral("alpha"), true, true, false);
        QVERIFY(rx.isValid());
        QVERIFY(rx.match(QStringLiteral("alpha")).hasMatch());
        QVERIFY(!rx.match(QStringLiteral("alphabet")).hasMatch());
    }

    // ── TextMatchFinder::findMatches ─────────────────────────────────────

    void findMatchesExactCaseInsensitiveDefault() {
        const QString path = createThreePagePdf(m_tmpDir, QStringLiteral("exact.pdf"));
        QVERIFY2(!path.isEmpty(), "PDF creation failed");
#ifdef HAS_PDFIUM
        const QRegularExpression rx = TextMatchFinder::buildPattern(
            QStringLiteral("alpha"), false, false, false);
        const QList<TextMatch> matches = TextMatchFinder::findMatches(
            path, {0, 1, 2}, rx);
        // page0: "Alpha" + inside "alphabet" = 2; page1 "ALPHA" = 1; page2 = 1.
        QCOMPARE(matches.size(), 4);
        for (const auto& m : matches) {
            QVERIFY2(!m.rect.isEmpty(), "Match must carry real geometry");
            QVERIFY2(!m.text.isEmpty(), "Match must carry the matched text");
            QVERIFY2(m.fontSize > 0.0,
                     "Match must carry the matched font size for the replacement draw");
        }
#else
        QSKIP("PDFium not available in this build");
#endif
    }

    void findMatchesMatchCaseRestricts() {
        const QString path = createThreePagePdf(m_tmpDir, QStringLiteral("case.pdf"));
        QVERIFY2(!path.isEmpty(), "PDF creation failed");
#ifdef HAS_PDFIUM
        const QRegularExpression rx = TextMatchFinder::buildPattern(
            QStringLiteral("alpha"), true, false, false);
        const QList<TextMatch> matches = TextMatchFinder::findMatches(
            path, {0, 1, 2}, rx);
        // Lowercase "alpha" only: inside page0 "alphabet" + page2 = 2
        // (page0 "Alpha" and page1 "ALPHA" are excluded by the case flag).
        QCOMPARE(matches.size(), 2);
#endif
    }

    void findMatchesWholeWords() {
        const QString path = createThreePagePdf(m_tmpDir, QStringLiteral("words.pdf"));
        QVERIFY2(!path.isEmpty(), "PDF creation failed");
#ifdef HAS_PDFIUM
        const QRegularExpression rx = TextMatchFinder::buildPattern(
            QStringLiteral("alpha"), false, true, false);
        const QList<TextMatch> matches = TextMatchFinder::findMatches(
            path, {0, 1, 2}, rx);
        // One whole word per page; the "alphabet" interior hit is excluded.
        QCOMPARE(matches.size(), 3);
#endif
    }

    void findMatchesRegex() {
        const QString path = createThreePagePdf(m_tmpDir, QStringLiteral("regex.pdf"));
        QVERIFY2(!path.isEmpty(), "PDF creation failed");
#ifdef HAS_PDFIUM
        const QRegularExpression rx = TextMatchFinder::buildPattern(
            QStringLiteral("alpha\\w*"), false, false, true);
        const QList<TextMatch> matches = TextMatchFinder::findMatches(
            path, {0, 1, 2}, rx);
        QCOMPARE(matches.size(), 4);
        bool sawAlphabet = false;
        for (const auto& m : matches)
            if (m.text == QStringLiteral("alphabet")) sawAlphabet = true;
        QVERIFY2(sawAlphabet, "Regex mode must match the longer 'alphabet' span");
#endif
    }

    void findMatchesPageScope() {
        const QString path = createThreePagePdf(m_tmpDir, QStringLiteral("scope.pdf"));
        QVERIFY2(!path.isEmpty(), "PDF creation failed");
#ifdef HAS_PDFIUM
        const QRegularExpression rx = TextMatchFinder::buildPattern(
            QStringLiteral("alpha"), false, false, false);
        QCOMPARE(TextMatchFinder::findMatches(path, {1}, rx).size(), 1);
        QCOMPARE(TextMatchFinder::findMatches(path, {0, 2}, rx).size(), 3);
        QCOMPARE(TextMatchFinder::findMatches(path, {}, rx).size(), 0);
#endif
    }

    // ── Pack A review F2: supplementary characters and match geometry ─────
    //
    // Pack A preservation review F2: extractCharBoxes stores one CharBox per
    // PDFium char index, but a supplementary code point (emoji etc.) becomes
    // TWO UTF-16 units in the reconstructed page text, so QString match
    // offsets (UTF-16 units) stopped lining up with the CharBox array —
    // matches on pages with supplementary text got the WRONG geometry and the
    // replace pipeline excised/redrew the wrong glyphs.

#ifdef HAS_PDFIUM
    // Premise probe: how does PDFium expose supplementary chars? Dump the raw
    // char stream of the supplementary fixture (index → unicode + box). The
    // F2 defect exists exactly when a supplementary code point occupies ONE
    // char index while contributing TWO UTF-16 units to the QString offsets.
    void probeSupplementaryExtraction() {
        const QString path = createSupplementaryPdf(
            m_tmpDir, QStringLiteral("probe.pdf"));
        if (path.isEmpty())
            QSKIP("seguiemj.ttf unavailable — supplementary premise not constructible");

        PdfiumEnvironment env;
        FPDF_DOCUMENT doc = FPDF_LoadDocument(path.toLocal8Bit().constData(), nullptr);
        QVERIFY2(doc, "probe fixture must load");
        FPDF_PAGE page = FPDF_LoadPage(doc, 0);
        QVERIFY(page);
        FPDF_TEXTPAGE textPage = FPDFText_LoadPage(page);
        QVERIFY(textPage);
        const int count = FPDFText_CountChars(textPage);
        qDebug() << "FPDFText_CountChars =" << count;
        QString reconstructed;
        for (int i = 0; i < count; ++i) {
            const unsigned int u = FPDFText_GetUnicode(textPage, i);
            double l = 0, r = 0, b = 0, t = 0;
            const bool hasBox = FPDFText_GetCharBox(textPage, i, &l, &r, &b, &t);
            const char32_t cp = static_cast<char32_t>(u);
            reconstructed.append(QString::fromUcs4(&cp, 1));
            qDebug().nospace() << "char[" << i << "] U+" << QString::number(u, 16)
                               << " box=" << hasBox;
        }
        QString unitsDump;
        for (const QChar c : reconstructed)
            unitsDump += QStringLiteral("U+%1 ").arg(QString::number(c.unicode(), 16));
        qDebug().noquote() << "reconstructed units:" << unitsDump;
        const char32_t emojiCp = 0x1F600;
        qDebug() << "reconstructed =" << reconstructed
                 << "utf16 units =" << reconstructed.size()
                 << "vs char indexes =" << count
                 << "contains emoji pair:" << reconstructed.contains(
                        QString::fromUcs4(&emojiCp, 1));
        FPDFText_ClosePage(textPage);
        FPDF_ClosePage(page);
        FPDF_CloseDocument(doc);
    }

    // F2 scenario A: supplementary chars BEFORE the match. The match rect
    // must stay on the "alpha" line — a UTF-16/CharBox misalignment drags
    // glyphs from the neighbouring lines (60pt apart) into the union box.
    void supplementaryBeforeMatchKeepsMatchGeometry() {
        const QString path = createSupplementaryPdf(
            m_tmpDir, QStringLiteral("supp_before.pdf"));
        if (path.isEmpty())
            QSKIP("seguiemj.ttf unavailable — supplementary premise not constructible");
        const QRegularExpression rx = TextMatchFinder::buildPattern(
            QStringLiteral("alpha"), true, false, false);
        const QList<TextMatch> matches = TextMatchFinder::findMatches(path, {0}, rx);
        QVERIFY2(matches.size() == 1,
                 qPrintable(QStringLiteral("expected exactly the 'alpha' match, got %1")
                                .arg(matches.size())));
        const QRectF rect = matches.first().rect;
        QVERIFY2(!rect.isEmpty(), "match must carry real geometry");
        QVERIFY2(rect.height() < 40.0,
                 qPrintable(QStringLiteral("match box must stay on the alpha line "
                                           "(single-line height), got height %1 — "
                                           "supplementary chars shifted the geometry")
                               .arg(rect.height())));
        // And it must be the alpha line (y=700), not the emoji (760) or the
        // omega line (640): in Qt top-left coords the alpha band is around
        // pageHeight(842)-712 .. 842-700.
        QVERIFY2(rect.top() > 842.0 - 760.0 && rect.bottom() < 842.0 - 640.0,
                 qPrintable(QStringLiteral("match box must sit in the alpha line band, "
                                           "got %1x%2+%3+%4")
                               .arg(rect.width()).arg(rect.height())
                               .arg(rect.x()).arg(rect.y())));
    }

    // F2 scenario B: supplementary char INSIDE the matched span. The match
    // text must come back as the exact decoded string (b + U+1F600 + c) and
    // the box must span only that run — not the far-away 'd' at x=500.
    void supplementaryInsideMatchKeepsNeighborGeometry() {
        const QString path = createSupplementaryInlinePdf(
            m_tmpDir, QStringLiteral("supp_inside.pdf"));
        if (path.isEmpty())
            QSKIP("seguiemj.ttf unavailable — supplementary premise not constructible");
        const char32_t emojiCp[1] = { 0x1F600 };
        const QString needle = QStringLiteral("b") + QString::fromUcs4(emojiCp, 1)
            + QStringLiteral("c");
        const QRegularExpression rx = TextMatchFinder::buildPattern(needle, true, false, false);
        const QList<TextMatch> matches = TextMatchFinder::findMatches(path, {0}, rx);
        QVERIFY2(matches.size() == 1,
                 qPrintable(QStringLiteral("the emoji-spanning needle must match once; "
                                           "got %1 (was the emoji text mangled?)")
                                .arg(matches.size())));
        QCOMPARE(matches.first().text, needle);
        QVERIFY2(matches.first().rect.width() < 200.0,
                 qPrintable(QStringLiteral("match box must not swallow the far "
                                           "neighbor 'd' (x=500); width=%1")
                               .arg(matches.first().rect.width())));
    }

    // F2 end-to-end: replace on a page WITH supplementary text must excise
    // exactly the matched operator, keep the emoji and the neighbour line,
    // and survive save + reopen.
    void supplementaryPageReplaceRoundTrip() {
        const QString path = createSupplementaryPdf(
            m_tmpDir, QStringLiteral("supp_replace.pdf"));
        if (path.isEmpty())
            QSKIP("seguiemj.ttf unavailable — supplementary premise not constructible");
        const QRegularExpression rx = TextMatchFinder::buildPattern(
            QStringLiteral("alpha"), true, false, false);
        const QList<TextMatch> matches = TextMatchFinder::findMatches(path, {0}, rx);
        QVERIFY(matches.size() == 1);

        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(path));
        TextReplacementSpec s;
        s.pageIndex = matches.first().pageIndex;
        s.rect = matches.first().rect;
        s.text = QStringLiteral("OMEGA");
        s.fontSize = matches.first().fontSize;
        QList<double> widths;
        QVERIFY2(engine.replaceTextRegions({s}, &widths),
                 "replace must succeed on the supplementary page");
        QVERIFY(engine.saveDocument(path));

        PdfiumBackend reader;
        QVERIFY(reader.loadDocument(path));
        const QString page0 = reader.extractText(0);
        QVERIFY2(page0.contains(QStringLiteral("OMEGA")),
                 "the replacement must land in the saved artifact");
        QVERIFY2(!page0.contains(QStringLiteral("alpha")),
                 "the matched text must be excised");
        QVERIFY2(page0.contains(QStringLiteral("omega")),
                 "the neighbour line must survive the replace");
        const char32_t emojiCp[1] = { 0x1F600 };
        QVERIFY2(page0.contains(QString::fromUcs4(emojiCp, 1)),
                 "the supplementary text must survive the replace");
    }
#endif

    // ── Engine replace: excision + redraw, measured widths, SAVED artifact ──

    void engineReplaceRoundTripInSavedArtifact() {
        const QString src = createThreePagePdf(m_tmpDir, QStringLiteral("repl_src.pdf"));
        QVERIFY2(!src.isEmpty(), "PDF creation failed");
        const QString out = m_tmpDir.filePath(QStringLiteral("repl_out.pdf"));

        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(src));

        // Find first (the real pipeline order: matcher decides the specs).
        const QRegularExpression rx = TextMatchFinder::buildPattern(
            QStringLiteral("alpha"), false, false, false);
        const QList<TextMatch> matches = TextMatchFinder::findMatches(src, {0, 1, 2}, rx);
        QCOMPARE(matches.size(), 4);

        QList<TextReplacementSpec> specs;
        for (const auto& m : matches) {
            TextReplacementSpec s;
            s.pageIndex = m.pageIndex;
            s.rect = m.rect;
            s.text = QStringLiteral("omega");
            s.fontSize = m.fontSize;
            specs.append(s);
        }

        QList<double> drawnWidths;
        QVERIFY2(engine.replaceTextRegions(specs, &drawnWidths),
                 "replaceTextRegions must succeed on an editable text PDF");
        QCOMPARE(drawnWidths.size(), specs.size());

        // Save + reopen: the SAVED artifact must carry the replacement text
        // and must no longer carry the matched text (real excision, not an
        // overlay painted on top of the original glyphs).
        QVERIFY(engine.saveDocument(out));

        PdfiumBackend reader;
        QVERIFY(reader.loadDocument(out));
        const QString page0 = reader.extractText(0);
        const QString page1 = reader.extractText(1);
        QVERIFY2(page0.contains(QStringLiteral("omega")),
                 "Replacement text must be present in the saved artifact");
        QVERIFY2(page1.contains(QStringLiteral("omega")),
                 "Replacement text must be present on every scoped page");
        QVERIFY2(!page0.contains(QStringLiteral("Alpha"), Qt::CaseInsensitive),
                 "Original match must be excised from the saved artifact");
        QVERIFY2(!page1.contains(QStringLiteral("ALPHA"), Qt::CaseInsensitive),
                 "Original match must be excised from the saved artifact");
        // Untouched page content survives.
        QVERIFY2(page2Contains(reader, QStringLiteral("delta")),
                 "Unmatched text on a scoped page must survive the replace");
    }

    void engineReplaceReportsWidthChanges() {
        const QString src = createThreePagePdf(m_tmpDir, QStringLiteral("width.pdf"));
        QVERIFY2(!src.isEmpty(), "PDF creation failed");

        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(src));

        const QRegularExpression rx = TextMatchFinder::buildPattern(
            QStringLiteral("ALPHA"), true, false, false);
        const QList<TextMatch> matches = TextMatchFinder::findMatches(src, {1}, rx);
        QCOMPARE(matches.size(), 1);

        // A much longer replacement must produce a measured drawn width that
        // differs from the match width — this is the reflow warning signal.
        QList<TextReplacementSpec> specs;
        TextReplacementSpec s;
        s.pageIndex = matches.first().pageIndex;
        s.rect = matches.first().rect;
        s.text = QStringLiteral("OMEGA-OMEGA-OMEGA-OMEGA");
        s.fontSize = matches.first().fontSize;
        specs.append(s);

        QList<double> drawnWidths;
        QVERIFY(engine.replaceTextRegions(specs, &drawnWidths));
        QCOMPARE(drawnWidths.size(), 1);
        QVERIFY2(drawnWidths.first() > matches.first().rect.width() + 0.5,
                 "A longer replacement must measure wider than the match box "
                 "(the M8 geometry warning input)");
    }

    void engineReplaceInvalidPageRefuses() {
        const QString src = createThreePagePdf(m_tmpDir, QStringLiteral("badpage.pdf"));
        QVERIFY2(!src.isEmpty(), "PDF creation failed");

        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(src));

        TextReplacementSpec s;
        s.pageIndex = 99;   // out of range
        s.rect = QRectF(50, 690, 40, 12);
        s.text = QStringLiteral("x");
        s.fontSize = 12;
        QList<double> widths;
        QVERIFY2(!engine.replaceTextRegions({s}, &widths),
                 "Out-of-range page must refuse the whole replace pass");
    }

    void mockReplaceSeamRecordsSpecs() {
        // The interface-change convention: the mock member has NO `override`,
        // so this translation unit must also compile against pre-baseline
        // headers where ITextReplacer does not exist yet.
        MockPdfEditorEngine mock;
        mock.m_loaded = true;
        TextReplacementSpec s;
        s.pageIndex = 2;
        s.rect = QRectF(10, 10, 30, 10);
        s.text = QStringLiteral("replacement");
        s.fontSize = 14;
        QList<double> widths;
        QVERIFY(mock.replaceTextRegions({s}, &widths));
        QCOMPARE(mock.m_lastReplaceSpecs.size(), 1);
        QCOMPARE(mock.m_lastReplaceSpecs.first().pageIndex, 2);
        QCOMPARE(widths.size(), 1);
        QCOMPARE(widths.first(), 30.0);   // mock reports the rect width
    }

    void reflowWarningsListLengthDifferences() {
        QList<TextMatch> matches;
        TextMatch a; a.pageIndex = 0; a.text = QStringLiteral("Alpha");
        TextMatch b; b.pageIndex = 2; b.text = QStringLiteral("alpha");
        matches.append(a);
        matches.append(b);
        const auto warnings = TextMatchFinder::reflowWarnings(
            matches, QStringLiteral("OMEGA"));
        QCOMPARE(warnings.size(), 2);
        QCOMPARE(warnings.first().pageIndex, 0);
        QCOMPARE(warnings.first().matched, QStringLiteral("Alpha"));
        QCOMPARE(warnings.first().replacement, QStringLiteral("OMEGA"));
    }

    // ── FindReplaceDialog (modeless; invoker injected — no MainWindow) ──────

    void dialogShowsMatchCountBeforeReplace() {
        const QString path = createThreePagePdf(m_tmpDir, QStringLiteral("dialog.pdf"));
        QVERIFY2(!path.isEmpty(), "PDF creation failed");

        FindReplaceDialog dlg;
        bool invoked = false;
        dlg.setDocumentContext(path, 3, 2,
            [&](const ReplaceOptions&) {
                invoked = true;
                ReplaceOutcome out;
                out.ok = true;
                return out;
            });

        auto* search = dlg.findChild<QLineEdit*>(QStringLiteral("frSearch"));
        QVERIFY(search);
        QTest::keyClicks(search, QStringLiteral("alpha"));
        dlg.recount();

        // The research row: show match count BEFORE replace.
        QVERIFY2(dlg.matchSummaryText().contains(QStringLiteral("4")),
                 qPrintable(QStringLiteral("Expected the default 4-match count, got: ")
                            + dlg.matchSummaryText()));
        QVERIFY2(!invoked, "Counting must never mutate the document");
    }

    void dialogScopeCurrentPageAndRange() {
        const QString path = createThreePagePdf(m_tmpDir, QStringLiteral("dialog_scope.pdf"));
        QVERIFY2(!path.isEmpty(), "PDF creation failed");

        FindReplaceDialog dlg;
        dlg.setDocumentContext(path, 3, 2, [](const ReplaceOptions&) { return ReplaceOutcome{}; });

        auto* search = dlg.findChild<QLineEdit*>(QStringLiteral("frSearch"));
        auto* scope = dlg.findChild<QComboBox*>(QStringLiteral("frScope"));
        auto* range = dlg.findChild<QLineEdit*>(QStringLiteral("frRange"));
        QVERIFY(search && scope && range);
        search->setText(QStringLiteral("alpha"));

        // Current page = page 2 (1-based) → exactly one match on page index 1.
        scope->setCurrentIndex(1);
        dlg.recount();
        QVERIFY2(!dlg.matchSummaryText().contains(QStringLiteral("4 match")),
                 qPrintable(dlg.matchSummaryText()));
        QVERIFY2(dlg.matchSummaryText().contains(QStringLiteral("1 match")),
                 qPrintable(dlg.matchSummaryText()));

        // Range "1-2" (pages 0..1) → 3 matches on that scope: "Alpha" +
        // the "alphabet" interior hit on page 0, "ALPHA" on page 1.
        scope->setCurrentIndex(2);
        range->setText(QStringLiteral("1-2"));
        dlg.recount();
        QVERIFY2(dlg.matchSummaryText().contains(QStringLiteral("3 match")),
                 qPrintable(dlg.matchSummaryText()));

        // The composed options must carry the 0-based scope {0, 1}.
        const ReplaceOptions options = dlg.currentOptions();
        QCOMPARE(options.pages.size(), 2);
        QCOMPARE(options.pages.first(), 0);
        QCOMPARE(options.pages.last(), 1);
    }

    void dialogReplaceRoutesThroughSingleInvoker() {
        const QString path = createThreePagePdf(m_tmpDir, QStringLiteral("dialog_apply.pdf"));
        QVERIFY2(!path.isEmpty(), "PDF creation failed");

        FindReplaceDialog dlg;
        ReplaceOptions captured;
        int calls = 0;
        dlg.setDocumentContext(path, 3, 1,
            [&](const ReplaceOptions& o) {
                ++calls;
                captured = o;
                ReplaceOutcome out;
                out.ok = true;
                out.requested = 4;
                out.applied = 4;
                out.widthChanged = 2;
                out.message = QStringLiteral("Replaced 4 of 4 occurrence(s).");
                return out;
            });

        auto* search = dlg.findChild<QLineEdit*>(QStringLiteral("frSearch"));
        auto* replace = dlg.findChild<QLineEdit*>(QStringLiteral("frReplace"));
        auto* regex = dlg.findChild<QCheckBox*>(QStringLiteral("frRegex"));
        auto* caseBox = dlg.findChild<QCheckBox*>(QStringLiteral("frMatchCase"));
        QVERIFY(search && replace && regex && caseBox);
        search->setText(QStringLiteral("al\\w+"));
        replace->setText(QStringLiteral("OMEGA"));
        regex->setChecked(true);
        caseBox->setChecked(true);
        dlg.applyReplace();

        QCOMPARE(calls, 1);
        QCOMPARE(captured.searchText, QStringLiteral("al\\w+"));
        QCOMPARE(captured.replaceText, QStringLiteral("OMEGA"));
        QVERIFY(captured.useRegex);
        QVERIFY(captured.matchCase);
        QVERIFY2(captured.pages.isEmpty(),
                 "Default scope (All pages) must pass an empty page list");

        // The outcome pane must surface the measured geometry warnings.
        QVERIFY2(dlg.outcomeText().contains(QStringLiteral("width")),
                 qPrintable(dlg.outcomeText()));
        QVERIFY2(dlg.outcomeText().contains(QStringLiteral("Replaced 4 of 4")),
                 qPrintable(dlg.outcomeText()));
    }

    // WP-R07 (WHOLE-PRODUCT-AND-PLAN-REVIEW-2026-09-10): Replace All success
    // counts come from COMMITTED outcomes only. When the save fails, the
    // pipeline hands back ok=false with the in-memory count — the dialog must
    // report the failure, never a "Replaced N" success line, and no geometry
    // breakdown for an outcome that never landed on disk.
    void refusedOutcomeIsNeverReportedAsSuccessCount() {
        FindReplaceDialog dlg;
        dlg.setDocumentContext(QStringLiteral("unused.pdf"), 3, 1,
            [](const ReplaceOptions&) {
                ReplaceOutcome out;
                out.ok = false;
                out.requested = 4;
                out.applied = 4;   // drawn in memory, then the save failed
                out.message = QStringLiteral(
                    "The replacements were applied in memory, but the file "
                    "could not be saved. Check that the disk is not full and "
                    "the file is not write-protected.");
                return out;
            });

        auto* search = dlg.findChild<QLineEdit*>(QStringLiteral("frSearch"));
        auto* btn = dlg.findChild<QPushButton*>(QStringLiteral("frReplaceAll"));
        QVERIFY(search && btn);
        search->setText(QStringLiteral("alpha"));
        btn->click();

        const QString outcome = dlg.outcomeText();
        QVERIFY2(outcome.contains(QStringLiteral("could not be saved")),
                 qPrintable(QStringLiteral("the failure reason must be shown; got: ")
                            + outcome));
        QVERIFY2(!outcome.contains(QStringLiteral("Replaced")),
                 "WP-R07: a failed save must never be reported as a success count");
        QVERIFY2(!outcome.contains(QStringLiteral("Geometry:")),
                 "WP-R07: no geometry breakdown for an outcome that never committed");
    }

    // WP-R07: an unusable scope must be refused EXPLICITLY. An empty page
    // list flows downstream as "all pages", so a malformed range (or one that
    // lies entirely outside the document) must never silently widen the
    // mutation to the whole document — the dialog refuses in plain text and
    // the invoker is never called for the refused scope.
    void unusableRangeIsRefusedInsteadOfWideningScope() {
        const QString path = createThreePagePdf(m_tmpDir, QStringLiteral("dialog_range_refusal.pdf"));
        QVERIFY2(!path.isEmpty(), "PDF creation failed");

        FindReplaceDialog dlg;
        int calls = 0;
        ReplaceOptions captured;
        dlg.setDocumentContext(path, 3, 2,
            [&](const ReplaceOptions& o) {
                ++calls;
                captured = o;
                ReplaceOutcome out;
                out.ok = true;
                return out;
            });

        auto* search = dlg.findChild<QLineEdit*>(QStringLiteral("frSearch"));
        auto* scope = dlg.findChild<QComboBox*>(QStringLiteral("frScope"));
        auto* range = dlg.findChild<QLineEdit*>(QStringLiteral("frRange"));
        auto* replaceAll = dlg.findChild<QPushButton*>(QStringLiteral("frReplaceAll"));
        QVERIFY(search && scope && range && replaceAll);
        search->setText(QStringLiteral("alpha"));
        scope->setCurrentIndex(2);

        // Garbage range text must be refused, not counted over all pages.
        range->setText(QStringLiteral("abc"));
        dlg.recount();
        QVERIFY2(dlg.matchSummaryText().contains(QStringLiteral("refused")),
                 qPrintable(QStringLiteral("malformed range must be refused; got: ")
                            + dlg.matchSummaryText()));

        // A range entirely outside the 3-page document: the same refusal, and
        // Replace All must not fall back to the whole document. packa-F1
        // strengthened this: Count/Replace are DISABLED for a refused scope,
        // so the click path itself is closed (the mouse path on a disabled
        // button is a no-op).
        range->setText(QStringLiteral("9-9"));
        dlg.recount();
        QVERIFY2(dlg.matchSummaryText().contains(QStringLiteral("refused")),
                 qPrintable(QStringLiteral("out-of-document range must be refused; got: ")
                            + dlg.matchSummaryText()));
        QVERIFY2(!replaceAll->isEnabled(),
                 "packa-F1: Replace All must be disabled for a refused scope");
        QCOMPARE(calls, 0);
        // Defense in depth: even a FORCED applyReplace() (bypassing the
        // disabled button) refuses and never reaches the invoker.
        dlg.applyReplace();
        QVERIFY2(dlg.outcomeText().contains(QStringLiteral("refused")),
                 qPrintable(QStringLiteral("the refusal must be shown in the outcome; got: ")
                            + dlg.outcomeText()));

        // A VALID range still goes through — the refusal is not sticky — and
        // the invoked scope is exactly the requested 0-based page.
        range->setText(QStringLiteral("2-2"));
        dlg.recount();
        QVERIFY2(!dlg.matchSummaryText().contains(QStringLiteral("refused")),
                 qPrintable(dlg.matchSummaryText()));
        replaceAll->click();
        QCOMPARE(calls, 1);
        QCOMPARE(captured.pages.size(), 1);
        QCOMPARE(captured.pages.first(), 1);
    }

    // ── packa-F1: invalid scope is its own state; Count/Replace disabled ───

    // packa-F1: a malformed range must disable Count AND Replace All (not
    // merely show a refusal while the buttons stay armed). A disabled button
    // ignores the real mouse path; a valid scope re-enables them.
    void invalidScopeDisablesCountAndReplaceButtons() {
        FindReplaceDialog dlg;   // no document yet
        auto* count = dlg.findChild<QPushButton*>(QStringLiteral("frCount"));
        auto* replaceAll = dlg.findChild<QPushButton*>(QStringLiteral("frReplaceAll"));
        QVERIFY(count && replaceAll);
        QVERIFY2(!count->isEnabled() && !replaceAll->isEnabled(),
                 "without a document, Count/Replace must start disabled");

        const QString path = createThreePagePdf(
            m_tmpDir, QStringLiteral("dlg_buttons.pdf"));
        QVERIFY2(!path.isEmpty(), "PDF creation failed");
        int calls = 0;
        dlg.setDocumentContext(path, 3, 2,
            [&](const ReplaceOptions&) { ++calls; return ReplaceOutcome{}; });
        auto* search = dlg.findChild<QLineEdit*>(QStringLiteral("frSearch"));
        auto* scope = dlg.findChild<QComboBox*>(QStringLiteral("frScope"));
        auto* range = dlg.findChild<QLineEdit*>(QStringLiteral("frRange"));
        QVERIFY(search && scope && range);

        search->setText(QStringLiteral("alpha"));
        dlg.recount();
        QVERIFY2(count->isEnabled() && replaceAll->isEnabled(),
                 "a usable whole-document scope must leave Count/Replace enabled");

        scope->setCurrentIndex(2);   // Range… with garbage text
        range->setText(QStringLiteral("abc"));
        QVERIFY2(!count->isEnabled() && !replaceAll->isEnabled(),
                 "a malformed range must disable Count AND Replace All (packa F1)");
        // The real mouse path on a disabled button is a no-op.
        QTest::mouseClick(replaceAll, Qt::LeftButton);
        QTest::mouseClick(count, Qt::LeftButton);
        QCOMPARE(calls, 0);

        // The encoded options carry the refused state, never an empty list
        // that downstream would widen to all pages.
        const ReplaceOptions refused = dlg.currentOptions();
        QVERIFY2(!refused.scopeValid,
                 "a malformed range must set scopeValid=false");

        range->setText(QStringLiteral("2-2"));   // valid — not sticky
        dlg.recount();
        QVERIFY2(count->isEnabled() && replaceAll->isEnabled(),
                 "a valid range must re-enable Count/Replace");
        QVERIFY(dlg.currentOptions().scopeValid);
    }

    // packa-F1, controller boundary: ReplaceOptions that mark the scope
    // unusable are refused by EditController::replaceAllInDocument itself
    // with ZERO mutation — the whole-document widening path is closed at the
    // shared boundary, not only in the dialog. Runs the REAL window +
    // controller over a real artifact (bytes compared before/after).
    void controllerRefusesInvalidScopeWithZeroMutation() {
        const QString path = createThreePagePdf(
            m_tmpDir, QStringLiteral("ctrl_zero.pdf"));
        QVERIFY2(!path.isEmpty(), "PDF creation failed");

        gp::MainWindow win(Bootstrapper::createContext());
        win.show();
        win.openDocument(path);
        QTRY_COMPARE_WITH_TIMEOUT(win.pdfViewer()->pageCount(), 3, 20000);
        auto* edit = win.findChild<gp::EditController*>();
        QVERIFY2(edit, "the window must own the canonical EditController");

        QFile f(path);
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QByteArray before = f.readAll();
        f.close();

        ReplaceOptions bad;
        bad.searchText = QStringLiteral("alpha");
        bad.replaceText = QStringLiteral("MUTATED");
        bad.scopeValid = false;   // pages empty: without the guard = ALL pages
        const ReplaceOutcome refused = edit->replaceAllInDocument(bad);
        QVERIFY2(!refused.ok,
                 "an invalid scope must be refused at the pipeline boundary");
        QVERIFY2(refused.message.contains(QStringLiteral("scope")),
                 qPrintable(refused.message));
        QVERIFY2(refused.applied == 0 && refused.requested == 0,
                 "a refused scope must not even count, let alone replace");

        QFile g(path);
        QVERIFY(g.open(QIODevice::ReadOnly));
        QCOMPARE(g.readAll(), before);   // ZERO mutation on bad input
        g.close();

        // Contrast: the same request with a valid scope DOES run the
        // pipeline (proves the guard, not a broken pipeline, refused above).
        ReplaceOptions good = bad;
        good.scopeValid = true;
        const ReplaceOutcome applied = edit->replaceAllInDocument(good);
        QVERIFY2(applied.ok && applied.applied == 4,
                 qPrintable(applied.message));
        QFile h(path);
        QVERIFY(h.open(QIODevice::ReadOnly));
        QVERIFY2(h.readAll() != before,
                 "the valid-scope control must actually mutate the document");
        h.close();
    }

private:
    static bool page2Contains(PdfiumBackend& reader, const QString& needle) {
        return reader.extractText(2).contains(needle, Qt::CaseInsensitive);
    }
};

QTEST_MAIN(TestFindReplace)
#include "TestFindReplace.moc"
