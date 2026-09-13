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
#include <QDateTime>
#include <QCryptographicHash>
#include <functional>
#include <utility>
#include <vector>
#include <QPdfWriter>
#include <QPainter>
#include <QSettings>
#include <QUndoStack>
#include <podofo/podofo.h>
#include "engines/PdfEditorEngine.h"
#include "engines/SafeSave.h"
#include "engines/qpdf/QpdfBackend.h"
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

    // WP-R09b: the "colleague's" version — deliberately different text, so
    // the bytes can never coincide with the user's fixture (same generator
    // within the same second would otherwise be byte-identical).
    static QString makeColleaguePdf(const QString& dir, const QString& name) {
        const QString path = dir + "/" + name;
        QPdfWriter writer(path);
        writer.setPageSize(QPageSize(QPageSize::A4));
        writer.setResolution(72);
        QPainter p(&writer);
        p.drawText(80, 100, QStringLiteral("R09B COLLEAGUE DOC"));
        p.end();
        return path;
    }

    // One-page PDF carrying an (unsigned) signature field — the writeUpdate
    // signed incremental-update contract control.
    static QString makeSignatureFieldPdf(const QString& dir, const QString& name) {        const QString path = dir + "/" + name;
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

    // WP-R09b: hash of in-memory bytes (the external replacement content).
    static QByteArray shaOfBytes(const QByteArray& bytes) {
        QCryptographicHash hash(QCryptographicHash::Sha256);
        hash.addData(bytes);
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

    // ── WP-R02 (WHOLE-ARCHITECTURE-REVIEW A01) ── a failed user Save must
    // preserve EARLIER resident edits: watermark → failed commit → the
    // watermark survives resident-side (Save-As keeps it), a retry persists
    // watermark + retry in the same artifact. Pre-fix the rollback rebuilt
    // the resident from DISK bytes and destroyed the watermark.
    void watermarkSurvivesFailedSaveAndRetryPersistsBoth();
    // The mutation flavor: a failed rotate after a watermark removes ONLY the
    // attempted rotation — the earlier watermark survives on disk after the
    // later save, and the rotation is truthfully absent until retried.
    void failedRotateAfterWatermarkPreservesPriorWork();
    // ── WP-R09a (A04) ── a successful qpdf repair is a NEW resident: the
    // load identity must advance, and a worker holding the pre-repair id
    // must be refused. Pre-fix the repaired load kept the stale id.
    void repairedLoadMintsNewIdentityAndStaleWorkerRefuses();
    // ── WP-R09b (A05) ── an external modification while the document is open
    // must never be silently overwritten by an in-place save.
    void externalChangeRefusesInPlaceSaveAndKeepsResidentWork();
    // Same-size replacement (the stat-only shortcut would miss it).
    void externalSameSizeReplacementIsStillDetected();
    // Preserved mtime + changed content (mtime alone is no equality proof).
    void externalChangeWithPreservedMtimeIsStillDetected();
    // No false positives: saves without an external change keep working, and
    // a mutation commit is conflict-guarded the same way.
    void noFalsePositiveAndMutationCommitsAreGuarded();
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

// ── WP-R02 (WHOLE-ARCHITECTURE-REVIEW A01) ──────────────────────────────────
// The reviewed desktop trigger: a resident-only watermark (WatermarkDialog →
// addTextWatermark, no commit), then a user Save whose COMMIT fails. The
// failure must leave the disk AND the complete pre-operation resident state
// intact; a later retry persists the watermark too. Pre-fix, the G06 rollback
// rebuilt the resident from DISK bytes and destroyed the watermark — a later
// successful Save silently saved WITHOUT it.
void TestEngineSave::watermarkSurvivesFailedSaveAndRetryPersistsBoth() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = makeTwoPageTextPdf(tmp.path(), QStringLiteral("r02-wm.pdf"));
    QVERIFY(QFile::exists(pdf));
    const QByteArray before = sha256(pdf);

    PdfEditorEngine editor;
    QVERIFY(editor.loadDocumentForEditing(pdf));

    TextWatermarkOptions wm;
    wm.text = QStringLiteral("GATEE-R02-WM");
    wm.pageFrom = -1;
    wm.pageTo = -1;
    QVERIFY2(editor.addTextWatermark(wm), "the resident-only watermark must succeed");

    // The user Save route: an in-place commit that is REFUSED at the shared
    // commit-fault seam.
    gp::SafeSave::setCommitFaultForTesting(gp::SafeSave::CommitFaultForTesting::FailBeforeCommit);
    const bool saved = editor.writeUpdate(pdf);
    gp::SafeSave::setCommitFaultForTesting(gp::SafeSave::CommitFaultForTesting::None);
    QVERIFY2(!saved, "the injected commit failure must be reported");

    QCOMPARE(sha256(pdf), before);              // disk unchanged

    // The heart of WP-R02: the EARLIER resident edit survives the failed
    // commit. A Save-As (a different path — never conflict-guarded) must
    // contain the watermark. Pre-fix the rollback reloaded the disk bytes and
    // the watermark was gone.
    const QString saveAs = tmp.path() + QStringLiteral("/r02-wm-saved-as.pdf");
    QVERIFY2(editor.saveDocument(saveAs), "Save-As after the failed commit must work");
    QVERIFY2(extractedText(saveAs, 0).contains(QStringLiteral("GATEE-R02-WM")),
             "the earlier resident watermark must survive the failed commit");

    // Retry: a later in-place Save persists watermark + document in one
    // artifact.
    QVERIFY2(editor.writeUpdate(pdf), "the retried in-place Save must succeed");
    QVERIFY(sha256(pdf) != before);             // the retry really committed
    const QString text0 = extractedText(pdf, 0);
    QVERIFY2(text0.contains(QStringLiteral("GATEE-R02-WM")),
             "the retried artifact must contain the earlier watermark");
    QVERIFY2(text0.contains(QStringLiteral("EC01 page one marker")),
             "the retried artifact must still contain the page content");
    QCOMPARE(leftoverCandidates(), 0);
}

// The mutation flavor of the same contract: a failed rotate after a watermark
// removes ONLY the attempted rotation; the earlier resident watermark
// survives, is persisted by a later ordinary Save, and the retried rotation
// lands on top of it.
void TestEngineSave::failedRotateAfterWatermarkPreservesPriorWork() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = makeTwoPageTextPdf(tmp.path(), QStringLiteral("r02-rot.pdf"));
    QVERIFY(QFile::exists(pdf));
    const QByteArray before = sha256(pdf);
    QCOMPARE(pdfRotation(pdf), 0);

    PdfEditorEngine editor;
    QVERIFY(editor.loadDocumentForEditing(pdf));

    TextWatermarkOptions wm;
    wm.text = QStringLiteral("GATEE-R02B-WM");
    QVERIFY(editor.addTextWatermark(wm));

    gp::SafeSave::setCommitFaultForTesting(gp::SafeSave::CommitFaultForTesting::FailBeforeCommit);
    const bool rotated = editor.rotatePage(pdf, 0, 90);
    gp::SafeSave::setCommitFaultForTesting(gp::SafeSave::CommitFaultForTesting::None);
    QVERIFY2(!rotated, "the rotate's commit failed and must be reported");

    QCOMPARE(sha256(pdf), before);              // disk unchanged (G06)
    QCOMPARE(pdfRotation(pdf), 0);              // rotation truthfully absent

    // A later ordinary Save persists the PRIOR work (the watermark) and NO
    // rotation. Pre-fix the rollback destroyed the watermark.
    QVERIFY2(editor.saveDocument(pdf), "later ordinary save must succeed");
    QCOMPARE(pdfRotation(pdf), 0);
    QVERIFY2(extractedText(pdf, 0).contains(QStringLiteral("GATEE-R02B-WM")),
             "the watermark must survive the failed rotate");

    // The retried rotation lands on top of the preserved watermark.
    QVERIFY2(editor.rotatePage(pdf, 0, 90), "the retried rotate must succeed");
    QCOMPARE(pdfRotation(pdf), 90);
    QVERIFY2(extractedText(pdf, 0).contains(QStringLiteral("GATEE-R02B-WM")),
             "the retried artifact keeps the earlier watermark");
    QCOMPARE(leftoverCandidates(), 0);
}

