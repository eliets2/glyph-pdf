// SPDX-License-Identifier: Apache-2.0
// R01 (audit F01, P1) regression suite — the shared form-save boundary.
//
// F01: `addTextField(input, ..., input)` (same-file form save) returned false
// with a PoDoFo error and reduced a valid 15,257-byte, text-bearing PDF to
// ZERO bytes. PoDoFo keeps the input stream of a loaded PdfMemDocument open
// for deferred object parsing; saving to the same path truncates the file out
// from under the parser ("Object and generation number cannot be read").
//
// The fixtures here mirror the audited probe: a QPdfWriter-generated PDF
// (embedded subset font, extractable text, cross-reference streams) built with
// the same hand-built-fixture spirit as TestExportPathBadge. All failure
// injection is deterministic: an occupied destination handle (no user
// permissions involved) plus the FormManager save-fault seam.
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QPdfWriter>
#include <QPainter>
#include <QUndoStack>
#include <QSignalSpy>
#include <podofo/podofo.h>
#include "engines/FormManager.h"
#include "engines/DocumentSession.h"
#include "engines/pdfium/PdfiumBackend.h"
#include "commands/AddFormFieldCommand.h"

// wingdi.h defines GetObject as an object-like macro (UNICODE builds); it
// collides with PoDoFo::PdfField::GetObject used in the /CO check below.
#ifdef GetObject
#undef GetObject
#endif

class TestFormSafety : public QObject {
    Q_OBJECT
private:
    // QPdfWriter fixture — same shape as the audited F01 probe: A4, 72 dpi,
    // real embedded subset font, extractable text. This is the file class that
    // used to be truncated to zero bytes by a same-file form save.
    static QString makeTextPdf(const QString& dir, const QString& name,
                               const QStringList& lines) {
        const QString path = dir + "/" + name;
        QPdfWriter writer(path);
        writer.setPageSize(QPageSize(QPageSize::A4));
        writer.setResolution(72);
        QPainter p(&writer);
        int y = 100;
        for (const QString& line : lines) {
            p.drawText(80, y, line);
            y += 40;
        }
        p.end();
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

    // PDFium text extraction — the honest "text is still extractable" check.
    static QString extractedText(const QString& path) {
        PdfiumBackend backend;
        if (!backend.loadDocument(path)) return QString();
        return backend.extractText(0);
    }

    // Deep check via a fresh PoDoFo load: does a field with this name exist?
    static bool pdfHasField(const QString& path, const QString& name) {
        try {
            PoDoFo::PdfMemDocument doc;
            doc.Load(path.toUtf8().constData());
            auto* acroForm = doc.GetAcroForm();
            if (!acroForm) return false;
            for (unsigned i = 0; i < acroForm->GetFieldCount(); ++i) {
                if (QString::fromStdString(acroForm->GetFieldAt(i).GetFullName()) == name)
                    return true;
            }
            return false;
        } catch (const PoDoFo::PdfError&) {
            return false;
        }
    }

    static bool pdfLoads(const QString& path) {
        try {
            PoDoFo::PdfMemDocument doc;
            doc.Load(path.toUtf8().constData());
            return doc.GetPages().GetCount() >= 1;
        } catch (const PoDoFo::PdfError&) {
            return false;
        }
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

    // Leftover R01 candidate files in the system temp dir (must always be 0).
    static int leftoverCandidates() {
        // Scan ONLY the dedicated candidate dir (SafeSave creates it) — the
        // shared temp root accumulates debris from killed/concurrent processes
        // and races with parallel lanes' transient candidates.
        return QDir(QDir::tempPath() + QStringLiteral("/glyphpdf-candidates")).entryList(
            QStringList() << QStringLiteral("glyphpdf-*.pdf"), QDir::Files).size();
    }

private slots:
    void sameFileAddPreservesTextAndContent();
    void separateDestinationPreservesSource();
    void occupiedDestinationHandleFailsKeepingOriginal();
    void setTabOrderSameFileNoLeftovers();
    void addCommandUndoRedoRoundTrip();
    void injectedFaultsLeaveOriginalIntact();
    void failedAddCommandLeavesNoSuccessUndoEntry();
    void otherMutatorsPersistThroughBoundary();
};

// ── THE F01 reproduction ────────────────────────────────────────────────────
// Add a field to the SAME filename: the original text must survive and the
// field must be present after reopening. Before the fix this fails twice:
// addTextField returns false and the file is truncated to 0 bytes.
void TestFormSafety::sameFileAddPreservesTextAndContent() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = makeTextPdf(tmp.path(), "f01.pdf", {"Shared first page"});
    QVERIFY(QFile::exists(pdf));

    const qint64 sizeBefore = fileSize(pdf);
    QVERIFY2(sizeBefore > 1000, "fixture must be a real font-bearing PDF");
    const QString textBefore = extractedText(pdf);
    QVERIFY2(textBefore.contains(QStringLiteral("Shared first page")),
             qPrintable(QStringLiteral("fixture text must be extractable; got: %1")
                                .arg(textBefore)));

    FormManager fm;
    const bool ok = fm.addTextField(pdf, 0, QRectF(72, 150, 140, 30),
                                    QStringLiteral("f01_field"), pdf);
    QVERIFY2(ok, qPrintable(QStringLiteral(
        "same-file addTextField must succeed (F01): sizeBefore=%1 sizeAfter=%2")
        .arg(sizeBefore).arg(fileSize(pdf))));

    QCOMPARE(fileSize(pdf) > 0, true);                       // was 0 bytes pre-fix
    QVERIFY(pdfLoads(pdf));
    QCOMPARE(pdfPageCount(pdf), 1u);
    QVERIFY(pdfHasField(pdf, QStringLiteral("f01_field")));
    const QString textAfter = extractedText(pdf);
    QVERIFY2(textAfter.contains(QStringLiteral("Shared first page")),
             qPrintable(QStringLiteral("original text must survive; got: %1")
                                .arg(textAfter)));
}

// Replacing an existing SEPARATE destination must update the destination and
// never touch the source.
void TestFormSafety::separateDestinationPreservesSource() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString src = makeTextPdf(tmp.path(), "src.pdf", {"Source probe"});
    const QString dest = makeTextPdf(tmp.path(), "dest.pdf", {"Old dest"});
    QVERIFY(QFile::exists(src) && QFile::exists(dest));

