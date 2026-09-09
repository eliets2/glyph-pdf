// SPDX-License-Identifier: Apache-2.0
// EC01 (P1) regression suite — the shared engine save boundary.
//
// EC01 (TEAM-ENGINE-CODE-REVIEW-2026-09-07): PoDoFoBackend::saveDocument sent
// the destination straight to PdfMemDocument::Save(). A loaded PdfMemDocument
// keeps its source device open for deferred object parsing
// (PdfMemDocument::m_device), so a save to the LOADED SOURCE PATH truncated the
// very file the parser was still reading: a valid two-page, 7,008-byte PDF
// became ZERO bytes and the operation returned failure ("InvalidNumber").
// writeUpdate() delegates unsigned documents to saveDocument(), so Rotate (and
// every other unsigned path mutator) reached the same defect through
// PdfEditorEngine::rotatePage → PoDoFoBackend::rotatePage → writeUpdate.
//
// The fixtures mirror the audited EC01 probe: a QPdfWriter-generated two-page
// PDF with an embedded subset font and extractable text (the same file class
// that R01/F01 proved triggers deferred parsing). Failure injection is
// deterministic: an occupied destination handle (no FILE_SHARE_DELETE on
// Windows) plus the shared SafeSave commit-fault seam.
//
// Required contract (TEAM-QUALITY-REVIEW-2026-09-08, EC01 row):
//   - same-file Save and Rotate keep page count and content,
//   - failure injection leaves the source byte-identical (SHA-256),
//   - distinct-path save still works and preserves the source,
//   - the signed incremental-update (writeUpdate/SaveUpdate) contract is
//     untouched and keeps working.
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QPdfWriter>
#include <QPainter>
#include <QSettings>
#include <QUndoStack>
#include <podofo/podofo.h>
#include "engines/PdfEditorEngine.h"
#include "engines/SafeSave.h"
#include "engines/DocumentSession.h"
#include "engines/pdfium/PdfiumBackend.h"
#include "commands/CropPageCommand.h"

// wingdi.h defines GetObject as an object-like macro (UNICODE builds); it
// collides with PoDoFo::PdfField::GetObject below.
#ifdef GetObject
#undef GetObject
#endif

class TestEngineSave : public QObject {
    Q_OBJECT
private:
    // QPdfWriter fixture — the same file class as the audited F01/EC01 probes:
    // A4, 72 dpi, real embedded subset font, extractable text, cross-reference
    // streams (deferred object parsing). Two pages, matching the EC01 repro.
    static QString makeTwoPageTextPdf(const QString& dir, const QString& name) {
        const QString path = dir + "/" + name;
        QPdfWriter writer(path);
        writer.setPageSize(QPageSize(QPageSize::A4));
        writer.setResolution(72);
        QPainter p(&writer);
        p.drawText(80, 100, QStringLiteral("EC01 page one marker"));
        writer.newPage();
        p.drawText(80, 100, QStringLiteral("EC01 page two marker"));
        p.end();
        return path;
    }

    // One-page PDF carrying an (unsigned) signature field — the writeUpdate
    // signed incremental-update contract control.
    static QString makeSignatureFieldPdf(const QString& dir, const QString& name) {
        const QString path = dir + "/" + name;
        try {
            PoDoFo::PdfMemDocument doc;
            auto& page = doc.GetPages().CreatePage(
                PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
            page.CreateField<PoDoFo::PdfSignature>(
                "SignatureField1", PoDoFo::Rect(50, 50, 200, 100));
            doc.Save(path.toUtf8().constData());
        } catch (const std::exception& e) {
            qWarning() << "fixture creation failed:" << e.what();
            return QString();
        }
        return path;
    }

    static QByteArray sha256(const QString& path) {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) return {};
        QCryptographicHash hash(QCryptographicHash::Sha256);
        hash.addData(&f);
        return hash.result();
    }

    static qint64 fileSize(const QString& path) {
        return QFileInfo(path).size();
    }

    // PDFium text extraction — the honest "content is still there" check.
    static QString extractedText(const QString& path, int page) {
        PdfiumBackend backend;
        if (!backend.loadDocument(path)) return QString();
        return backend.extractText(page);
    }

    static unsigned pdfPageCount(const QString& path) {
        try {
            PoDoFo::PdfMemDocument doc;
            doc.Load(path.toUtf8().constData());
            return doc.GetPages().GetCount();
        } catch (const PoDoFo::PdfError&) {
            return 0;
        }
    }

