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
//   G07  (QUALITY-GATE-2026-09-09) — the crop snapshot reused PDF corners
//          [x0 y0 x1 y1] as Qt (x,y,w,h) and resolved an INHERITED CropBox to
//          the whole MediaBox. The restored engine resolves the effective box
//          with correct corner→pos/size conversion, and undo restores the
//          ORIGINAL semantics: explicit box rewritten, inherited/absent
//          restored by removing the page's explicit key again.
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
#include "commands/CheckedHistory.h"
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

// ── G07 (QUALITY-GATE-2026-09-09) helpers ─────────────────────────────────────
// CropBox fixture exactly like the reviewer's probe: one page whose box is
// [10 20 510 720] (PDF corners — offset (10,20), size 500x700) either as an
// explicit page key or inherited from the root /Pages node.
void makeCropBoxPdf(const QString &path, bool inherited)
{
    PoDoFo::PdfMemDocument pdf;
    auto &p = pdf.GetPages().CreatePage(PoDoFo::Rect(0, 0, 612, 792));
    if (inherited) {
        auto *parent = p.GetDictionary().FindKey("Parent");
        parent->GetDictionary().AddKey("CropBox", PoDoFo::Rect(10, 20, 500, 700).ToArray());
    } else {
        p.GetDictionary().AddKey("CropBox", PoDoFo::Rect(10, 20, 500, 700).ToArray());
    }
    pdf.Save(path.toUtf8().constData());
}

// The page dictionary's OWN /CropBox corners, exactly as saved (-1 entries on
// absence/malformed).
QList<double> ownCropBoxCorners(const QString &path)
{
    try {
        PoDoFo::PdfMemDocument doc;
        doc.Load(path.toUtf8().constData());
        auto &page = doc.GetPages().GetPageAt(0);
        const PoDoFo::PdfObject *o = page.GetDictionary().FindKey("CropBox");
        const QList<double> missing = { -1.0, -1.0, -1.0, -1.0 };
        if (!o || !o->IsArray() || o->GetArray().GetSize() != 4)
            return missing;
        const PoDoFo::PdfArray &arr = o->GetArray();
        QList<double> out;
        for (int i = 0; i < 4; ++i)
            out.append(arr[i].IsNumberOrReal() ? arr[i].GetReal() : -1.0);
        return out;
    } catch (const std::exception &) {
        return QList<double>{ -1.0, -1.0, -1.0, -1.0 };
    }
}

