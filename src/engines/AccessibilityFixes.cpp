// SPDX-License-Identifier: Apache-2.0
#include "AccessibilityFixes.h"

#include "SafeSave.h"

#include <QFile>
#include <podofo/podofo.h>

// ── T2-4 accessibility P1: the CHEAP STRUCTURAL FIXES ────────────────────────
//
// Transaction shape (SafeSave, same as every mutation): load the CURRENT
// destination → mutate in memory → save a unique temp candidate → validate
// the candidate with an INDEPENDENT PoDoFo reopen (the pin for "the key
// really landed") → checked atomic commit. The original is untouched unless
// the validated candidate commits. Viewer-held handles are coordinated via
// the shell-installed SafeSave coordinator (ScopedFileHandleCoordination).
//
// /TU is intentionally NOT written here: it goes through the existing
// FormManager::setFieldMetadata seam (see header).

using PoDoFo::PdfDictionary;
using PoDoFo::PdfName;
using PoDoFo::PdfObject;
using PoDoFo::PdfString;

namespace gp {
namespace {

const PdfObject* resolve(const PdfObject* obj, PoDoFo::PdfMemDocument& doc) {
    if (obj == nullptr) return nullptr;
    if (obj->IsReference()) {
        try {
            return &doc.GetObjects().MustGetObject(obj->GetReference());
        } catch (const PoDoFo::PdfError&) {
            return nullptr;
        }
    }
    return obj;
}

QString stringAt(const PdfDictionary& dict, const char* key) {
    const PdfObject* o = dict.FindKey(PdfName(key));
    if (o == nullptr || !o->IsString()) return {};
    return QString::fromUtf8(o->GetString().GetString().data(),
                             static_cast<qsizetype>(o->GetString().GetString().size()));
}

// Independent candidate validation: reopen the candidate from disk and
// verify the expected key/value. Returns empty when valid, else a reason.
QString validateCandidate(const QString& candidate, const A11yFixRequest& req) {
    try {
        PoDoFo::PdfMemDocument doc;
        doc.Load(candidate.toUtf8().constData());

        switch (req.kind) {
            case A11yFixKind::SetLanguage: {
                const PdfObject* lang =
                    doc.GetCatalog().GetDictionary().FindKey(PdfName("Lang"));
                if (lang == nullptr || !lang->IsString()
                    || stringAt(doc.GetCatalog().GetDictionary(), "Lang") != req.language)
                    return QStringLiteral("/Lang missing on candidate");
                break;
            }
            case A11yFixKind::EnableDisplayDocTitle: {
                const PdfObject* vp = resolve(
                    doc.GetCatalog().GetDictionary().FindKey(PdfName("ViewerPreferences")),
                    doc);
                const PdfObject* ddt =
                    vp != nullptr && vp->IsDictionary()
                        ? vp->GetDictionary().FindKey(PdfName("DisplayDocTitle"))
                        : nullptr;
                if (ddt == nullptr || !ddt->IsBool() || !ddt->GetBool())
                    return QStringLiteral("/DisplayDocTitle not true on candidate");
                break;
            }
            case A11yFixKind::SetImageAltText: {
                if (req.page < 0
                    || req.page >= static_cast<int>(doc.GetPages().GetCount()))
                    return QStringLiteral("page out of range on candidate");
                auto& page = doc.GetPages().GetPageAt(static_cast<unsigned>(req.page));
                const PdfObject* xobjs = resolve(
                    page.GetResources().GetObject().GetDictionary().FindKey(PdfName("XObject")),
                    doc);
                const PdfObject* img =
                    xobjs != nullptr && xobjs->IsDictionary()
                        ? xobjs->GetDictionary().FindKey(PdfName(req.resourceName.toStdString()))
                        : nullptr;
                img = resolve(img, doc);
                const PdfObject* alt =
                    img != nullptr && img->IsDictionary()
                        ? img->GetDictionary().FindKey(PdfName("Alt"))
                        : nullptr;
                if (alt == nullptr || !alt->IsString())
                    return QStringLiteral("/Alt missing on candidate image");
                break;
            }
            case A11yFixKind::SetFieldTu:
                return {};   // validated by the FormManager seam's own reopen
        }
    } catch (const PoDoFo::PdfError& e) {
        return QString::fromUtf8(e.what());
    } catch (const std::exception& e) {
        return QString::fromUtf8(e.what());
    }
    return {};
}

} // namespace

A11yFixOutcome applyAccessibilityFix(const QString& path,
                                     const A11yFixRequest& request) {
    A11yFixOutcome out;

    if (path.isEmpty()) {
        out.message = QStringLiteral("no document");
        return out;
    }
    if (request.kind == A11yFixKind::SetLanguage && request.language.trimmed().isEmpty()) {
        out.message = QStringLiteral("no language chosen");
        return out;
    }

    try {
        QString candidate, err;
        if (!SafeSave::makeUniqueCandidate(&candidate, &err)) {
            out.message = err;
            return out;
        }

        // ── SafeSave transaction: candidate → validate → atomic commit ──
        {
            // Release the viewer's OS handle for THIS destination only (the
            // atomic rename needs delete access); restore on every outcome.
            SafeSave::ScopedFileHandleCoordination scope(path);
            {
                // IMPORTANT: the loaded document keeps its input device open
                // until destroyed. It must die BEFORE the commit — commitFile
                // is an atomic rename over the destination, denied while any
                // handle (even our own) holds the file. Refusal paths remove
                // the reserved candidate and report honestly.
                PoDoFo::PdfMemDocument doc;
                try {
                    doc.Load(path.toUtf8().constData());
                } catch (const PoDoFo::PdfError& e) {
                    QFile::remove(candidate);
                    out.message = QString::fromUtf8(e.what());
                    return out;
                } catch (const std::exception& e) {
                    QFile::remove(candidate);
                    out.message = QString::fromUtf8(e.what());
                    return out;
                }

                auto& catalog = doc.GetCatalog().GetDictionary();

                switch (request.kind) {
                    case A11yFixKind::SetLanguage: {
                        catalog.AddKey(PdfName("Lang"),
                                       PdfObject(PdfString(request.language.toStdString())));
                        out.message = QStringLiteral("document language set to %1")
                                          .arg(request.language);
                        break;
                    }
                    case A11yFixKind::EnableDisplayDocTitle: {
                        // "title from /Info": the fix exists to flip the
                        // DISPLAY flag; it refuses honestly when there is no
                        // title to display.
                        QString title;
                        const PdfObject* infoObj = resolve(
                            doc.GetTrailer().GetDictionary().FindKey(PdfName("Info")),
                            doc);
                        if (infoObj != nullptr && infoObj->IsDictionary())
                            title = stringAt(infoObj->GetDictionary(), "Title");
                        if (title.isEmpty()) {
                            QFile::remove(candidate);
                            out.message = QStringLiteral(
                                "no /Info /Title in this document — set a title "
                                "under Document Properties first, then enable "
                                "display");
                            return out;
                        }
                        auto& vp = doc.GetObjects().CreateDictionaryObject();
                        vp.GetDictionary().AddKey(PdfName("DisplayDocTitle"),
                                                  PdfObject(true));
                        catalog.AddKey(PdfName("ViewerPreferences"),
                                       vp.GetIndirectReference());
                        out.message = QStringLiteral("display title enabled (\"%1\")")
                                          .arg(title);
                        break;
                    }
                    case A11yFixKind::SetImageAltText: {
                        if (request.page < 0
                            || request.page
                                   >= static_cast<int>(doc.GetPages().GetCount())) {
                            QFile::remove(candidate);
                            out.message = QStringLiteral("page %1 not in document")
                                              .arg(request.page + 1);
                            return out;
                        }
                        auto& page = doc.GetPages().GetPageAt(
                            static_cast<unsigned>(request.page));
                        // Resolve the /XObject dictionary (direct or indirect)
                        // to a MUTABLE object — /Alt is written into the
                        // named entry.
                        PdfObject* xobjMut = nullptr;
                        {
                            PdfObject* direct =
                                page.GetResources().GetObject().GetDictionary().FindKey(
                                    PdfName("XObject"));
                            if (direct && direct->IsDictionary())
                                xobjMut = direct;
                            else if (direct && direct->IsReference())
                                xobjMut = &doc.GetObjects().MustGetObject(
                                    direct->GetReference());
                        }
                        if (xobjMut == nullptr || !xobjMut->IsDictionary()) {
                            QFile::remove(candidate);
                            out.message = QStringLiteral("page %1 has no /XObject resources")
                                              .arg(request.page + 1);
                            return out;
                        }
                        PdfObject* img = xobjMut->GetDictionary().FindKey(
                            PdfName(request.resourceName.toStdString()));
                        if (img && img->IsReference())
                            img = &doc.GetObjects().MustGetObject(img->GetReference());
                        if (img == nullptr || !img->IsDictionary()) {
                            QFile::remove(candidate);
                            out.message = QStringLiteral("no image named %1 on page %2")
                                              .arg(request.resourceName)
                                              .arg(request.page + 1);
                            return out;
                        }
                        img->GetDictionary().AddKey(
                            PdfName("Alt"),
                            PdfObject(PdfString(request.text.toStdString())));
                        out.message = QStringLiteral("/Alt added to image %1 (page %2)")
                                          .arg(request.resourceName)
                                          .arg(request.page + 1);
                        break;
                    }
                    case A11yFixKind::SetFieldTu: {
                        // Routed to the existing FormManager seam by the
                        // shell's fix runner — never written here.
                        QFile::remove(candidate);
                        out.message = QStringLiteral(
                            "/TU goes through the form-field mutation seam");
                        return out;
                    }
                }

                try {
                    // PoDoFo 1.1.0 Save returns void and throws PdfError on
                    // failure — the catch removes the candidate.
                    doc.Save(candidate.toUtf8().constData());
                } catch (...) {
                    QFile::remove(candidate);
                    out.message = QStringLiteral("candidate write failed");
                    return out;
                }
            }
            // `doc` is destroyed here — its input device is closed, the
            // destination is renameable.

            const QString invalid = validateCandidate(candidate, request);
            if (!invalid.isEmpty()) {
                QFile::remove(candidate);
                out.message = QStringLiteral("candidate rejected: %1").arg(invalid);
                return out;
            }
            if (!SafeSave::commitFileToDestination(candidate, path, &err)) {
                QFile::remove(candidate);
                out.message = err;
                return out;
            }
        }
        out.ok = true;
        return out;
    } catch (const PoDoFo::PdfError& e) {
        out.message = QString::fromUtf8(e.what());
        return out;
    } catch (const std::exception& e) {
        out.message = QString::fromUtf8(e.what());
        return out;
    }
}

bool readFieldRequiredFlag(const QString& path, const QString& fieldName,
                           bool* required, QString* err) {
    if (required) *required = false;
    if (fieldName.isEmpty()) {
        if (err) *err = QStringLiteral("no field name");
        return false;
    }
    try {
        PoDoFo::PdfMemDocument doc;
        doc.Load(path.toUtf8().constData());
        auto* acroForm = doc.GetAcroForm();
        if (acroForm == nullptr) {
            if (err) *err = QStringLiteral("no AcroForm in document");
            return false;
        }
        for (unsigned i = 0; i < acroForm->GetFieldCount(); ++i) {
            auto& field = acroForm->GetFieldAt(i);
            if (QString::fromStdString(field.GetFullName()) != fieldName) continue;
            const PdfObject* ff =
                field.GetObject().GetDictionary().FindKey(PdfName("Ff"));
            const int flags =
                (ff != nullptr && ff->IsNumber()) ? static_cast<int>(ff->GetNumber()) : 0;
            if (required) *required = (flags & (1 << 1)) != 0;
            return true;
        }
        if (err) *err = QStringLiteral("field not found: %1").arg(fieldName);
        return false;
    } catch (const PoDoFo::PdfError& e) {
        if (err) *err = QString::fromUtf8(e.what());
        return false;
    } catch (const std::exception& e) {
        if (err) *err = QString::fromUtf8(e.what());
        return false;
    }
}

} // namespace gp
