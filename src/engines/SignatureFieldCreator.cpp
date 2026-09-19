// SPDX-License-Identifier: Apache-2.0
// R26 send-for-signing P1 — see SignatureFieldCreator.h for the additive-exposure note.
#include "SignatureFieldCreator.h"

#include "SafeSave.h"
#include "core/PageSpaceTransform.h"

#include <podofo/podofo.h>

#include <QFile>
#include <QSet>

namespace gp {
namespace {

using PoDoFo::PdfMemDocument;
using PoDoFo::PdfField;

// Locate a field by full name on a freshly loaded document (first occurrence
// wins on illegal duplicate names — the same field a mutation would address;
// idiom verbatim from FormManager.cpp's findFieldByName).
const PdfField *findFieldByName(const PdfMemDocument &doc, const QString &name)
{
    auto *acroForm = doc.GetAcroForm();
    if (!acroForm) return nullptr;
    for (unsigned i = 0; i < acroForm->GetFieldCount(); ++i) {
        auto &f = acroForm->GetFieldAt(i);
        if (QString::fromStdString(f.GetFullName()) == name) return &f;
    }
    return nullptr;
}

QString pdfErrorText(const PoDoFo::PdfError &e)
{
    return QString::fromLatin1(e.what());
}

// Does the document carry a REAL signature (a signature field whose /V has a
// /ByteRange — the engine's own signed-field rule, SignatureManager's
// unsigned-field scan)? Only those fields pin file bytes: a full save
// re-serializes the document and moves every ByteRange window, so when a real
// signature exists the new fields MUST be appended as an incremental revision
// instead (PoDoFoBackend::writeUpdate E-08 discipline; the engine's own
// multi-signature flow appends the same way).
bool documentHasSignedFields(const PoDoFo::PdfMemDocument &doc)
{
    for (const auto *field : doc.GetFieldsIterator()) {
        if (field == nullptr || field->GetType() != PoDoFo::PdfFieldType::Signature)
            continue;
        const auto *sig = static_cast<const PoDoFo::PdfSignature *>(field);
        const PoDoFo::PdfObject *vObj = sig->GetDictionary().FindKey(PoDoFo::PdfName("V"));
        if (!vObj)
            continue;
        const PoDoFo::PdfObject *valObj = vObj;
        if (valObj->IsReference()) {
            valObj = &doc.GetObjects().MustGetObject(valObj->GetReference());
        }
        if (valObj && valObj->IsDictionary()
            && valObj->GetDictionary().HasKey(PoDoFo::PdfName("ByteRange")))
            return true;
    }
    return false;
}

} // namespace

bool SignatureFieldCreator::createSignatureFields(const QString &srcPath,
                                                  const QVector<Spec> &specs,
                                                  const QString &destPath,
                                                  QString *err)
{
    if (srcPath.isEmpty() || destPath.isEmpty()) {
        if (err) *err = QStringLiteral("source or destination path is empty");
        return false;
    }
    if (specs.isEmpty()) {
        if (err) *err = QStringLiteral("no signature fields requested");
        return false;
    }
    // Fail-loud name lint: empty or duplicate fully-qualified names would
    // silently bind two request entries to one field — the workflow's whole
    // honesty depends on 1 field == 1 signer.
    QSet<QString> seen;
    for (const Spec &s : specs) {
        if (s.fieldName.trimmed().isEmpty()) {
            if (err) *err = QStringLiteral("a requested signature field has an empty name");
            return false;
        }
        if (seen.contains(s.fieldName)) {
            if (err) *err = QStringLiteral("duplicate signature field name requested: %1")
                                 .arg(s.fieldName);
            return false;
        }
        seen.insert(s.fieldName);
        if (s.viewerRect.width() <= 0.0 || s.viewerRect.height() <= 0.0) {
            if (err) *err = QStringLiteral("signature field %1 has a non-positive rect")
                                 .arg(s.fieldName);
            return false;
        }
        if (s.pageIndex < 0) {
            if (err) *err = QStringLiteral("signature field %1 has an invalid page index")
                                 .arg(s.fieldName);
            return false;
        }
    }

    QString candidate;
    if (!SafeSave::makeUniqueCandidate(&candidate, err)) return false;
    auto dropCandidate = [&candidate]() { QFile::remove(candidate); };

    unsigned sourcePageCount = 0;
    try {
        PdfMemDocument doc;
        doc.Load(srcPath.toUtf8().constData());
        sourcePageCount = doc.GetPages().GetCount();

        for (const Spec &s : specs) {
            if (static_cast<unsigned>(s.pageIndex) >= doc.GetPages().GetCount()) {
                if (err) *err = QStringLiteral("page index %1 out of range (%2 pages)")
                                     .arg(s.pageIndex).arg(sourcePageCount);
                dropCandidate();
                return false;
            }
            PoDoFo::PdfPage &page = doc.GetPages().GetPageAt(s.pageIndex);
            // THE one shared page-space law (SEP13 L5/L8): viewer rect (top-left,
            // Y down, displayed size) → RAW USER space for the /Rect. Never
            // re-derive this flip locally.
            const QRectF userRect = PageSpace::viewerToUser(
                s.viewerRect, PageSpace::pageGeometry(page));
            auto &field = page.CreateField<PoDoFo::PdfSignature>(
                s.fieldName.toStdString(),
                PoDoFo::Rect(userRect.x(), userRect.y(),
                             userRect.width(), userRect.height()));
            // DEFENSE-IN-DEPTH (negative-control-verified no-op on PoDoFo
            // 1.1.0: CreateField<PdfSignature> creates NO /V — the value
            // object appears only at the real signing step via
            // EnsureValueObject). Other PoDoFo writer versions are known to
            // pre-create a placeholder /V (pre-sign beacon shape
            // /ByteRange[0 1234567890 …]); left in place, every engine
            // consumer would classify the fresh field as ALREADY SIGNED. The
            // honest prepared state is UNSIGNED: drop any placeholder /V (it
            // can sit on the field dict, the widget dict, or both) — and the
            // post-save validation below refuses the candidate if one
            // survived anyway.
            field.GetDictionary().RemoveKey(PoDoFo::PdfName("V"));
            if (auto *widget = field.GetWidget())
                widget->GetDictionary().RemoveKey(PoDoFo::PdfName("V"));
        }

        if (documentHasSignedFields(doc)) {
            // Incremental revision: stage the exact source bytes onto the
            // candidate first (the reservation is an empty file — drop it so
            // QFile::copy accepts the path, the engine's staging idiom), then
            // append the new-field revision on top of those bytes. The prior
            // signatures' ByteRange windows stay byte-identical — a full save
            // would invalidate them and the NEXT signing step's post-condition
            // would (honestly) refuse.
            QFile::remove(candidate);
            if (!QFile::copy(srcPath, candidate)) {
                if (err) *err = QStringLiteral("could not stage source bytes for the "
                                               "incremental field placement");
                dropCandidate();
                return false;
            }
            doc.SaveUpdate(candidate.toUtf8().constData());
        } else {
            doc.Save(candidate.toUtf8().constData());
        }
    } catch (const PoDoFo::PdfError &e) {
        if (err) *err = QStringLiteral("could not write the signature fields: %1")
                             .arg(pdfErrorText(e));
        dropCandidate();
        return false;
    } catch (const std::exception &e) {
        if (err) *err = QStringLiteral("could not write the signature fields: %1")
                             .arg(QString::fromLatin1(e.what()));
        dropCandidate();
        return false;
    }
    // `doc` is destroyed above: the PoDoFo writer is closed BEFORE the
    // candidate is validated and committed (R01 shape).

    // Reopen + validate: page count unchanged, every requested field present
    // and of type Signature. The commit is refused otherwise — the
    // destination is never replaced by unverified bytes.
    try {
        PdfMemDocument reopened;
        reopened.Load(candidate.toUtf8().constData());
        if (reopened.GetPages().GetCount() != sourcePageCount) {
            if (err) *err = QStringLiteral("candidate page count changed (%1 -> %2)")
                                 .arg(sourcePageCount)
                                 .arg(reopened.GetPages().GetCount());
            dropCandidate();
            return false;
        }
        for (const Spec &s : specs) {
            const PdfField *f = findFieldByName(reopened, s.fieldName);
            if (!f || f->GetType() != PoDoFo::PdfFieldType::Signature) {
                if (err) *err = QStringLiteral("candidate rejected: signature field %1 "
                                               "not present after save").arg(s.fieldName);
                dropCandidate();
                return false;
            }
            // The honest prepared state is UNSIGNED: no /V (placeholder or
            // otherwise) may survive the save — a placeholder /ByteRange makes
            // every engine consumer classify the field as already signed.
            const PoDoFo::PdfObject *vField = f->GetDictionary().FindKey(PoDoFo::PdfName("V"));
            const PoDoFo::PdfAnnotation *widget = f->GetWidget();
            const PoDoFo::PdfObject *vWidget =
                widget ? widget->GetDictionary().FindKey(PoDoFo::PdfName("V")) : nullptr;
            if (vField || vWidget) {
                if (err) *err = QStringLiteral("candidate rejected: signature field %1 "
                                               "still carries a placeholder signature value")
                                     .arg(s.fieldName);
                dropCandidate();
                return false;
            }
        }
    } catch (const PoDoFo::PdfError &e) {
        if (err) *err = QStringLiteral("candidate is not a valid PDF: %1").arg(pdfErrorText(e));
        dropCandidate();
        return false;
    }

    const bool ok = SafeSave::commitFileToDestination(candidate, destPath, err);
    if (!ok) dropCandidate();   // commit failed: the candidate is ours, drop it
    else QFile::remove(candidate);   // 5c8fd08 discipline: commit COPIES — never leak
    return ok;
}
} // namespace gp