// The EFFECTIVE CropBox as PoDoFo consumers see it (inheritance resolved,
// MediaBox fallback) — position/size form.
QRectF effectiveCropBox(const QString &path)
{
    try {
        PoDoFo::PdfMemDocument doc;
        doc.Load(path.toUtf8().constData());
        const PoDoFo::Rect r = doc.GetPages().GetPageAt(0).GetCropBox();
        return QRectF(r.X, r.Y, r.Width, r.Height);
    } catch (const std::exception &) {
        return QRectF();
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

// G08: the two-step primitives are only recorded to prove the commands NEVER
// take them any more; the restoration itself is faulted through the atomic
// seam (m_pageRestoreOk / m_restorePageCalls come from the mock base).
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
    // G07 (QUALITY-GATE-2026-09-09): the command now snapshots through the
    // origin-aware seam and restores absent/inherited semantics through the
    // removal seam. Deliberately NO `override` — plain members pre-fix,
    // interface virtuals post-fix (same rule as pageCropBox above).
    bool pageCropBoxInfo(const QString &, int, QRectF *outBox, int *outOrigin) {
        if (outBox) *outBox = QRectF(0, 0, 595, 842);
        // literal 1 == IPdfEditorEngine::kCropBoxExplicit (compile-compatible
        // with pre-fix baselines for revert verification)
        if (outOrigin) *outOrigin = 1;
        return !m_snapshotFails && m_loaded;
    }
    bool removePageCropBox(const QString &, int) {
        ++m_removeCropBoxCalls;
        return !m_cropFails && m_loaded;
    }
    int m_removeCropBoxCalls = 0;
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

        engine.m_pageRestoreOk = false;          // the atomic restore will fail
        stack.undo();

        QVERIFY2(engine.m_deletePageCalls == 0,
                 "EC03: a failed restoration must never delete the following page");
        QVERIFY2(engine.m_insertCalls == 0,
                 "G08: the restore must be ONE committed step — no insert half-step");
        QCOMPARE(engine.m_restorePageCalls, 1);  // the atomic seam was the one attempt
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

        engine.m_pageRestoreOk = false;
        stack.undo();

        QVERIFY2(engine.m_deletePageCalls == 0,
                 "EC03: a failed restoration must never delete the following page");
        QVERIFY2(engine.m_insertCalls == 0,
                 "G08: the restore must be ONE committed step — no insert half-step");
        QCOMPARE(engine.m_restorePageCalls, 1);
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
    // G07 (QUALITY-GATE-2026-09-09): this fixture has NO pre-existing CropBox,
    // so the snapshot's origin is ABSENT and undo restores that semantic — the
    // explicit key is removed and the effective box is the MediaBox again.
    void cropUndoRestoresOriginalCropBoxOnDisk()
    {
        QTemporaryDir dir;
        const QString f = makeTwoPageTextPdf(dir.path(), "crop.pdf");
        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(f));
        DocumentSession doc;
        doc.beginDocument(f);
        QUndoStack stack;

        const QRectF media = effectiveCropBox(f);
        QVERIFY(!media.isEmpty());

        stack.push(new CropPageCommand(&engine, &doc, 0, QRectF(50, 50, 300, 400)));
        const double cropped = pdfBoxHeight(f, "CropBox");
        QVERIFY2(qAbs(cropped - 400.0) < 0.01,
                 "EC05: the crop mutation must persist the CropBox");
        QCOMPARE(pdfPageCount(f), 2u);

        stack.undo();
        // G07: the original state was CropBox-ABSENT — the explicit key is
        // gone again and the effective geometry is the MediaBox.
        QVERIFY2(ownCropBoxCorners(f).at(0) < 0,
                 "G07: undo of a crop of an uncropped page must remove the "
                 "explicit /CropBox (restore the absent semantics)");
        const QRectF restored = effectiveCropBox(f);
        QVERIFY2(qAbs(restored.x() - media.x()) < 0.01 &&
                     qAbs(restored.y() - media.y()) < 0.01 &&
                     qAbs(restored.width() - media.width()) < 0.01 &&
                     qAbs(restored.height() - media.height()) < 0.01,
                 "EC05: undo must restore the original geometry on disk, not reload the viewer");

        stack.redo();
        QVERIFY2(qAbs(pdfBoxHeight(f, "CropBox") - 400.0) < 0.01,
                 "EC05: redo re-applies the same crop");
        QCOMPARE(pdfPageCount(f), 2u);
        QVERIFY2(extractedText(f, 1).contains(QLatin1String("page two marker")),
                 "EC05: the FOLLOWING page must be untouched by crop undo/redo");
    }

    // ── G07 (QUALITY-GATE-2026-09-09) ─────────────────────────────────────────
    // The snapshot treated the PDF corners [x0 y0 x1 y1] as Qt (x,y,w,h): undo
    // of a crop of a box at offset (10,20) wrote (10,20 510x720) — a box that
    // never existed. The saved/reopened geometry must be the ORIGINAL box.
    void cropUndoRestoresNonZeroOriginExplicitBoxExactly()
    {
        QTemporaryDir dir;
        const QString f = dir.filePath(QStringLiteral("g07_offset.pdf"));
        makeCropBoxPdf(f, /*inherited=*/false);
        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(f));
        DocumentSession doc;
        doc.beginDocument(f);
        QUndoStack stack;

        // Engine contract: the effective box converts corners to pos+size.
        bool ok = false;
        const QRectF snap = engine.pageCropBox(f, 0, &ok);
        QVERIFY(ok);
        QVERIFY2(qAbs(snap.x() - 10.0) < 0.01 && qAbs(snap.y() - 20.0) < 0.01 &&
                     qAbs(snap.width() - 500.0) < 0.01 && qAbs(snap.height() - 700.0) < 0.01,
                 "G07: pageCropBox must convert [10 20 510 720] corners to "
                 "QRectF(10,20 500x700), not reuse the corners as size");

        stack.push(new CropPageCommand(&engine, &doc, 0, QRectF(50, 60, 400, 500)));
        QVERIFY2(ownCropBoxCorners(f).at(0) > 0, "precondition: the crop persisted a box");
        stack.undo();

        // Saved/reopened geometry: exact original corners, byte-for-byte values.
        const QList<double> corners = ownCropBoxCorners(f);
        QVERIFY2(qAbs(corners.at(0) - 10.0) < 0.01 && qAbs(corners.at(1) - 20.0) < 0.01 &&
                     qAbs(corners.at(2) - 510.0) < 0.01 && qAbs(corners.at(3) - 720.0) < 0.01,
                 qPrintable(QStringLiteral(
                                "G07: undo must restore the original corners [10 20 510 720], got "
                                "[%1 %2 %3 %4]").arg(corners.at(0)).arg(corners.at(1))
                                .arg(corners.at(2)).arg(corners.at(3))));
        const QRectF eff = effectiveCropBox(f);
        QVERIFY2(qAbs(eff.width() - 500.0) < 0.01 && qAbs(eff.height() - 700.0) < 0.01,
                 "G07: the reopened effective box must be 500x700 again");
    }

    // The snapshot resolved an INHERITED CropBox to the whole MediaBox. Undo
    // must restore the inherited semantics: no page-level key, effective box
    // equal to the INHERITED box.
    void cropUndoRestoresInheritedBoxNotMediaBox()
    {
        QTemporaryDir dir;
        const QString f = dir.filePath(QStringLiteral("g07_inherited.pdf"));
        makeCropBoxPdf(f, /*inherited=*/true);
        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(f));
        DocumentSession doc;
        doc.beginDocument(f);
        QUndoStack stack;

        // Precondition: the inherited box is the effective box before the crop.
        bool ok = false;
        const QRectF snap = engine.pageCropBox(f, 0, &ok);
        QVERIFY(ok);
        QVERIFY2(qAbs(snap.width() - 500.0) < 0.01 && qAbs(snap.height() - 700.0) < 0.01,
                 "G07 precondition: the inherited box (500x700) must be the "
                 "effective box, not the 612x792 MediaBox");
        QVERIFY2(ownCropBoxCorners(f).at(0) < 0,
                 "G07 precondition: the box is inherited, not an own key");

        stack.push(new CropPageCommand(&engine, &doc, 0, QRectF(50, 60, 400, 500)));
        QVERIFY2(ownCropBoxCorners(f).at(0) > 0, "the crop wrote an explicit key");
        stack.undo();

        QVERIFY2(ownCropBoxCorners(f).at(0) < 0,
                 "G07: undo must remove the explicit key again so the INHERITED "
                 "semantics are restored");
        const QRectF eff = effectiveCropBox(f);
        QVERIFY2(qAbs(eff.x() - 10.0) < 0.01 && qAbs(eff.y() - 20.0) < 0.01 &&
                     qAbs(eff.width() - 500.0) < 0.01 && qAbs(eff.height() - 700.0) < 0.01,
                 qPrintable(QStringLiteral(
                                "G07: the inherited effective box (10,20 500x700) must be "
                                "restored — NOT the whole MediaBox (got %1,%2 %3x%4)")
                                .arg(eff.x()).arg(eff.y()).arg(eff.width()).arg(eff.height())));
    }

    // Repeated undo/redo cycles: every restore lands on the saved artifact
    // exactly, and a consumer (QtPdf/PDFium) reads the original page size.
    void cropUndoRedoCyclesKeepSavedGeometryStable()
    {
        QTemporaryDir dir;
        const QString f = dir.filePath(QStringLiteral("g07_cycles.pdf"));
        makeCropBoxPdf(f, /*inherited=*/false);
        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(f));
        DocumentSession doc;
        doc.beginDocument(f);
        QUndoStack stack;

        stack.push(new CropPageCommand(&engine, &doc, 0, QRectF(50, 60, 400, 500)));
        for (int cycle = 0; cycle < 3; ++cycle) {
            stack.undo();
            QList<double> c = ownCropBoxCorners(f);
            QVERIFY2(qAbs(c.at(0) - 10.0) < 0.01 && qAbs(c.at(2) - 510.0) < 0.01 &&
                         qAbs(c.at(1) - 20.0) < 0.01 && qAbs(c.at(3) - 720.0) < 0.01,
                     qPrintable(QStringLiteral("G07: undo cycle %1 must restore the "
                                               "original saved corners").arg(cycle)));
            stack.redo();
            c = ownCropBoxCorners(f);
            QVERIFY2(qAbs(c.at(0) - 50.0) < 0.01 && qAbs(c.at(1) - 60.0) < 0.01 &&
                         qAbs(c.at(2) - 450.0) < 0.01 && qAbs(c.at(3) - 560.0) < 0.01,
                     qPrintable(QStringLiteral("G07: redo cycle %1 must re-apply the "
                                               "crop corners [50 60 450 560], got "
                                               "[%2 %3 %4 %5]").arg(cycle)
                                     .arg(c.at(0)).arg(c.at(1)).arg(c.at(2)).arg(c.at(3))));
        }
        stack.undo();
        QCOMPARE(effectiveCropBox(f), QRectF(10.0, 20.0, 500.0, 700.0));
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

    // ── G08 (QUALITY-GATE-2026-09-09) ─────────────────────────────────────────
    // Qt gives undo() NO failure channel — the stack index moved even when the
    // restoration mutated nothing, so a failed form undo landed on a false
    // clean state (index 0, isClean() true, canUndo() false) with EDITED still
    // on disk, and the command was no longer reachable for a retry. The
    // production traversal seam (CheckedHistory::undo — the one HomeController
    // uses) must move the history position only after a successful restore.
    void failedFormUndoCheckedTraversalKeepsPositionAndIsRetryable()
    {
        QTemporaryDir dir;
        const QString f = makeTwoPageTextPdf(dir.path(), "form_undo_retry.pdf");
        FormManager forms;
        QVERIFY(forms.addTextField(f, 0, QRectF(72, 100, 144, 24), QStringLiteral("F1"), f));
        QVariantMap seed;
        seed[QStringLiteral("F1")] = QStringLiteral("ORIGINAL");
        QVERIFY(forms.fillForm(f, seed, f, /*lockFields=*/false));

        DocumentSession doc;
        doc.beginDocument(f);
        EditFormFieldProperties props;
        props.defaultVal = QStringLiteral("EDITED");
        QUndoStack stack;
        stack.push(new EditFormFieldCommand(&forms, &doc, QStringLiteral("F1"), props));
        QCOMPARE(stack.index(), 1);
        QVERIFY(!stack.isClean());

        QSignalSpy failed(&doc, SIGNAL(mutationFailed(QString)));
        FormManager::setSaveFaultForTesting(FormManager::SaveFault::Commit);
        QVERIFY2(!CheckedHistory::undo(&stack),
                 "G08: the checked traversal must report the failed restoration");
        FormManager::setSaveFaultForTesting(FormManager::SaveFault::None);

        // History position unchanged: the command stays CURRENT and retryable.
        QCOMPARE(stack.index(), 1);
        QVERIFY2(!stack.isClean(),
                 "G08: a failed undo must NOT land on the clean state while the "
                 "edited values are still on disk");
        QVERIFY2(stack.canUndo(), "G08: the failed command must stay undoable (retryable)");
        QVERIFY2(!stack.canRedo(), "G08: nothing was traversed — nothing to redo");
        QCOMPARE(failed.count(), 1);   // the failure is still reported
        const FormFieldSnapshot stillEdited = forms.captureFieldSnapshot(f, QStringLiteral("F1"));
        QVERIFY2(stillEdited.found && stillEdited.value == QLatin1String("EDITED"),
                 "G08: the failed restore must not have changed the document");

        // Retryability: clear the fault and the SAME command restores.
        QVERIFY2(CheckedHistory::undo(&stack), "G08: retrying the undo must succeed");
        QCOMPARE(stack.index(), 0);
        QVERIFY(stack.isClean());
        const FormFieldSnapshot restored = forms.captureFieldSnapshot(f, QStringLiteral("F1"));
        QVERIFY2(restored.found && restored.value == QLatin1String("ORIGINAL"),
                 "G08: the retried restore must persist the original value");
    }

    // Same contract for the image-restore path, plus the exactly-once
    // restoration rule: the failed attempt leaves no committed state, the
    // retry performs the single atomic restoration again.
    void failedImageUndoCheckedTraversalIsRetryable()
    {
        QTemporaryDir dir;
        const QString f = makeTwoPageTextPdf(dir.path(), "img_retry.pdf");
        FaultRestoreEngine engine;
        engine.m_loaded = true;
        engine.m_file = f;
        DocumentSession doc;
        doc.beginDocument(f);
        QUndoStack stack;
        stack.push(new DeleteImageCommand(&engine, &doc, 0, QStringLiteral("Im0"),
                                          QByteArray("backup-page-bytes")));
        QCOMPARE(stack.index(), 1);

        QSignalSpy failed(&doc, SIGNAL(mutationFailed(QString)));
        engine.m_pageRestoreOk = false;
        QVERIFY2(!CheckedHistory::undo(&stack),
                 "G08: the checked traversal must report the failed restoration");
        QCOMPARE(stack.index(), 1);
        QVERIFY(!stack.isClean());
        QVERIFY(stack.canUndo());
        QVERIFY(!stack.canRedo());
        QCOMPARE(failed.count(), 1);
        QCOMPARE(engine.m_restorePageCalls, 1);
        QCOMPARE(engine.m_deletePageCalls, 0);

        engine.m_pageRestoreOk = true;
        QVERIFY2(CheckedHistory::undo(&stack), "G08: retrying the undo must succeed");
        QCOMPARE(stack.index(), 0);
        QCOMPARE(engine.m_restorePageCalls, 2);   // one attempt per traversal
        QCOMPARE(engine.m_deletePageCalls, 0);    // the two-step path never runs
        QCOMPARE(engine.m_insertCalls, 0);
    }

    // Raw QUndoStack::undo() (the non-checked path) still applies the
    // restoration through the atomic seam exactly once per call.
    void checkedCommandUndoAppliesRestoreExactlyOncePerTraversal()
    {
        QTemporaryDir dir;
        const QString f = makeTwoPageTextPdf(dir.path(), "img_once.pdf");
        FaultRestoreEngine engine;
        engine.m_loaded = true;
        engine.m_file = f;
        DocumentSession doc;
        doc.beginDocument(f);
        QUndoStack stack;
        stack.push(new ReplaceImageCommand(&engine, &doc, 0, QStringLiteral("Im0"),
                                           QStringLiteral("replacement.png"),
                                           QByteArray("backup-page-bytes")));

        engine.m_pageRestoreOk = false;
        stack.undo();                       // raw path: restore attempted (fails)
        QCOMPARE(engine.m_restorePageCalls, 1);
        stack.undo();                       // not retryable via the raw path —
        QCOMPARE(engine.m_restorePageCalls, 1);   // but also not applied twice
    }
};

QTEST_MAIN(TestHistoryIntegrity)
#include "TestHistoryIntegrity.moc"
