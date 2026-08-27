// SPDX-License-Identifier: Apache-2.0
// Audit 9.13 P0 regression test (DEFECT 3, commit 6c41e12): the duplicate-image
// dedup hash must fold in the image-dict params, not just the stream bytes.
// Two images with IDENTICAL stream bytes but DIFFERENT /SMask are NOT equivalent
// and must NOT be merged — merging would silently drop the soft mask (transparency
// corruption). This test pins that behavior.
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>

#include <podofo/podofo.h>
#include "engines/PdfEditorEngine.h"

class TestDedupSMask : public QObject {
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

    void identicalBytesDifferentSMaskAreNotMerged() {
        QString pdf = tmpPath("dedup_smask.pdf");

        // Two images with IDENTICAL stream bytes (same 8x8 RGB24 pattern).
        const int W = 8, H = 8;
        QByteArray pixels;
        for (int i = 0; i < W * H; ++i) {
            pixels.append('\x12'); pixels.append('\x34'); pixels.append('\x56');
        }

        {
            PoDoFo::PdfMemDocument doc;
            auto& page = doc.GetPages().CreatePage(
                PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));

            // Image 1: plain RGB, NO /SMask.
            auto img1 = doc.CreateImage();
            img1->SetData(PoDoFo::bufferview(pixels.constData(), pixels.size()),
                          W, H, PoDoFo::PdfPixelFormat::RGB24);

            // Image 2: identical bytes, but WITH an /SMask (a small grayscale mask).
            auto img2 = doc.CreateImage();
            img2->SetData(PoDoFo::bufferview(pixels.constData(), pixels.size()),
                          W, H, PoDoFo::PdfPixelFormat::RGB24);
            auto mask = doc.CreateImage();
            QByteArray maskPixels;
            for (int i = 0; i < W * H; ++i) maskPixels.append('\x80');
            mask->SetData(PoDoFo::bufferview(maskPixels.constData(), maskPixels.size()),
                          W, H, PoDoFo::PdfPixelFormat::Grayscale);
            img2->GetDictionary().AddKey("SMask", mask->GetObject().GetIndirectReference());

            // Draw both on the page so they are referenced.
            PoDoFo::PdfPainter painter;
            painter.SetCanvas(page);
            painter.DrawImage(*img1, 100, 700, 40.0, 40.0);
            painter.DrawImage(*img2, 200, 700, 40.0, 40.0);
            painter.FinishDrawing();

            doc.Save(pdf.toUtf8().constData());
        }
        QVERIFY2(QFileInfo::exists(pdf), "Source PDF must be written");

        // Run the optimize pass with deduplicateImages enabled.
        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(pdf));
        OptimizeOptions opts;
        opts.downsampleImages = false;   // keep the 8x8 images untouched
        opts.deduplicateImages = true;
        opts.subsetFonts = false;
        opts.removeUnusedObjects = true;
        QString out = tmpPath("dedup_smask_out.pdf");
        QVERIFY2(engine.optimizeDocument(out, opts), "optimizeDocument must succeed");

        // Reload the optimized file and count surviving image XObjects.
        PoDoFo::PdfMemDocument doc;
        doc.Load(out.toUtf8().constData());
        int imageCount = 0;
        int withSMask = 0;
        int withoutSMask = 0;
        for (auto it = doc.GetObjects().begin(); it != doc.GetObjects().end(); ++it) {
            PoDoFo::PdfObject* o = *it;
            if (!o->IsDictionary()) continue;
            auto& d = o->GetDictionary();
            auto* st = d.FindKey("Subtype");
            if (!st || !st->IsName() || std::string(st->GetName().GetString()) != "Image") continue;
            ++imageCount;
            if (d.FindKey("SMask")) ++withSMask;
            else ++withoutSMask;
        }

        // BOTH images must survive: one with an /SMask, one without. They must
        // NOT have been merged despite identical stream bytes.
        QVERIFY2(imageCount >= 2,
                 qPrintable(QString("Expected both images to survive, found %1").arg(imageCount)));
        QVERIFY2(withSMask >= 1, "the /SMask image must survive with its mask intact");
        QVERIFY2(withoutSMask >= 1, "the plain image must survive without an /SMask");
    }
};

QTEST_MAIN(TestDedupSMask)
#include "TestDedupSMask.moc"