    static int pdfRotation(const QString& path) {
        try {
            PoDoFo::PdfMemDocument doc;
            doc.Load(path.toUtf8().constData());
            return static_cast<int>(doc.GetPages().GetPageAt(0).GetRotation());
        } catch (const PoDoFo::PdfError&) {
            return -1;
        }
    }

    static bool pdfHasSignatureField(const QString& path) {
        try {
            PoDoFo::PdfMemDocument doc;
            doc.Load(path.toUtf8().constData());
            for (auto field : doc.GetFieldsIterator()) {
                if (field != nullptr && field->GetType() == PoDoFo::PdfFieldType::Signature)
                    return true;
            }
        } catch (const PoDoFo::PdfError&) {
            return false;
        }
        return false;
    }

    // Effective /CropBox reader — the gate probe's `box()` equivalent. PoDoFo
    // normalizes GetCropBox()/GetMediaBox() to (X, Y, Width, Height), while the
    // raw /CropBox array is [x0, y0, x1, y1] — reading the array directly as
    // (x, y, w, h) would import the G07 geometry bug this suite must stay
    // independent of. Returns a null rect when the document cannot be parsed.
    static QRectF pdfEffectiveBox(const QString& path) {
        try {
            PoDoFo::PdfMemDocument pdf;
            pdf.Load(path.toUtf8().constData());
            const PoDoFo::Rect box = pdf.GetPages().GetPageAt(0).GetCropBox();
            return QRectF(box.X, box.Y, box.Width, box.Height);
        } catch (const PoDoFo::PdfError&) {
            return QRectF();
        }
    }

    // One-page fixture carrying an explicit /CropBox — the gate probe's
    // boxFixture(): real PoDoFo-written document, non-zero box origin.
    static QString makeCropBoxPdf(const QString& dir, const QString& name) {
        const QString path = dir + "/" + name;
        try {
            PoDoFo::PdfMemDocument pdf;
            auto& page = pdf.GetPages().CreatePage(PoDoFo::Rect(0, 0, 612, 792));
            page.GetDictionary().AddKey("CropBox",
                PoDoFo::Rect(10, 20, 500, 700).ToArray());
            pdf.Save(path.toUtf8().constData());
        } catch (const PoDoFo::PdfError& e) {
            qWarning() << "crop-box fixture creation failed:" << e.what();
            return QString();
        }
        return path;
    }

    static bool pdfIsEncrypted(const QString& path) {
        try {
            PoDoFo::PdfMemDocument doc;
            doc.Load(path.toUtf8().constData());
            return doc.IsEncrypted();
        } catch (const PoDoFo::PdfError&) {
            return false;
        }
    }

    // Leftover SafeSave candidates in the dedicated temp dir (must be 0).
    static int leftoverCandidates() {
        return QDir(QDir::tempPath() + QStringLiteral("/glyphpdf-candidates")).entryList(
            QStringList() << QStringLiteral("glyphpdf-*.pdf"), QDir::Files).size();
    }

    // RAII reset of the shared SafeSave commit-fault seam: even if a QVERIFY
    // fails mid-test, later tests never inherit the fault.
    struct SeamReset {
        ~SeamReset() {
            gp::SafeSave::setCommitFaultForTesting(
                gp::SafeSave::CommitFaultForTesting::None);
        }
    };

private slots:
    // Sweep candidate-dir debris from killed processes, then every leftover
    // assertion is delta-based (mirrors TestFormSafety::initTestCase).
    void initTestCase() {
        // The engine save reads this QSettings key; make the default explicit
        // so a stale value from any earlier run cannot change the path under
        // test (mirrors smoke_test's set/remove discipline).
        QSettings settings;
        settings.remove(QStringLiteral("export/linearizeOnSave"));

        const QDir dir(QDir::tempPath() + QStringLiteral("/glyphpdf-candidates"));
        const auto debris = dir.entryList(
            QStringList() << QStringLiteral("glyphpdf-*.pdf"), QDir::Files);
        for (const QString& f : debris) QFile::remove(dir.absoluteFilePath(f));
    }

    void cleanupTestCase() {
        QSettings settings;
        settings.remove(QStringLiteral("export/linearizeOnSave"));
    }