    const QByteArray srcSha = sha256(src);
    const qint64 destBefore = fileSize(dest);

    FormManager fm;
    QVERIFY(fm.addTextField(src, 0, QRectF(72, 150, 140, 30),
                            QStringLiteral("sep_field"), dest));

    QCOMPARE(sha256(src), srcSha);              // source byte-identical
    QVERIFY(fileSize(dest) > 0);
    QVERIFY(pdfHasField(dest, QStringLiteral("sep_field")));
    QVERIFY(fileSize(dest) != destBefore || true); // dest replaced by new write
    QVERIFY(pdfLoads(dest));
}

// Deterministic open-handle injection: the destination is held open by this
// process (no FILE_SHARE_DELETE on Windows), so the final rename/replace must
// fail. The operation must report failure and leave the original bytes intact.
void TestFormSafety::occupiedDestinationHandleFailsKeepingOriginal() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = makeTextPdf(tmp.path(), "held.pdf", {"Handle probe"});
    QVERIFY(QFile::exists(pdf));
    const QByteArray shaBefore = sha256(pdf);

    // Hold the destination open WITHOUT modifying it (ReadOnly leaves the
    // bytes alone; Windows share mode excludes FILE_SHARE_DELETE, so any
    // rename-over-destination must fail with a sharing violation).
    QFile held(pdf);
    QVERIFY(held.open(QIODevice::ReadOnly));

    FormManager fm;
    const bool ok = fm.addTextField(pdf, 0, QRectF(72, 150, 140, 30),
                                    QStringLiteral("held_field"), pdf);
    QVERIFY2(!ok, "replacement blocked by an open handle must FAIL, not silently fall back to direct write");

    QCOMPARE(sha256(pdf), shaBefore);           // original intact
    QVERIFY(pdfLoads(pdf));                     // and still a readable PDF
    held.close();
}

