// SPDX-License-Identifier: Apache-2.0
// §9.16 P1: round-trip CONTRACT test for bookmark (outline) and hyperlink
// preservation. The redaction/sanitize path DELIBERATELY strips Outlines
// (PoDoFoBackend sanitizeDocumentContents step "18. Remove Outlines
// (bookmarks)") and strips URI link actions (step 24: URI/Launch/JS actions
// are exfiltration vectors; only internal GoTo survives). These tests pin
// that contract honestly, on both sides:
//
//   (a) a PLAIN saveDocument round-trip PRESERVES outline bookmarks, URI
//       link annotations and internal GoTo links — navigation data must
//       never be lost by an ordinary save;
//   (b) the SANITIZE path's stripping is INTENTIONAL AND VISIBLE: outlines
//       removed, URI actions removed, the annotation shells and internal
//       GoTo navigation kept. If this test ever fails, the sanitization
//       contract changed — it must never regress silently.
//
// Fixture idiom: hand-built via PoDoFo (same as TestHyperlinkNavigation /
// TestCompressStripSanitize): a 2-page document whose page 1 carries an
// outline bookmark, a URI link annotation and an internal GoTo link.
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <podofo/podofo.h>

#include "engines/podofo/PoDoFoBackend.h"

namespace {

constexpr const char* kBookmarkTitle = "Chapter 1";
constexpr const char* kContractUri   = "https://example.com/contract";

// Build the fixture; returns the path or an empty string on failure.
QString makeBookmarkLinkPdf(const QString& path) {
    try {
        PoDoFo::PdfMemDocument doc;
        auto& p0 = doc.GetPages().CreatePage(
            PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
        doc.GetPages().CreatePage(
            PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));

        // Outline bookmark: /Outlines with a single item whose /Dest targets
        // page 1 (index 1).
        auto& outlines = doc.GetObjects().CreateDictionaryObject();
        outlines.GetDictionary().AddKey("Type", PoDoFo::PdfObject(PoDoFo::PdfName("Outlines")));
        auto& item = doc.GetObjects().CreateDictionaryObject();
        item.GetDictionary().AddKey("Title",
            PoDoFo::PdfObject(PoDoFo::PdfString(kBookmarkTitle)));
        item.GetDictionary().AddKey("Parent", outlines.GetIndirectReference());
        PoDoFo::PdfArray dest;
        dest.Add(doc.GetPages().GetPageAt(1).GetObject().GetIndirectReference());
        dest.Add(PoDoFo::PdfObject(PoDoFo::PdfName("Fit")));
        item.GetDictionary().AddKey("Dest", PoDoFo::PdfObject(dest));
        outlines.GetDictionary().AddKey("First", item.GetIndirectReference());
        outlines.GetDictionary().AddKey("Last", item.GetIndirectReference());
        outlines.GetDictionary().AddKey("Count", PoDoFo::PdfObject(static_cast<long long>(1)));
        doc.GetCatalog().GetDictionary().AddKey("Outlines",
            PoDoFo::PdfObject(outlines.GetIndirectReference()));

        // URI link annotation on page 1.
        auto& uriAnnot = p0.GetAnnotations().CreateAnnot(
            PoDoFo::PdfAnnotationType::Link, PoDoFo::Rect(50, 700, 200, 20));
        PoDoFo::PdfDictionary uriAction;
        uriAction.AddKey("S", PoDoFo::PdfName("URI"));
        uriAction.AddKey("URI", PoDoFo::PdfString(kContractUri));
        uriAnnot.GetObject().GetDictionary().AddKey("A", PoDoFo::PdfObject(uriAction));

        // Internal GoTo link on page 1 → page index 1 (must survive sanitize).
        auto& gotoAnnot = p0.GetAnnotations().CreateAnnot(
            PoDoFo::PdfAnnotationType::Link, PoDoFo::Rect(50, 600, 150, 20));
        PoDoFo::PdfArray gotoDest;
        gotoDest.Add(doc.GetPages().GetPageAt(1).GetObject().GetIndirectReference());
        gotoDest.Add(PoDoFo::PdfObject(PoDoFo::PdfName("XYZ")));
        gotoAnnot.GetObject().GetDictionary().AddKey("Dest", PoDoFo::PdfObject(gotoDest));

        doc.Save(path.toUtf8().constData());
        return path;
    } catch (const PoDoFo::PdfError& e) {
        qWarning() << "makeBookmarkLinkPdf failed:" << e.what();
        return {};
    } catch (...) {
        return {};
    }
}

// Reload `path` and return the /Title of the FIRST outline item (empty when
// /Outlines or /First is missing).
QString firstOutlineTitle(const QString& path) {
    try {
        PoDoFo::PdfMemDocument doc;
        doc.Load(path.toUtf8().constData());
        const PoDoFo::PdfObject* outlines =
            doc.GetCatalog().GetDictionary().FindKey("Outlines");
        if (!outlines) return {};
        if (outlines->IsReference())
            outlines = &doc.GetObjects().MustGetObject(outlines->GetReference());
        if (!outlines || !outlines->IsDictionary()) return {};
        const PoDoFo::PdfObject* first = outlines->GetDictionary().FindKey("First");
        if (!first) return {};
        if (first->IsReference())
            first = &doc.GetObjects().MustGetObject(first->GetReference());
        if (!first || !first->IsDictionary()) return {};
        const PoDoFo::PdfObject* title = first->GetDictionary().FindKey("Title");
        if (!title || !title->IsString()) return {};
        return QString::fromStdString(std::string(title->GetString().GetString()));
    } catch (...) {
        return {};
    }
}

} // namespace