// ── WP-R09a (WHOLE-ARCHITECTURE-REVIEW A04) ─────────────────────────────────
// A successful qpdf repair replaces the resident backend, but the OLD code
// kept the previous incarnation's load id. A deferred writer (autosave)
// holding that id could then promote its recovery file over the repaired
// replacement. The repaired load must mint a NEW identity and a worker with
// the stale id must be refused.
void TestEngineSave::repairedLoadMintsNewIdentityAndStaleWorkerRefuses() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    // Fixture: a real PDF that PoDoFo REJECTS and qpdf REPAIRS. Deterministic
    // byte surgery on a QPdfWriter artifact; the candidate shapers run in a
    // fixed order and the FIRST corruption that (a) PoDoFo rejects AND
    // (b) qpdf repairs into a readable two-page PDF wins — both properties
    // are verified BEFORE the identity assertions, so the repair path is
    // guaranteed to be exercised.
    const QString good = makeTwoPageTextPdf(tmp.path(), QStringLiteral("r09a-good.pdf"));
    QByteArray baseBytes;
    {
        QFile f(good);
        QVERIFY(f.open(QIODevice::ReadOnly));
        baseBytes = f.readAll();
    }
    using Shaper = std::function<QByteArray(const QByteArray&)>;
    const std::vector<std::pair<const char*, Shaper>> corruptionCandidates = {
        {"last xref offset misdirected to the previous object",
         [](const QByteArray& base) {
             // The table itself parses cleanly (no rebuild trigger), but one
             // entry points at the WRONG object header: a strict loader that
             // trusts the table refuses on the object-number mismatch, while
             // qpdf detects the bad offset and reconstructs the table.
             QByteArray b = base;
             const int xs = b.indexOf("xref");
             if (xs < 0) return QByteArray();
             const int firstFree = b.indexOf("0000000000 65535 f", xs);
             if (firstFree < 0) return QByteArray();
             // Entries are a fixed 20-byte stride: OOOOOOOOOO GGGGG n \n
             std::vector<int> entries;
             int pos = firstFree;
             while (pos + 20 <= b.size()) {
                 const QByteArray line = b.mid(pos, 20);
                 if (line.at(10) != ' ' || line.at(16) != ' '
                     || (line.at(17) != 'n' && line.at(17) != 'f'))
                     break;
                 entries.push_back(pos);
                 pos += 20;
             }
             if (entries.size() < 3) return QByteArray();
             const int victim = entries.back();
             const int donor = entries[entries.size() - 2];
             const QByteArray donorOffset = b.mid(donor, 10);
             b.replace(victim, 10, donorOffset);   // misdirect, keep format
             return b;
         }},
        {"metadata /Length made a dangling indirect reference",
         [](const QByteArray& base) {
             // /Length 1264 -> /Length 99 0 R: object 99 does not exist, and
             // even a resolved value could not be a number here. A strict
             // loader refuses the stream; qpdf recovers the length by
             // scanning for "endstream".
             QByteArray b = base;
             const int xml = b.indexOf("/Subtype /XML");
             if (xml < 0) return QByteArray();
             const int lenKey = b.indexOf("/Length", xml);
             if (lenKey < 0) return QByteArray();
             int p = lenKey + 7;
             while (p < b.size() && b.at(p) == ' ') ++p;
             int e = p;
             while (e < b.size() && b.at(e) >= '0' && b.at(e) <= '9') ++e;
             if (e == p) return QByteArray();
             b.replace(p, e - p, "99 0 R");
             return b;
         }},
        {"metadata /Length inflated in-file",
         [](const QByteArray& base) {
             // The XMP metadata stream: /Type /Metadata /Subtype /XML /Length N.
             // Inflate N to a value that stays INSIDE the file: a strict
             // parser then reads past the stream's real end and rejects;
             // qpdf recovers the stream by rescanning for "endstream".
             QByteArray b = base;
             const int xml = b.indexOf("/Subtype /XML");
             if (xml < 0) return QByteArray();
             const int lenKey = b.indexOf("/Length", xml);
             if (lenKey < 0) return QByteArray();
             int p = lenKey + 7;
             while (p < b.size() && b.at(p) == ' ') ++p;
             int e = p;
             while (e < b.size() && b.at(e) >= '0' && b.at(e) <= '9') ++e;
             if (e == p || e - p < 2) return QByteArray();
             // same digit width (no offset shifts), inflated but in-file
             QByteArray repl(e - p, '9');
             repl[e - p - 1] = '0';   // e.g. 1264 -> 9990
             b.replace(p, e - p, repl);
             return b;
         }},
        {"truncated after last endobj (no xref/trailer/EOF)",
         [](const QByteArray& base) {
             const int i = base.lastIndexOf("endobj");
             if (i < 0) return QByteArray();
             return base.left(i + 6);
         }},
        {"pages /Count inflated",
         [](const QByteArray& base) {
             QByteArray b = base;
             const int i = b.indexOf("/Count 2");
             if (i < 0) return QByteArray();
             b.replace(i, 8, "/Count 5");
             return b;
         }},
        {"trailer /Size too small",
         [](const QByteArray& base) {
             QByteArray b = base;
             const int i = b.lastIndexOf("/Size ");
             if (i < 0) return QByteArray();
             b.replace(i, 6, "/Size 1");
             return b;
         }},
        {"garbage between objects",
         [](const QByteArray& base) {
             QByteArray b = base;
             const int i = b.indexOf("endobj");
             if (i < 0) return QByteArray();
             b.insert(i + 6, "\n@@@@ GARBAGE @@@@\n");
             return b;
         }},
        {"missing trailing %%EOF",
         [](const QByteArray& base) {
             QByteArray b = base;
             const int i = b.lastIndexOf("%%EOF");
             if (i < 0) return QByteArray();
             return b.left(i);
         }},
        {"trailer /Root redirected to the /Info object",
         [](const QByteArray& base) {
             QByteArray b = base;
             // Locate the trailer's /Root and /Info object numbers.
             const int rootIdx = b.lastIndexOf("/Root");
             if (rootIdx < 0) return QByteArray();
             const int infoIdx = b.lastIndexOf("/Info");
             if (infoIdx < 0) return QByteArray();
             auto parseRef = [](const QByteArray& bytes, int keyEnd) -> int {
                 int p = keyEnd;
                 while (p < bytes.size() && bytes.at(p) == ' ') ++p;
                 int e = p;
                 while (e < bytes.size() && bytes.at(e) >= '0' && bytes.at(e) <= '9') ++e;
                 if (e == p) return -1;
                 if (!bytes.mid(e).startsWith(" 0 R")) return -1;
                 return bytes.mid(p, e - p).toInt();
             };
             const int rootNum = parseRef(b, rootIdx + 5);
             const int infoNum = parseRef(b, infoIdx + 5);
             if (rootNum < 0 || infoNum < 0 || infoNum == rootNum) return QByteArray();
             // Redirect /Root at the /Info object: no valid catalog there, so
             // a strict parser refuses, while qpdf's rebuild re-finds the
             // real /Type /Catalog object.
             const QByteArray from = "/Root " + QByteArray::number(rootNum) + " 0 R";
             const QByteArray to = "/Root " + QByteArray::number(infoNum) + " 0 R";
             const int at = b.lastIndexOf(from);
             if (at < 0) return QByteArray();
             b.replace(at, from.size(), to);
             return b;
         }},
        {"bogus startxref offset",
         [](const QByteArray& base) {
             QByteArray b = base;
             const int i = b.lastIndexOf("startxref");
             if (i < 0) return QByteArray();
             const int nl = b.indexOf('\n', i);
             const int nl2 = b.indexOf('\n', nl + 1);
             if (nl < 0 || nl2 < 0 || nl2 - nl - 1 < 5) return QByteArray();
             b.replace(nl + 1, nl2 - nl - 1, "99999");
             return b;
         }},
        {"destroyed xref section + bogus trailer",
         [](const QByteArray& base) {
             QByteArray b = base;
             const int i = b.lastIndexOf("xref");
             if (i < 0) return QByteArray();
             b.truncate(i + 1);
             b += "trailer<</Size 99>>\nstartxref\n0\n%%EOF\n";
             return b;
         }},
        {"stream /Length beyond EOF",
         [](const QByteArray& base) {
             QByteArray b = base;
             int i = 0;
             while ((i = b.indexOf("/Length", i + 1)) >= 0) {
                 int numStart = i + 7;
                 while (numStart < b.size() && b.at(numStart) == ' ') ++numStart;
                 int numEnd = numStart;
                 while (numEnd < b.size() && b.at(numEnd) >= '0' && b.at(numEnd) <= '9')
                     ++numEnd;
                 if (numEnd == numStart) continue;
                 // only DIRECT lengths (an "N 0 R" reference is skipped)
                 if (b.mid(numEnd).startsWith(" 0 R")) continue;
                 b.replace(numStart, numEnd - numStart, "99999999");
                 return b;
             }
             return QByteArray();
         }},
    };

    const QString corrupt = tmp.path() + QStringLiteral("/r09a-corrupt.pdf");
    const QString repairProbe = tmp.path() + QStringLiteral("/r09a-repair-probe.pdf");
    bool fixtureFound = false;
    for (const auto& cand : corruptionCandidates) {
        const QByteArray shaped = cand.second(baseBytes);
        if (shaped.isEmpty()) {
            qInfo() << "R09a candidate EMPTY:" << cand.first;
            continue;
        }
        {
            QFile f(corrupt);
            QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
            f.write(shaped);
        }
        // Property (a): PoDoFo must really reject it — otherwise the repair
        // path would not be exercised.
        const unsigned pofoPages = pdfPageCount(corrupt);
        if (pofoPages != 0u) {
            qInfo() << "R09a candidate accepted by PoDoFo:" << cand.first
                    << "pages:" << pofoPages;
            continue;
        }
        // Property (b): qpdf must repair it into a readable two-page PDF.
        QFile::remove(repairProbe);
        if (!QpdfBackend::repair(corrupt, repairProbe)) {
            qInfo() << "R09a candidate: qpdf repair FAILED:" << cand.first;
            continue;
        }
        if (pdfPageCount(repairProbe) != 2u) {
            qInfo() << "R09a candidate: repaired parse mismatch:" << cand.first
                    << "pages:" << pdfPageCount(repairProbe);
            continue;
        }
        fixtureFound = true;
        qInfo() << "R09a fixture corruption:" << cand.first;
        break;
    }
    QVERIFY2(fixtureFound,
             "no corruption candidate produced a PoDoFo-rejected, qpdf-repairable "
             "fixture — the qpdf repair path cannot be exercised on this generator");

    PdfEditorEngine editor;
    QVERIFY(editor.loadDocumentForEditing(good));
    const qint64 idBeforeRepair = editor.documentLoadId();

    // The repaired load: PoDoFo fails, qpdf repairs, PoDoFo loads the copy.
    QVERIFY2(editor.loadDocumentForEditing(corrupt),
             "the repaired load must succeed");
    // WP-R09a: a NEW resident incarnation — the identity must advance.
    // (Pre-fix this kept idBeforeRepair.)
    QVERIFY2(editor.documentLoadId() != idBeforeRepair,
             "a repaired load must mint a new load identity");

    // The G04 stale-worker contract must hold across the repair: a worker
    // that captured the PRE-repair identity is refused and leaves the
    // recovery file byte-identical.
    const QString recovery = tmp.path() + QStringLiteral("/r09a-recovery.pdf");
    {
        QFile f(recovery);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("OLD-RECOVERY-SENTINEL");
    }
    const QByteArray recoveryBefore = sha256(recovery);
    QVERIFY2(!editor.saveDocumentIfCurrent(corrupt, idBeforeRepair, recovery),
             "a worker holding the pre-repair identity must be refused");
    QCOMPARE(sha256(recovery), recoveryBefore);

    // Control: the CURRENT identity is accepted.
    QVERIFY2(editor.saveDocumentIfCurrent(corrupt, editor.documentLoadId(), recovery),
             "the current identity must be accepted");
    QVERIFY(sha256(recovery) != recoveryBefore);
    QCOMPARE(pdfPageCount(recovery), 2u);   // the repaired resident is the real 2-page document
}