    // ── THE EC01 reproduction ───────────────────────────────────────────────
    // Save to the loaded source path: must succeed and keep both pages and
    // their text. Before the fix: saveDocument returns false and the source is
    // truncated to ZERO bytes.
    void sameFileSaveKeepsPageCountAndContent();
    // Rotate through the engine (unsigned writeUpdate → saveDocument): the
    // same defect reached through PdfEditorEngine::rotatePage.
    void sameFileRotateKeepsPagesAndAppliesRotation();
    // Distinct-path save must keep working and never touch the source
    // (pre-existing behavior — a control that must not regress).
    void distinctPathSavePreservesSource();
    // Deterministic commit failure via an occupied destination handle: the
    // replacement must FAIL and leave the original bytes byte-identical —
    // never a silent direct-write fallback that truncates.
    void commitBlockedByOpenHandlePreservesSource();
    // Deterministic commit failure via the shared SafeSave fault seam, for
    // both the same-file source and a pre-existing distinct destination.
    void injectedCommitFaultPreservesSourceAndDestination();
    // Signed documents keep the separate writeUpdate (SaveUpdate) incremental
    // contract — including same-file updates. Control: passes before and after
    // the EC01 repair.
    void signedWriteUpdateSameFileUnaffected();
    // ── G01 (P1, QUALITY-GATE-2026-09-09) ── the documented encrypted-branch
    // residual: encrypt an ordinary lazily loaded two-page PDF, then same-path
    // Save. Pre-fix the encrypted branch bypassed the candidate transaction:
    // Save returned false and the 15,090-byte source became ZERO bytes.
    void encryptedSameFileSavePreservesSourceAndStaysReadable();
    // ── G06 (P1, QUALITY-GATE-2026-09-09) ── the gate probe's EC05 shape: a
    // crop whose COMMIT fails is dropped from history with dirty=false and an
    // unchanged disk, but the RESIDENT document kept the mutation — so a later
    // ordinary Save persisted the rejected crop (EC05_LATER_SAVE).
    void rejectedCropDoesNotLeakIntoLaterSave();
    // The SAME common rollback rule must hold for the other mutator family
    // (rotate), i.e. it is a shared save-boundary transaction, not a
    // crop-specific workaround.
    void rejectedRotateDoesNotLeakIntoLaterSave();
    // Control: a crop whose commit SUCCEEDS still persists its geometry
    // (passes before and after the G06 repair).
    void successfulCropStillPersists();
};

void TestEngineSave::sameFileSaveKeepsPageCountAndContent() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = makeTwoPageTextPdf(tmp.path(), QStringLiteral("ec01.pdf"));
    QVERIFY(QFile::exists(pdf));

    const qint64 sizeBefore = fileSize(pdf);
    QVERIFY2(sizeBefore > 1000, "fixture must be a real font-bearing PDF");
    QCOMPARE(pdfPageCount(pdf), 2u);

    PdfEditorEngine editor;
    QVERIFY(editor.loadDocumentForEditing(pdf));

    const bool ok = editor.saveDocument(pdf);
    QVERIFY2(ok, qPrintable(QStringLiteral(
        "same-file saveDocument must succeed (EC01): sizeBefore=%1 sizeAfter=%2")
        .arg(sizeBefore).arg(fileSize(pdf))));

    QVERIFY2(fileSize(pdf) > 0, "saved file must not be zero bytes");
    QCOMPARE(pdfPageCount(pdf), 2u);
    const QString text0 = extractedText(pdf, 0);
    QVERIFY2(text0.contains(QStringLiteral("EC01 page one marker")),
             qPrintable(QStringLiteral("page 1 content must survive; got: %1").arg(text0)));
    const QString text1 = extractedText(pdf, 1);
    QVERIFY2(text1.contains(QStringLiteral("EC01 page two marker")),
             qPrintable(QStringLiteral("page 2 content must survive; got: %1").arg(text1)));

    QCOMPARE(leftoverCandidates(), 0);
}

void TestEngineSave::sameFileRotateKeepsPagesAndAppliesRotation() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = makeTwoPageTextPdf(tmp.path(), QStringLiteral("ec01rot.pdf"));
    QVERIFY(QFile::exists(pdf));
    const qint64 sizeBefore = fileSize(pdf);
    QCOMPARE(pdfPageCount(pdf), 2u);

    PdfEditorEngine editor;
    QVERIFY(editor.loadDocumentForEditing(pdf));

    const bool ok = editor.rotatePage(pdf, 0, 90);
    QVERIFY2(ok, qPrintable(QStringLiteral(
        "same-file rotatePage must succeed (EC01): sizeBefore=%1 sizeAfter=%2")
        .arg(sizeBefore).arg(fileSize(pdf))));

    QVERIFY2(fileSize(pdf) > 0, "saved file must not be zero bytes");
    QCOMPARE(pdfPageCount(pdf), 2u);
    QCOMPARE(pdfRotation(pdf), 90);
    QVERIFY2(extractedText(pdf, 0).contains(QStringLiteral("EC01 page one marker")),
             "page 1 content must survive a same-file rotate");
    QVERIFY2(extractedText(pdf, 1).contains(QStringLiteral("EC01 page two marker")),
             "page 2 content must survive a same-file rotate");

    QCOMPARE(leftoverCandidates(), 0);
}

