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
#include <podofo/podofo.h>
#include "engines/PdfEditorEngine.h"
#include "engines/SafeSave.h"
#include "engines/pdfium/PdfiumBackend.h"

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

QTEST_MAIN(TestEngineSave)
#include "TestEngineSave.moc"
