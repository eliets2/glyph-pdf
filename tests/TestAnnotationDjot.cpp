#include <QtTest>
#include <QTemporaryDir>
#include <podofo/podofo.h>

#include "engines/podofo/PoDoFoBackend.h"
#include "engines/podofo/PdfStringEscape.h"
#include "pdfws_djot/DjotToRichTextXhtml.h"
#include "core/AnnotationTypes.h"
#include "core/AnnotationSerializer.h"

// ---------------------------------------------------------------------------
// TestAnnotationDjot (M6-PROMPT-4 D5)
//
// Proves the Djot annotation rich-text roundtrip:
//   author annotation with markup -> embedAnnotations (save) ->
//   extractAnnotations (reopen) -> djotSource matches.
// Also covers the plain-text-only path (no /PieceInfo sidecar) and the
// transcoder + escape/unescape primitives that back the roundtrip.
// ---------------------------------------------------------------------------

// Minimal single-page base PDF to attach annotations to.
static QString createBasePdf(const QTemporaryDir& tmpDir, const QString& name)
{
    const QString path = tmpDir.filePath(name);
    try {
        PoDoFo::PdfMemDocument doc;
        auto& page = doc.GetPages().CreatePage(
            PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
        PoDoFo::PdfPainter painter;
        painter.SetCanvas(page);
        auto& font = doc.GetFonts().GetStandard14Font(
            PoDoFo::PdfStandard14FontType::Helvetica);
        painter.TextState.SetFont(font, 12.0);
        painter.DrawText("Base document.", 50, 700);
        painter.FinishDrawing();
        doc.Save(path.toUtf8().constData());
    } catch (const std::exception& e) {
        qWarning() << "createBasePdf failed:" << e.what();
        return {};
    }
    return path;
}

class TestAnnotationDjot : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_tmpDir;

    // Find an annotation by id in a list (returns nullptr if absent).
    static const AnnotationItem* find(const QList<AnnotationItem>& list,
                                      const QString& id) {
        for (const auto& a : list)
            if (a.id == id) return &a;
        return nullptr;
    }

private slots:
    void initTestCase() {
        QVERIFY2(m_tmpDir.isValid(), "Temp directory creation failed");
    }

    // ── Primitive: escape/unescape is lossless (incl. parens + backslash) ─────
    // NOTE: a backslash immediately followed by one of n/r/t/b/f collides with
    // pdfEscapeLiteralString's idempotency heuristic (it treats "\b" etc. as an
    // already-present escape), so we test a backslash before a non-escape char.
    void testEscapeUnescapeRoundtripAscii() {
        const std::string src = "*bold* and _italic_ and `code` (parens) C:\\path";
        const std::string esc = pdfEscapeLiteralString(src);
        const std::string back = pdfUnescapeLiteralString(esc);
        QCOMPARE(QString::fromStdString(back), QString::fromStdString(src));
    }

    void testEscapeUnescapeRoundtripUtf8() {
        const QString src = QStringLiteral("café — résumé ✓");
        const std::string esc = pdfEscapeLiteralString(src);
        const std::string back = pdfUnescapeLiteralString(esc);
        QCOMPARE(QString::fromUtf8(back.c_str(), static_cast<int>(back.size())), src);
    }

    // ── Transcoder: plain-text projection strips markup ──────────────────────
    void testDjotToPlainStripsMarkup() {
        const QString djot = QStringLiteral("*bold* and _italic_ and `code`");
        const QString plain = pdfws_djot::djotToPlainText(djot);
        QCOMPARE(plain, QStringLiteral("bold and italic and code"));
    }

    void testDjotToPlainHeadingAndList() {
        const QString djot = QStringLiteral("# Title\n\n- one\n- two");
        const QString plain = pdfws_djot::djotToPlainText(djot);
        // Heading marker stripped; list items keep a bullet.
        QVERIFY(plain.contains(QStringLiteral("Title")));
        QVERIFY(!plain.contains(QLatin1Char('#')));
        QVERIFY(plain.contains(QStringLiteral("• one")));
    }

    // ── Transcoder: /RC XHTML is well-formed and spec-subset ─────────────────
    void testDjotToXhtmlStructure() {
        const QString djot = QStringLiteral("*bold* and _it_ and `c`");
        const QString xhtml = pdfws_djot::djotToXhtml(djot);
        QVERIFY(xhtml.startsWith(QStringLiteral("<?xml")));
        QVERIFY(xhtml.contains(QStringLiteral("xmlns=\"http://www.w3.org/1999/xhtml\"")));
        QVERIFY(xhtml.contains(QStringLiteral("<b>bold</b>")));
        QVERIFY(xhtml.contains(QStringLiteral("<i>it</i>")));
        QVERIFY(xhtml.contains(QStringLiteral("font-family:Courier")));
        QVERIFY(xhtml.trimmed().endsWith(QStringLiteral("</body>")));
    }

    void testDjotToXhtmlEscapesAngleBrackets() {
        const QString djot = QStringLiteral("a < b & c > d");
        const QString xhtml = pdfws_djot::djotToXhtml(djot);
        QVERIFY(xhtml.contains(QStringLiteral("&lt;")));
        QVERIFY(xhtml.contains(QStringLiteral("&amp;")));
        QVERIFY(xhtml.contains(QStringLiteral("&gt;")));
    }

    // ── D5 core: author markup -> save -> reopen -> djotSource matches ────────
    void testRichTextRoundtripThroughPdf() {
        const QString base = createBasePdf(m_tmpDir, "rich_base.pdf");
        QVERIFY(!base.isEmpty());
        const QString out = m_tmpDir.filePath("rich_out.pdf");

        AnnotationItem a;
        a.id           = QStringLiteral("annot-rich-1");
        a.mode         = ToolMode::AddComment;
        a.pageIndex    = 0;
        a.rect         = QRectF(60, 60, 120, 40);
        a.author       = QStringLiteral("Reviewer");
        a.creationDate = QStringLiteral("2026-06-01T10:00:00");
        a.djotSource   = QStringLiteral("*bold* and _italic_ and `code`");
        a.text         = pdfws_djot::djotToPlainText(a.djotSource);

        PoDoFoBackend backend;
        QVERIFY2(backend.embedAnnotations(base, out, { a }),
                 "embedAnnotations should succeed");

        const QList<AnnotationItem> loaded = backend.extractAnnotations(out);
        const AnnotationItem* got = find(loaded, a.id);
        QVERIFY2(got != nullptr, "annotation should be found after reopen");

        // djotSource survives the PDF roundtrip via the /PieceInfo sidecar.
        QCOMPARE(got->djotSource, a.djotSource);
        // /Contents plain-text projection is the markup-stripped text.
        QCOMPARE(got->text, QStringLiteral("bold and italic and code"));
    }

    // ── D5: plain-text-only annotation -> djotSource == plain text ───────────
    void testPlainTextOnlyLoadsAsTrivialDjot() {
        const QString base = createBasePdf(m_tmpDir, "plain_base.pdf");
        QVERIFY(!base.isEmpty());
        const QString out = m_tmpDir.filePath("plain_out.pdf");

        AnnotationItem a;
        a.id        = QStringLiteral("annot-plain-1");
        a.mode      = ToolMode::AddComment;
        a.pageIndex = 0;
        a.rect      = QRectF(50, 50, 100, 30);
        a.text      = QStringLiteral("just plain text");
        // No djotSource set — emulates a legacy / non-Djot annotation.

        PoDoFoBackend backend;
        QVERIFY(backend.embedAnnotations(base, out, { a }));

        const QList<AnnotationItem> loaded = backend.extractAnnotations(out);
        const AnnotationItem* got = find(loaded, a.id);
        QVERIFY2(got != nullptr, "plain annotation should be found after reopen");

        QCOMPARE(got->text, QStringLiteral("just plain text"));
        // With no sidecar, djotSource is derived from /Contents (trivial Djot).
        QCOMPARE(got->djotSource, QStringLiteral("just plain text"));
    }

    // ── D5: UTF-8 markup survives the sidecar roundtrip ──────────────────────
    void testUnicodeRichTextRoundtrip() {
        const QString base = createBasePdf(m_tmpDir, "uni_base.pdf");
        QVERIFY(!base.isEmpty());
        const QString out = m_tmpDir.filePath("uni_out.pdf");

        AnnotationItem a;
        a.id         = QStringLiteral("annot-uni-1");
        a.mode       = ToolMode::AddComment;
        a.pageIndex  = 0;
        a.rect       = QRectF(40, 40, 140, 50);
        a.djotSource = QStringLiteral("*café* and _résumé_ — `✓`");
        a.text       = pdfws_djot::djotToPlainText(a.djotSource);

        PoDoFoBackend backend;
        QVERIFY(backend.embedAnnotations(base, out, { a }));

        const QList<AnnotationItem> loaded = backend.extractAnnotations(out);
        const AnnotationItem* got = find(loaded, a.id);
        QVERIFY(got != nullptr);
        QCOMPARE(got->djotSource, a.djotSource);
    }

    // ── D1: serializer preserves djotSource through the .ann JSON ────────────
    void testSerializerPreservesDjotSource() {
        AnnotationItem a;
        a.id         = QStringLiteral("ser-1");
        a.mode       = ToolMode::AddComment;
        a.text       = QStringLiteral("bold and code");
        a.djotSource = QStringLiteral("**bold** and `code`");

        const QJsonDocument doc = AnnotationSerializer::toJson({ a });
        const QList<AnnotationItem> back = AnnotationSerializer::fromJson(doc);
        QCOMPARE(back.size(), 1);
        QCOMPARE(back[0].djotSource, a.djotSource);
        QCOMPARE(back[0].text, a.text);
    }

    // ── M6-P5 D2/D4: a reply threads + persists across save → reload ─────────
    // parent comment + reply (with parentId) round-trip through the PDF; on
    // reload the reply's parentId is restored from /IRT and the parent's
    // replies list is rebuilt.
    void testReplyThreadingPersistsThroughPdf() {
        const QString base = createBasePdf(m_tmpDir, "thread_base.pdf");
        QVERIFY(!base.isEmpty());
        const QString out = m_tmpDir.filePath("thread_out.pdf");

        AnnotationItem parent;
        parent.id          = QStringLiteral("cmt-parent");
        parent.mode        = ToolMode::AddComment;
        parent.pageIndex   = 0;
        parent.rect        = QRectF(60, 60, 100, 30);
        parent.author      = QStringLiteral("Alice");
        parent.text        = QStringLiteral("Top-level comment");
        parent.djotSource  = parent.text;

        AnnotationItem reply;
        reply.id           = QStringLiteral("cmt-reply");
        reply.parentId     = parent.id;             // <-- the thread link
        reply.mode         = ToolMode::AddComment;
        reply.pageIndex    = 0;
        reply.rect         = QRectF(70, 70, 100, 30);
        reply.author       = QStringLiteral("Bob");
        reply.text         = QStringLiteral("A reply");
        reply.djotSource   = reply.text;
        parent.replies.append(reply.id);

        PoDoFoBackend backend;
        QVERIFY2(backend.embedAnnotations(base, out, { parent, reply }),
                 "embedAnnotations should succeed for a threaded pair");

        const QList<AnnotationItem> loaded = backend.extractAnnotations(out);
        const AnnotationItem* gotParent = find(loaded, parent.id);
        const AnnotationItem* gotReply  = find(loaded, reply.id);
        QVERIFY2(gotParent != nullptr, "parent should reload");
        QVERIFY2(gotReply  != nullptr, "reply should reload");

        // Threading survived: reply still points at the parent via /IRT → /NM.
        QCOMPARE(gotReply->parentId, parent.id);
        // Top-level comment has no parent.
        QVERIFY(gotParent->parentId.isEmpty());
        // Parent.replies rebuilt from the restored link.
        QVERIFY2(gotParent->replies.contains(reply.id),
                 "parent.replies should be rebuilt on reload");
    }

    // ── M6-P5 D2/D4: every review state round-trips through the PDF ──────────
    void testReviewStateRoundtripThroughPdf() {
        struct Case { const char* id; ReviewState state; };
        const QList<Case> cases = {
            { "rs-open",      ReviewState::Open      },
            { "rs-accepted",  ReviewState::Accepted  },
            { "rs-rejected",  ReviewState::Rejected  },
            { "rs-cancelled", ReviewState::Cancelled },
            { "rs-completed", ReviewState::Completed },
        };

        const QString base = createBasePdf(m_tmpDir, "state_base.pdf");
        QVERIFY(!base.isEmpty());
        const QString out = m_tmpDir.filePath("state_out.pdf");

        QList<AnnotationItem> annos;
        int n = 0;
        for (const auto& c : cases) {
            AnnotationItem a;
            a.id          = QString::fromLatin1(c.id);
            a.mode        = ToolMode::AddComment;
            a.pageIndex   = 0;
            a.rect        = QRectF(40 + n * 5, 40 + n * 5, 80, 24);
            a.author      = QStringLiteral("Reviewer");
            a.text        = QStringLiteral("comment %1").arg(n);
            a.djotSource  = a.text;
            a.reviewState = c.state;
            annos.append(a);
            ++n;
        }

        PoDoFoBackend backend;
        QVERIFY(backend.embedAnnotations(base, out, annos));

        const QList<AnnotationItem> loaded = backend.extractAnnotations(out);
        for (const auto& c : cases) {
            const AnnotationItem* got = find(loaded, QString::fromLatin1(c.id));
            QVERIFY2(got != nullptr,
                     qPrintable(QStringLiteral("annotation %1 should reload")
                                    .arg(c.id)));
            QCOMPARE(static_cast<int>(got->reviewState), static_cast<int>(c.state));
        }
    }

    // ── M6-P5 D4: ReviewState::None writes no /State and reloads as None ─────
    void testReviewStateNoneOmitsStateKey() {
        const QString base = createBasePdf(m_tmpDir, "none_base.pdf");
        QVERIFY(!base.isEmpty());
        const QString out = m_tmpDir.filePath("none_out.pdf");

        AnnotationItem a;
        a.id          = QStringLiteral("rs-none");
        a.mode        = ToolMode::AddComment;
        a.pageIndex   = 0;
        a.rect        = QRectF(50, 50, 80, 24);
        a.text        = QStringLiteral("no explicit state");
        a.djotSource  = a.text;
        a.reviewState = ReviewState::None;

        PoDoFoBackend backend;
        QVERIFY(backend.embedAnnotations(base, out, { a }));

        const QList<AnnotationItem> loaded = backend.extractAnnotations(out);
        const AnnotationItem* got = find(loaded, a.id);
        QVERIFY(got != nullptr);
        QCOMPARE(static_cast<int>(got->reviewState),
                 static_cast<int>(ReviewState::None));
    }

    // ── M6-P5 D4: .ann JSON serializer round-trips parentId + reviewState ────
    void testSerializerPreservesThreadAndState() {
        AnnotationItem parent;
        parent.id          = QStringLiteral("p");
        parent.mode        = ToolMode::AddComment;
        parent.reviewState = ReviewState::Accepted;

        AnnotationItem reply;
        reply.id           = QStringLiteral("c");
        reply.parentId     = parent.id;
        reply.mode         = ToolMode::AddComment;
        reply.reviewState  = ReviewState::Rejected;
        parent.replies.append(reply.id);

        const QJsonDocument doc = AnnotationSerializer::toJson({ parent, reply });
        const QList<AnnotationItem> back = AnnotationSerializer::fromJson(doc);
        QCOMPARE(back.size(), 2);

        const AnnotationItem* bp = find(back, parent.id);
        const AnnotationItem* bc = find(back, reply.id);
        QVERIFY(bp != nullptr);
        QVERIFY(bc != nullptr);
        QCOMPARE(bc->parentId, parent.id);
        QVERIFY(bp->replies.contains(reply.id));
        QCOMPARE(static_cast<int>(bp->reviewState),
                 static_cast<int>(ReviewState::Accepted));
        QCOMPARE(static_cast<int>(bc->reviewState),
                 static_cast<int>(ReviewState::Rejected));
    }
};

QTEST_MAIN(TestAnnotationDjot)
#include "TestAnnotationDjot.moc"