// ── WP-R09b (WHOLE-ARCHITECTURE-REVIEW A05) ─────────────────────────────────
// Open a PDF, have another process replace it while it is open, then Save
// from the still-open resident: the save must REFUSE (never silently
// overwrite), the external bytes stay intact, and the local work must remain
// reviewable/Save-As-able.
void TestEngineSave::externalChangeRefusesInPlaceSaveAndKeepsResidentWork() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = makeTwoPageTextPdf(tmp.path(), QStringLiteral("r09b.pdf"));
    const QByteArray mine = sha256(pdf);

    // The colleague version: a DIFFERENT valid PDF.
    const QString colleagueFile = makeColleaguePdf(tmp.path(), QStringLiteral("r09b-colleague.pdf"));
    QByteArray colleague;
    {
        QFile f(colleagueFile);
        QVERIFY(f.open(QIODevice::ReadOnly));
        colleague = f.readAll();
    }

    PdfEditorEngine editor;
    QVERIFY(editor.loadDocumentForEditing(pdf));

    // Commit once BEFORE the external change: the resident is re-seated from
    // the validated candidate bytes (buffer-backed), which is the state in
    // which local work remains fully recoverable for Save-As.
    QVERIFY2(editor.saveDocument(pdf), "the initial commit must succeed");

    TextWatermarkOptions wm;
    wm.text = QStringLiteral("GATEE-R09B-WM");
    QVERIFY(editor.addTextWatermark(wm));

    // External replacement while the document is open.
    {
        QFile f(pdf);
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
        f.write(colleague);
    }
    QByteArray externalHash = sha256(pdf);
    QVERIFY(externalHash != mine);

    // The in-place save must refuse — never silently overwrite.
    QVERIFY2(!editor.saveDocument(pdf),
             "an in-place save over an externally replaced file must be refused");
    QCOMPARE(sha256(pdf), externalHash);   // the colleague bytes survive

    // The local work is retained: Save-As (a different path) persists it.
    const QString saveAs = tmp.path() + QStringLiteral("/r09b-save-as.pdf");
    QVERIFY2(editor.saveDocument(saveAs), "Save-As must stay available");
    QVERIFY2(extractedText(saveAs, 0).contains(QStringLiteral("GATEE-R09B-WM")),
             "the local work must survive the conflict for Save-As");

    // After a reload (the honest adopt-external-version path), the new
    // baseline makes in-place saves work again.
    QVERIFY(editor.loadDocumentForEditing(pdf));
    QVERIFY2(editor.saveDocument(pdf), "after a reload the save must succeed");
    QCOMPARE(leftoverCandidates(), 0);
}

