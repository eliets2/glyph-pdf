# Named Batch Presets — Phase 2: Runner Residuals, Bates Continuity, Manager UI (DESIGN ONLY)

**Date:** 2026-09-21 · **Repo:** pdf-parity @ `ec9f16f6`, branch `feat/feature-plans` (from `feat/parity-glm`)
**Status:** Design proposal for **Phase 2** of `docs/research/batch-presets-implementation-plan.md`
(R26), scoped to the P1 residuals of the landed core (`f17f47f8` + `29d2a3f9` ledger rows, W1-01
adversary hardening, `TestBatchPresets` 13 slots). No production code and no build files are
touched by this document. **Decision requests: none** — every residual below was already
specified by the P1 plan's schema/semantics sections (§2.2, §3.2, §3.3, §3.5, §4.2, §4.3);
P2 implements the recorded design, and the two NEW calls this document makes (the ordered
Bates lane §4.2, the unattended-conflict rule §4.5) carry their alternatives on the record
(§7).

**Sources for status claims (all read at pinned revision `ec9f16f6` unless noted):**
`src/core/BatchPreset.{h,cpp}` (194/929 ln: `kSchemaVersion = 1`, fail-closed codec, the
"not-implemented" refusal contract for `onConflict "rename"` / `onFileFailure "stop"`,
`knownOps()` = 6 ops WITHOUT `bates` + the recorded residual rationale at `BatchPreset.h:85–88`,
`resolveNaming` containment guard + W1-01 note, `BatchPresetStore` without import/export),
`src/modes/BatchMode.{h,cpp}` (`OpPresetPipeline` + cfg panel `:2196–2326`,
`resolveOutputPath` preset arm `:951–966`, `confirmOverwrite` `:972–979`,
`runPresetMutatingStep` `:1016–1053`, `runPresetCheckStep` `:1061–1087`, `runPresetChain`
`:1089–1176` (candidate chain + page-count invariant + one SafeSave commit + intermediate
cleanup), worker capture `:1382–1465`, `BatchFileResult` `BatchMode.h:39–59` (success /
errorMessage / techDetail / reviewNote / skipped — NO per-step records), hot folder
`:830–857` (`onHotFolderChanged` ingest + auto-run), `onToggleHotFolder` `:830`),
`src/engines/SafeSave.h` (candidate/commit/fault seams), `src/engines/VeraPdfValidator.h`
(`pdfa-check` engine), `tests/TestBatchPresets.cpp` (13 slots incl.
`transactionalFailureLeavesOriginalUntouched`, `presetRunMatchesManualConfiguration`),
`tests/TestSweepW1PresetAdversary.cpp` (W1-01 pins), `tests/TestBatchOpsCoverage.cpp`,
`docs/audit/CURRENT-EVIDENCE-LEDGER-2026-09-05.md` (R26 rows `29d2a3f9`/`f17f47f8`; W1-01
`b67d18be`/SWEEP-W1 audit), `docs/research/batch-presets-implementation-plan.md` (§1.3 op
table, §2.2 schema, §2.4 fail-closed policy, §3.1–3.6 semantics, §4.2/4.3 UI, §6 phases).

Confidence legend follows the research-specialist 6-level scale (TRUE … UNVERIFIABLE).

---

## 0. Executive summary (3 sentences)

