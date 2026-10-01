// SPDX-License-Identifier: Apache-2.0
// PARITY-SCORECARD-2026-09-30 §4 row 18: stamp tool — image-as-stamp import.
//
// Pins:
//   * the library dialog offers an image import entry point
//     (RED at base: the button does not exist yet),
//   * a catalog entry carrying the image-variant slot survives a real
//     loadCustomFrom round trip (RED at base: such entries are silently
//     DROPPED — the loader required a non-empty text template),
//   * tampered image paths (absolute / parent-traversing) are refused,
//     never resolved (RED at base, for the base-specific reason that every
//     image entry was dropped; stays green after for the right reason),
//   * catalog semantics unchanged: the five built-ins stay text stamps,
//   * REUSE PROOF (green at base, must stay green): an image AnnotationItem
//     armed through the existing signature-Upload placement path persists
//     through the §9.7 P0 /Stamp + image-appearance writer and reloads with
//     its raster — image-as-stamp placement rides that writer unchanged.

#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QIODevice>
#include <QImage>
#include <QListWidget>
#include <QPushButton>
#include <QStandardPaths>
#include <podofo/podofo.h>

#include "core/StampLibrary.h"
#include "core/PdfEnums.h"
#include "ui/StampLibraryDialog.h"
#include "ui/AnnotationLayer.h"
#include "engines/podofo/PoDoFoBackend.h"

#ifdef DrawText
#undef DrawText
#endif

class TestStampImageImport : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_tmpDir;

    static bool writeJson(const QString& path, const QByteArray& payload) {
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
        const qint64 n = f.write(payload);
        f.close();
        return n == payload.size();
    }

private slots:
    void initTestCase() {
        QVERIFY2(m_tmpDir.isValid(), "Temp directory creation failed");
        QStandardPaths::setTestModeEnabled(true);
    }

    // ── RED at base: no import entry point in the library dialog ────────

    void dialogOffersImageImport() {
        StampLibraryDialog dlg;
        auto* importBtn = dlg.findChild<QPushButton*>(QStringLiteral("stampImportButton"));
        QVERIFY2(importBtn, "the stamp library dialog must offer an image import button");
    }

    // ── RED at base: image-variant catalog entries are silently dropped ──

    void imageCatalogEntrySurvivesReload() {
        const QString path = m_tmpDir.filePath(QStringLiteral("stamps-image.json"));
        QVERIFY(writeJson(path,
            "{\"stamps\":[{\"id\":\"custom:img1\",\"name\":\"Company Logo\","
            "\"template\":\"\",\"image\":\"stamp-images/logo.png\"}]}"));
        const QList<StampTemplate> loaded = StampLibrary::loadCustomFrom(path);
        QCOMPARE(loaded.size(), 1);
        if (loaded.size() == 1) {
            QCOMPARE(loaded.first().id, QStringLiteral("custom:img1"));
            QCOMPARE(loaded.first().name, QStringLiteral("Company Logo"));
        }
    }

    // ── tampered image paths must never be resolved ──────────────────────

    void tamperedImagePathEntriesRefused() {
        // Absolute path escape.
        const QString abs = m_tmpDir.filePath(QStringLiteral("stamps-abs.json"));
        QVERIFY(writeJson(abs,
            "{\"stamps\":[{\"id\":\"custom:e1\",\"name\":\"Evil\","
            "\"template\":\"X\",\"image\":\"C:/evil.png\"}]}"));
        QVERIFY2(StampLibrary::loadCustomFrom(abs).isEmpty(),
                 "an absolute image path in the catalog must refuse the entry");

        // Parent-directory traversal escape.
        const QString trav = m_tmpDir.filePath(QStringLiteral("stamps-trav.json"));
        QVERIFY(writeJson(trav,
            "{\"stamps\":[{\"id\":\"custom:e2\",\"name\":\"Evil\","
            "\"template\":\"X\",\"image\":\"../../evil.png\"}]}"));
        QVERIFY2(StampLibrary::loadCustomFrom(trav).isEmpty(),
                 "a parent-traversing image path must refuse the entry");
    }

    // ── catalog semantics unchanged: built-ins stay text stamps ──────────

    void builtInCatalogSemanticsUnchanged() {
        const auto builtins = StampLibrary::builtIns();
        QCOMPARE(builtins.size(), 5);
        for (const auto& t : builtins) {
            QVERIFY2(!t.name.isEmpty(), "built-in stamps keep their names");
            QVERIFY2(!t.textTemplate.isEmpty(),
                     "built-in stamps remain TEXT stamps (non-empty template)");
        }
        QVERIFY(StampLibrary::findById(QStringLiteral("builtin:approved")).has_value());
    }

    // ── REUSE PROOF: image placement rides the existing /Stamp writer ────

    void placedImageStampRidesTheExistingStampWriter() {
        // The signature-Upload placement mechanics: arm the mode, install the
        // pending raster, plain click → committed item carries the image in a
        // visible default box (never a zero-sized rect).
        AnnotationLayer layer;
        layer.resize(400, 400);
        QImage img(40, 20, QImage::Format_ARGB32);
        img.fill(Qt::red);
        layer.setPendingSignatureImage(img);
        layer.setMode(ToolMode::AddSignatureUpload);
        QTest::mouseClick(&layer, Qt::LeftButton, Qt::NoModifier, QPoint(100, 100));
        QCOMPARE(layer.annotations().size(), 1);
        AnnotationItem placed = layer.annotations().first();
        QCOMPARE(placed.mode, ToolMode::AddSignatureUpload);
        QVERIFY2(!placed.image.isNull(), "the committed item must carry the image");
        QVERIFY2(placed.rect.width() > 2.0 && placed.rect.height() > 2.0,
                 "a plain click must place a visible box, not a zero rect");
        placed.id = QStringLiteral("img-stamp-1");
        placed.pageIndex = 0;
        placed.reviewState = ReviewState::None;

        // REAL engine roundtrip through the §9.7 P0 writer: /Stamp subtype +
        // image appearance stream, restored (mode AND raster) on extract.
        const QString base = m_tmpDir.filePath(QStringLiteral("img_stamp_base.pdf"));
        try {
            PoDoFo::PdfMemDocument doc;
            doc.GetPages().CreatePage(
                PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
            doc.Save(base.toUtf8().constData());
        } catch (const std::exception& e) {
            QFAIL(qPrintable(QStringLiteral("base pdf failed: %1").arg(e.what())));
        }
        PoDoFoBackend backend;
        const QString out = m_tmpDir.filePath(QStringLiteral("img_stamp_out.pdf"));
        QVERIFY(backend.embedAnnotations(base, out, { placed }));
        const QList<AnnotationItem> loaded = backend.extractAnnotations(out);
        bool found = false;
        for (const auto& a : loaded) {
            if (a.id == QStringLiteral("img-stamp-1")) {
                found = true;
                QCOMPARE(a.mode, ToolMode::AddSignatureUpload);
                QVERIFY2(!a.image.isNull(),
                         "the saved /Stamp must reload with its image appearance");
            }
        }
        QVERIFY2(found, "the placed image stamp must reload from the saved PDF");
    }
};

QTEST_MAIN(TestStampImageImport)
#include "TestStampImageImport.moc"
