// SPDX-License-Identifier: Apache-2.0
// harness_a11y.cpp — W1 sweep S3: accessibility scan + fix surfaces.
//
// Surfaces under test:
//   gp::scanAccessibility(path)        (src/engines/AccessibilityChecker.cpp)
//   gp::applyAccessibilityFix(...)     (src/engines/AccessibilityFixes.cpp)
//
// Adversarial input classes (fixture corpus + byte mutations):
//   - /StructTreeRoot presence/absence, /MarkInfo /Marked weirdness, /Lang
//     as non-string, /Info /Title as non-string, /ViewerPreferences non-dict
//   - image XObjects with adversarial dicts; Form XObject CYCLES (the depth-8
//     cap must hold without crash)
//   - /Fields hierarchies with CYCLES and extreme depth (the checker's field
//     walk has no explicit depth cap — a stack overflow here is a FINDING)
//
// Per-input flow: bytes → scratch/in.pdf → scanAccessibility → for each fix
// kind (hostile params): copy to scratch/fix.pdf → applyAccessibilityFix.
// All writes stay in the driver's own scratch dir (SWEEP_SCRATCH).
#include <cstdio>
#include <string>

#include <QCoreApplication>
#include <QFile>
#include <QString>

#include <podofo/podofo.h>

#include "engines/AccessibilityChecker.h"
#include "engines/AccessibilityFixes.h"
#include "sweep_common.h"

using namespace gp;

namespace {

std::string scratchPath() {
    const char* env = getenv("SWEEP_SCRATCH");
    return env ? env : "sweep-scratch";
}

bool copyScratch(const std::string& from, const std::string& to) {
    QFile::remove(QString::fromStdString(to));
    return QFile::copy(QString::fromStdString(from),
                       QString::fromStdString(to));
}

const char* runOne(const std::vector<uint8_t>& data) {
    const std::string in = scratchPath() + "\\a11y-in.pdf";
    if (!sweep::writeFileBytes(in, data)) return "ERR scratch-unwritable";

    // ── detection surface (contract: scanAccessibility NEVER throws) ─────
    A11yReport rep;
    try {
        rep = scanAccessibility(QString::fromStdString(in));
    } catch (const PoDoFo::PdfError& e) {
        static std::string v;
        v = std::string("FINDING S3_SCAN_THREW_PODOFO: ") + e.what();
        return v.c_str();
    } catch (const std::exception& e) {
        static std::string v;
        v = std::string("FINDING S3_SCAN_THREW_STD: ") + e.what();
        return v.c_str();
    }
    if (rep.loadOk) {
        // Bounded-sample contract: reported never exceeds total, both are
        // capped at the named constants (AccessibilityChecker.h).
        if (rep.imagesReported > rep.imagesTotal
            || rep.fieldsReported > rep.fieldsTotal
            || rep.imagesReported > kA11yMaxImageFindings
            || rep.fieldsReported > kA11yMaxFieldFindings)
            return "FINDING S3_UNBOUNDED_FINDING_SAMPLE";
        for (const A11yFinding& f : rep.findings)
            if (f.checkId.isEmpty() || f.whyNot.isEmpty())
                return "FINDING S3_FINDING_MISSING_DISCLOSURE";
    }

    // ── fix surface, every kind, hostile parameters ──────────────────────
    static const std::string hugeLang(20000, 'L');
    static const std::string hugeAlt(100000, 'A');
    struct Req { A11yFixKind kind; int page; const char* a; const char* b; };
    const Req reqs[] = {
        { A11yFixKind::SetLanguage,         0, "en",       nullptr },
        { A11yFixKind::SetLanguage,         0, "\xF0\x9F\x98\x80\x01\x02", nullptr },
        { A11yFixKind::SetLanguage,         0, hugeLang.c_str(), nullptr },
        { A11yFixKind::EnableDisplayDocTitle, 0, nullptr, nullptr },
        { A11yFixKind::SetImageAltText,    -1, "Im0",      "alt" },
        { A11yFixKind::SetImageAltText,     0, "Im0",      "alt" },
        { A11yFixKind::SetImageAltText,  9999, "Im0",      "alt" },
        { A11yFixKind::SetImageAltText,     0, "..\\..\\Im0", "alt" },
        { A11yFixKind::SetImageAltText,     0, "",         "alt" },
        { A11yFixKind::SetImageAltText,     0, "Im0",      hugeAlt.c_str() },
        { A11yFixKind::SetFieldTu,          0, "a.b",      "tu" },
    };
    for (const Req& r : reqs) {
        const std::string target = scratchPath() + "\\a11y-fix.pdf";
        if (!copyScratch(in, target)) return "ERR copy-failed";
        A11yFixRequest fr;
        fr.kind = r.kind;
        fr.page = r.page;
        fr.resourceName = QString::fromUtf8(r.a);
        fr.language = QString::fromUtf8(r.a);
        fr.text = QString::fromUtf8(r.b ? r.b : "");
        fr.fieldName = QString::fromUtf8(r.a);
        // Contract (AccessibilityFixes.h): applyAccessibilityFix "never throws".
        A11yFixOutcome out;
        try {
            out = applyAccessibilityFix(QString::fromStdString(target), fr);
        } catch (const PoDoFo::PdfError& e) {
            static std::string v;
            v = std::string("FINDING S3_FIX_THREW_PODOFO kind=") + std::to_string((int)r.kind)
              + " page=" + std::to_string(r.page) + ": " + e.what();
            return v.c_str();
        } catch (const std::exception& e) {
            static std::string v;
            v = std::string("FINDING S3_FIX_THREW_STD kind=") + std::to_string((int)r.kind)
              + " page=" + std::to_string(r.page) + ": " + e.what();
            return v.c_str();
        }
        // ok or an honest refusal message — both fine; only crashes/timeouts
        // are findings. A claimed-ok fix must not leave the target unreadable.
        if (out.ok) {
            A11yReport rep2 = scanAccessibility(QString::fromStdString(target));
            if (!rep2.loadOk)
                return "FINDING S3_OK_FIX_LEFT_UNREADABLE_DOC";
        }
    }
    return rep.loadOk ? "OK" : "LOAD_ERROR";
}

}  // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    return sweep::driverMain(argc, argv, runOne);
}