void TestEngineSave::distinctPathSavePreservesSource() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString src = makeTwoPageTextPdf(tmp.path(), QStringLiteral("src.pdf"));
    const QString dest = makeTwoPageTextPdf(tmp.path(), QStringLiteral("old_dest.pdf"));
    QVERIFY(QFile::exists(src) && QFile::exists(dest));

    const QByteArray srcSha = sha256(src);
    PdfEditorEngine editor;
    QVERIFY(editor.loadDocumentForEditing(src));

    QVERIFY(editor.saveDocument(dest));

    QCOMPARE(sha256(src), srcSha);          // source byte-identical
    QCOMPARE(pdfPageCount(dest), 2u);       // destination replaced by the save
    QVERIFY2(extractedText(dest, 0).contains(QStringLiteral("EC01 page one marker")),
             "destination must carry the source content");
    QCOMPARE(leftoverCandidates(), 0);
}

void TestEngineSave::commitBlockedByOpenHandlePreservesSource() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = makeTwoPageTextPdf(tmp.path(), QStringLiteral("held.pdf"));
    QVERIFY(QFile::exists(pdf));
    const QByteArray shaBefore = sha256(pdf);

    // Hold the destination open WITHOUT modifying it (ReadOnly leaves the
    // bytes alone; Windows share mode excludes FILE_SHARE_DELETE, so the
    // rename-over-destination at commit must fail with a sharing violation).
    QFile held(pdf);
    QVERIFY(held.open(QIODevice::ReadOnly));

    PdfEditorEngine editor;
    QVERIFY(editor.loadDocumentForEditing(pdf));

    const bool ok = editor.saveDocument(pdf);
    QVERIFY2(!ok, "a replacement blocked by an open handle must FAIL, "
                  "not silently fall back to a direct write that truncates");

    QCOMPARE(sha256(pdf), shaBefore);       // original byte-identical
    QCOMPARE(pdfPageCount(pdf), 2u);        // and still a readable two-page PDF
    held.close();
}

void TestEngineSave::injectedCommitFaultPreservesSourceAndDestination() {
    SeamReset seamReset;
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    // Same-file: injected commit failure must leave the loaded source
    // byte-identical and still a readable PDF.
    {
        const QString pdf = makeTwoPageTextPdf(tmp.path(), QStringLiteral("fault_same.pdf"));
        QVERIFY(QFile::exists(pdf));
        const QByteArray shaBefore = sha256(pdf);

        PdfEditorEngine editor;
        QVERIFY(editor.loadDocumentForEditing(pdf));

        gp::SafeSave::setCommitFaultForTesting(
            gp::SafeSave::CommitFaultForTesting::FailBeforeCommit);
        const bool ok = editor.saveDocument(pdf);
        gp::SafeSave::setCommitFaultForTesting(gp::SafeSave::CommitFaultForTesting::None);

        QVERIFY2(!ok, "injected commit failure must be reported as failure");
        QCOMPARE(sha256(pdf), shaBefore);
        QCOMPARE(pdfPageCount(pdf), 2u);
    }

    // Distinct paths: injected commit failure must leave the source
    // byte-identical AND the pre-existing destination byte-identical.
    {
        const QString src = makeTwoPageTextPdf(tmp.path(), QStringLiteral("fault_src.pdf"));
        const QString dest = makeTwoPageTextPdf(tmp.path(), QStringLiteral("fault_dest.pdf"));
        QVERIFY(QFile::exists(src) && QFile::exists(dest));
        const QByteArray srcSha = sha256(src);
        const QByteArray destSha = sha256(dest);

        PdfEditorEngine editor;
        QVERIFY(editor.loadDocumentForEditing(src));

        gp::SafeSave::setCommitFaultForTesting(
            gp::SafeSave::CommitFaultForTesting::FailBeforeCommit);
        const bool ok = editor.saveDocument(dest);
        gp::SafeSave::setCommitFaultForTesting(gp::SafeSave::CommitFaultForTesting::None);

        QVERIFY2(!ok, "injected commit failure must be reported as failure");
        QCOMPARE(sha256(src), srcSha);
        QCOMPARE(sha256(dest), destSha);    // existing destination preserved
        QCOMPARE(pdfPageCount(dest), 2u);
    }

    QCOMPARE(leftoverCandidates(), 0);
}

