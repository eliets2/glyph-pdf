// SPDX-License-Identifier: Apache-2.0
// Repair-order step 3 (2026-09-08): history truthfulness.
//
// Regression tests for TEAM-ENGINE-CODE-REVIEW-2026-09-07 findings EC03 and
// EC05 and the residual V02 (PARITY-BRANCH-REVIEW) form-undo contract:
//
//   EC03 — undoing an image delete/replace restores via "insert the backup
//          page, then remove the displaced edited page". A FAILED insertion
//          used to be ignored and the command went on to deletePage(m_page+1)
//          — destroying the ORIGINAL FOLLOWING page. The restored commands
//          stop on insertion failure, refuse to start a destructive edit
//          without a restorable backup, and report the failure through
//          DocumentSession::mutationFailed.
//   EC05 — CropPageCommand::undo used to be a no-op ("reload only"): the
//          history index moved back while the on-disk CropBox stayed cropped.
//          The restored command captures the effective original geometry
//          before the first mutation, refuses to run when the snapshot or the
//          initial mutation fails (obsolete ⇒ never enters history), and undo
//          restores through the same safe mutation boundary (cropPage).
//   V02  — EditFormFieldCommand::undo logged a failed restore and returned
//          silently. The failure is now reported (mutationFailed) and the
//          state stays truthful: no markReload, disk keeps the edited values.
//
// Evidence rules: real saved/reopened artifacts (page count, CropBox bytes,
// field /V, page text via PDFium) carry the contract; fault-injected engines
// (the reviewed "isolated fault engine") carry the failure paths. Mock call
// counts alone never assert PDF correctness.
//
// COMPILE-COMPATIBILITY (revert verification: `git stash push -- src/`): this
// file must compile against the PRE-fix sources. New production symbols
// (DocumentSession::mutationFailed) are therefore observed ONLY through
// runtime-resolved old-style signal strings (QSignalSpy), never through
// new-style connects; mock helpers added for this suite live under tests/ and
// declare pre-fix-safe signatures without the `override` keyword where the
// interface member is itself new.
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QUndoStack>
#include <QSignalSpy>
#include <QPainter>
#include <QPdfWriter>
#include <sstream>
#include <podofo/podofo.h>
#include "engines/FormManager.h"
#include "engines/DocumentSession.h"
#include "engines/PdfEditorEngine.h"
#include "engines/pdfium/PdfiumBackend.h"
#include "commands/CropPageCommand.h"
#include "commands/DeleteImageCommand.h"
#include "commands/ReplaceImageCommand.h"
#include "commands/EditFormFieldCommand.h"
#include "mocks/MockPdfEditorEngine.h"

// wingdi.h defines GetObject as an object-like macro (UNICODE builds); it
// collides with PoDoFo::PdfField::GetObject below.
#ifdef GetObject
#undef GetObject
#endif