The P1 wave landed the versioned fail-closed preset model, the canonical codec, the
file-per-preset store and the transactional per-file candidate chain inside BatchMode — but
every residual that makes presets a *workflow* feature is still open: the `bates` op was
excluded from `knownOps()` because run-ordered cross-file continuity needs serialization the
parallel mapped worker cannot honestly provide, `onConflict "rename"` and
`onFileFailure "stop"` are schema-legal-but-refused, there is no import/export, no per-step
measured-bytes report, no preset-on-ingest, no batch-scoped failure abort, and no multi-step
editor dialog (only save-from-configured-run). This plan closes all seven residuals with a
**zero-schema-churn** claim as its load-bearing discipline: every P2 value already fits the
v1 grammar the P1 plan specified (the `bates` param table, both enum values), so
`kSchemaVersion` stays **1** and every v1 file written by the P1 build loads unchanged; the
only genuinely new call is the **ordered Bates lane** — bates-bearing presets run through a
dedicated sequential worker lane in list order (continuity is then a loop invariant, honest
and testable) at the disclosed cost of losing file parallelism for that run, with the
phase-split pipeline (parallel pre-bates chain → sequential bates pass → parallel post)
recorded as the P3 optimization. The manager + editor dialogs, the per-step measured-bytes
report (JSON + CSV) and import/export complete the §4.2/§4.3 surface the P1 plan already
described.

---

## 1. Ground truth: what P1 landed and what it refused (the residual list, code-verified)

| P1 plan item | State at `ec9f16f6` | Evidence |
|---|---|---|
| Schema + fail-closed codec + store | LANDED | `BatchPreset.{h,cpp}`; V1–V9 diagnostics; `brokenFiles()` disclosure; golden round-trip |
| Capability mapping (design-time + run-time honesty) | LANDED | `batchPresetStepCapability` (null-registry → UnavailableRuntime for gated ops) |
| Candidate chain (per-step SafeSave candidate → page-count validate → ONE commit → intermediate cleanup) | LANDED | `BatchMode.cpp:1089–1176`; test `transactionalFailureLeavesOriginalUntouched` |
| Op set `compress` `strip-metadata` `pdfa-export` `pdfa-check` `watermark` `redact` | LANDED | `knownOps()` (6) |
| `bates` + run-ordered continuity | **P2 (this plan §4.2)** | `BatchPreset.h:85–88` residual note |
| `onConflict "rename"` | **P2 (§4.3)** | refused as not-implemented (header note `BatchPreset.h:58–62`) |
| `onFileFailure "stop"` | **P2 (§4.4)** | refused as not-implemented (`BatchPreset.h:63`) |
| Batch-scoped failure abort (plan §3.2 bullet 3) | **P2 (§4.6)** | unimplemented — every failure is file-scoped today |
| Import/export | **P2 (§4.7)** | store has no import/export surface |
| Per-step measured-bytes report | **P2 (§4.8)** | `BatchFileResult` carries file-level fields only |
| Hot-folder preset ingest | **P2 (§4.5)** | watcher ingests + auto-runs whatever op is selected — unattended conflict semantics undefined for presets |
| Manager + multi-step editor dialog | **P2 (§4.9)** | combo picker + steps label + save-from-run only |

