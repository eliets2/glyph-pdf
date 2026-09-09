# Named Batch Presets — Implementation Plan (DESIGN ONLY, no code)

**Date:** 2026-09-09 · **Repo:** pdf-parity @ `f443f59`, branch `feat/parity-glm`
**Status:** Design proposal for Tier-2 item **T2-1** (`docs/research/synthesis.md` §2), the
single most-recommended build item across the 16 competitor reports (foxit.md rec #1, acrobat.md
rec #3, nitro.md rec #5, sejda.md "most copyable high-value surface"). This document is
design-only: it adds no production code, no build files, no dependencies. PRD anchor: §9.12
"Preset workflow creation" (PRD.md:220) and §28 v1.5 "Batch preset workflows — named, reusable
multi-step batch pipelines" (PRD.md:407); ledger row §9.12 gap "named preset workflows"
(PRD.md:386).

**Sources for status claims (all read at pinned revision `f443f59` unless noted):**
`src/modes/BatchMode.{h,cpp}`, `src/modes/CompressDialog.h`, `src/core/Capability.h`,
`src/engines/SafeSave.{h,cpp}`, `src/engines/DocumentSession.h`,
`src/engines/RedactOperation.h`, `src/core/interfaces/IPdfEditorEngine.h`,
`src/ui/ExportPresetsPanel.h`, `src/shell/controllers/HomeController.cpp`,
`tests/TestBatchOpsCoverage.cpp`, `PRD.md`,
`docs/audit/CURRENT-EVIDENCE-LEDGER-2026-09-05.md`, `docs/research/synthesis.md`,
`docs/research/{acrobat,foxit,pdfxchange,sejda,nitro,abbyy-finereader,pdf24,bluebeam}.md`,
`docs/research/form-js-implementation-plan.md` (house design-doc format),
`.context/RESEARCH-RECONCILIATION-2026-09-08.md` (RQ10 discipline, referenced not modified).

Confidence legend follows the research-specialist 6-level scale (TRUE … UNVERIFIABLE). Web
primaries verified 2026-09-09; all engine facts cite code at `f443f59`. Claims that could not
be verified against a primary are marked **UNVERIFIED** inline.

---

## 0. Executive summary (3 sentences)

GlyphPDF already owns the hard parts of this feature — an uncapped per-file batch engine with
capability-honest pre-flight (`BatchMode`), a transactional write boundary (`SafeSave`
candidate→validate→commit), a full `CapabilityRegistry` disclosure system, and seven working
single-op pipelines (convert/compress/watermark/PDF-A/merge/OCR/redact) — but a preset today is
*one operation with sliders* (the `ExportPresetsPanel` precedent), so the Action-Wizard class of
workflow ("web-optimize = compress + strip metadata + PDF/A check", run over a folder, repeatable,
shareable) is impossible; that gap is PRD §9.12's named remaining item and the synthesis's top
Tier-2 recommendation. This plan adds a **versioned, strictly-validated JSON preset schema**
(fail-closed: unknown op/param/key rejected — justified in §2.4), a **per-file transactional
pipeline runner** (one SafeSave commit per file; a failed step never leaves a half-pipelined
file), and a **preset manager/run UI** in the Batch mode — all on existing engines, **zero new
dependencies** (QJson only). Three things gate or shape the build: the known
`TestBatchOpsCoverage` queued-drain flake must be fixed **before** the runner lands (P0
prerequisite, ledger-documented), per-step capability honesty is enforced at design time *and*
run time (never silently skipped), and the runner's report (per file × per step status + measured
bytes) extends the app's measured-readout honesty moat (M8).

---

## 1. Feature contract

### 1.1 What ships (one paragraph)

A **named batch preset** is a user-created, locally-stored, JSON-serialized, ordered list of
PDF operations with per-op parameters, runnable over a set of files with per-file/per-step
results. The user can create, edit, rename, duplicate, delete, import, and export presets; run
any preset over multiple files from the Batch mode; and receive a run report (per file, per
step: status, error/whyNot, measured bytes before/after). A preset step whose underlying
capability is `Unavailable*`/`Degraded` is disclosed at design time (editor) and at run time
(pre-flight), and is **never silently skipped** — it blocks the run with whyNot + alternative,
per the U08 idiom already pinned by `BatchMode::preFlightBlocker` (`BatchMode.h:74`) and
`CapabilityRegistry::applyToWidget` (`Capability.h:107`).

### 1.2 The competitor model of "preset" (what we are copying, per spec sheet)

| Tool | Surface | What it models | Cite |
|---|---|---|---|
| Acrobat Pro | Action Wizard (`Manage Actions`, `.sequ` files) | Named ordered multi-step actions over file sets/folders; pause-and-prompt steps; default actions (Make Accessible, Create PDF/A, OCR scans, Print); export/import for sharing; institutional reliance (state-DOT scanning manuals) | acrobat.md §10 row "Action Wizard" (:135), batch redaction via Action Wizard (:94), L5 (:224), rec #3 (:253) |
| Foxit Pro | Action Wizard | Named multi-step actions; custom commands built from existing commands (v13); default-equivalent actions incl. Save As / remove hidden info / view logs / batch conversion; make-accessible action (autotag + properties + language + PDF/UA check+fix) | foxit.md §1.10 (:153), delta "GAP (biggest)" (:243), rec #1 (:271) |
| PDF-XChange | PDF-Tools "customized tools" (user action sequences) + job profiles | Build custom tools from operations; job profiles + saved settings; full settings export with preset merge on import (10.4) | pdfxchange.md §10 (:163–165), settings export/import (:203) |
| Sejda | Workflows (web, BETA) | "Execute series of tasks on PDF documents. Configure tool chains"; create, **export/import** workflow definitions, launch; free tier = 1 workflow; filename templating keywords | sejda.md §1.9 (:122), G1 (:212), G2 (:213), loved #3 (:273) |
| Nitro | Automate (cloud, May 2026) | Agentic document processing (convert/merge/extract/compress/secure); validates multi-step pipeline demand; rec: "named batch presets as shareable files + per-step capability pre-flight" | nitro.md A6 (:67), rec #5 (:168) |
| ABBYY | Automated Tasks (Corporate Hot Folder) | Step pipelines (open → analyze → OCR → multi-save); export/import/share tasks; scope-limited to digitization (no redact/sign/compare automation) | abbyy-finereader.md §11 (:197), §10 scope limit (:190) |
| PDF24 | Output profiles | "The automation backbone": default/user/machine-wide profiles; params overridable per call (`-profileParam`); formats: PDF, PDF/A, security, watermark, page numbers, overlay | pdf24.md §1.9 (:139–140) |
| Bluebeam | Batch Sign & Seal | Saved batches (certify+seal+date across sheets), per-file page ranges, skip-docs-without-match — presets as saved batch definitions | bluebeam.md §7 (:160) |