namespace {

// Two-page A4 fixture with per-page marker text (the EC01 fixture class: real
// subset font, deferred object parsing).
QString makeTwoPageTextPdf(const QString &dir, const QString &name)
{
    const QString path = dir + QLatin1Char('/') + name;
    QPdfWriter w(path);
    w.setPageSize(QPageSize(QPageSize::A4));
    w.setResolution(72);
    QPainter p(&w);
    p.drawText(80, 100, QStringLiteral("HIST page one marker"));
    w.newPage();
    p.drawText(80, 100, QStringLiteral("HIST page two marker"));
    p.end();
    return path;
}

// Two-page fixture whose SECOND page carries a real embedded image XObject
// (page-resources /XObject + a real "Do" operator in the content stream — the
// structure listImages/deleteImage act on; a QPdfWriter image would be nested
// in a form XObject and invisible to the backend).
QString makeImagePagePdf(const QString &dir, const QString &name)
{
    const QString path = dir + QLatin1Char('/') + name;
    try {
        PoDoFo::PdfMemDocument doc;

        // Standard-14 Helvetica so PDFium can extract the marker text.
        auto &font = doc.GetObjects().CreateDictionaryObject();
        font.GetDictionary().AddKey("Type", PoDoFo::PdfName("Font"));
        font.GetDictionary().AddKey("Subtype", PoDoFo::PdfName("Type1"));
        font.GetDictionary().AddKey("BaseFont", PoDoFo::PdfName("Helvetica"));

        auto makePage = [&](const char *marker) -> PoDoFo::PdfPage & {
            auto &page = doc.GetPages().CreatePage(
                PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
            PoDoFo::PdfDictionary fonts;
            fonts.AddKey(PoDoFo::PdfName("F1"), PoDoFo::PdfObject(font.GetIndirectReference()));
            PoDoFo::PdfDictionary res;
            res.AddKey("Font", PoDoFo::PdfObject(fonts));
            page.GetDictionary().AddKey("Resources", PoDoFo::PdfObject(res));
            std::string content;
            content += "BT /F1 12 Tf 72 740 Td (";
            content += marker;
            content += ") Tj ET\n";
            page.GetOrCreateContents().CreateStreamForAppending().SetData(
                PoDoFo::bufferview(content));
            return page;
        };
        makePage("HIST page one marker");

        // A real 4x4 RGB image XObject on page 2, drawn via "Do".
        std::string imgBytes;
        imgBytes.reserve(4 * 4 * 3);
        for (int i = 0; i < 16; ++i) {
            imgBytes.push_back(static_cast<char>(0xC8));
            imgBytes.push_back(static_cast<char>(0x1E));
            imgBytes.push_back(static_cast<char>(0x1E));
        }
        auto &img = doc.GetObjects().CreateDictionaryObject();
        img.GetDictionary().AddKey("Type", PoDoFo::PdfName("XObject"));
        img.GetDictionary().AddKey("Subtype", PoDoFo::PdfName("Image"));
        img.GetDictionary().AddKey("Width", static_cast<int64_t>(4));
        img.GetDictionary().AddKey("Height", static_cast<int64_t>(4));
        img.GetDictionary().AddKey("ColorSpace", PoDoFo::PdfName("DeviceRGB"));
        img.GetDictionary().AddKey("BitsPerComponent", static_cast<int64_t>(8));
        img.GetOrCreateStream().SetData(
            PoDoFo::bufferview(imgBytes.data(), imgBytes.size()));

        PoDoFo::PdfDictionary xobjs;
        xobjs.AddKey(PoDoFo::PdfName("Im1"), PoDoFo::PdfObject(img.GetIndirectReference()));
        PoDoFo::PdfDictionary imgRes;
        imgRes.AddKey("XObject", PoDoFo::PdfObject(xobjs));

        // Page 2: ONE content stream containing the text and the image "Do".
        {
            auto &page2 = doc.GetPages().CreatePage(
                PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
            PoDoFo::PdfDictionary fonts2;
            fonts2.AddKey(PoDoFo::PdfName("F1"), PoDoFo::PdfObject(font.GetIndirectReference()));
            PoDoFo::PdfDictionary res2;
            res2.AddKey("Font", PoDoFo::PdfObject(fonts2));
            res2.AddKey("XObject", PoDoFo::PdfObject(xobjs));
            page2.GetDictionary().AddKey("Resources", PoDoFo::PdfObject(res2));

            std::string content2;
            content2 += "BT /F1 12 Tf 72 700 Td (HIST page two marker) Tj ET\n";
            content2 += "q 150 0 0 150 200 300 cm /Im1 Do Q\n";
            page2.GetOrCreateContents().CreateStreamForAppending().SetData(
                PoDoFo::bufferview(content2));
        }

        doc.Save(path.toUtf8().constData());
    } catch (const std::exception &e) {
        qWarning() << "image fixture failed:" << e.what();
        return QString();
    }
    return path;
}

QString extractedText(const QString &path, int page)
{
    PdfiumBackend backend;
    if (!backend.loadDocument(path)) return QString();
    return backend.extractText(page);
}

unsigned pdfPageCount(const QString &path)
{
    try {
        PoDoFo::PdfMemDocument doc;
        doc.Load(path.toUtf8().constData());
        return doc.GetPages().GetCount();
    } catch (const std::exception &) {
        return 0;
    }
}

double pdfBoxHeight(const QString &path, const char *key)
{
    try {
        PoDoFo::PdfMemDocument doc;
        doc.Load(path.toUtf8().constData());
        auto &page = doc.GetPages().GetPageAt(0);
        const PoDoFo::PdfObject *o = page.GetDictionary().FindKey(key);
        if (!o || !o->IsArray()) return -1.0;
        const PoDoFo::PdfArray &arr = o->GetArray();
        auto num = [](const PoDoFo::PdfObject &v) {
            return v.IsNumberOrReal() ? v.GetReal() : 0.0;
        };
        return num(arr[3]) - num(arr[1]);
    } catch (const std::exception &) {
        return -1.0;
    }
}

// Make a file read-only (Windows FILE_ATTRIBUTE_READONLY) / writable again.
void setWritable(const QString &path, bool writable)
{
    QFile::Permissions perms = QFile::ReadOwner | QFile::ReadUser
        | QFile::ReadGroup | QFile::ReadOther;
    if (writable)
        perms |= QFile::WriteOwner | QFile::WriteUser | QFile::WriteGroup | QFile::WriteOther;
    QFile::setPermissions(path, perms);
}

// ── Fault engines (the reviewed "isolated fault engine" pattern) ─────────────

// insertPageFromBytes fails on demand; every page deletion is recorded.
class FaultRestoreEngine : public MockPdfEditorEngine {
public:
    bool m_insertOk = true;
    int m_deletePageCalls = 0;
    int m_lastDeletedPage = -1;
    int m_insertCalls = 0;

    bool insertPageFromBytes(const QString &, int, const QByteArray &) override {
        ++m_insertCalls;
        return m_insertOk;
    }
    bool deletePage(const QString &, int pageIndex) override {
        ++m_deletePageCalls;
        m_lastDeletedPage = pageIndex;
        return true;
    }
};

// The crop snapshot / initial mutation fail on demand. pageCropBox is declared
// WITHOUT `override`: the interface member is new in this repair (pre-fix
// baselines have no such virtual, and this file must compile against them).
class FaultCropEngine : public MockPdfEditorEngine {
public:
    bool m_snapshotFails = false;
    bool m_cropFails = false;
    int m_cropCalls = 0;

    bool cropPage(const QString &, int, const QRectF &) override {
        ++m_cropCalls;
        return !m_cropFails && m_loaded;
    }
    QRectF pageCropBox(const QString &, int, bool *ok) {
        if (ok) *ok = !m_snapshotFails && m_loaded;
        return QRectF(0, 0, 595, 842);
    }
};

} // namespace

class TestHistoryIntegrity : public QObject {
    Q_OBJECT

private slots:
    // ── EC03 ─────────────────────────────────────────────────────────────────
    // THE anchor: with the backup-page insertion failing, undo must NOT touch
    // the page at m_page+1 (the original following page). Pre-fix the command
    // unconditionally issued one deletePage(m_page+1).
    void deleteImageUndoFailureNeverDeletesFollowingPage()
    {
        QTemporaryDir dir;
        const QString f = makeTwoPageTextPdf(dir.path(), "del_img.pdf");
        FaultRestoreEngine engine;
        engine.m_loaded = true;
        engine.m_file = f;
        DocumentSession doc;
        doc.beginDocument(f);
        QUndoStack stack;

        QSignalSpy failed(&doc, SIGNAL(mutationFailed(QString)));
        auto *cmd = new DeleteImageCommand(&engine, &doc, 0, QStringLiteral("Im0"),
                                           QByteArray("backup-page-bytes"));
        stack.push(cmd);
        QCOMPARE(engine.m_deletePageCalls, 0);   // redo deletes an image, not a page

        engine.m_insertOk = false;               // the restore will fail
        stack.undo();

        QVERIFY2(engine.m_deletePageCalls == 0,
                 "EC03: a failed backup insertion must never delete the following page");
        QCOMPARE(failed.count(), 1);             // the failure is reported, not silent
        QVERIFY(doc.isDirty());                  // redo's mutation is still in effect
    }

    // Same contract for ReplaceImageCommand.
    void replaceImageUndoFailureNeverDeletesFollowingPage()
    {
        QTemporaryDir dir;
        const QString f = makeTwoPageTextPdf(dir.path(), "rep_img.pdf");
        FaultRestoreEngine engine;
        engine.m_loaded = true;
        engine.m_file = f;
        DocumentSession doc;
        doc.beginDocument(f);
        QUndoStack stack;

        QSignalSpy failed(&doc, SIGNAL(mutationFailed(QString)));
        stack.push(new ReplaceImageCommand(&engine, &doc, 0, QStringLiteral("Im0"),
                                           QStringLiteral("replacement.png"),
                                           QByteArray("backup-page-bytes")));

        engine.m_insertOk = false;
        stack.undo();

        QVERIFY2(engine.m_deletePageCalls == 0,
                 "EC03: a failed backup insertion must never delete the following page");
        QCOMPARE(failed.count(), 1);
    }

    // The destructive edit refuses to start without a restorable backup: the
    // command becomes obsolete during its initial redo, so it never enters
    // history (QUndoStack push() — Qt 5.15 through 6.x — deletes an
    // obsolete-after-redo command) — a
    // failed mutation must not silently advance history.
    void emptyBackupRefusedBeforeDestructiveEdit()
    {
        QTemporaryDir dir;
        const QString f = makeTwoPageTextPdf(dir.path(), "nobackup.pdf");
        FaultRestoreEngine engine;
        engine.m_loaded = true;
        engine.m_file = f;
        DocumentSession doc;
        doc.beginDocument(f);
        QUndoStack stack;

        QSignalSpy failed(&doc, SIGNAL(mutationFailed(QString)));
        stack.push(new DeleteImageCommand(&engine, &doc, 0, QStringLiteral("Im0"),
                                          QByteArray()));   // empty backup

        QCOMPARE(engine.m_deleteImageCalls, 0);
        QVERIFY2(stack.count() == 0,
                 "EC03: a refused destructive edit must not become an undoable step");
        QVERIFY(!stack.canUndo());
        QVERIFY2(failed.count() == 1, "the refusal must be reported to the history owner");
    }

    // Real-engine control: a VALID backup round-trips — delete image on page 2
    // of 2, undo — and the saved artifact keeps BOTH page identities (page
    // count, both markers, the image back on page 2).
    void imageDeleteUndoRoundTripRestoresPageOnDisk()
    {
        QTemporaryDir dir;
        const QString f = makeImagePagePdf(dir.path(), "roundtrip.pdf");
        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(f));
        DocumentSession doc;
        doc.beginDocument(f);
        QUndoStack stack;

        const QList<PdfImageInfo> imagesBefore = engine.listImages(1);
        QVERIFY2(!imagesBefore.isEmpty(),
                 "EC03 precondition: the fixture's image must be discoverable "
                 "(DoXObject handling in listImages)");
        const QByteArray backup = engine.extractPageAsBytes(f, 1);
        QVERIFY2(!backup.isEmpty(), "EC03: the destructive edit needs a restorable backup");

        stack.push(new DeleteImageCommand(&engine, &doc, 1, imagesBefore.first().xobjectName,
                                          backup));
        QCOMPARE(pdfPageCount(f), 2u);
        QVERIFY(engine.listImages(1).isEmpty());   // image gone (engine-resident view)

        stack.undo();

        // Saved-artifact truth: page count AND both page identities survive.
        QCOMPARE(pdfPageCount(f), 2u);
        QVERIFY2(extractedText(f, 0).contains(QLatin1String("page one marker")),
                 "EC03: page 1 identity must survive the undo");
        QVERIFY2(extractedText(f, 1).contains(QLatin1String("page two marker")),
                 "EC03: page 2 identity must survive the undo");
        QVERIFY2(!engine.listImages(1).isEmpty(),
                 "EC03: undo must restore the deleted image on page 2");
    }

    // ── EC05 ─────────────────────────────────────────────────────────────────
    // THE anchor: undo must restore the on-disk CropBox. Pre-fix undo was a
    // reload-only no-op and the artifact stayed cropped.
    void cropUndoRestoresOriginalCropBoxOnDisk()
    {
        QTemporaryDir dir;
        const QString f = makeTwoPageTextPdf(dir.path(), "crop.pdf");
        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(f));
        DocumentSession doc;
        doc.beginDocument(f);
        QUndoStack stack;

        const double mediaHeight = pdfBoxHeight(f, "MediaBox");
        QVERIFY(mediaHeight > 0);

        stack.push(new CropPageCommand(&engine, &doc, 0, QRectF(50, 50, 300, 400)));
        const double cropped = pdfBoxHeight(f, "CropBox");
        QVERIFY2(qAbs(cropped - 400.0) < 0.01,
                 "EC05: the crop mutation must persist the CropBox");
        QCOMPARE(pdfPageCount(f), 2u);

        stack.undo();
        QVERIFY2(qAbs(pdfBoxHeight(f, "CropBox") - mediaHeight) < 0.01,
                 "EC05: undo must restore the original geometry on disk, not reload the viewer");

        stack.redo();
        QVERIFY2(qAbs(pdfBoxHeight(f, "CropBox") - 400.0) < 0.01,
                 "EC05: redo re-applies the same crop");
        QCOMPARE(pdfPageCount(f), 2u);
        QVERIFY2(extractedText(f, 1).contains(QLatin1String("page two marker")),
                 "EC05: the FOLLOWING page must be untouched by crop undo/redo");
    }

