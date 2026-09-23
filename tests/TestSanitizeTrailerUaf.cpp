// SPDX-License-Identifier: Apache-2.0
// E-2 regression pin — soak 2026-09-20 crash class (PoDoFo
// PdfDataContainer::AssertMutable, 0xc0000005, 2x in D:\soak-48h.log inside
// TestSanitization::testSanitizeGeneratesUniqueTrailerID).
//
// Shape: sanitizeDocumentContents() used to remove the trailer /Info key.
// PoDoFo caches a PdfInfo wrapper over the Info object for the lifetime of
// the loaded document (PdfDocument::SetTrailer -> m_Info), so removing the
// key orphans the object and PdfMemDocument::Save()'s CollectGarbage()
// deletes it out from under the cached wrapper. The NEXT Save() on the same
// loaded document stamps /Info/ModDate through the dangling wrapper
// (beforeWrite -> PdfMetadata::SetModifyDate -> PdfInfo::SetModDate ->
// PdfDictionary::AddKey -> AssertMutable reads a freed m_Owner).
// TestSanitization::testSanitizeGeneratesUniqueTrailerID is the minimal
// trigger: it sanitizes the SAME loaded engine document twice.
//
// Fail-before: the second sanitize either AVs in AssertMutable (churned heap,
// the soak signature) or — on a quiet heap — writes into freed memory and the
// reloaded output carries NO /Info at all. Both are failures.
// Pass-after: /Info stays alive and referenced, scrubbed in place (zero user
// metadata), and the document survives repeated sanitizes.
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <vector>
#include <string>
#include <podofo/podofo.h>
#include "engines/PdfEditorEngine.h"

namespace {
// Deterministic small-block churn: overwrite whatever the allocator placed in
// freshly freed blocks so a use-after-free reads garbage instead of stale
// (still-valid) bytes. Harmless when the code is correct.
void churnHeap()
{
    std::vector<std::string> spray;
    spray.reserve(512);
    for (int i = 0; i < 512; ++i)
        spray.emplace_back(48 + (i % 64), static_cast<char>('A' + (i % 26)));
    // Touch every block so the pages are truly dirty, then release.
    for (auto& s : spray)
        s[0] = static_cast<char>('X');
}
} // namespace

class TestSanitizeTrailerUaf : public QObject {
    Q_OBJECT

private slots:
    void pinSanitizeTwiceKeepsTrailerInfoObjectAlive()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString pdf = tmp.filePath("uaf_src.pdf");
        {
            PoDoFo::PdfMemDocument doc;
            doc.GetPages().CreatePage(
                PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
            // User metadata that must not survive, and that makes the stale
            // bytes of a freed Info dict observable.
            doc.GetMetadata().SetTitle(PoDoFo::PdfString("Secret Title"));
            doc.GetMetadata().SetAuthor(PoDoFo::PdfString("Secret Author"));
            doc.Save(pdf.toUtf8().constData());
        }

        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(pdf));

        const QString out1 = tmp.filePath("uaf_out1.pdf");
        const QString out2 = tmp.filePath("uaf_out2.pdf");
        const QString out3 = tmp.filePath("uaf_out3.pdf");

        // Save #1: with the regression this orphans + frees the Info object
        // inside Save's CollectGarbage().
        QVERIFY2(engine.sanitizeDocument(out1), "sanitize #1 must succeed");

        churnHeap();

        // Save #2 on the SAME loaded document: with the regression this stamps
        // /Info/ModDate through the dangling PdfDocument::m_Info wrapper — the
        // soak's AssertMutable AV, or silent corruption on a quiet heap.
        QVERIFY2(engine.sanitizeDocument(out2),
                 "sanitize #2 on the same loaded document must succeed (E-2)");

        churnHeap();

        // Save #3: extra pressure on the same invariant.
        QVERIFY2(engine.sanitizeDocument(out3),
                 "sanitize #3 on the same loaded document must succeed (E-2)");

        // The engine document must still report metadata without touching
        // freed memory.
        PdfMetadata meta;
        QVERIFY2(engine.getMetadata(meta),
                 "getMetadata on the sanitized engine document must succeed");

        // The reloaded output must still carry a live, metadata-free /Info.
        // (With the regression the orphaned object is gone and the trailer has
        // no /Info at all — a deterministic fail signal even when the heap
        // swallows the UAF silently.)
        try {
            PoDoFo::PdfMemDocument outDoc;
            outDoc.Load(out3.toUtf8().constData());
            auto* info = outDoc.GetTrailer().GetDictionary().FindKey("Info");
            QVERIFY2(info != nullptr,
                     "E-2: /Info must stay referenced after sanitize+save "
                     "(missing /Info = the object was orphaned and collected "
                     "while PdfDocument::m_Info still wrapped it)");
            QVERIFY2(info->IsDictionary(), "E-2: /Info must remain a dictionary");
            static const char* kUserKeys[] = { "Title", "Author", "Subject",
                "Keywords", "Creator", "Producer", "Trapped" };
            for (const char* k : kUserKeys) {
                QVERIFY2(!info->GetDictionary().HasKey(k),
                         qPrintable(QStringLiteral(
                                        "E-2: user metadata /%1 must be scrubbed")
                                        .arg(k)));
            }
        } catch (const PoDoFo::PdfError& e) {
            QFAIL(qPrintable(QStringLiteral("failed to reload sanitized pdf: %1")
                                 .arg(QString::fromLatin1(e.what()))));
        }
    }
};

QTEST_GUILESS_MAIN(TestSanitizeTrailerUaf)
#include "TestSanitizeTrailerUaf.moc"