**The schema claim (LOAD-BEARING):** P2 requires **zero** grammar changes.
`batch-presets-implementation-plan.md` §2.2 already specifies `bates` params
(`prefix`/`suffix`/`startNumber`/`digitCount`/`position` with ranges) and both
`onConflict: rename` and `onFileFailure: stop` as v1 enum values; the P1 build merely refuses
to implement three of them (`BatchPreset.h:30–33` records exactly this policy: "Values that
schema v1 grammar allows but THIS BUILD does not implement … are refused with an explicit
not-implemented diagnostic — never silently reinterpreted"). P2 flips refusals into
implementations; `kSchemaVersion` stays 1; **no v2 handshake, no migration, and every v1
golden fixture is byte-stable across the wave** (pinned by the existing
`goldenRoundTripByteStable`). The version-bump path is thereby documented by contrast: a NEW
key or op (e.g. P3 `ocr`/`rotate`/`convert` already in the plan's op table, or any P3 report
field that must live in the FILE) would still be v1 for ops/params the table names, and v2
only for genuinely novel keys — the handshake discipline (`versionHandshakeRefusesNewerSchema`)
is already tested and does not move.

---

## 2. Transactional-semantics contract (per step, per file, per run — unchanged invariants + three new ones)

The landed chain contract is RESTATEDED as the floor P2 may not regress (all verifiable by
the existing tests):

1. Per mutating step: unique SafeSave candidate → engine seam writes the candidate →
   validation (reopens as PDF; page count equals the chain entry's) → promote to `current`.
2. `pdfa-check` validates the candidate in flight, never touches a destination.
3. A failed step aborts the file's chain; every intermediate removed on every outcome; the
   original byte-identical; ONE `commitFileToDestination` at the end.
4. Failure text is `techDetail`-grade and file-attributed; blocked steps fail pre-flight
   with whyNot + alternative — never a silent skip.

New contracts P2 adds:

- **N1 (Bates):** within a bates-bearing run, file *n+1*'s `startNumber` = file *n*'s
  `lastNumberOut` + 1, evaluated strictly in the run's list order; a `bates` step explicitly
  pinning `startNumber` in its params restarts the sequence AT THAT FILE and discloses the
  restart in the report (the P1 plan §2.2/§3.5 semantics). Bates is page-count invariant
  (overlay), so the chain validation is unchanged; the bates step additionally reports
  per-file first/last numbers.
- **N2 (stop):** `onFileFailure: "stop"` halts the queue at the next file boundary after a
  file-scoped failure — never mid-file, never mid-chain; not-yet-run files are reported as
  not-run with the policy reason (never `success`, never silently dropped — the U08
  remaining-count truthfulness rule).
- **N3 (batch-scoped abort):** batch-scoped failures (§4.6 classification) abort the run at
  the current file boundary; files already committed STAY committed (a completed file is
  user data — no rollback theater), and the report names the batch-scoped cause once, plus
  every file not attempted.

---

## 3. Report shape extension (per-step records)

`BatchFileResult` gains an ordered per-step vector
`{ stepIndex, op, label, status ∈ {ok, failed, blocked, skipped}, bytesIn, bytesOut,
durationMs, firstBates, lastBates, detail }`:

- `bytesIn`/`bytesOut` are measured on-disk sizes of the chain links (input file for step 0;
  each candidate as produced) — the measured-readout honesty moat (M8), the same discipline
  as `CompressDialog::formatCompletionReport`. No estimates anywhere.
- The report table renders per file (rows expandable to per-step lines) in the existing
  batch report surface, and exports **JSON** (M5 precedent) + **CSV** (RFC-4180-pinned, the
  U07 discipline). Failure `detail` carries the engine `techDetail`; blocked lines carry
  non-empty whyNot + alternative (enforced by `Capability::query`).
- Threading: the struct crosses the existing mapped-pipeline result plumbing unchanged
  (per-file value semantics, E-13 exception-to-result conversion untouched).

---

## 4. The residuals, one by one

### 4.1 `bates` op adapter (schema as specified)

`runPresetMutatingStep` gains the `bates` arm: `applyBatesNumbering(dest, opts, &lastOut)`
— options from the v1 param table (`prefix`/`suffix` ≤ 32, `startNumber` 1–2,147,483,000,
`digitCount` 1–12, six `position` values — validated by the EXISTING V4 range checks;
`knownOps()` gains `"bates"` and the not-implemented diagnostic disappears). The
`lastNumberOut` value is captured into the step record + run state (§4.2). Capability:
pure built-in engine → Available by construction (`batchPresetStepCapability`'s
built-in arm).

### 4.2 Run-ordered Bates continuity: the ordered lane (new call of record)

The P1 residual names the problem honestly: `QtConcurrent::mapped` gives no execution-order
guarantee, and per-file Bates with continuity is a strict sequence over the file list.
Options considered:

1. **Two-phase pipeline** (parallel pre-bates chain → ordered bates pass on candidates →
   parallel post-bates chain + commits): preserves parallelism, but triples the candidate
   bookkeeping (candidates must survive across phases, commits happen in a second phase,
   cancellation/reporting each need phase-aware shapes) — a large accounting surface on top
   of the G12 ledger discipline, for a speed win only large farms would notice.
2. **Ordered lane (CHOSEN):** a bates-bearing preset run executes on a dedicated sequential
   lane — one worker iterating the file list IN ORDER, each file through the unchanged
   chain (bates reads run state, writes `lastNumberOut` back). Continuity becomes a loop
   invariant; cancellation stays at file boundaries; the report rows and accounting are the
   existing shapes; the only new worker code is the loop. Cost: no file parallelism for
   that run — DISCLOSED in the run dialog pre-flight ("this preset numbers pages with Bates
   sequences, so files are processed in order — slower for large batches").
3. **Serialize per-file via a global mutex inside the mapped pipeline:** rejected — it
   quietly burns a thread pool slot per file and makes ordering an accident of lock
   acquisition rather than a stated invariant; honesty requires the order to be BY
   CONSTRUCTION, not by scheduling luck.

Rule: only bates-bearing presets take the lane (detected at staging from the preset's op
list); everything else keeps the mapped pipeline unchanged. The lane still enforces the
same pre-flight, overwrite policy and per-file transactional contract — the file-loop is
the only difference. (Two-phase remains the P3 optimization candidate if ordered-lane
throughput is measured as a problem — measurement, not taste, would reopen it.)

### 4.3 `onConflict: "rename"` (the last enum value, implemented)

Output path exists → rename instead of ask/overwrite: `stem-2`, `stem-3`, … (first free;
Windows Explorer semantics — the same de-conflict idiom `BatchPresetStore::save` uses for
ids `-2`/`-3`), re-resolved through `BatchPresetSchema::resolveNaming` for the FINAL name so
the W1-01 containment guard ("single bare path component, no separators, no `..`") applies
to the renamed result exactly as to the template result, and the overwrite pre-check and
the actual commit still agree (the resolved name is pinned at staging and re-checked at
commit; if it appeared between pre-check and commit, that is a rename-resolution retry
bounded at 999, then an honest file-scoped failure). The resolved name is reported per
file. `"ask"` and `"overwrite"` are unchanged — W1-01's rule stands untouched: `"overwrite"`
still maps to the interactive confirm path and there is NO silent overwrite anywhere.

### 4.4 `onFileFailure: "stop"` (N2)

Staging captures the policy; the worker/lanes check it at file boundaries: after any
file-scoped failure, the remaining queue is not attempted and each remaining file is
reported with `skipped=true, skipReason="run stopped by onFileFailure=stop after <file>"`
(the N3 skip bucket exists — `BatchFileResult.skipped` — so the accounting buckets stay
truthful). Pre-flight discloses the policy. Default `"continue"` unchanged.

### 4.5 Hot-folder preset ingest (unattended semantics made explicit)

The watcher (`onHotFolderChanged`) already ingests new PDFs into the file list and
auto-runs the selected op; with `OpPresetPipeline` selected this becomes preset-on-ingest
for free — EXCEPT the unattended corners, which this plan pins:

- **Unattended conflict rule (new call of record):** auto-run ingest pins the run's
  effective `onConflict` to `"rename"` when the preset says `"ask"` — a modal overwrite
  prompt from the watcher path would block the GUI on a dropped file (and W1-01 already
  forbids silent overwrite), so "ask" degrades to the safest non-interactive policy with a
  log line per degraded run ("unattended run: conflicting outputs were renamed"). The
  interactive run dialog honors `"ask"` unchanged.
- Auto-run stays single-flight (`!m_watcher.isRunning()` guard, existing); ingested files
  are marked processed by the existing `m_hotProcessed` keying (re-ingest of the same path
  requires a content change — the existing key includes stamp/size, unchanged).
- Bates-bearing presets on ingest run on the ordered lane with continuity per ingest batch;
  the report/log names the number range consumed.
- Failure policy in unattended mode: `onFileFailure` honored; batch-scoped abort (§4.6)
  leaves the watcher ARMED (the folder keeps watching; the next ingest attempts a fresh
  run) with the abort disclosed in the log.

### 4.6 Batch-scoped failure abort (N3 — the §3.2 bullet implemented)

Classification at the report/failure layer, exactly the classes the P1 plan §3.2 named:
output directory unwritable/absent, candidate creation failure (SafeSave), commit
coordination failure, disk-full-class commit errors, and a capability discovered
Unavailable mid-run that pre-flight passed (recorded as a defect AND treated as
batch-scoped — continuing would multiply identical failures). Detection rule of record:
batch-scoped = the error class is independent of file CONTENT (the same failure would hit
any file). On detection: run stops at the current file boundary (N3); report carries the
batch-scoped cause once + the files-not-attempted list; GUI and unattended surfaces differ
only in the §4.5 watcher-armed note. Already-committed files stay committed.

### 4.7 Import/export (file-copy semantics, validation before appearance)

- `BatchPresetStore::importFrom(path, QString* err)`: `loadFile` (which enforces V8
  `id == stem` + V9 caps) → conflict policy: an existing id in the store is NOT silently
  replaced — the import refuses with the existing-id diagnostic unless the caller asks for
  replace (manager dialog: "Replace existing preset 'X'?" — file copy after confirm).
  Atomic write (QSaveFile) so a failed import leaves the store unchanged (the transactional
  discipline the P1 plan §4.2 stated for the manager).
- `BatchPresetStore::exportTo(id, targetPath, QString* err)`: byte-identical copy of the
  store file (no re-serialization — the file on disk IS the shareable artifact; its bytes
  are already canonical per the codec). Existing target → interactive confirm (ask), never
  silent.
- Manager toolbar gains Import…/Export… (§4.9); rejected imports show the precise
  diagnostic and change nothing — the V1–V9 diagnostics carry the JSON path + version
  handshake wording already built.

### 4.8 Per-step measured-bytes report (§3 of this document)

Chain instrumentation: `runPresetChain` fills the step vector as it promotes candidates
(size measured before/after each step's engine call; `pdfa-check` contributes a
non-mutating row with the validator verdict + duration, no byte delta). Cost note: the
chain already serializes per step, so measuring is free I/O; the report rows replace the
current single `techDetail` string as the file's failure detail carrier (the string stays
for the log).

### 4.9 Manager + multi-step editor dialog (the §4.2 surface, now with real steps)

- `PresetManagerDialog`: left list (name, description, step-count badge, modified date) +
  toolbar New/Duplicate/Edit/Rename/Delete/Import…/Export…/Run…; right detail pane with
  per-step capability badges (green/amber+detail/red+whyNot — the `applyToWidget` idiom);
  broken preset files in the store root listed with their diagnostics (the
  `brokenFiles()` honesty surface — never silently hidden).
- `PresetEditorDialog` (modal, the "multi-step editor" residual): name/description;
  ordered step list with add (op palette — `UnavailableBuild` ops not offered),
  remove, reorder (up/down buttons; drag optional later); per-step parameter forms from the
  SAME widget families the Batch panels use (DPI spin with the 36–600 clamp seam, quality
  slider, PDF/A level combo, watermark text/opacity, redact preset checkboxes + regex list,
  Bates fields mirroring `BatesNumberingDialog`); `UnavailableRuntime` steps stay selectable
  but visibly disabled with whyNot tooltip (disclosed at design time, blocked at pre-flight
  — the landed capability rule); Save writes through `BatchPresetCodec::validate` (the
  editor cannot produce an invalid file).
- The save-from-configured-run flow (landed) remains as the second authoring path; the
  editor is where its output can be re-opened and composed into longer chains.

---

## 5. Security & honesty (delta against the P1 plan)

Nothing new enters the threat surface: the schema gains no paths/commands/URIs (bates
params are strings/ints/enum — V4-validated; import/export move bytes the validator already
governs); W1-01's containment + no-silent-overwrite rules are re-pinned EXTENDED to renamed
outputs (§4.3) and unattended ingest (§4.5); the measured-bytes report keeps the run
verifiable instead of marketing (M8); every policy degradation in unattended mode is logged,
never silent.

---

## 6. Test plan (extensions to `tests/TestBatchPresets.cpp` + one new suite)

- **Schema immobility (the zero-churn claim):** every v1 golden fixture (incl. the P1
  `web-optimize` golden) round-trips byte-stable; the three formerly-refused values
  (`onConflict: rename`, `onFileFailure: stop`, op `bates` with the full v1 param table)
  now load + validate; a NEW unknown op/key still rejects with the unchanged handshake
  diagnostic (the fail-closed policy did not loosen).
- **Bates ordered lane:** 3-file bates run → expected sequence pinned (file n+1 starts at
  file n's `lastOut+1`; per-file first/last in the report); explicit `startNumber` pin
  restarts + discloses; a bates step fault at file 2 → file 1 committed with its numbers,
  files 2–3 fail/not-run honestly, no temp residue (SHA-256 on file 1); cancellation at a
  file boundary on the ordered lane reports `canceled` rows (U08 buckets add up).
- **Rename conflict:** pre-existing output → `stem-2` created, original untouched,
  containment guard re-pinned on renamed results (adversary suite:
  `TestSweepW1PresetAdversary` gains the rename arm — template + rename cannot escape the
  output directory); pre-check/commit race pin via a fault seam that creates the resolved
  path between the two → bounded retry → honest failure.
- **Stop policy:** failure at file 2 of 4 with `stop` → files 3–4 reported not-run with the
  reason, buckets truthful; `continue` default unchanged.
- **Batch-scoped abort:** read-only output dir → run aborts with the batch-scoped cause,
  files-not-attempted listed, committed files intact; capability-vanished-mid-run defect
  class → abort + defect note.
- **Import/export:** round-trip (export → delete → import → byte-identical file, same id);
  V8 id≠stem import refused, store unchanged; existing-id import refuses, replace path
  confirms then replaces; export-to-existing confirms.
- **Per-step report:** hand-computed byte table for a two-step golden run (the P1 plan's
  §6-P1 test, now with real rows); CSV/JSON export shapes pinned (RFC-4180 discipline).
- **Hot-folder ingest:** auto-run ingests + runs the preset; conflict `ask` degrades to
  `rename` with the log line (modal-free pin — the watcher path must never open a message
  box); single-flight guard holds; bates ingest batch consumes and reports its range.
- **Editor/manager honesty (offscreen, QT_QPA_PLATFORM discipline):** editor cannot save an
  invalid preset (widgets clamped/range-checked); `UnavailableRuntime` step rendered
  disabled with whyNot; rejected import leaves the list unchanged; broken-file disclosure
  rendered.
- Effort shape: bates+lane ~1–1.5 wk; conflicts/stop/abort ~1 wk; report+export ~0.5–1 wk;
  import/export+dialogs ~1.5–2 wk; hot-folder ~0.5 wk; tests ride each unit (the house
  fail-first/negative/revert-verify discipline, `SafeSave::FailBeforeCommit` +
  `StepFaultForTesting` seams).

---

## 7. Design calls of record (pick-and-justify, none blocking)

1. **Zero schema churn — `kSchemaVersion` stays 1** (§1). Alternative: bump to v2 to mark
   the capability boundary — rejected: the v1 grammar already specified every P2 value; a
   bump would strand every P1 file behind a needless handshake and contradict the
   not-implemented-refusal policy that documented the gap.
2. **Ordered lane for bates-bearing presets** (§4.2). Alternative two-phase pipeline —
   documented, deferred to P3 on accounting-complexity grounds; measurement reopens it.
3. **Unattended ingest degrades `ask` → `rename`** (§4.5). Alternative: skip conflicted
   files — rejected: silently not-processing an ingested file is the hot-folder equivalent
   of a silent skip; renaming processes the file and says so.
4. **Batch-scoped abort keeps committed files committed** (§4.6). A run-level rollback
   (deleting committed outputs) would destroy user data on the strength of a heuristic
   classification — never.

---

## 8. TL;DR (10 lines)

1. P1 landed model+codec+store+chain; P2 closes the seven residuals: bates, rename/stop,
   batch-scoped abort, import/export, per-step byte report, preset-on-ingest, editor UI.
2. Zero schema churn: every P2 value was already in the v1 grammar; the three
   not-implemented refusals flip to implementations; `kSchemaVersion` stays 1; P1 goldens
   byte-stable.
3. Bates continuity via the ordered lane: bates-bearing presets run sequentially in list
   order (continuity = loop invariant, disclosed parallelism cost); two-phase pipeline is
   the recorded P3 optimization.
4. `onConflict: rename` = stem-2/3… resolved through the W1-01 containment guard; no silent
   overwrite anywhere, W1-01 rule untouched.
5. `onFileFailure: stop` halts at the next file boundary; remaining files reported with the
   reason (U08 truthfulness).
6. Batch-scoped failures (disk, dir, commit coordination, vanished capability) abort at the
   boundary, keep committed files, name the cause once.
7. Per-step measured-bytes report (on-disk sizes, durations, Bates ranges) + JSON/CSV
   export — measured readouts, never estimates (M8).
8. Import/export = validated, atomic, confirm-on-conflict file copies; broken files never
   silently hidden (`brokenFiles()` surface).
9. Hot-folder preset ingest with the unattended rule: `ask` degrades to `rename`, logged;
   watcher never opens a modal.
10. Manager + multi-step editor dialogs complete the §4.2 surface; capability disclosure at
    design/import/run time unchanged; no decision requests, no new dependencies.

---

## 9. Source register

**Codebase (pinned `ec9f16f6`):** `src/core/BatchPreset.h:30–33` (not-implemented refusal
policy), `:37–73` (model incl. `onConflict`/`onFileFailure` + W1-01 note), `:79–88`
(`kSchemaVersion`, caps, `knownOps` residual rationale), `:96–111` (`resolveNaming` +
containment), `:156–192` (store: no import/export — the §4.7 subject; `brokenFiles`);
`src/core/BatchPreset.cpp` (V1–V9 diagnostics, canonical serialization, id de-conflict
`-2/-3` — the §4.3 rename idiom); `src/modes/BatchMode.h:39–59` (`BatchFileResult` — the
§3 extension point; `skipped`/`skipReason` bucket), `:152/234/241/301` (store + display +
`m_selectedPreset`); `src/modes/BatchMode.cpp:951–979` (output resolution + overwrite
confirm), `:830–857` (hot-folder ingest + auto-run), `:991–1053` (level codes + mutating
arms — the §4.1 `bates` arm joins here), `:1061–1087` (check step), `:1089–1176` (the
chain — §2 floor + §4.8 instrumentation), `:1382–1465` (worker capture + preset arm);
`src/engines/SafeSave.h` (candidate/commit/fault seams); `src/engines/VeraPdfValidator.h`
(check engine); `tests/TestBatchPresets.cpp` (13 slots — the immobility + transactional
pins extend these), `tests/TestSweepW1PresetAdversary.cpp` (W1-01 pins — the rename arm);
`docs/audit/CURRENT-EVIDENCE-LEDGER-2026-09-05.md` (R26 rows; SWEEP-W1 W1-01);
`docs/research/batch-presets-implementation-plan.md` §1.3 (op table incl. bates row),
§2.2 (param table), §2.4 (fail-closed + handshake), §3.2 (failure policy incl. the
batch-scoped bullet P2 implements), §3.3 (report shape), §3.5 (Bates continuity),
§4.2/4.3 (manager/editor/run surfaces), §6 (phase gates).

**Format note:** follows the house design-doc format of
`docs/research/batch-presets-implementation-plan.md` and
`docs/research/send-for-signing-implementation-plan.md` (pinned-revision source register,
graded claims, honest scope cuts, explicit design-calls section).