Common denominator across all eight: **an ordered op list under a name, parameterized per op,
runnable over many files, shareable via a file**. Differences that matter for our design:
Acrobat serializes to an *undocumented proprietary* `.sequ` format (see §2.4 — the lesson is
"don't"); Sejda/ABBYY prove export/import is the shareability lever; Nitro's rec names per-step
capability pre-flight as the differentiator we are uniquely positioned to do honestly; PDF24's
profile-param-override shows run-time parameterization is a loved detail (adopted in §3.6).

### 1.3 In scope / out of scope

**In scope (v1):** preset CRUD + local store + import/export; ordered steps with per-op
parameters; run over multiple files with per-file/per-step report; capability-honest gating;
cancel; hot-folder hook ("run preset on ingest"); output filename templating (minimal token
set); per-run Bates sequence continuity across files (deliver the T2-5 cross-document Bates
seam as pipeline state — synthesis.md T2-5).

**Op set at each phase** (chainable = the step consumes the previous step's output; only the
final candidate is committed):

| Op | Engine seam (all existing at `f443f59`) | Phase | Notes |
|---|---|---|---|
| `compress` | `IPdfEditorEngine::optimizeDocument(outputPath, OptimizeOptions)` (IPdfEditorEngine.h:291; :88–100 struct) | P1 | `quality`, `targetDpi` (range 36–600 = `BatchMode::kMinTargetDpi/kMaxTargetDpi`, BatchMode.h:111–113) |
| `strip-metadata` | `sanitizeDocument(outputPath)` (:148) and/or `setMetadata(PdfMetadata)` (:150) | P1 | `sanitize: true` runs the sanitize pass (the redaction bundle's contract, RedactOperation.h); `setMetadata` clears Info dict fields |
| `pdfa-export` | `exportPdfA(outputPath, conformanceLevel)` (:283) | P1 | level ∈ {1b,2b,2u,3b,3u} — the ledger-verified N03 shipped set |
| `pdfa-check` | veraPDF CLI via existing validator (CapabilityRegistry `PdfAValidation`, Capability.h:49) | P1 | **check step** (non-mutating): validates the current candidate; failure = file fails the preset with a report entry, original untouched |
| `watermark` | `addTextWatermark(TextWatermarkOptions)` + save (worker precedent BatchMode.cpp:1233–1241) | P1 | text, opacity |
| `bates` | `applyBatesNumbering(path, BatesNumberingOptions, int* lastNumberOut)` (:222; options :46–59) | P1 | run-scoped sequence: `startNumber` of file *n+1* = `lastNumberOut` of file *n* + 1 (§3.5) |
| `redact` | `applyPatternRedactionsMulti(patterns, pages, outputPath)` (worker precedent BatchMode.cpp:1265–1273) | P1 | preset keys ("email","phone-us","ssn"… via `BatchMode::effectiveRedactPatterns`, BatchMode.h:126) + free-form regex |
| `ocr` | existing batch OCR path (`OcrPipeline` + `exportMrcPdfA`, BatchMode.cpp:1163–1216) | P3 | heavy; language param + honesty note (`lowConfidenceNote`, BatchMode.h:104) |
| `rotate-pages` | `rotatePage(path, pageIndex, degrees)` (:184) | P3 | page selector param |
| `convert` | `IConversionEngine::convertTo` (worker precedent BatchMode.cpp:1155–1162) | P3 | **terminal op** — output is non-PDF; must be the last step (validation rule V6) |

**Out of scope (v1, stated as gates):** step *conditions* (if/loop — Acrobat's pause-and-prompt
is deliberately not copied: it requires a GUI in the loop and breaks headless runs); arbitrary
plugins/scripted steps (no scripting runtime — RQ10 discipline; form-JS plan §6 owns that
decision independently); per-step alternate branches; cloud anything (anti-recommendation,
synthesis.md §3.1); scheduled runs (hot folder covers the ingest-driven case).

---

## 2. Preset schema (LOAD-BEARING SECTION)

### 2.1 Format decision

**Versioned JSON, one file per preset, extension `.glyphpreset.json`** (double extension so the
JSON-ness is visible in Explorer; the stem is the preset id). JSON because: (a) Qt6 ships
QJson — zero new dependency; (b) the file is a *work order users may hand-edit* (PDF24's
profiles and Sejda's workflows set this expectation; pdf24.md :140, sejda.md :122); (c) every
serious exportable-pipeline predecessor (n8n workflows, Sejda export/import) is JSON. XML (the
shape of Acrobat's internal settings) buys nothing here and matches nothing else in the app.

Storage location: `QStandardPaths::AppDataLocation/presets/<id>.glyphpreset.json` — file-per-preset
so import/export is a file copy, users can back the folder up, and QSettings never becomes the
source of truth (the `ExportPresetsPanel` precedent, ExportPresetsPanel.h:45–51, keeps presets
in QSettings; we deliberately diverge because QSettings is not portable-file-shaped and cannot
be shared; the *dialog UX* precedent of that panel is retained).

### 2.2 The schema (v1, exact fields)

```json
{
  "glyphpreset": {
    "schemaVersion": 1,
    "kind": "batch-preset",
    "minAppVersion": "1.4.0"
  },
  "id": "web-optimize",
  "name": "Web optimize",
  "description": "Compress for web, strip metadata, verify PDF/A-2b",
  "created": "2026-09-09T00:00:00Z",
  "modified": "2026-09-09T00:00:00Z",
  "authorApp": "GlyphPDF 1.4.0",
  "steps": [
    {
      "op": "compress",
      "label": "Compress images to 150 DPI",
      "params": { "quality": 60, "targetDpi": 150 }
    },
    {
      "op": "strip-metadata",
      "label": "Remove metadata",
      "params": { "sanitize": true, "clearInfoDict": true }
    },
    {
      "op": "pdfa-check",
      "label": "Verify PDF/A-2b",
      "params": { "level": "2b" }
    }
  ],
  "output": {
    "naming": "{basename}_web.pdf",
    "onConflict": "ask"
  },
  "onFileFailure": "continue"
}
```

Field contract (normative):

| Field | Type | Rules |
|---|---|---|
| `glyphpreset.schemaVersion` | int | **Required.** Only `1` is accepted by this build. Wrong version → reject with version-handshake message (§2.4). |
| `glyphpreset.kind` | string | **Required.** Must be `"batch-preset"` (guards against importing an unrelated JSON). |
| `glyphpreset.minAppVersion` | string | Optional. Semantic-version comparison against the running app; running older → import allowed, run blocked with whyNot ("preset requires GlyphPDF ≥ 1.5"), consistent with capability honesty. |
| `id` | string | Required. `[a-z0-9-]{1,64}`. = filename stem. Rename edits `name`, not `id` (ids are stable references for CLI/hot-folder). |
| `name` | string | Required. 1–80 chars, non-empty after trim. Duplicate names are allowed (ids disambiguate) but the manager warns. |
| `description` | string | Optional, ≤ 300 chars. |
| `created`/`modified` | string | ISO-8601 UTC. `modified` updated by the editor on every save. |
| `authorApp` | string | Optional, informational only (round-trips verbatim; never parsed for behavior). |
| `steps` | array | Required, 1–16 items. Each item: object with **exactly** the keys `op`, `label` (optional string ≤120), `params`. |
| `steps[].op` | string | Must be one of the registered op ids (§1.3 table) **known to this build** — else reject (V1). |
| `steps[].params` | object | Must be a JSON object. Every key must be a **named param of that op** (V2), every value must type-match (V3) and range-match (V4) the op's param spec. |
| `output.naming` | string | Optional; default `"{basename}_{preset}.pdf"`. Token charset constrained (§3.6): only `{basename}`, `{preset}`, `{n}`, `{date}`; any other `{…}` token → reject (V7). Result must end `.pdf` unless the last step is terminal (§1.3). |
| `output.onConflict` | enum | `"ask"` (default) \| `"overwrite"` \| `"rename"` — mirrors `BatchMode::confirmOverwrite` semantics (BatchMode.cpp:900–905, :965–987). |
| `onFileFailure` | enum | `"continue"` (default) \| `"stop"` — file-scope policy, §3.2. |

Op parameter specs (v1, normative — one row per param; type/range checked by V3/V4):

| Op | Param | Type | Range / values | Default |
|---|---|---|---|---|
| `compress` | `quality` | int | 10–100 | 75 (worker default, BatchMode.cpp:1019) |
| `compress` | `targetDpi` | int | 36–600 | 150 (`kDefaultTargetDpi`) |
| `strip-metadata` | `sanitize` | bool | — | true |
| `strip-metadata` | `clearInfoDict` | bool | — | true |
| `pdfa-export` | `level` | string | `"1b"`,`"2b"`,`"2u"`,`"3b"`,`"3u"` | `"2b"` |
| `pdfa-check` | `level` | string | same set | `"2b"` |
| `watermark` | `text` | string | 1–120 chars | `"CONFIDENTIAL"` (worker default, BatchMode.cpp:1027) |
| `watermark` | `opacity` | int | 1–100 | 30 (BatchMode.cpp:1029) |
| `bates` | `prefix`/`suffix` | string | ≤ 32 chars each | `""` |
| `bates` | `startNumber` | int | 1–2,147,483,000 | 1 (first file of run; subsequent files continue per §3.5) |
| `bates` | `digitCount` | int | 1–12 | 6 (`BatesNumberingOptions`, IPdfEditorEngine.h:50) |
| `bates` | `position` | string | `"bottom-right"`,`bottom-center`,`bottom-left`,`top-right`,`top-center`,`top-left` | `"bottom-right"` |
| `redact` | `presets` | array of string | known `PatternRedactor::namedPattern` keys ("email","phone-us","ssn",…) | `[]` |
| `redact` | `patterns` | array of string | valid regex (compile-checked at import — V4) | `[]` |
| `redact` | — | — | at least one effective pattern after dedup (`effectiveRedactPatterns` rule, BatchMode.h:126–128) | run-time check |

Param values in the schema are **run-time-invariant declarations, not captured GUI state**: no
file paths, no output directories (destinations are chosen at run time via the run dialog or
`output.naming`), and no environment-dependent values. This is the property that makes a preset
file inert data (§5).

### 2.3 The golden example (`web-optimize`)

The schema block in §2.2 **is** the golden fixture — committed as
`tests/fixtures/presets/web-optimize.glyphpreset.json` in P1 and loaded by every schema test:
parse → validate → serialize → byte-stable round-trip (canonical QJson ordering pinned), plus
capability disclosure assertions (all three ops Available on a standard test context).

### 2.4 Validation rules + forward-compat policy (the decision, picked and justified)

**Validation rules (all reject with a precise diagnostic: JSON path, offending value, current
schemaVersion, what this build supports):**

- **V1 unknown op** → reject. *"Unknown operation 'ocr-skip' (schema v1, this app supports: compress, strip-metadata, …)".*
- **V2 unknown param key inside a known op** → reject. Same precision.
- **V3 wrong type** (string where int belongs, object where array…) → reject.
- **V4 out-of-range / invalid value** (dpi outside 36–600; bad regex; unknown Bates position;
  unknown PDF/A level) → reject.
- **V5 unknown key anywhere** (top level, `glyphpreset.*`, step, `output.*`) → reject.
- **V6 terminal-op placement** (`convert` not last; no steps after a check-then-terminal
  sequence that leaves no PDF) → reject.
- **V7 bad naming token** → reject. **V8 structural**: empty `steps`, >16 steps, `id`≠filename
  stem on import, `kind` mismatch, `schemaVersion`≠1 → reject.
- **V9 resource caps**: file > 256 KiB, `steps` > 16, any string param > 1 MiB → reject with
  honest message (JSON-bomb / pathalogical-file guard; QJson also enforces its own nesting cap).

**Forward-compat policy: FAIL-CLOSED (reject unknown), with a version handshake.** A file with
keys this build does not know is *rejected at import*, not tolerated-and-preserved. The
alternatives considered:

1. *Tolerant Reader* (Fowler, <https://martinfowler.com/bliki/TolerantReader.html>): ignore
   unknown fields, keep what you understand. **Rejected** — the pattern is designed for
   *documents/messages* where each consumer reads its projection of data. A preset is an
   *imperative work order* over user files: silently dropping a step or a parameter means the
   run does something other than what its author specified, and the divergence is invisible at
   exactly the layer where this app's moat is "never silent" (M8, synthesis.md §4). A tolerated
   unknown key is also untestable by construction — the run can no longer be pinned to the file.
2. *Preserve-and-round-trip unknown metadata* (tolerant top level, strict steps). **Rejected** —
   it re-introduces unread state that accumulates across app versions and must be explained in
   every honesty surface ("some fields in this preset were not understood"), for zero user
   benefit over the handshake below. Simplicity wins; strictness is uniform.

**The honest forward-compat path is the handshake, not tolerance:** a newer app writes
`schemaVersion: 2`; an older app rejects with *"This preset uses schema v2 (created by GlyphPDF
1.6). This app understands v1. Update GlyphPDF or ask for a v1 export."* — actionable, truthful,
and matches how `CapabilityRegistry` discloses *whyNot + alternative* instead of faking
compatibility. Portability across machines on the *same* app version — the actual sharing
use case — is fully served. Precedent for not copying the alternative: Acrobat's proprietary
undocumented `.sequ` format produces exactly the version-friction threads we would be signing
up for (import failures across Acrobat versions are a community staple —
<https://community.adobe.com/questions-9/adobe-acrobat-pro-2017-importation-of-action-files-1245754>;
format itself unpublished per <https://acrobatusers.com/forum/actions/sequ-actions-file-reference/>,
both fetched 2026-09-09, grade TRUE for "undocumented/proprietary"). This policy is reversible
pre-P2 if the user prefers tolerant metadata round-tripping; it is called out in §7 as a
recorded design call, not a blocking decision.

---

## 3. Execution semantics

### 3.0 P0 PREREQUISITE — the BatchMode queued-accounting drain race

The ledger documents, three times, a known flake: *"Parallel `-j 4` runs remain subject to the
known TestBatchOpsCoverage queued-drain flake"* and *"the first gate attempt hit the known
TestBatchOpsCoverage queued-drain flake (successCount()==0)"*
(`docs/audit/CURRENT-EVIDENCE-LEDGER-2026-09-05.md:185, :237–239, :267`). The code shows why the
class exists: per-run accounting is driven by `resultReadyAt` (BatchMode.cpp:1305–1308) with a
cursor (`m_accountedResultIdx`, :997, :1463–1465), and `onBatchFinished` drains
reported-but-unaccounted results **only for merge runs** (`if (m_mergeRun)`, :1528–1538) — the
per-file `QtConcurrent::mapped` path has no such drain, so a lagging resultReadyAt under load
under-counts (`successCount()==0` observed).

The preset runner multiplies accounting surface (per file × per step), so landing it on top of
this is building on a known race. **P0 gate for P1:** unify the accounting so the summary is a
pure function of the finished future — drain reported-but-unaccounted results for **all** run
kinds in `onBatchFinished` (not only `m_mergeRun`), or account from `future.resultAt()` at
finish instead of signal timing — plus a regression stress test (batch of N files run under a
saturated thread pool; assert `successCount()+failCount()+remainingCount()==fileCount()` and
`errorLogCount()` consistency). Reference: ledger lines above; BatchMode.cpp:1528–1546;
BatchMode.h:64–67 (`remainingCount` U08 invariant). No behavior change to interactive batch ops
beyond the accounting fix.

### 3.1 Transactional per file (one SafeSave candidate per file — never a half-pipelined file)

Per input file the runner executes a **candidate chain**:

1. Resolve steps → per-step capability check (run-time re-query, §5.2). Any blocking step →
   file staged as failed-with-whyNot **before any I/O** (the `preFlightBlocker` pattern,
   BatchMode.h:74–76).
2. `current = inputPath`. For each mutating step *k*: produce `candK` via
   `SafeSave::makeUniqueCandidate` (SafeSave.h:27) into the temp dir; run the step's engine seam
   writing to `candK`; **validate** `candK` (reopens as PDF; page count equals the previous
   link's — every P1 op is page-count-invariant; PDF/A ops additionally verified when
   `pdfa-check` runs); promote `candK` → `current`. On any step failure: delete all
   intermediates, leave the original byte-identical, record failure, next file. (This chain
   shape is chosen over in-memory chaining deliberately: the existing engine seams are
   load→mutate→write-to-path shaped — e.g. `optimizeDocument(outputPath, …)`,
   `exportPdfA(outputPath, …)` — so N single-purpose temp candidates reuse them verbatim, keep
   each step independently fault-injectable, and bound memory; the cost, a few serializations,
   is what the per-step byte report wants anyway.)
3. **Atomic commit once:** `SafeSave::commitFileToDestination(finalCandidate, outputPath, …)`
   (SafeSave.h:42–43 — QSaveFile + checked commit + atomic rename; failure leaves the
   destination byte-identical; there is deliberately NO direct-write fallback). Intermediates
   deleted on every outcome. The GUI-held-handle coordinator pair (SafeSave.h:59–66) is armed
   around the commit exactly as every other writer does.
4. A `pdfa-check` step validates the *candidate in flight* — a check never touches the
   destination; a failed check fails the file per §3.2 (the honest contract: "web-optimize"
   that cannot pass PDF/A-2b reports the failure, it does not ship an unverified file).

Per-step status, bytes before/after (measured on disk, per the §9.13 measured-completion
precedent — `CompressDialog::formatCompletionReport`, CompressDialog.h:37), timing, and any
review note (`reviewNote` field of `BatchFileResult`, BatchMode.h:46–48, e.g. OCR
low-confidence wording) are accumulated into the run report.

### 3.2 Failure policy (recommended with rationale)

Two orthogonal scopes, fixed as follows:

- **Within a file: a failed step always aborts that file's chain.** There is no "continue to
  next step after a failed mutation" — the candidate is invalid by definition and continuing
  would run steps against unverified bytes. Not configurable; not a mode.
- **Across files: default `continue`** (schema `onFileFailure`, §2.2). Rationale: a batch run
  over hundreds of files must not be held hostage by one locked/corrupt input — that is the
  entire value of the hot-folder rail (M2); per-file isolation with a truthful report is what
  every ledger-honest failure to date has looked like (E-13 exception-to-result conversion,
  BatchMode.cpp:1089–1100, is the same philosophy). `onFileFailure: "stop"` exists for
  money-pipeline users who prefer halt-on-any-defect.
- **Batch-scoped failures abort the run regardless of `onFileFailure`:** output directory
  unwritable, disk full, a capability discovered Unavailable at run time that pre-flight should
  have caught (recorded as a defect), commit coordination failure. These fail identically for
  every remaining file — continuing would only multiply identical errors; the run stops with
  the first occurrence plus the per-file results already collected. Detection: classify each
  failure as file-scoped or batch-scoped at the report layer (batch-scoped = error class
  independent of file content).

### 3.3 Report shape

Per run: preset id + schemaVersion, app version, start/end time, policy settings, aggregate
counts (files ok/failed/blocked, steps ok/failed). Per file: input path, resolved output path,
outcome ∈ {ok, failed, blocked, canceled}, and an ordered per-step array:

```json
{ "step": 0, "op": "compress", "status": "ok",
  "bytesIn": 4821130, "bytesOut": 1204555, "durationMs": 412 },
{ "step": 1, "op": "pdfa-check", "status": "failed",
  "detail": "<veraPDF assertion summary>", "whyNot": "", "alternative": "" }
```

`bytesIn`/`bytesOut` are the measured on-disk sizes of the candidate chain links (the original
file for step 0) — this is what makes the "web-optimize" promise verifiable instead of
marketing. Export: **JSON (machine-readable, the M5 report precedent)** and **CSV**
(RFC-4180-pinned, the U07 CSV-export discipline). Failure `detail` carries `techDetail`-grade
text; blocked steps carry non-empty `whyNot` + `alternative` (the CapabilityRegistry contract,
Capability.h:27–28).

### 3.4 Cancellation

Cancel is checked at file boundaries and between steps (cheap atomic flag read); a cancel never
interrupts a step's engine call or the final commit mid-write — the in-flight file either
completes its transactional commit or aborts cleanly at the next boundary, so cancel can never
produce a half-pipelined file (same contract as the existing watcher cancel;
`onCancelClicked` precedent, BatchMode.h:150). Canceled files are reported `canceled`, never
`success` and never silently dropped (the U08 remaining-count truthfulness rule,
BatchMode.h:64–67).

### 3.5 Run-scoped state (Bates continuity — the T2-5 seam)

The runner carries a small typed run-state object. Its one v1 member: the Bates counter — after
file *n*, `applyBatesNumbering`'s `lastNumberOut` (IPdfEditorEngine.h:222) seeds file *n+1*'s
`startNumber` (unless the step explicitly pins `startNumber`). This delivers cross-document
Bates continuity — a synthesis T2-5 item ("GlyphPDF PARTIAL — Bates is single-document only") —
as a natural pipeline property, and it is report-visible (per-file first/last Bates number).

### 3.6 Output naming (minimal Sejda-class token grammar)

`output.naming` supports exactly four tokens: `{basename}` (input stem), `{preset}` (preset id),
`{n}` (1-based file index), `{date}` (run date, ISO). Unknown token → schema reject (V7);
tokens are replaced with sanitized components (no path separators can be smuggled through a
token — replacement values are filename-sanitized). Based on Sejda's templating grammar
(sejda.md G2, :213 — "one keyword engine reused by split/rename/batch") with a deliberately
smaller v1 surface; `onConflict` ("ask"/"overwrite"/"rename") reuses the overwrite pre-check
discipline of BatchMode.cpp:965–987.

### 3.7 Threading

Unchanged from the existing engine: one file per `QtConcurrent::mapped` worker behind the
existing `QFutureWatcher`; the shared-engine steps (OCR; anything touching non-thread-safe
backends) serialize behind the captured `m_engineMutex` (worker precedent, BatchMode.cpp:1088,
:1157–1160). Steps are strictly sequential *within* a file; files run in parallel exactly to the
degree the existing single-op pipeline does — no new concurrency model is introduced.

---

## 4. UI (text descriptions, no code)

### 4.1 Placement

The Batch mode gains a **"Preset pipeline"** entry as the first item of the existing operation
combo (`m_opCombo`, the `OpIndex` enum at BatchMode.h:244–252), with its own config panel in the
`m_cfgStack`: preset picker + "Manage presets…" + "Run" (which points the run at the files
already in the file panel — the whole existing file-list/hot-folder/progress/log machinery is
reused as-is). A second entry point lives in the Home controller next to the
`ExportPresetsPanel` host (HomeController.cpp:549–556 precedent): **"Batch presets…"** opens
the manager; **"Run preset…"** opens the run dialog directly. The manager is a new
`src/ui/PresetManagerDialog.{h,cpp}`; the editor a `PresetEditorDialog`; the run either the
Batch-mode panel or a `PresetRunDialog` hosting the same widgets (one implementation, two
hosts).

### 4.2 Preset Manager dialog

- **Left:** list of presets (name, one-line description, step count badge, modified date);
  toolbar: New / Duplicate / Edit / Rename / Delete / Import… / Export… / Run…. Delete asks for
  confirmation (files are user data); Import validates **before** the file appears in the list
  (a rejected import shows the diagnostic and nothing else — a failed import must not leave a
  half-imported preset, mirroring the transactional discipline of the runner itself).
- **Right (detail pane):** read-only summary — ordered op list with per-step capability badge
  (green available / amber degraded-with-detail / red unavailable-with-whyNot, the
  `applyToWidget` visual idiom, Capability.h:107), output naming, failure policy.
- **Editor (modal):** name/description at top; **step list** with add (from the op palette —
  only ops whose capability is not `UnavailableBuild` are offered), remove, reorder (up/down
  buttons; drag optional later); selecting a step shows its **parameter form** — the same
  widget families the Batch panels already use (DPI spin with the 36–600 clamp seam
  `resolveCompressTargetDpi`, BatchMode.h:117; quality slider; PDF/A level combo with the
  version-correctness discipline; redact preset checkboxes + regex list; Bates fields mirroring
  `BatesNumberingDialog`; watermark text/opacity). Parameter widgets are capability-wrapped:
  an `UnavailableRuntime` capability (e.g. veraPDF absent for `pdfa-check`) keeps the step
  selectable but visibly disabled with tooltip whyNot + alternative — the step is *disclosed at
  design time* and the run is blocked at pre-flight, never silently skipped. Save writes the
  JSON through the validator (the editor cannot produce an invalid file; hand-edited files are
  re-validated on load).

### 4.3 Run dialog

- **Files:** drag-drop / add-files / add-folder (reuse of the Batch file panel behavior incl.
  the hot-folder toggle — "run this preset on every file that lands in the folder" is the
  institutional rail Acrobat proved, acrobat.md L5 :224).
- **Pre-flight summary (before Run is enabled):** per-step capability status re-queried now;
  per-file blockers count (files that would be blocked, with the first whyNot); output
  directory + conflict preview (which outputs exist). Any blocking disclosure disables Run with
  the combined whyNot text (`CapabilityRegistry::combineWhyNot`, Capability.h:118).
- **Progress:** overall bar (files), current-file line (step k of n, op name), elapsed/ETA
  (existing labels), Cancel.
- **Report (on finish):** the summary counts + the per-file/per-step table of §3.3 with
  measured bytes and a per-file delta badge; Export report (JSON/CSV); "reveal output folder".
  Failed/blocked rows expand to the full detail. The measured before/after readout is the same
  honesty surface class as `formatCompletionReport` (CompressDialog.h:31–38).

---

## 5. Security & honesty

### 5.1 Presets are data — importing can never execute anything

An imported `.glyphpreset.json` is parsed by QJsonDocument and validated against §2.2; nothing
else happens. Concretely: **the schema carries no paths** (destinations are chosen at run time;
`naming` tokens are sanitized per §3.6), **no commands** (ops are enum strings, never shell or
JS), **no URIs** (a value that smells like one is just an invalid param per V4), and no code
of any kind (RQ10 discipline: no scripting runtime is introduced by or for this feature — the
form-JS decision is independent and owned by that plan). Resource caps (V9) bound parsing
cost. The strongest statement of the property: a preset file can only *describe* work the
validator already enumerates; the worst a malicious file can do is fail validation or, at run
time, cause operations the user can read in the pre-flight summary before pressing Run. This
is the deliberate inverse of Acrobat's model, where importing an Action can carry scripted
steps (JavaScript action steps are an Acrobat Action Wizard feature class — acrobat.md §4
frames the run-side JS risk surface; GlyphPDF presets have no script step type at all).

### 5.2 Capability honesty — design time AND run time

- **Design time:** the editor's op palette and parameter forms query the registry once per open
  (memoized per Capability.h:33–34); `UnavailableBuild` ops are not offered at all (they are
  not in this binary); `UnavailableRuntime`/`Degraded` ops are offered but visibly disclosed
  (§4.2).
- **Import time:** validation is schema-only (§5.1), but the manager's detail pane shows each
  step's *current* capability status next to the file — importing a preset containing a
  currently-unavailable step succeeds (data is data) and the badge says so.
- **Run time:** pre-flight re-queries (registry `invalidate` on preference changes keeps the
  cache honest, Capability.h:98–99) and blocks per file with whyNot + alternative — the same
  blocking-and-staging-as-failed discipline as `preFlightBlocker`/`preFlightReviewNote`
  (BatchMode.h:74–83), so a disclosed-unavailable step is never a silent skip and the summary
  stays truthful. A step that *degrades* mid-run (e.g. models removed between pre-flight and
  step execution) fails that file file-scoped with the registry text in the report.
- **Report honesty:** measured bytes only (§3.1, §3.3) — never estimates presented as results
  (the §9.13/R12 rule); failed steps carry the engine's `techDetail`-grade text; blocked steps
  carry non-empty whyNot + alternative (enforced by `Capability::query`, Capability.h:92–94).

---

## 6. Phased plan (integration anchors at `f443f59`)

### P0 — prerequisite gate (0.5–1 wk, must land before P1's runner tests)

- **Drain-race fix** in `BatchMode` accounting (§3.0): drain-always at finish or finish-time
  accounting from the future; regression stress test added to `TestBatchOpsCoverage` (and the
  flake note in the ledger updated on landing). Coordinates with the measurement lane currently
  touching `src/modes/` — small, isolated diff to the accounting block (BatchMode.cpp:1460–1465,
  :1528–1538).
- **Anchor files:** `src/modes/BatchMode.cpp`, `tests/TestBatchOpsCoverage.cpp`.

### P1 — core engine + schema + tests (2.5–3.5 wk)

- `src/core/BatchPreset.h/.cpp`: schema types, param specs, `validate()` (V1–V9), JSON
  read/write (canonical serialization), `BatchPresetStore` (AppData file-per-preset, import/
  export, id-from-stem). Pure Qt, fully unit-testable without GUI.
- `src/engines/PresetRunner.{h,cpp}` (or `src/modes/` if the lane prefers, engine-leaning
  recommended): candidate chain per §3.1 over `SafeSave`; step adapters for compress /
  strip-metadata / pdfa-export / pdfa-check / watermark / bates / redact (§1.3 table, all
  existing seams); run state (Bates continuity §3.5); failure classification (§3.2);
  cancellation flag; report model (§3.3).
- **Test seams, house pattern:** `SafeSave::setCommitFaultForTesting` (SafeSave.h:32–33) covers
  commit faults; add a `StepFaultForTesting` injection (fail step *k* of the chain after the
  engine call, before validation) mirroring `SaveFault`/`RedactOperation::Fault` precedents
  (form-js plan §"Integration architecture" documents the same pattern choice).
- **Tests** (`tests/TestBatchPresetSchema.cpp`, `tests/TestBatchPresetRunner.cpp`, fixture
  style of `TestBatchOpsCoverage.cpp`):
  - *Golden files:* `web-optimize` (§2.3) round-trips byte-stable; a second golden with every
    op × every param at defaults pins the full param spec.
  - *Negative validation matrix:* one case per V1–V9, each asserting the diagnostic names the
    JSON path; malformed JSON; oversize file (V9); `kind` mismatch.
  - *Transactional per-file with injected failures:* commit fault at final commit → original
    byte-identical (SHA-256 compared — the E-1/R01 discipline), no temp residue; step fault at
    chain position k ∈ {1, 2, 3} → intermediates cleaned, output never created; disk-full-class
    failure → batch-scoped abort with partial report.
  - *Report assertions:* per-file/per-step statuses and byte counts match a hand-computed table
    for a two-step golden run over a small fixture PDF; canceled run reports `canceled` rows;
    Bates continuity across 3 files produces the expected sequence (continues via
    `lastNumberOut`).
  - *Capability disclosure:* unavailable `pdfa-check` (veraPDF probe stubbed off) → run blocked
    pre-flight with non-empty whyNot+alternative; degraded step surfaces its detail (idiom test
    per `TestCompressDialogHonesty`).
- **Effort note:** schema+store ~4–5 d; runner+adapters ~6–8 d; tests incl. stress ~4–5 d.

### P2 — manager UI (1.5–2 wk)

- `PresetManagerDialog` + `PresetEditorDialog` (§4.2) wired to the store and registry;
  Batch-mode "Preset pipeline" op entry (combo + cfg panel) reusing the file panel /
  progress / log plumbing; run flow with pre-flight summary (§4.3).
- Tests: dialog-level honesty tests (badge/tooltip text pinned, offscreen platform per the
  ledger's QT_QPA_PLATFORM discipline); editor cannot save an invalid preset (param widgets
  clamped/range-checked); import-reject leaves list unchanged.
- **Effort note:** manager+editor ~5–6 d; batch-mode integration + run flow ~3–4 d; tests ~2 d.

### P3 — import/export + reports polish (1 wk)

- `.glyphpreset.json` import/export polish (file dialog filters via
  `CapabilityRegistry::fileFilterFor` pattern, Capability.h:113); report export JSON+CSV;
  hot-folder "run preset on ingest" toggle; `{n}`/`{date}` token edge cases; docs (user-facing
  help page + PRD §9.12 ledger row update).
- OCR / rotate / convert terminal steps (the P3 op set of §1.3) if the lane has capacity —
  each is an adapter + params + tests; `convert` terminality enforces V6 in the editor UI.
- **Effort note:** polish ~2–3 d; optional ops ~2–3 d total for rotate+convert, OCR +2 d.

**Sequencing constraint:** P0 → P1 → P2 → P3. P1's schema/store half has no dependency on P0
and can start immediately in parallel; only the *runner* sits behind the P0 gate.

---

## 7. Decision requests

**None.** No new dependency (QJson only), no network surface, no scripting runtime, no pricing
or scope change — RQ10 self-authorization concerns do not arise. Two design calls were made
under the tasking's "pick and justify" mandate and are reversible pre-P2 if the user disagrees:
(1) fail-closed schema validation with version handshake (§2.4) — the alternative
(Tolerant-Reader metadata preservation) is documented there; (2) file-per-preset storage in
AppData instead of the `ExportPresetsPanel` QSettings precedent (§2.1). The P0 drain-race fix
touches `BatchMode.cpp`, currently co-edited by the measurement lane — a coordination note, not
a decision: the diff is confined to the accounting block (§3.0).

---

## 8. TL;DR (10 lines) + recommendation

1. T2-1 named batch presets is the corpus's top recommendation (6/16 tools ship an analog; foxit rec #1, acrobat rec #3, nitro rec #5, sejda "most copyable").
2. GlyphPDF has every hard part already: batch engine, SafeSave transaction, CapabilityRegistry, seven single-op pipelines, hot folder.
3. Ship: versioned JSON preset (`.glyphpreset.json`) = ordered op list + per-op params; one file per preset; import/export for shareability.
4. Schema is FAIL-CLOSED: unknown op / param / key / bad version → reject with precise diagnostics; honesty moat M8 forbids silently ignoring work-order semantics (Tolerant Reader rejected — Fowler cited).
5. Execution: per file one candidate chain (temp candidates per step, each validated), ONE SafeSave atomic commit per file — a half-pipelined file is impossible.
6. Failure policy: failed step aborts the file; batch continues by default; batch-scoped failures (disk, capability) abort the run.
7. Report: per file × per step status + measured bytes before/after — extends the measured-readout honesty moat (M8); JSON+CSV export.
8. P0 gate first: fix the ledger-documented TestBatchOpsCoverage queued-drain flake (successCount()==0) — the runner multiplies its accounting surface.
9. Bates continuity across files falls out of run-scoped state (`lastNumberOut`) — delivers the T2-5 cross-document Bates seam for free.
10. Phases: P1 engine+schema+tests (2.5–3.5 wk) → P2 manager UI (1.5–2 wk) → P3 import/export+reports polish (1 wk); zero new dependencies.

**Recommendation:** build it in this order — P0 drain fix, P1 schema+runner with the
`web-optimize` golden fixture, P2 manager, P3 shareability polish — on the existing engines,
with capability disclosure enforced at design time, import time, and run time, and with the
strict-schema policy recorded as the reversible design call of record (§7).

---

## 9. Source register

**Codebase (pinned `f443f59`):**
`src/modes/BatchMode.h` :39–49 (`BatchFileResult` incl. `reviewNote`), :64–67
(`remainingCount` U08), :74–83 (`preFlightBlocker`/`preFlightReviewNote`), :104–113 (DPI seams
+ constants), :117 (`resolveCompressTargetDpi`), :126–128 (`effectiveRedactPatterns`),
:244–252 (`OpIndex`); `src/modes/BatchMode.cpp` :900–905/:965–987 (overwrite policy), :909
(`onRunClicked`), :997/:1460–1465 (accounting cursor), :1085–1100 (E-13 exception→result),
:1155–1162 (convert step), :1230–1241 (watermark step), :1251–1255 (PDF/A step), :1265–1273
(redact step), :1305–1308 (resultReadyAt wiring), :1388/:1456 (merge worker), :1528–1546
(`onBatchFinished` merge-only drain — the P0 subject); `src/engines/SafeSave.h` :22–43
(candidate/commit primitives + fault seam), :59–75 (handle coordinator);
`src/core/Capability.h` :22–34 (registry contract), :36–51 (`CapId`), :53–61
(`Availability`/`Capability`), :92–94 (`query` honesty enforcement), :107 (`applyToWidget`),
:113 (`fileFilterFor`), :118 (`combineWhyNot`); `src/core/interfaces/IPdfEditorEngine.h`
:46–59 (`BatesNumberingOptions`), :88–100 (`OptimizeOptions`), :136/:148/:150
(save/sanitize/metadata), :184 (rotatePage), :215/:222 (`applyBatesNumbering` +
`lastNumberOut`), :283 (`exportPdfA`), :290–291 (optimize); `src/engines/RedactOperation.h`
(transaction shape, `sanitizeCommittedFile`); `src/ui/ExportPresetsPanel.h` :17–55 (preset CRUD
+ QSettings precedent); `src/ui/CompressDialog.h` :23–38 (R12/§9.13 honesty seams);
`src/shell/controllers/HomeController.cpp` :549–556 (panel host precedent);
`tests/TestBatchOpsCoverage.cpp` :363–378/:576+ (test harness + slots); `PRD.md` :220, :386,
:407; `docs/audit/CURRENT-EVIDENCE-LEDGER-2026-09-05.md` :185, :237–239, :267 (drain flake);
`docs/research/synthesis.md` §2 T2-1/T2-5, §4 M2/M8, §5 C3.

**Spec sheets (per-tool rows):** acrobat.md :94, :131–140 (§10), :224, :253; foxit.md :149–153
(§1.10), :243, :271; pdfxchange.md :159–165 (§10), :203; sejda.md :122 (§1.9), :212–213 (G1/G2),
:273; nitro.md :67–72 (A6), :168; abbyy-finereader.md :181–190 (§10), :193–197 (§11); pdf24.md
:135–140 (§1.9); bluebeam.md :160.

**Web (verified 2026-09-09):**

- Acrobat `.sequ` undocumented/proprietary: <https://acrobatusers.com/forum/actions/sequ-actions-file-reference/>; export/import via Manage Actions: <https://mapsoft.com/posts/acrobat-action-wizard.html>, <https://www.nysed.gov/webaccess/tools/save-batch-action>; cross-version import friction: <https://community.adobe.com/questions-9/adobe-acrobat-pro-2017-importation-of-action-files-1245754>; Action-Wizard command ordering model: <https://evermap.com/Tutorial_ABM_ActionWizardDC.asp>
- Sejda Workflows (task chains, export/import surface): <https://www.sejda.com/workflows>
- JSON workflow export/import cross-domain precedent (n8n): <https://docs.n8n.io/build/manage-workflows/export-and-import>
- Tolerant Reader (considered and rejected, §2.4): <https://martinfowler.com/bliki/TolerantReader.html>; related: <https://martinfowler.com/articles/refactoring-document-load.html>

**Format note:** this document follows the house design-doc format established by
`docs/research/form-js-implementation-plan.md` and
`docs/research/send-for-signing-implementation-plan.md` (pinned-revision source register,
graded claims, phased plan with integration anchors, explicit decision-request section).