void TestEngineSave::signedWriteUpdateSameFileUnaffected() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = makeSignatureFieldPdf(tmp.path(), QStringLiteral("signed.pdf"));
    QVERIFY2(QFile::exists(pdf), "signature-field fixture must be created");
    QCOMPARE(pdfPageCount(pdf), 1u);
    QVERIFY(pdfHasSignatureField(pdf));

    PdfEditorEngine editor;
    QVERIFY(editor.loadDocumentForEditing(pdf));
    QVERIFY(editor.hasPdfSignatures());

    // Same-file incremental update: the signed contract appends a revision.
    QVERIFY2(editor.writeUpdate(pdf), "same-file signed writeUpdate must keep working");

    QCOMPARE(pdfPageCount(pdf), 1u);
    QVERIFY(pdfHasSignatureField(pdf));
}

// ─────────────────────────── G01 ────────────────────────────────────────────
// THE G01 reproduction (gate probe EC01_SAVE_MODE 2): encrypt a lazily loaded
// ordinary two-page PDF with a synthetic owner password, then save to the same
// path. Pre-fix: the IsEncrypted branch bypassed the candidate transaction, the
// direct Save truncated the still-open source device's file, returned false and
// left a ZERO-byte source. Post-fix: the transaction covers the newly-encrypted
// document — the encrypted candidate is reopened with the captured credentials
// before the checked replacement, and the source is preserved on every failure.
void TestEngineSave::encryptedSameFileSavePreservesSourceAndStaysReadable() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = makeTwoPageTextPdf(tmp.path(), QStringLiteral("g01.pdf"));
    QVERIFY(QFile::exists(pdf));
    const qint64 sizeBefore = fileSize(pdf);
    QVERIFY2(sizeBefore > 1000, "fixture must be a real font-bearing PDF");
    QCOMPARE(pdfPageCount(pdf), 2u);
    QVERIFY2(!pdfIsEncrypted(pdf), "fixture starts unencrypted");

    PdfEditorEngine editor;
    QVERIFY(editor.loadDocumentForEditing(pdf));
    QVERIFY2(editor.encryptDocument(QString(), QStringLiteral("synthetic-owner-password"),
                                    DocumentPermissions{}),
             "encryption setup must succeed");

    // Failure phase: a blocked replacement must FAIL and leave the unencrypted
    // source byte-identical (G01: "preserve source AND prior destination on
    // every failure") — the direct pre-fix write instead truncated it.
    {
        const QByteArray shaBefore = sha256(pdf);
        gp::SafeSave::setCommitFaultForTesting(
            gp::SafeSave::CommitFaultForTesting::FailBeforeCommit);
        const bool failed = editor.saveDocument(pdf);
        gp::SafeSave::setCommitFaultForTesting(gp::SafeSave::CommitFaultForTesting::None);
        QVERIFY2(!failed, "injected commit failure must be reported");
        QCOMPARE(sha256(pdf), shaBefore);
        QCOMPARE(pdfPageCount(pdf), 2u);
        QVERIFY2(!pdfIsEncrypted(pdf), "a failed save must not encrypt the source");
    }

    const bool ok = editor.saveDocument(pdf);
    QVERIFY2(ok, qPrintable(QStringLiteral(
        "same-file save of a newly-encrypted document must succeed (G01): "
        "sizeBefore=%1 sizeAfter=%2").arg(sizeBefore).arg(fileSize(pdf))));

    QVERIFY2(fileSize(pdf) > 0, "saved file must not be zero bytes");
    QVERIFY2(pdfIsEncrypted(pdf), "the committed document must actually be encrypted");
    QCOMPARE(pdfPageCount(pdf), 2u);
    // The encrypted candidate was validated before replacement — the document
    // must still be readable (an empty-user-password document opens directly).
    QCOMPARE(leftoverCandidates(), 0);
}