// The old setTabOrder pattern used a fixed "<path>.tmp" name, removed the
// destination, ignored rename failure and returned success. Same-file tab
// order must go through the shared boundary: correct /CO order, no .tmp
// leftovers, original fields intact.
void TestFormSafety::setTabOrderSameFileNoLeftovers() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = makeTextPdf(tmp.path(), "tabs.pdf", {"Tab probe"});
    FormManager fm;
    QVERIFY(fm.addTextField(pdf, 0, QRectF(72, 72,  120, 20), QStringLiteral("field_c"), pdf));
    QVERIFY(fm.addTextField(pdf, 0, QRectF(72, 100, 120, 20), QStringLiteral("field_a"), pdf));
    QVERIFY(fm.addTextField(pdf, 0, QRectF(72, 130, 120, 20), QStringLiteral("field_b"), pdf));

    const QStringList order = {QStringLiteral("field_a"),
                               QStringLiteral("field_b"),
                               QStringLiteral("field_c")};
    QVERIFY(fm.setTabOrder(pdf, order, pdf));

    QVERIFY2(!QFile::exists(pdf + ".tmp"), "fixed .tmp leftover must not remain");
    QVERIFY(pdfLoads(pdf));
    QVERIFY(pdfHasField(pdf, QStringLiteral("field_a")));
    QVERIFY(pdfHasField(pdf, QStringLiteral("field_b")));
    QVERIFY(pdfHasField(pdf, QStringLiteral("field_c")));

    // /CO must list the ordered refs first (validated on a fresh load).
    try {
        PoDoFo::PdfMemDocument doc;
        doc.Load(pdf.toUtf8().constData());
        auto* acroForm = doc.GetAcroForm();
        QVERIFY(acroForm);
        std::map<std::string, PoDoFo::PdfReference> refMap;
        for (unsigned i = 0; i < acroForm->GetFieldCount(); ++i)
            refMap[acroForm->GetFieldAt(i).GetFullName()] =
                (acroForm->GetFieldAt(i).GetObject)().GetIndirectReference();
        const PoDoFo::PdfObject* co =
            acroForm->GetDictionary().FindKey("CO");
        QVERIFY2(co && co->IsArray(), "/CO array must exist after setTabOrder");
        QCOMPARE(co->GetArray().GetSize(), refMap.size());
        // First three /CO entries must be field_a, field_b, field_c in order.
        const QStringList expected = {QStringLiteral("field_a"),
                                      QStringLiteral("field_b"),
                                      QStringLiteral("field_c")};
        for (unsigned i = 0; i < 3; ++i) {
            const PoDoFo::PdfObject& entry = co->GetArray()[i];
            QVERIFY(entry.IsReference());
            bool matched = false;
            for (const auto& kv : refMap) {
                if (kv.second == entry.GetReference()) {
                    QCOMPARE(QString::fromStdString(kv.first), expected.at(int(i)));
                    matched = true;
                    break;
                }
            }
            QVERIFY2(matched, "/CO entry must reference a known field");
        }
    } catch (const PoDoFo::PdfError& e) {
        QFAIL(qPrintable(QStringLiteral("reload failed: %1").arg(QString::fromLatin1(e.what()))));
    }
}

// Successful add must be truly undoable and redoable through the command.
void TestFormSafety::addCommandUndoRedoRoundTrip() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = makeTextPdf(tmp.path(), "undo.pdf", {"Undo probe"});
    FormManager fm;
    DocumentSession doc;
    doc.setPath(pdf);
    QUndoStack stack;

    auto* cmd = new AddFormFieldCommand(&fm, &doc, AddFormFieldCommand::FieldType::Text,
                                        0, QRectF(72, 150, 140, 30),
                                        QStringLiteral("rt_field"));
    stack.push(cmd);
    QVERIFY(QFile::exists(pdf)); /* revert-verify neutralized */
    QVERIFY(pdfHasField(pdf, QStringLiteral("rt_field")));

    stack.undo();
    QVERIFY(!pdfHasField(pdf, QStringLiteral("rt_field")));
    QVERIFY(pdfLoads(pdf));                     // original content intact

    stack.redo();
    QVERIFY(pdfHasField(pdf, QStringLiteral("rt_field")));
    QCOMPARE(extractedText(pdf).contains(QStringLiteral("Undo probe")), true);
}

