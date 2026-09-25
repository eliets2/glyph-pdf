// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QList>
#include <QDateTime>

namespace gp {

class CapabilityRegistry;
struct Capability;

// ── Named batch presets (R26, docs/research/batch-presets-implementation-plan.md) ─
//
// A named batch preset is a user-created, locally-stored, JSON-serialized,
// ordered list of PDF operations with per-op parameters (plan §1.1). One file
// per preset at <AppDataLocation>/presets/<id>.glyphpreset.json so import/
// export is a file copy and QSettings never becomes the source of truth
// (plan §2.1 — the deliberate divergence from the ExportPresetsPanel
// QSettings precedent).
//
// SCHEMA POLICY — FAIL-CLOSED with a version handshake (plan §2.4): unknown
// op ids, unknown param keys, wrong-typed or out-of-range values and unknown
// keys anywhere are REJECTED with a precise diagnostic naming the JSON path,
// the offending value, the file's schemaVersion and what this build supports.
// Tolerant-Reader metadata preservation was considered and rejected (plan
// §2.4): a preset is an imperative work order over user files — silently
// dropping a step would make the run do something its author never specified.
// A newer schema is refused with an actionable upgrade message, never
// misparsed. Values that schema v1 grammar allows but THIS BUILD does not
// implement (onConflict "rename", onFileFailure "stop") are refused with an
// explicit not-implemented diagnostic — never silently reinterpreted.
//
// Presets are DATA (plan §5.1): the schema carries no paths, no commands,
// no URIs and no script of any kind. Destinations are chosen at run time.
struct BatchPresetStep {
    QString op;             // one of BatchPresetSchema::knownOps()
    QString label;          // optional user-facing label (<= 120 chars)
    QVariantMap params;     // validated against the op's param spec

    bool operator==(const BatchPresetStep& other) const {
        return op == other.op && label == other.label && params == other.params;
    }
};

struct BatchPreset {
    int       schemaVersion = 1;
    QString   id;           // [a-z0-9-]{1,64}; == filename stem; stable reference
    QString   name;         // display name, 1..80 chars after trim; renames do NOT touch id
    QString   description;  // optional, <= 300 chars
    QDateTime created;
    QDateTime modified;     // UTC; refreshed on every save
    QString   authorApp;    // informational, round-trips verbatim, never parsed
    QString   minAppVersion;// optional semver; older running app may load but not run
    QList<BatchPresetStep> steps;
    QString   outputNaming; // "" = default "{basename}_{preset}.pdf"; tokens {basename},{preset},{n},{date}
    QString   onConflict;   // "ask" (default) | "overwrite" | "rename".
                            // W1-01: "overwrite" never bypasses the run-time
                            // AR-8 overwrite confirmation — it maps to the
                            // same interactive "ask" path (decline cancels);
                            // there is no silent overwrite.
                            // "rename" (P2 plan §4.3) never overwrites at
                            // all: the output de-conflicts to stem-2/3…,
                            // every candidate re-checked through the
                            // resolveNaming containment guard.
    QString   onFileFailure;// "continue" (default) | "stop" (P2 plan §4.4: halt at
                            // the next FILE BOUNDARY after a file-scoped failure —
                            // never mid-file, never mid-chain; every not-yet-run
                            // file is reported as not-run with the policy reason,
                            // never success, never silently dropped).