// ─────────────────────────── G06 ────────────────────────────────────────────
// THE G06 reproduction (gate probe EC05_FAILED_PUSH_COUNT / EC05_LATER_SAVE):
// a real crop with an injected COMMIT failure must leave the resident document
// un-mutated, so a later ordinary Save persists the ORIGINAL geometry.
void TestEngineSave::rejectedCropDoesNotLeakIntoLaterSave() {
    SeamReset seamReset;
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = makeCropBoxPdf(tmp.path(), QStringLiteral("g06-crop.pdf"));
    QVERIFY2(QFile::exists(pdf), "crop-box fixture must be created");
    const QRectF originalBox = pdfEffectiveBox(pdf);
    QCOMPARE(originalBox, QRectF(10, 20, 500, 700));
    const QByteArray before = sha256(pdf);

    PdfEditorEngine editor;
    QVERIFY(editor.loadDocumentForEditing(pdf));
    DocumentSession doc;
    doc.beginDocument(pdf);

    int failures = 0;
    QObject::connect(&doc, &DocumentSession::mutationFailed, &doc,
                     [&failures](const QString&) { ++failures; });

    QUndoStack history;
    gp::SafeSave::setCommitFaultForTesting(gp::SafeSave::CommitFaultForTesting::FailBeforeCommit);
    history.push(new CropPageCommand(&editor, &doc, 0, QRectF(80, 90, 200, 300)));
    gp::SafeSave::setCommitFaultForTesting(gp::SafeSave::CommitFaultForTesting::None);

    QCOMPARE(history.count(), 0);              // rejected command is dropped
    QVERIFY2(!doc.isDirty(), "a rejected mutation must not mark the session dirty");
    QCOMPARE(failures, 1);                     // the failure is reported
    QCOMPARE(sha256(pdf), before);             // disk unchanged

    // The heart of G06 (gate marker EC05_LATER_SAVE): the supposedly REJECTED
    // crop must not survive anywhere — a later ordinary Save persists the
    // ORIGINAL geometry, byte-for-byte unchanged disk. Pre-fix the resident
    // document kept the mutation and this save persisted it.
    QVERIFY2(editor.saveDocument(pdf), "later ordinary save must succeed");
    QCOMPARE(pdfEffectiveBox(pdf), originalBox);
    QCOMPARE(leftoverCandidates(), 0);
}

// The common rule must not be crop-specific: a rotate whose commit fails is
// likewise not resident, so a later ordinary Save persists rotation 0.
void TestEngineSave::rejectedRotateDoesNotLeakIntoLaterSave() {
    SeamReset seamReset;
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = makeTwoPageTextPdf(tmp.path(), QStringLiteral("g06-rot.pdf"));
    QVERIFY(QFile::exists(pdf));
    const QByteArray before = sha256(pdf);
    QCOMPARE(pdfRotation(pdf), 0);

    PdfEditorEngine editor;
    QVERIFY(editor.loadDocumentForEditing(pdf));

    gp::SafeSave::setCommitFaultForTesting(gp::SafeSave::CommitFaultForTesting::FailBeforeCommit);
    const bool rotated = editor.rotatePage(pdf, 0, 90);
    gp::SafeSave::setCommitFaultForTesting(gp::SafeSave::CommitFaultForTesting::None);
    QVERIFY2(!rotated, "the rotate's commit failed and must be reported");

    QCOMPARE(sha256(pdf), before);             // disk unchanged
    QCOMPARE(pdfRotation(pdf), 0);             // and rotation 0 on disk

    QVERIFY2(editor.saveDocument(pdf), "later ordinary save must succeed");
    QCOMPARE(pdfRotation(pdf), 0);             // rejected rotation did not leak
    QCOMPARE(leftoverCandidates(), 0);
}

// Control (passes before and after the G06 repair): a successful crop persists.
void TestEngineSave::successfulCropStillPersists() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = makeCropBoxPdf(tmp.path(), QStringLiteral("g06-ok.pdf"));
    QVERIFY(QFile::exists(pdf));

    PdfEditorEngine editor;
    QVERIFY(editor.loadDocumentForEditing(pdf));
    DocumentSession doc;
    doc.beginDocument(pdf);

    QUndoStack history;
    history.push(new CropPageCommand(&editor, &doc, 0, QRectF(80, 90, 200, 300)));
    QCOMPARE(history.count(), 1);
    QVERIFY(doc.isDirty());

    QCOMPARE(pdfEffectiveBox(pdf), QRectF(80, 90, 200, 300));
}

QTEST_MAIN(TestEngineSave)
#include "TestEngineSave.moc"
