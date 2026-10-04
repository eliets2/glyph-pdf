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
#include <QDir>
#include <QFile>
#include <QIODevice>
#include <QImage>
#include <QImageReader>
#include <QListWidget>
#include <QPushButton>
#include <QSignalSpy>
#include <QStandardPaths>
#include <podofo/podofo.h>

#include "core/StampLibrary.h"
#include "core/PdfEnums.h"
#include "ui/StampLibraryDialog.h"
#include "ui/AnnotationLayer.h"
#include "engines/podofo/PoDoFoBackend.h"
#include "shell/controllers/EditController.h"

#ifdef DrawText
#undef DrawText
#endif

class TestStampImageImport : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_tmpDir;

    static bool writePayload(const QString& path, const QByteArray& payload) {
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
        const qint64 n = f.write(payload);
        f.close();
        return n == payload.size();
    }

    QString validPngPath() const {
        // A small real PNG the import must fully decode.
        const QString path = m_tmpDir.filePath(QStringLiteral("valid_stamp.png"));
        QImage img(40, 20, QImage::Format_ARGB32);
        img.fill(Qt::red);
        img.save(path, "PNG");
        return path;
    }

    // PNG CRC-32 (the zlib polynomial, as the PNG spec chunks require).
    static quint32 pngCrc32(const char* data, int len) {
        quint32 c = 0xFFFFFFFFu;
        for (int i = 0; i < len; ++i) {
            c ^= static_cast<quint8>(data[i]);
            for (int k = 0; k < 8; ++k)
                c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
        }
        return c ^ 0xFFFFFFFFu;
    }

    // Rewrites the IHDR-declared dimensions of a real PNG (and fixes the
    // chunk CRC) without touching the pixel data: the classic dimension bomb
    // — tiny on disk, enormous to decode.
    static bool patchPngDimensions(const QString& path, quint32 w, quint32 h) {
        QFile f(path);
        if (!f.open(QIODevice::ReadWrite)) return false;
        QByteArray buf = f.readAll();
        f.close();
        if (buf.size() < 33 || buf.mid(12, 4) != QByteArrayLiteral("IHDR"))
            return false;
        auto putU32 = [&buf](int at, quint32 v) {
            buf[at]     = static_cast<char>(v >> 24);
            buf[at + 1] = static_cast<char>(v >> 16);
            buf[at + 2] = static_cast<char>(v >> 8);
            buf[at + 3] = static_cast<char>(v);
        };
        putU32(16, w);
        putU32(20, h);
        const QByteArray chunk = buf.mid(12, 17);  // "IHDR" + 13 data bytes
        putU32(29, pngCrc32(chunk.constData(), chunk.size()));
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
        const bool ok = f.write(buf) == buf.size();
        f.close();
        return ok;
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
        QVERIFY(writePayload(path,
            "{\"stamps\":[{\"id\":\"custom:img1\",\"name\":\"Company Logo\","
            "\"template\":\"\",\"image\":\"stamp-images/logo.png\"}]}"));
        const QList<StampTemplate> loaded = StampLibrary::loadCustomFrom(path);
        QCOMPARE(loaded.size(), 1);
        QCOMPARE(loaded.first().id, QStringLiteral("custom:img1"));
        QCOMPARE(loaded.first().name, QStringLiteral("Company Logo"));
        // The stored (relative) path survives verbatim — resolution happens
        // against the catalog location, never against CWD.
        QCOMPARE(loaded.first().imagePath, QStringLiteral("stamp-images/logo.png"));

        // Save → load keeps the image variant (no degradation to text).
        QVERIFY(StampLibrary::saveCustomTo(path, loaded));
        const QList<StampTemplate> reloaded = StampLibrary::loadCustomFrom(path);
        QCOMPARE(reloaded.size(), 1);
        QCOMPARE(reloaded.first().imagePath, QStringLiteral("stamp-images/logo.png"));
        QVERIFY2(reloaded.first().textTemplate.isEmpty(),
                 "an image stamp must not grow a text carrier on save");
    }

    // ── tampered image paths must never be resolved ──────────────────────

    void tamperedImagePathEntriesRefused() {
        // Absolute path escape.
        const QString abs = m_tmpDir.filePath(QStringLiteral("stamps-abs.json"));
        QVERIFY(writePayload(abs,
            "{\"stamps\":[{\"id\":\"custom:e1\",\"name\":\"Evil\","
            "\"template\":\"X\",\"image\":\"C:/evil.png\"}]}"));
        QVERIFY2(StampLibrary::loadCustomFrom(abs).isEmpty(),
                 "an absolute image path in the catalog must refuse the entry");

        // Parent-directory traversal escape.
        const QString trav = m_tmpDir.filePath(QStringLiteral("stamps-trav.json"));
        QVERIFY(writePayload(trav,
            "{\"stamps\":[{\"id\":\"custom:e2\",\"name\":\"Evil\","
            "\"template\":\"X\",\"image\":\"../../evil.png\"}]}"));
        QVERIFY2(StampLibrary::loadCustomFrom(trav).isEmpty(),
                 "a parent-traversing image path must refuse the entry");

        // BOTH carriers at once is an ambiguous/tampered entry — refused.
        const QString both = m_tmpDir.filePath(QStringLiteral("stamps-both.json"));
        QVERIFY(writePayload(both,
            "{\"stamps\":[{\"id\":\"custom:e3\",\"name\":\"Evil\","
            "\"template\":\"X\",\"image\":\"stamp-images/e.png\"}]}"));
        QVERIFY2(StampLibrary::loadCustomFrom(both).isEmpty(),
                 "an entry carrying template AND image must refuse");
    }

    // ── catalog semantics unchanged: built-ins stay text stamps ──────────

    void builtInCatalogSemanticsUnchanged() {
        const auto builtins = StampLibrary::builtIns();
        QCOMPARE(builtins.size(), 5);
        for (const auto& t : builtins) {
            QVERIFY2(!t.name.isEmpty(), "built-in stamps keep their names");
            QVERIFY2(!t.textTemplate.isEmpty(),
                     "built-in stamps remain TEXT stamps (non-empty template)");
            QVERIFY2(t.imagePath.isEmpty(),
                     "no built-in grew an image carrier — catalog semantics unchanged");
        }
        QVERIFY(StampLibrary::findById(QStringLiteral("builtin:approved")).has_value());
    }

    // ── import honesty: unusable images refuse with a TYPED reason ───────
    // r4-ux: the WORDING is pinned, not merely non-emptiness — the audit
    // point was that a refusal message of literally "error" passed. The
    // exact strings are the user-facing honesty contract (cause + next
    // action); a wording regression must fail this pin.

    void importRefusesUnusableImagesWithTypedError() {
        const QString json = m_tmpDir.filePath(QStringLiteral("import-refuse.json"));
        QVERIFY(StampLibrary::saveCustomTo(json, {}));

        // Garbage bytes that merely claim a name.
        const QString garbage = m_tmpDir.filePath(QStringLiteral("garbage.png"));
        QVERIFY(writePayload(garbage, "this is not an image at all"));
        // A truncated real PNG (valid header, cut mid-stream).
        const QString truncated = m_tmpDir.filePath(QStringLiteral("truncated.png"));
        {
            QImage img(30, 30, QImage::Format_ARGB32);
            img.fill(Qt::blue);
            img.save(truncated, "PNG");
            QFile f(truncated);
            QVERIFY(f.resize(40)); // keep only the header bytes
            f.close();
        }

        QString error;
        QVERIFY2(!StampLibrary::addImageStampTo(json, QStringLiteral("Bad"),
                                                garbage, &error).has_value(),
                 "garbage bytes must not import");
        QCOMPARE(error, QStringLiteral("Could not read garbage.png as an image "
                                       "(unsupported or corrupt file)."));
        QVERIFY2(!StampLibrary::addImageStampTo(json, QStringLiteral("Bad"),
                                                truncated, &error).has_value(),
                 "a truncated image must not import");
        QCOMPARE(error, QStringLiteral("Could not read truncated.png as an image "
                                       "(unsupported or corrupt file)."));
        QVERIFY2(!StampLibrary::addImageStampTo(json, QStringLiteral("Bad"),
                                                QStringLiteral("Z:/no/such/file.png"),
                                                &error).has_value(),
                 "a missing file must not import");
        QCOMPARE(error, QStringLiteral("Choose an image file first."));
        QVERIFY2(!StampLibrary::addImageStampTo(json, QStringLiteral("   "),
                                                validPngPath(), &error).has_value(),
                 "an empty name must refuse");
        QCOMPARE(error, QStringLiteral("Enter a name for the stamp."));

        // No silent accept anywhere: the catalog must be untouched.
        QVERIFY2(StampLibrary::loadCustomFrom(json).isEmpty(),
                 "refused imports must not leave catalog entries");
        QVERIFY2(!QDir(StampLibrary::imageStampsDirFor(json)).exists()
                 || QDir(StampLibrary::imageStampsDirFor(json))
                        .entryList(QDir::Files).isEmpty(),
                 "refused imports must not leave copied images behind");
    }

    // ── r4-misc dynamic probe (adversary: "needs dynamic probe") ────────────
    // A dimension bomb through the stamp-import path: a PNG whose IHDR
    // declares 20000x20000 (a ~1.6 GB ARGB32 decode) while the file stays
    // tiny on disk. With Qt's reader allocation limit pulled down (the probe
    // tasking), the import must refuse BEFORE decoding AND the refusal must
    // be honest — the file IS a valid image, so "unsupported or corrupt"
    // would be a lie; the refusal names the declared size. (RED pre-fix:
    // Qt's limit fired inside read() and the null image was lumped into the
    // corrupt/unsupported wording.)
    void dimensionBombRefusedWithHonestSizeDisclosure() {
        const QString bomb = m_tmpDir.filePath(QStringLiteral("dimension-bomb.png"));
        {
            QImage img(40, 20, QImage::Format_ARGB32);
            img.fill(Qt::green);
            QVERIFY(img.save(bomb, "PNG"));
        }
        QVERIFY2(patchPngDimensions(bomb, 20000, 20000),
                 "IHDR patch failed — probe premise broken");
        // Premise: the reader reports the bomb's DECLARED header size without
        // decoding it.
        QCOMPARE(QImageReader(bomb).size(), QSize(20000, 20000));

        const QString json = m_tmpDir.filePath(QStringLiteral("bomb-catalog.json"));
        QVERIFY(StampLibrary::saveCustomTo(json, {}));

        const int defaultLimitMb = QImageReader::allocationLimit();
        struct RestoreLimit {
            int mb;
            explicit RestoreLimit(int m) : mb(m) {}
            ~RestoreLimit() { QImageReader::setAllocationLimit(mb); }
        } restoreLimit(defaultLimitMb);
        QImageReader::setAllocationLimit(1);  // MB — Qt's own guard, tiny

        QString error;
        const auto t = StampLibrary::addImageStampTo(json, QStringLiteral("Bomb"),
                                                     bomb, &error);
        QVERIFY2(!t.has_value(), "a dimension bomb must not import as a stamp");
        QVERIFY2(!error.isEmpty(), "the refusal must carry a reason");
        QVERIFY2(error.contains(QStringLiteral("20000")),
                 qPrintable(QStringLiteral("the refusal must name the declared "
                                          "dimension, not claim corruption: %1")
                                .arg(error)));
        QVERIFY2(error.contains(QStringLiteral("large"), Qt::CaseInsensitive)
                     || error.contains(QStringLiteral("limit"), Qt::CaseInsensitive),
                 qPrintable(QStringLiteral("the refusal must be honest about "
                                          "the size problem: %1").arg(error)));

        // No half-import: the catalog stays empty and no copied image leaks.
        QVERIFY2(StampLibrary::loadCustomFrom(json).isEmpty(),
                 "the refused bomb must not leave a catalog entry");
        QVERIFY2(!QDir(StampLibrary::imageStampsDirFor(json)).exists()
                 || QDir(StampLibrary::imageStampsDirFor(json))
                        .entryList(QDir::Files).isEmpty(),
                 "the refused bomb must not leave a copied image behind");
    }

    // ── the happy path: pick image → name → lands in the catalog ─────────

    void importLandsInCatalogAndPlacesAndDeletes() {
        const QString json = m_tmpDir.filePath(QStringLiteral("import-ok.json"));
        QVERIFY(StampLibrary::saveCustomTo(json, {}));

        QString error;
        const auto t = StampLibrary::addImageStampTo(
            json, QStringLiteral("  Company Logo  "), validPngPath(), &error);
        QVERIFY2(t.has_value(), qPrintable(QStringLiteral("import failed: %1").arg(error)));
        QVERIFY(t->id.startsWith(QStringLiteral("custom:")));
        QCOMPARE(t->name, QStringLiteral("Company Logo"));   // name is trimmed
        QVERIFY2(t->imagePath.startsWith(QStringLiteral("stamp-images/")),
                 qPrintable(QStringLiteral("stored path: %1").arg(t->imagePath)));
        QVERIFY2(t->textTemplate.isEmpty(),
                 "an image stamp carries exactly one placement carrier");

        // The catalog owns a REAL decoded copy under stamp-images/.
        const QString abs = StampLibrary::imageAbsolutePath(json, t->imagePath);
        QVERIFY2(QFile::exists(abs), "the imported copy must exist on disk");
        QVERIFY2(abs.startsWith(StampLibrary::imageStampsDirFor(json)),
                 "the copy must live inside the managed stamp-images folder");
        const QImage decoded = StampLibrary::loadStampImage(json, *t);
        QVERIFY2(!decoded.isNull(), "the stored copy must decode for placement");
        QCOMPARE(decoded.width(), 40);
        QCOMPARE(decoded.height(), 20);

        // The persisted catalog round-trips the image stamp.
        const auto loaded = StampLibrary::loadCustomFrom(json);
        QCOMPARE(loaded.size(), 1);
        QCOMPARE(loaded.first().imagePath, t->imagePath);

        // ── dialog seam: same flow the button drives (no modals) ────────────
        StampLibraryDialog dlg;
        QVERIFY(dlg.addCustomImageStamp(QStringLiteral("Imported Seal"),
                                        validPngPath(), &error));
        QVERIFY2(dlg.selectedTemplateId().startsWith(QStringLiteral("custom:")),
                 "the imported stamp must be selected after import");
        QVERIFY2(!dlg.selectedStampImage().isNull(),
                 "the preview/placement image must decode");
        QSignalSpy spy(&dlg, &StampLibraryDialog::placeRequested);
        auto* placeBtn = dlg.findChild<QPushButton*>(QStringLiteral("stampPlaceButton"));
        QVERIFY(placeBtn);
        QTest::mouseClick(placeBtn, Qt::LeftButton);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().toString(), dlg.selectedTemplateId());

        // Delete removes the entry AND the catalog's own copy. The id is
        // captured BEFORE the delete — deleteSelectedCustom() reloads the
        // list and the selection moves to another (still existing) stamp.
        const QString doomedId = dlg.selectedTemplateId();
        const auto before = StampLibrary::custom();
        QString doomedPath;
        for (const auto& s : before)
            if (s.id == doomedId) doomedPath = s.imagePath;
        QVERIFY2(!doomedPath.isEmpty(), "the selected stamp must be the imported one");
        QVERIFY(dlg.deleteSelectedCustom());
        QVERIFY2(!StampLibrary::findById(doomedId).has_value(),
                 "the deleted stamp must leave the catalog");
        const QString doomedAbs2 =
            StampLibrary::imageAbsolutePath(StampLibrary::defaultCustomPath(), doomedPath);
        QVERIFY2(!QFile::exists(doomedAbs2),
                 "deleting an image stamp must delete the catalog's copy");
    }

    // ── the arm decision: image stamps ride the signature-Upload path ────

    void armSeamRoutesImageStampsThroughSignatureUpload() {
        StampTemplate text;
        text.id = QStringLiteral("builtin:approved");
        text.name = QStringLiteral("Approved");
        text.textTemplate = QStringLiteral("Approved | ${date}");
        QCOMPARE(gp::EditController::stampArmModeForTemplate(text), ToolMode::Stamp);

        StampTemplate img;
        img.id = QStringLiteral("custom:img");
        img.name = QStringLiteral("Logo");
        img.imagePath = QStringLiteral("stamp-images/logo.png");
        QCOMPARE(gp::EditController::stampArmModeForTemplate(img),
                 ToolMode::AddSignatureUpload);
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