// Same-size replacement: a stat shortcut (size+mtime) alone would miss it;
// the full content hash must catch it.
void TestEngineSave::externalSameSizeReplacementIsStillDetected() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = makeTwoPageTextPdf(tmp.path(), QStringLiteral("r09b-same.pdf"));

    PdfEditorEngine editor;
    QVERIFY(editor.loadDocumentForEditing(pdf));

    // Same-size different bytes: flip bits in the middle of the file.
    QByteArray external;
    {
        QFile f(pdf);
        QVERIFY(f.open(QIODevice::ReadOnly));
        external = f.readAll();
    }
    const int mid = external.size() / 2;
    external[mid] = static_cast<char>(external[mid] ^ 0xFF);
    external[mid + 1] = static_cast<char>(external[mid + 1] ^ 0xFF);
    const QByteArray externalHash = shaOfBytes(external);
    {
        QFile f(pdf);
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
        f.write(external);
    }
    QCOMPARE((int)QFileInfo(pdf).size(), (int)external.size());

    QVERIFY2(!editor.saveDocument(pdf),
             "a same-size external replacement must still be refused");
    QCOMPARE(sha256(pdf), externalHash);
}

// Preserved mtime + changed content: mtime alone is no equality proof.
void TestEngineSave::externalChangeWithPreservedMtimeIsStillDetected() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = makeTwoPageTextPdf(tmp.path(), QStringLiteral("r09b-mtime.pdf"));
    const QDateTime originalMtime = QFileInfo(pdf).lastModified();

    PdfEditorEngine editor;
    QVERIFY(editor.loadDocumentForEditing(pdf));

    const QString colleagueFile = makeColleaguePdf(tmp.path(), QStringLiteral("r09b-mtime-colleague.pdf"));
    QByteArray colleague;
    {
        QFile f(colleagueFile);
        QVERIFY(f.open(QIODevice::ReadOnly));
        colleague = f.readAll();
    }
    const QByteArray colleagueHash = shaOfBytes(colleague);
    {
        QFile f(pdf);
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
        f.write(colleague);
        f.setFileTime(originalMtime, QFile::FileModificationTime);  // preserve timestamps
    }
    QCOMPARE(QFileInfo(pdf).lastModified(), originalMtime);

    QVERIFY2(!editor.saveDocument(pdf),
             "changed content with a preserved mtime must still be refused");
    QCOMPARE(sha256(pdf), colleagueHash);
}