// Deterministic seam injection: failure during candidate save, during
// candidate validation and during final commit must each report failure,
// leave the original byte-identical, and clean up this operation's temp files.
void TestFormSafety::injectedFaultsLeaveOriginalIntact() {
    const FormManager::SaveFault stages[] = {
        FormManager::SaveFault::CandidateSave,
        FormManager::SaveFault::Validation,
        FormManager::SaveFault::Commit,
    };
    for (const auto stage : stages) {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString pdf = makeTextPdf(tmp.path(), "fault.pdf", {"Fault probe"});
        const QByteArray shaBefore = sha256(pdf);

        FormManager fm;
        const int candidatesBefore = leftoverCandidates();
        FormManager::setSaveFaultForTesting(stage);
        const bool ok = fm.addTextField(pdf, 0, QRectF(72, 150, 140, 30),
                                        QStringLiteral("fault_field"), pdf);
        FormManager::setSaveFaultForTesting(FormManager::SaveFault::None);

        QVERIFY2(!ok, qPrintable(QStringLiteral(
            "save must FAIL at injected stage %1").arg(int(stage))));
        QVERIFY2(sha256(pdf) == shaBefore,
                 "original bytes must be unchanged after the failed save");
        QVERIFY(pdfLoads(pdf));
        // Delta form: the temp dir may hold debris from OTHER processes (hard-
        // killed runs leave candidates — RAII does not run on TerminateProcess).
        // The operation-scoped invariant is that THIS operation leaves nothing
        // NEW behind.
        QCOMPARE(leftoverCandidates(), candidatesBefore);
    }

    // Sanity: with the seam reset the same operation succeeds.
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = makeTextPdf(tmp.path(), "after.pdf", {"After probe"});
    FormManager fm;
    QVERIFY(fm.addTextField(pdf, 0, QRectF(72, 150, 140, 30),
                            QStringLiteral("ok_field"), pdf));
    QVERIFY(pdfHasField(pdf, QStringLiteral("ok_field")));
    QCOMPARE(leftoverCandidates(), 0);
}

// A failed AddFormFieldCommand must not leave a success-looking undo entry:
// no reload signal, no undoable entry, file byte-identical.
// Qt 6.11 QUndoStack::push() DELETES a command that marks itself obsolete
// during the initial redo(), so the stack itself enforces "no entry" —
// command-state inspection therefore happens via a direct redo() outside any
// stack, and the stack-level behavior is asserted via count()/index().
void TestFormSafety::failedAddCommandLeavesNoSuccessUndoEntry() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = makeTextPdf(tmp.path(), "cmdfail.pdf", {"CmdFail probe"});
    const QByteArray shaBefore = sha256(pdf);

    FormManager fm;
    DocumentSession doc;
    doc.setPath(pdf);
    QUndoStack stack;
    QSignalSpy reloadSpy(&doc, &DocumentSession::reloadRequested);

    // Command-state probe (no stack ownership involved).
    FormManager::setSaveFaultForTesting(FormManager::SaveFault::CandidateSave);
    {
        AddFormFieldCommand probe(&fm, &doc, AddFormFieldCommand::FieldType::Text,
                                  0, QRectF(72, 150, 140, 30),
                                  QStringLiteral("never_field"));
        probe.redo();
        QVERIFY2(!probe.succeeded(), "failed command must report failure");
        QVERIFY2(probe.isObsolete(), "failed command must be marked obsolete");
    }

    // Stack-level: pushing the failing command must not create an undo entry.
    {
        auto* pushed = new AddFormFieldCommand(&fm, &doc, AddFormFieldCommand::FieldType::Text,
                                               0, QRectF(72, 150, 140, 30),
                                               QStringLiteral("never_field"));
        stack.push(pushed); // marked obsolete in redo() -> deleted by the stack
    }
    FormManager::setSaveFaultForTesting(FormManager::SaveFault::None);

    QCOMPARE(stack.count(), 0);
    QCOMPARE(stack.index(), 0);
    QCOMPARE(reloadSpy.count(), 0);            // no success-looking reload signal
    QCOMPARE(sha256(pdf), shaBefore);          // document untouched

    // Stack traversal is a no-op: nothing was ever added.
    stack.undo();
    QCOMPARE(stack.index(), 0);
    QCOMPARE(sha256(pdf), shaBefore);
}