    // A failed snapshot or initial mutation refuses the command: no undoable
    // step appears (obsolete after the initial redo), the failure is reported.
    void cropCommandRefusedWhenSnapshotOrMutationFails()
    {
        QTemporaryDir dir;
        const QString f = makeTwoPageTextPdf(dir.path(), "crop_refuse.pdf");
        DocumentSession doc;
        doc.beginDocument(f);

        FaultCropEngine snapshotFail;
        snapshotFail.m_loaded = true;
        snapshotFail.m_file = f;
        snapshotFail.m_snapshotFails = true;
        {
            QUndoStack stack;
            QSignalSpy failed(&doc, SIGNAL(mutationFailed(QString)));
            stack.push(new CropPageCommand(&snapshotFail, &doc, 0, QRectF(0, 0, 10, 10)));
            QCOMPARE(snapshotFail.m_cropCalls, 0);
            QVERIFY2(stack.count() == 0,
                     "EC05: an unreadable snapshot must not become an undoable step");
            QCOMPARE(failed.count(), 1);
        }
        {
            FaultCropEngine mutationFail;
            mutationFail.m_loaded = true;
            mutationFail.m_file = f;
            mutationFail.m_cropFails = true;
            QUndoStack stack;
            QSignalSpy failed(&doc, SIGNAL(mutationFailed(QString)));
            stack.push(new CropPageCommand(&mutationFail, &doc, 0, QRectF(0, 0, 10, 10)));
            QVERIFY2(stack.count() == 0,
                     "EC05: a failed initial mutation must not become an undoable step");
            QCOMPARE(failed.count(), 1);
            QVERIFY(!doc.isDirty());   // no markReload for a mutation that never happened
        }
    }