// Controls: no false positives on unchanged files (repeated saves keep
// working), and a mutation commit is conflict-guarded exactly like an
// ordinary Save.
void TestEngineSave::noFalsePositiveAndMutationCommitsAreGuarded() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = makeTwoPageTextPdf(tmp.path(), QStringLiteral("r09b-ctl.pdf"));

    PdfEditorEngine editor;
    QVERIFY(editor.loadDocumentForEditing(pdf));

    // Repeated in-place saves without any external change keep working (the
    // baseline refreshes after every committed save).
    QVERIFY2(editor.saveDocument(pdf), "first save must succeed");
    QVERIFY2(editor.saveDocument(pdf), "second save must succeed");

    // Mutation commit under conflict: refuse and never overwrite.
    const QString pdf2 = makeTwoPageTextPdf(tmp.path(), QStringLiteral("r09b-ctl2.pdf"));
    QVERIFY(editor.loadDocumentForEditing(pdf2));
    const QString colleagueFile = makeColleaguePdf(tmp.path(), QStringLiteral("r09b-ctl2-colleague.pdf"));
    QByteArray colleague;
    {
        QFile f(colleagueFile);
        QVERIFY(f.open(QIODevice::ReadOnly));
        colleague = f.readAll();
    }
    const QByteArray colleagueHash = shaOfBytes(colleague);
    {
        QFile f(pdf2);
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
        f.write(colleague);
    }
    QVERIFY2(!editor.rotatePage(pdf2, 0, 90),
             "a mutation commit must refuse to clobber an externally replaced file");
    QCOMPARE(pdfRotation(pdf2), 0);            // nothing was written
    QCOMPARE(sha256(pdf2), colleagueHash);     // colleague bytes intact
}

QTEST_MAIN(TestEngineSave)
#include "TestEngineSave.moc"
