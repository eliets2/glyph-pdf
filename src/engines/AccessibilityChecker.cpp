// SPDX-License-Identifier: Apache-2.0
#include "AccessibilityChecker.h"

#include <podofo/podofo.h>

#include <QFile>

#include <set>
#include <string>

// ── T2-4 accessibility P1: DETECTION engine ──────────────────────────────────
//
// Scope discipline (see AccessibilityChecker.h): the checks below report
// what a TAGGED document needs. P1 explicitly does NOT build structure
// trees from page content (auto-tagging) and NEVER issues a PDF/UA
// conformance verdict — a findings-free report is "no gaps found by these
// checks", nothing more. The panel carries the same disclosure verbatim.
//
// Walk discipline: dictionary-level access through the existing repo idiom
// (PoDoFo 1.1.0's struct-tree API is getters only — construction and deep
// traversal are done here against raw dictionaries, as in exportPdfA).

using PoDoFo::PdfArray;
using PoDoFo::PdfDictionary;
using PoDoFo::PdfName;
using PoDoFo::PdfObject;
using PoDoFo::PdfReference;
using PoDoFo::PdfString;

namespace gp {
namespace {

// Resolve an indirect (or direct) object.
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

// First non-empty string from a dictionary text key.
QString stringAt(const PdfDictionary& dict, const char* key) {
    const PdfObject* o = dict.FindKey(PdfName(key));
    if (o == nullptr || !o->IsString()) return {};
    return QString::fromUtf8(o->GetString().GetString().data(),
                             static_cast<qsizetype>(o->GetString().GetString().size()));
}

bool isImageSubtype(const PdfDictionary& dict) {
    const PdfObject* sub = dict.FindKey(PdfName("Subtype"));
    return sub != nullptr && sub->IsName() && sub->GetName() == PdfName("Image");
}

// Recursive image walk over a page's /Resources /XObject dictionary. Also
// descends into Form XObjects (nested resources) with a visited set and a
// depth cap, so images hidden one level deep are not silently missed. The
// /Alt presence test is dictionary-level (ISO 32000-1 14.7.5.6 puts /Alt on
// the image XObject itself).
void collectImageGaps(PoDoFo::PdfMemDocument& doc, const PdfObject* resources,
                      int pageIdx, const QString& prefix, int depth,
                      std::set<PdfReference>& visitedImages,
                      A11yReport& report, bool pageNamedInWhere) {
    if (resources == nullptr || depth > 8) return;
    resources = resolve(resources, doc);
    if (resources == nullptr || !resources->IsDictionary()) return;

    const PdfObject* xobjs = resources->GetDictionary().FindKey(PdfName("XObject"));
    xobjs = resolve(xobjs, doc);
    if (xobjs == nullptr || !xobjs->IsDictionary()) return;

    for (const auto& entry : xobjs->GetDictionary()) {
        const PdfName& name = entry.first;
        const PdfObject* xo = resolve(&entry.second, doc);
        if (xo == nullptr || !xo->IsDictionary()) continue;
        const PdfDictionary& dict = xo->GetDictionary();

        if (isImageSubtype(dict)) {
            const PdfObject* alt = dict.FindKey(PdfName("Alt"));
            const bool hasAlt = alt != nullptr && alt->IsString()
                                && !stringAt(dict, "Alt").isEmpty();
            if (hasAlt) continue;  // healed / not defective

            // Totals count DEFECTIVE items only, so truncated() reads
            // "findings were capped", never "healthy items exist".
            report.imagesTotal++;
            if (static_cast<int>(visitedImages.size()) < 100000)
                visitedImages.insert(xo->GetIndirectReference());

            if (report.imagesReported < kA11yMaxImageFindings) {
                A11yFinding f;
                f.checkId = QStringLiteral("image-alt");
                f.severity = A11ySeverity::Medium;
                f.page = pageIdx;
                f.where = (pageNamedInWhere
                               ? QStringLiteral("page %1 · image %2%3")
                                     .arg(pageIdx + 1)
                                     .arg(prefix)
                                     .arg(QString::fromLatin1(name.GetString().data(),
                                          static_cast<qsizetype>(name.GetString().size())))
                               : QStringLiteral("image %1%2")
                                     .arg(prefix)
                                     .arg(QString::fromLatin1(name.GetString().data(),
                                          static_cast<qsizetype>(name.GetString().size()))));
                f.whyNot = QObject::tr(
                    "this image has no /Alt text — a screen reader announces "
                    "nothing for it (not auto-repairable: the description "
                    "must come from you)");
                report.findings.append(f);
                report.imagesReported++;
            }
        } else {
            // Form XObject → descend into its own /Resources.
            const PdfObject* sub = dict.FindKey(PdfName("Subtype"));
            if (sub != nullptr && sub->IsName()
                && sub->GetName() == PdfName("Form")) {
                collectImageGaps(doc, dict.FindKey(PdfName("Resources")), pageIdx,
                                 prefix + QString::fromLatin1(name.GetString().data(),
                                          static_cast<qsizetype>(name.GetString().size()))
                                     + QLatin1Char('/'),
                                 depth + 1, visitedImages, report, pageNamedInWhere);
            }
        }
    }
}

// Terminal fields are the dicts carrying /FT (PDF 32000-1 12.7.3.2); nodes
// without /FT are pure hierarchy containers → recurse into /Kids. /TU is
// expected on the FIELD dict (for radio groups: the parent), so a /FT dict
// with kids is still checked at its own level.
void collectFieldGaps(PoDoFo::PdfMemDocument& doc, const PdfObject* fieldsArray,
                      const QString& parentName, A11yReport& report) {
    if (fieldsArray == nullptr) return;
    fieldsArray = resolve(fieldsArray, doc);
    if (fieldsArray == nullptr || !fieldsArray->IsArray()) return;

    for (const PdfObject& kidObj : fieldsArray->GetArray()) {
        const PdfObject* kid = resolve(&kidObj, doc);
        if (kid == nullptr || !kid->IsDictionary()) continue;
        const PdfDictionary& dict = kid->GetDictionary();

        const QString ownName = stringAt(dict, "T");
        const QString fullName = parentName.isEmpty()
                                     ? ownName
                                     : parentName + QLatin1Char('.') + ownName;

        const PdfObject* ft = dict.FindKey(PdfName("FT"));
        if (ft != nullptr && stringAt(dict, "TU").isEmpty()) {
            // Defective field — totals count defective only (symmetric with
            // the image walk).
            report.fieldsTotal++;
            if (report.fieldsReported < kA11yMaxFieldFindings) {
                A11yFinding f;
                f.checkId = QStringLiteral("field-tu");
                f.severity = A11ySeverity::Medium;
                f.page = -1;
                f.where = QStringLiteral("field \"%1\"")
                              .arg(fullName.isEmpty()
                                       ? QStringLiteral("(unnamed)")
                                       : fullName);
                f.whyNot = QObject::tr(
                    "this form field has no /TU alternate name — screen "
                    "readers cannot say what the field is for");
                report.findings.append(f);
                report.fieldsReported++;
            }
        } else {
            collectFieldGaps(doc, dict.FindKey(PdfName("Kids")), fullName, report);
        }
    }
}

} // namespace

A11yReport scanAccessibility(const QString& path) {
    A11yReport r;
    r.path = path;

    if (path.isEmpty()) {
        r.loadError = QStringLiteral("no document");
        return r;
    }

    PoDoFo::PdfMemDocument doc;
    try {
        doc.Load(path.toUtf8().constData());
    } catch (const PoDoFo::PdfError& e) {
        r.loadError = QString::fromUtf8(e.what());
        return r;
    } catch (const std::exception& e) {
        r.loadError = QString::fromUtf8(e.what());
        return r;
    }

    r.loadOk = true;
    auto& catalog = doc.GetCatalog().GetDictionary();

    // 1) Structure tree — the tagged-document gate. Detection only: P1 does
    //    not construct /StructTreeRoot (no auto-tagging).
    const PdfObject* structRoot = resolve(catalog.FindKey(PdfName("StructTreeRoot")), doc);
    if (structRoot == nullptr || !structRoot->IsDictionary()) {
        A11yFinding f;
        f.checkId = QStringLiteral("struct-tree");
        f.severity = A11ySeverity::High;
        f.page = -1;
        f.where = QStringLiteral("document");
        f.whyNot = QObject::tr(
            "untagged: there is no /StructTreeRoot, so assistive technology "
            "has no reading order, headings or table structure to navigate — "
            "content tagging is not yet available in this version");
        r.findings.append(f);

        // /MarkInfo /Marked true must accompany a structure tree; without
        // the tree it is meaningless, so only checked when tagged.
    } else {
        r.tagged = true;
        const PdfObject* markInfo = resolve(catalog.FindKey(PdfName("MarkInfo")), doc);
        const PdfObject* marked =
            markInfo != nullptr && markInfo->IsDictionary()
                ? markInfo->GetDictionary().FindKey(PdfName("Marked"))
                : nullptr;
        if (marked == nullptr || !marked->IsBool() || !marked->GetBool()) {
            A11yFinding f;
            f.checkId = QStringLiteral("struct-tree");
            f.severity = A11ySeverity::Low;
            f.page = -1;
            f.where = QStringLiteral("document");
            f.whyNot = QObject::tr(
                "the document has a structure tree but /MarkInfo /Marked is "
                "not true — processors cannot rely on the tagging being "
                "complete");
            r.findings.append(f);
        }
    }

    // 2) Document language — /Lang in the catalog.
    if (stringAt(catalog, "Lang").isEmpty()) {
        A11yFinding f;
        f.checkId = QStringLiteral("doc-language");
        f.severity = A11ySeverity::Medium;
        f.page = -1;
        f.where = QStringLiteral("document");
        f.whyNot = QObject::tr(
            "no /Lang in the document catalog — screen readers pick the "
            "wrong voice and spell-checking fails");
        r.findings.append(f);
    }

    // 3) Display title — /Info /Title AND /ViewerPreferences /DisplayDocTitle
    //    true. Two gaps, two findings: they have separate remedies (set a
    //    title; then let the window show it).
    QString title;
    {
        const PdfObject* infoObj =
            resolve(doc.GetTrailer().GetDictionary().FindKey(PdfName("Info")), doc);
        if (infoObj != nullptr && infoObj->IsDictionary())
            title = stringAt(infoObj->GetDictionary(), "Title");
    }
    if (title.isEmpty()) {
        A11yFinding f;
        f.checkId = QStringLiteral("doc-title");
        f.severity = A11ySeverity::Medium;
        f.page = -1;
        f.where = QStringLiteral("document");
        f.whyNot = QObject::tr(
            "the document has no /Title — window title and screen-reader "
            "summary fall back to the file name (set it under Document "
            "Properties)");
        r.findings.append(f);
    }

    bool displayDocTitle = false;
    {
        const PdfObject* vp = resolve(catalog.FindKey(PdfName("ViewerPreferences")), doc);
        const PdfObject* ddt =
            vp != nullptr && vp->IsDictionary()
                ? vp->GetDictionary().FindKey(PdfName("DisplayDocTitle"))
                : nullptr;
        displayDocTitle = ddt != nullptr && ddt->IsBool() && ddt->GetBool();
    }
    if (!displayDocTitle) {
        A11yFinding f;
        f.checkId = QStringLiteral("display-doc-title");
        f.severity = A11ySeverity::Medium;
        f.page = -1;
        f.where = QStringLiteral("document");
        f.whyNot = QObject::tr(
            "/ViewerPreferences /DisplayDocTitle is not true — viewers show "
            "the file name instead of the document title");
        r.findings.append(f);
    }

    // 4) Image /Alt — bounded sample across pages (resources walk).
    std::set<PdfReference> visitedImages;
    const int pageCount = static_cast<int>(doc.GetPages().GetCount());
    for (int i = 0; i < pageCount; ++i) {
        try {
            PoDoFo::PdfPage& page = doc.GetPages().GetPageAt(i);
            collectImageGaps(doc, &page.GetResources().GetObject(), i,
                             QString(), 0, visitedImages, r, /*pageNamedInWhere=*/true);
        } catch (const PoDoFo::PdfError&) {
            // A broken page is not an accessibility finding of its own —
            // keep scanning the rest.
        }
    }

    // 5) Field /TU — bounded sample over the AcroForm hierarchy.
    const PdfObject* acro = resolve(catalog.FindKey(PdfName("AcroForm")), doc);
    if (acro != nullptr && acro->IsDictionary())
        collectFieldGaps(doc, acro->GetDictionary().FindKey(PdfName("Fields")),
                         QString(), r);

    return r;
}

} // namespace gp