    bool operator==(const BatchPreset& other) const {
        return schemaVersion == other.schemaVersion && id == other.id
            && name == other.name && description == other.description
            && created == other.created && modified == other.modified
            && authorApp == other.authorApp && minAppVersion == other.minAppVersion
            && steps == other.steps && outputNaming == other.outputNaming
            && onConflict == other.onConflict && onFileFailure == other.onFileFailure;
    }
};

// The schema vocabulary + pure validation/codec helpers. All functions are
// GUI-free and unit-testable without a store on disk.
namespace BatchPresetSchema {

// Only version 1 is accepted by this build (plan §2.2 field contract).
inline constexpr int kSchemaVersion = 1;
inline constexpr int kMaxSteps = 16;
inline constexpr qint64 kMaxFileBytes = 256 * 1024;         // V9 resource cap
inline constexpr qint64 kMaxStringParamBytes = 1024 * 1024; // V9 resource cap

// The op ids this build implements (plan §1.3 op table). `bates` joined in
// P2: a bates-bearing preset runs on the ORDERED lane (BatchMode's sequential
// worker), so run-ordered cross-file continuity is a loop invariant rather
// than scheduling luck (P2 plan §4.2) — kSchemaVersion stays 1.
QStringList knownOps();
bool isKnownOp(const QString& op);

// Slugified id from a display name: [a-z0-9-]{1,64}; collisions are the
// caller's problem (BatchPresetStore::save assigns a fresh unique id).
QString idFromName(const QString& name);

// The default output-naming template (plan §2.2: "{basename}_{preset}.pdf").
QString defaultNamingTemplate();

// Resolve the naming template to a FILE NAME (no directory components).
// Tokens: {basename} (input stem), {preset} (preset id), {n} (1-based file
// index), {date} (ISO run date). Unknown {…} tokens are rejected (V7) and
// every replacement value is filename-sanitized so no path separator can be
// smuggled through a token (plan §3.6). The template itself must end ".pdf".
// W1-01: the RESOLVED result is containment-checked — it must be a single
// bare path component (no '/', '\', ':', no '.'/'..'), so the caller's
// QDir(outDir).filePath(result) can never leave the user-chosen output
// directory. Hostile templates are refused here, which fixes parse()-time
// validation, the GUI overwrite pre-check and the worker's captured output
// through the same guard.
bool resolveNaming(const QString& naming, const QString& basename,
                   const QString& presetId, int fileIndex, const QDate& runDate,
                   QString* outName, QString* err);

// R26-P2 (plan §4.3): the rename de-conflict NAMING rule — `stem-2.pdf`,
// `stem-3.pdf`, … (Windows Explorer semantics; the same idiom
// BatchPresetStore::save uses for ids). `resolvedName` must be a rendered
// resolveNaming result; returns an empty string when it carries separators
// or drive syntax, does not end ".pdf", or the de-conflicted stem would be
// empty. An already-renamed name continues the chain ("x-2.pdf" occupied
// next -> "x-3.pdf"). The caller re-checks EVERY candidate through
// resolveNaming — the W1-01 containment choke point — before using it, so a
// renamed output obeys exactly the same rules (bare component, no device
// names, bounded length) as the template render.
QString renameCandidate(const QString& resolvedName, int attempt);

// Pure semver-ish compare of "MAJOR[.MINOR[.PATCH]]" strings: -1 / 0 / 1.
// Missing components compare as 0; non-numeric tails compare as 0.
int compareVersions(const QString& a, const QString& b);

} // namespace BatchPresetSchema

// Validation + canonical JSON codec. parse()/loadFile() validate (V1–V9 of
// the plan) — a parsed BatchPreset is always runnable-shape; validate()
// re-checks an in-memory preset (the editor/store path cannot produce an
// invalid file, hand-edited files are re-validated on load).
namespace BatchPresetCodec {

// Canonical byte-stable serialization (QJsonDocument::Indented). Fields that
// hold their default value are omitted so the golden fixture is stable.
QByteArray serialize(const BatchPreset& preset);

// Parse + validate. `err` (when non-null) receives the precise diagnostic:
// "<json path>: <problem> (schema v<N> file; this build supports v1: …)".
bool parse(const QByteArray& json, BatchPreset* out, QString* err);

// parse() + the V8 import rule: the `id` must equal the file's stem
// (<stem>.glyphpreset.json) so a renamed copy is refused, not silently
// re-keyed. Also enforces the V9 file-size cap before parsing.
bool loadFile(const QString& path, BatchPreset* out, QString* err);

// Validate an in-memory preset (store.save gate). Same diagnostics as parse.
bool validate(const BatchPreset& preset, QString* err);

} // namespace BatchPresetCodec

// ── Capability honesty (plan §5.2) ───────────────────────────────────────────
// Maps a preset step onto the ONE capability registry. Ops implemented purely
// by the built-in PDF editor engine (compress, watermark, redact,
// strip-metadata) are Available by construction; registry-gated ops are
// queried through `registry` (null registry → an honest UnavailableRuntime
// answer for gated ops — a step whose availability cannot be probed must not
// claim availability). The run-time refusal and the design-time display both
// consume this mapping, so a disclosed-unavailable step is never silently
// skipped and never fakes success.
Capability batchPresetStepCapability(const BatchPresetStep& step,
                                     const CapabilityRegistry* registry);

// ── Local store: one file per preset (plan §2.1) ─────────────────────────────
class BatchPresetStore {
public:
    // `rootDir` empty → defaultRootDir() (AppDataLocation/presets). Tests pass
    // an explicit temp directory (settings isolation).
    explicit BatchPresetStore(const QString& rootDir = QString());

    static QString defaultRootDir();   // <AppDataLocation>/presets

    // Valid presets, sorted by display name (case-insensitive).
    QList<BatchPreset> list() const;

    // Honesty surface: files in the root dir this build REFUSES to load, with
    // the precise diagnostic. A broken preset file is never silently hidden —
    // the manager discloses it so the user can delete or fix it.
    struct BrokenFile { QString path; QString error; };
    QList<BrokenFile> brokenFiles() const;

    bool   contains(const QString& id) const;
    bool   get(const QString& id, BatchPreset* out, QString* err) const;

    // Validate → assign id (slug from name, de-conflicted with -2, -3 …) and
    // created/modified stamps → write atomically (QSaveFile). A rejected save
    // leaves the store unchanged.
    bool   save(BatchPreset* preset, QString* err);

    // Rename edits the DISPLAY NAME only — the id (and filename) stay stable
    // (plan §2.2: ids are stable references). Sets `modified`.
    bool   rename(const QString& id, const QString& newName, QString* err);

    // Delete by id. Returns false (store unchanged) when the file is absent.
    bool   remove(const QString& id, QString* err);

    // ── R26-P2 U6 (plan §4.7): import/export as validated atomic copies ──────
    // Import: loadFile() (V1–V9 + the V8 id==stem rule) validates the file
    // BEFORE anything appears in the store; the store file is then written
    // atomically through the canonical codec (validate→serialize — the store
    // path cannot produce an invalid file). An id already in the store is
    // NEVER silently replaced: the import refuses with the existing-id
    // diagnostic unless `replaceExisting` is true (the manager's post-confirm
    // action; the confirm itself is GUI, the store stays GUI-free). A failed
    // import leaves the store unchanged. `importedId` receives the id on
    // success.
    bool   importFrom(const QString& path, bool replaceExisting,
                      QString* err, QString* importedId = nullptr);

    // Export: a BYTE-IDENTICAL copy of the store file (no re-serialization —
    // the file on disk IS the shareable artifact; its bytes are already
    // canonical per the codec). An existing target is refused unless
    // `overwriteConfirmed` is true (the caller's interactive ask — never a
    // silent overwrite). An unknown id fails with the diagnostic.
    bool   exportTo(const QString& id, const QString& targetPath,
                    bool overwriteConfirmed, QString* err);

    QString rootDir() const { return m_rootDir; }

private:
    QString m_rootDir;
};

} // namespace gp