    // ── V02 ──────────────────────────────────────────────────────────────────
    // A failed form-field UNDO must be reported and must leave disk and dirty
    // state truthful. Pre-fix it logged and returned silently.
    void failedFormUndoIsReportedAndKeepsDiskTruthful()
    {
        QTemporaryDir dir;
        const QString f = makeTwoPageTextPdf(dir.path(), "form_undo.pdf");
        FormManager forms;
        QVERIFY(forms.addTextField(f, 0, QRectF(72, 100, 144, 24), QStringLiteral("F1"), f));
        QVariantMap seed;
        seed[QStringLiteral("F1")] = QStringLiteral("ORIGINAL");
        QVERIFY(forms.fillForm(f, seed, f, /*lockFields=*/false));

        DocumentSession doc;
        doc.beginDocument(f);
        EditFormFieldProperties props;
        props.tooltip = QStringLiteral("new tooltip");
        props.required = true;
        props.defaultVal = QStringLiteral("EDITED");
        EditFormFieldCommand *cmd = new EditFormFieldCommand(&forms, &doc, QStringLiteral("F1"), props);
        QUndoStack stack;
        stack.push(cmd);
        QVERIFY2(cmd->succeeded(), "V02 precondition: the edit itself must persist");

        QSignalSpy failed(&doc, SIGNAL(mutationFailed(QString)));

        // Deterministic failure injection: the restore's safe-save commit
        // cannot replace a read-only destination.
        setWritable(f, false);
        stack.undo();

        QVERIFY2(failed.count() == 1,
                 "V02: a failed form undo must be reported to the history owner");
        const FormFieldSnapshot after = forms.captureFieldSnapshot(f, QStringLiteral("F1"));
        QVERIFY2(after.found && after.value == QLatin1String("EDITED"),
                 "V02: a failed undo must not silently change the document");
        QVERIFY(doc.isDirty());   // the (edited) work is still uncommitted — dirty stays
        setWritable(f, true);
    }
};

QTEST_MAIN(TestHistoryIntegrity)
#include "TestHistoryIntegrity.moc"