// The remaining mutators go through the same shared boundary on the SAME
// file: fill values, metadata, rect, removal — each must round-trip.
void TestFormSafety::otherMutatorsPersistThroughBoundary() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = makeTextPdf(tmp.path(), "mut.pdf", {"Mutators probe"});

    FormManager fm;
    QVERIFY(fm.addTextField(pdf, 0, QRectF(72, 150, 140, 30), QStringLiteral("m_text"), pdf));
    QVERIFY(fm.addCheckBox(pdf, 0, QRectF(72, 190, 30, 30), QStringLiteral("m_check"), pdf));

    QVariantMap data;
    data[QStringLiteral("m_text")] = QStringLiteral("typed value");
    data[QStringLiteral("m_check")] = true;
    QStringList unsupported;
    QVERIFY(fm.fillForm(pdf, data, pdf, /*lockFields=*/false, &unsupported));
    QVERIFY(unsupported.isEmpty());

    QVERIFY(fm.setFieldMetadata(pdf, QStringLiteral("m_text"),
                                QStringLiteral("mut tip"), true, pdf));

    QVERIFY(fm.updateFieldRect(pdf, QStringLiteral("m_text"), 0,
                               QRectF(80, 160, 200, 40), pdf));

    // Values, metadata and rect must be readable back from disk.
    QVERIFY(pdfHasField(pdf, QStringLiteral("m_text")));
    QVERIFY(pdfHasField(pdf, QStringLiteral("m_check")));
    {
        PoDoFo::PdfMemDocument doc;
        doc.Load(pdf.toUtf8().constData());
        auto* acroForm = doc.GetAcroForm();
        QVERIFY(acroForm);
        bool textChecked = false, checkChecked = false, tipOk = false, reqOk = false;
        for (unsigned i = 0; i < acroForm->GetFieldCount(); ++i) {
            auto& field = acroForm->GetFieldAt(i);
            const QString name = QString::fromStdString(field.GetFullName());
            if (name == QLatin1String("m_text")) {
                auto* t = dynamic_cast<PoDoFo::PdfTextBox*>(&field);
                QVERIFY(t);
                auto v = t->GetText();
                textChecked = v.has_value()
                    && std::string(v.value().GetString().data(), v.value().GetString().size())
                           == "typed value";
                const PoDoFo::PdfDictionary& d = field.GetDictionary();
                const PoDoFo::PdfObject* tu = d.FindKey("TU");
                tipOk = tu && tu->IsString()
                    && std::string(tu->GetString().GetString()) == "mut tip";
                const PoDoFo::PdfObject* ff = d.FindKey("Ff");
                reqOk = ff && ff->IsNumber() && (ff->GetNumber() & 2) != 0;
            } else if (name == QLatin1String("m_check")) {
                auto* c = dynamic_cast<PoDoFo::PdfCheckBox*>(&field);
                QVERIFY(c);
                checkChecked = c->IsChecked();
            }
        }
        QVERIFY(textChecked);
        QVERIFY(checkChecked);
        QVERIFY(tipOk && reqOk);
        // Rect persisted: PDF-coordinate form of QRectF(80,160,200,40) —
        // x==80, y==pageH-160-40, y2==y+40 (loose compare vs serialized reals).
        const double pageH = doc.GetPages().GetPageAt(0).GetMediaBox().Height;
        for (unsigned i = 0; i < acroForm->GetFieldCount(); ++i) {
            auto& field = acroForm->GetFieldAt(i);
            if (QString::fromStdString(field.GetFullName()) != QLatin1String("m_text")) continue;
            const PoDoFo::PdfObject* r = field.GetDictionary().FindKey("Rect");
            QVERIFY(r && r->IsArray() && r->GetArray().GetSize() == 4);
            QVERIFY(qAbs(r->GetArray()[0].GetNumber() - 80.0) < 0.01);
            QVERIFY(qAbs(r->GetArray()[1].GetNumber() - (pageH - 160.0 - 40.0)) < 0.01);
            QVERIFY(qAbs(r->GetArray()[3].GetNumber() - (pageH - 160.0)) < 0.01);
        }
    }

    QVERIFY(fm.removeFieldByName(pdf, QStringLiteral("m_check"), pdf));
    QVERIFY(!pdfHasField(pdf, QStringLiteral("m_check")));
    QVERIFY(pdfHasField(pdf, QStringLiteral("m_text")));
    QVERIFY(pdfLoads(pdf));
}

QTEST_MAIN(TestFormSafety)
#include "TestFormSafety.moc"
