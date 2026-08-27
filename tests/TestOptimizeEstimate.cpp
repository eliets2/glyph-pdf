// SPDX-License-Identifier: Apache-2.0
// Audit 9.13 P0 regression test: the size-estimate heuristics must not claim
// savings for passes that don't run in the write path. optimizeDocument
// implements downsample, dedup and metadata stripping — but NOT font subsetting
// or unused-object removal. estimateOptimization previously added ~20% per font
// and ~5% for dead objects, systematically overstating savings on every real
// PDF. This test pins that those two contributions are zeroed out.
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFileInfo>

#include <podofo/podofo.h>
#include "engines/PdfEditorEngine.h"

class TestOptimizeEstimate : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_tmpDir;

    QString tmpPath(const QString &name) const {
        return m_tmpDir.filePath(name);
    }

private slots:
    void initTestCase() {
        QVERIFY2(m_tmpDir.isValid(), "Failed to create temp directory");
    }

    void estimateDoesNotClaimUnimplementedPassSavings() {
        QString pdf = tmpPath("estimate.pdf");

        // Build a PDF with a font (so fontCount > 0) and an unreferenced
        // dictionary object (so removeUnusedObjects would have something to
        // claim). The write path does not run either pass.
        {
            PoDoFo::PdfMemDocument doc;
            auto& page = doc.GetPages().CreatePage(
                PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));

            PoDoFo::PdfPainter painter;
            painter.SetCanvas(page);
            auto& font = doc.GetFonts().GetStandard14Font(PoDoFo::PdfStandard14FontType::Helvetica);
            painter.TextState.SetFont(font, 12.0);
            painter.DrawText("estimate test", 100, 700);
            painter.FinishDrawing();

            // An unreferenced dictionary object (dead object).
            auto& orphan = doc.GetObjects().CreateDictionaryObject();
            orphan.GetDictionary().AddKey("Type", PoDoFo::PdfName("Orphan"));
            orphan.GetDictionary().AddKey("Data", PoDoFo::PdfString("dead bytes"));

            doc.Save(pdf.toUtf8().constData());
        }
        QVERIFY2(QFileInfo::exists(pdf), "Source PDF must be written");

        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(pdf));

        // Enable every option so the estimate would claim savings if the
        // heuristics were still overstating.
        OptimizeOptions opts;
        opts.downsampleImages = false;   // keep image savings out of the picture
        opts.deduplicateImages = false;
        opts.subsetFonts = true;         // NOT implemented in write path
        opts.removeUnusedObjects = true; // NOT implemented in write path
        opts.stripMetadata = false;

        OptimizeEstimate est = engine.estimateOptimization(opts);

        // The document has a font, so fontCount must be > 0 — proving the
        // subsetFonts heuristic had something to claim and chose not to.
        QVERIFY2(est.fontCount > 0, "test PDF must contain a font object");

        // With downsample/dedup disabled and subsetFonts/removeUnusedObjects
        // zeroed, the estimate must equal the original size (no phantom savings).
        QCOMPARE(est.estimatedBytes, est.originalBytes);
        QCOMPARE(est.reductionPercent, 0.0);
    }
};

QTEST_MAIN(TestOptimizeEstimate)
#include "TestOptimizeEstimate.moc"