class TestLinkBookmarkRoundTrip : public QObject {
    Q_OBJECT

private slots:

    // (a) THE PRESERVATION CONTRACT: a plain load→save round-trip must keep
    // the outline bookmark, the URI link and the internal GoTo link. An
    // ordinary save is NOT a sanitization pass — navigation data must never
    // disappear silently.
    void plainSaveRoundTripPreservesBookmarksAndLinks() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString in = makeBookmarkLinkPdf(tmp.filePath("in.pdf"));
        QVERIFY2(!in.isEmpty(), "fixture build failed");

        // Guard: the fixture actually carries the features under contract.
        QCOMPARE(firstOutlineTitle(in), QString::fromUtf8(kBookmarkTitle));
        const QList<PdfLinkInfo> inLinks = PoDoFoBackend::extractLinks(in, 0);
        QCOMPARE(inLinks.size(), 2);

        PoDoFoBackend backend;
        QVERIFY2(backend.loadDocument(in), "loadDocument(fixture) failed");
        const QString out = tmp.filePath("plain_saved.pdf");
        QVERIFY2(backend.saveDocument(out), "saveDocument(round-trip) failed");

        // The outline bookmark survives.
        QCOMPARE(firstOutlineTitle(out), QString::fromUtf8(kBookmarkTitle));

        // Both links survive with their targets intact.
        const QList<PdfLinkInfo> outLinks = PoDoFoBackend::extractLinks(out, 0);
        QCOMPARE(outLinks.size(), 2);
        bool sawUri = false, sawGoTo = false;
        for (const auto& l : outLinks) {
            if (l.isUri) {
                sawUri = true;
                QCOMPARE(l.uri, QString::fromUtf8(kContractUri));
            } else if (l.targetPage == 1) {
                sawGoTo = true;
            }
        }
        QVERIFY2(sawUri,  "plain round-trip must preserve the URI link");
        QVERIFY2(sawGoTo, "plain round-trip must preserve the internal GoTo link");
    }

    // (b) THE SANITIZE CONTRACT, pinned as it behaves today (§9.16: honest
    // characterization — the strip is INTENTIONAL and VISIBLE, never silent):
    //   - /Outlines (bookmarks) are REMOVED — documented step 18;
    //   - URI link ACTIONS (/A) are REMOVED — documented step 24
    //     (default-deny: only internal GoTo survives);
    //   - the link annotation itself is KEPT (only its action is stripped);
    //   - the internal GoTo link KEEPS working.
    void sanitizeStrippingIsIntentionalAndVisible() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString in = makeBookmarkLinkPdf(tmp.filePath("in.pdf"));
        QVERIFY2(!in.isEmpty(), "fixture build failed");

        PoDoFoBackend backend;
        QVERIFY2(backend.loadDocument(in), "loadDocument(fixture) failed");
        const QString out = tmp.filePath("sanitized.pdf");
        QVERIFY2(backend.sanitizeDocument(out), "sanitizeDocument failed");

        // Bookmarks: intentionally and visibly gone (step 18).
        QVERIFY2(firstOutlineTitle(out).isEmpty(),
                 "sanitization removes outline bookmarks — /Outlines found "
                 "after sanitize; the sanitize contract changed and this test "
                 "must be updated consciously, never silently");

        // Links: the URI action is gone; the GoTo survives.
        const QList<PdfLinkInfo> outLinks = PoDoFoBackend::extractLinks(out, 0);
        for (const auto& l : outLinks)
            QVERIFY2(!l.isUri,
                     "sanitization must strip URI link actions (exfiltration "
                     "vector) — a URI survived; sanitize contract changed");

        bool sawGoTo = false;
        for (const auto& l : outLinks)
            if (!l.isUri && l.targetPage == 1) sawGoTo = true;
        QVERIFY2(sawGoTo,
                 "sanitization must PRESERVE internal GoTo navigation — its "
                 "removal would also be a contract change");

        // The URI annotation SHELL is kept (only its /A action was removed):
        // reload and inspect the annotation dictionaries directly.
        try {
            PoDoFo::PdfMemDocument doc;
            doc.Load(out.toUtf8().constData());
            auto& page = doc.GetPages().GetPageAt(0);
            auto& annos = page.GetAnnotations();
            QCOMPARE(static_cast<int>(annos.GetCount()), 2);
            int uriShells = 0;
            for (unsigned ai = 0; ai < annos.GetCount(); ++ai) {
                auto& dict = annos.GetAnnotAt(ai).GetDictionary();
                QVERIFY2(!dict.HasKey("A"),
                         "no link annotation may keep an /A action after sanitize");
                QVERIFY2(!dict.HasKey("AA"),
                         "no annotation may keep /AA after sanitize");
                // A shell = a Link annotation with Subtype /Link and no action.
                auto* sub = dict.FindKey("Subtype");
                if (sub && sub->IsName() &&
                    std::string(sub->GetName().GetString()) == "Link")
                    ++uriShells;
            }
            QCOMPARE(uriShells, 2);
        } catch (const PoDoFo::PdfError& e) {
            QFAIL(qPrintable(QStringLiteral("failed to reload sanitized pdf: %1")
                             .arg(QString::fromLatin1(e.what()))));
        }
    }
};

QTEST_MAIN(TestLinkBookmarkRoundTrip)
#include "TestLinkBookmarkRoundTrip.moc"
