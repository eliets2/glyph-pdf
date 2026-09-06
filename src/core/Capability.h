// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QObject>
#include <QString>
#include <QVariant>
#include <QHash>
#include <QList>
#include <QPair>
#include <functional>

class QWidget;

namespace gp {

// ── U08: one capability registry ─────────────────────────────────────────────
// Answers "available? why not? what alternative?" for every workflow BEFORE
// work runs, replacing the five scattered probe idioms (Office-import runtime
// probe, post-hoc export-engine disclosure, static unsupported-pass label,
// inline OCR model-path probe, #ifdef + mock gating).
//
// RULE (in-tree contract, mirrors ConversionManager.h:37-41 "runtime — never
// baked at build time"): an `#ifdef` may only declare a capability
// UnavailableBuild (structurally absent from this binary). Runtime probes
// decide Available vs Degraded vs UnavailableRuntime — and must NEVER fake
// availability. Every Unavailable*/Degraded result carries a non-empty
// whyNot AND a non-empty alternative (enforced in query(): a lazy provider
// gets a generic explanation plus a qWarning so it is visible in test logs).
//
// Threading: probes run on the GUI thread and must be cheap; results are
// memoized per (id, param) and refreshed only via invalidate()/invalidateAll()
// (e.g. after "Download LibreOffice…" or a Preferences change). QSettings and
// other GUI-affine reads stay in the caller on the GUI thread (precedent:
// EditController.cpp "QSettings is not thread-safe").

enum class CapId {
    OfficeImport,             // LibreOffice soffice present (runtime probe)
    WordExport, ExcelExport, PptExport, CsvExport, HtmlExport, TextExport, ImageExport,
    PdfAExport, Linearize,
    OcrTesseract,             // Tesseract engine compiled in + usable
    OcrRapidModels,           // PP-OCRv5 ONNX models present (runtime probe)
    OcrEnsemble,              // Tesseract+RapidOCR ensemble — needs OcrRapidModels
    OcrLanguageData,          // per-language traineddata (param = UI code, "EN")
    CompressSubsetFonts,      // UnavailableBuild (R12) — no font subsetter
    CompressRemoveUnused,     // UnavailableBuild (R12) — no object GC
    MrcCompression,           // Degraded — needs OCR-pipeline page images first
    DigitalSignature,         // certificate-backed X.509/P12 signing (OpenSSL)
    VisibleSignatureGraphic,  // SignaturePicker Draw/Type/Upload graphic stamp
    PdfAValidation,           // veraPDF CLI present (runtime probe)
    COUNT
};

enum class Availability { Available, Degraded, UnavailableBuild, UnavailableRuntime };

struct Capability {
    Availability status = Availability::UnavailableBuild;
    QString whyNot;        // user-facing, one or two sentences
    QString alternative;   // actionable supported alternative (non-empty when not Available)
    QString detail;        // technical appendix: paths, the writer that will run, versions
};

QString r12UnsupportedPassExplanation(); // canonical R12 wording (CompressDialog delegates)
QString officeImportWhyNot();            // canonical LibreOffice import wording
QString officeImportAlternative();
QString mrcWhyNot();                     // canonical MRC fallback wording
QString mrcAlternative();
QString visibleSignatureKindDisclosure(); // U08: graphic stamp vs certificate-backed kind label

class CapabilityRegistry : public QObject {
    Q_OBJECT
public:
    // Probe returns the capability and fills whyNot/alternative/detail when
    // the result is not Available. `param` carries parameterized probes
    // (OcrLanguageData is keyed by the UI language code).
    using Probe = std::function<Capability(const QVariant& param)>;

    explicit CapabilityRegistry(QObject* parent = nullptr);

    // Engine owners register probes at boot (Bootstrapper::createContext calls
    // this next to engine construction); tests construct a standalone registry
    // and call it directly.
    void registerProbe(CapId id, Probe probe);
    void registerEngineProbes();

    // Cached per (id, param). Enforces the non-empty whyNot/alternative rule.
    Capability query(CapId id, const QVariant& param = {}) const;
    bool   available(CapId id, const QVariant& param = {}) const;

    // Re-probe on next query (Preferences change, "Download LibreOffice…"
    // returned, models installed mid-session). Emits capabilityChanged(id).
    void   invalidate(CapId id);
    void   invalidateAll();   // emits capabilityChanged per registered id

    // ── Pre-execution surfacing helpers ──────────────────────────────────────

    // Available: no-op. Degraded: keeps the widget enabled, tooltip = detail
    // (or whyNot + alternative when no detail). Unavailable*: setEnabled(false)
    // and tooltip = statusTip = whyNot + " " + alternative — the idiom already
    // pinned by TestCompressDialogHonesty for CompressDialog's checkboxes.
    void applyToWidget(QWidget* w, CapId id, const QVariant& param = {}) const;

    // Only clauses whose capability is Available/Degraded contribute their
    // "Label (*.ext)" clause, in caller order; a Degraded clause is visibly
    // marked " (limited)". Unavailable capabilities never reach the file
    // dialog. `allLabel` (optional) is prepended as "<allLabel>;;".
    QString fileFilterFor(const QList<QPair<QString, CapId>>& clauses,
                          const QString& allLabel = {}) const;

    // whyNot + " " + alternative with generic fallbacks (never returns an
    // empty string for an Unavailable/Degraded capability).
    static QString combineWhyNot(const Capability& c);

signals:
    void capabilityChanged(gp::CapId id);   // after invalidate / invalidateAll

private:
    QHash<int, Probe> m_probes;               // CapId -> probe
    mutable QHash<QString, Capability> m_cache; // "(id)\nparam" -> result

    static QString cacheKey(CapId id, const QVariant& param);
};

} // namespace gp

Q_DECLARE_METATYPE(gp::CapId)
