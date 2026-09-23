# FEATURE-PLANS-2026-09-21 — Summary Addendum: the Next Implementation Wave's Three Design Plans

**Date:** 2026-09-21 (written 2026-09-22) · **Repo:** pdf-parity @ branch `feat/feature-plans`
(based on `feat/parity-glm` tip `ec9f16f6`) · **Lane:** feature-continuation DESIGN ONLY
(docs; zero src/tests changes). Worktree of record:
`C:\Users\User\Projects\pdf-featplans` (throwaway; coordinator folds the branch).

This addendum summarizes the three plans produced by the feature-continuation design lane,
each a full house-format design document in `docs/research/`:

| Plan doc | Covers |
|---|---|
| `docs/research/form-js-p3-openaction-plan-2026-09-21.md` (commit `9eefe410`) | form-JS Phase 3 — consent-gated document scripts + the JS-01 worker-process boundary |
| `docs/research/accessibility-auto-tagging-plan-2026-09-21.md` (commit `29952193`) | T2-4 accessibility auto-tagging P2 — structure-tree construction engine |
| `docs/research/batch-presets-p2-plan-2026-09-21.md` (commit `728e9284`) | batch-presets P2 — the seven P1 residuals (bates lane, policies, reports, manager UI) |

Each plan: pinned-revision source register, graded claims, phased plan with integration
anchors, explicit decision requests or recorded design calls, test plans with fail-first /
negative / revert-verify teeth, honest scope cuts.

---

## 1. form-JS P3 — consent-gated document scripts (`9eefe410`)

**Headline design.** Two independently landable movements. **P3a** implements the R05/JS-01
review's standing residual — *"in-process execution is not an OS sandbox … move execution
into a restricted worker process with a parent watchdog"* — as `gpformjs-worker`, a
script-only executable (quickjs-ng + AFormShim, nothing else; same pinned 0.15.0-1 MSYS2
package), driven by one length-prefixed JSON request/response over stdio with a version
handshake; the parent owns a watchdog timer (deadline + 750 ms grace → hard kill), a Windows
job object (kill-on-close + process-memory cap) and a new honest error kind `EngineLost`;
all four LANDED field-event seams migrate behind a `FormJsExecutor` seam with the 29
TestFormJsCalc goldens as the migration's safety net (the in-process executor is retained as
the revert-verify reference + escape hatch). **P3b** adds the document-level surface: an
open-time raw-dict probe (`/OpenAction` JS + `/Names /JavaScript` name tree + catalog `/AA`
presence) at the §9.16 `MainWindow::openDocument` choke point; a `FormScriptConsentDialog`
mirroring the R24-W2 `OcspConsentDialog` idiom (Allow once / Allow for this document / Deny;
ARC01-identity-keyed memory — reopen re-asks; deny never remembered); document scripts run
once per consent as ONE run unit (named scripts then OpenAction — a pinned convention,
disclosed) under the inherited sandbox caps (16 MiB, whole-operation deadlines 250 ms/script
+ 1 s/run unit, 4 MiB transfer, zero I/O, recorded egress no-ops); a session report surfaces
"ran / blocked verbs / failed" on the document badge. Doc-level field writes are
session-display only — disk `/V` still requires the user's own save.

**Reconciliation with R24.** A new managed key `forms/scriptPolicy` = `"ask" | "never"`
mirrors `signing/ocspNetworkPolicy`'s fail-closed floor (unknown values refuse; `"never"`
suppresses the dialog and refuses with a whyNot naming the key). No `"always"` value, ever.
Adding the key to the R24 bounded allowlist is a **decision request (D1)**, not taken by the
plan. Catalog `/AA` lifecycle events (`/WS /DS /WP /DP /DC`): probe-reported, never run —
deferred with reasons (lifecycle interception surface, close-boundary policy collision, low
parity value). Sanitize keeps stripping ALL script entry points unchanged. MOTW/
Zone.Identifier deferred: no ADS reading exists in the app; the uniform consent dialog is
P3's origin story.

**Decision requests:** D1 `forms/scriptPolicy` allowlist key; D2 approval of the worker
executable as a distribution surface; D3 keystroke-over-IPC vs disclosed in-process
keystroke (decided by a P95 ≤ 30 ms latency budget at implementation).

**Residuals after this wave:** field-event worker migration honesty depends on the goldens
over IPC; doc-global script persistence is per-run-unit only (disclosed); `/AA` lifecycle
events, MOTW, JS console/authoring remain out.

---

## 2. T2-4 accessibility auto-tagging P2 (`29952193`)

**Headline design.** A `AccessibilityTagger` engine that closes the checker's biggest
reported gap (untagged document) with two grounded realities stated up front: (1)
**PoDoFo 1.1.0 has no structure-tree construction API** (verified against the 143 vendored
headers; the P1 checker's own comment records the getters-only fact) — the tree is built
against RAW DICTIONARIES on the `exportPdfA`/sanitize idiom; (2) a useful tree REQUIRES
marked content (BDC/EMC + MCIDs), so the engine performs a token-faithful content-stream
rewrite (`PdfContentStreamReader` round-trip, insertions only) — the load-bearing risk,
which lands behind a **text-preservation invariant** (re-extract the candidate page; any
divergence fails the candidate, original byte-identical) with a fault-seam negative control,
inside the SafeSave transaction discipline. Pipeline: ToUnicode-honest text-run extraction
(undecodable pages skipped + disclosed, never wrong text) → line/paragraph clustering →
document-relative font-size/weight clustering for `H1–H6` (the cluster table is SHOWN to the
user; known failure modes named) → MCID assignment → raw-dict `/StructTreeRoot` +
`/ParentTree` + `/MarkInfo /Marked true` + page `/StructParents`.

**Scope + policy discipline.** P2 = paragraphs + headings ONLY (tables/lists/ActualText/
multi-column/outline-merge are P3, each with its reason); single-column reading order
assumed with a column-suspect flag; already-tagged documents REFUSED (merge is P3 with
confirmation); `/Alt` is NEVER auto-generated — images are prompted per-image through the
landed `SetImageAltText` fix seam; undescribed images are excluded from the tree and
disclosed (an empty `/Alt` would be a false statement). The not-a-conformance-claim sentence
is extended verbatim into the tagging UI; "PDF/UA" appears only inside the disclaimer.

**Acceptance tests.** Self-owned structural walk (every `/K` resolves, MCIDs match marked
content, `/Pg` consistency), the text-preservation invariant, ToUnicode honesty fixtures,
Alt-policy pins, commit-fault atomicity; then the INDEPENDENT reader through the existing
veraPDF subprocess integration (UA-1 profile where the bundled CLI supports it, else PDF/A
with the CLI dependence recorded) — including a corrupted-tree negative control proving the
validator reads what we wrote, plus cross-verifier agreement on well-formed vs broken.

**Decision requests:** none. **Residuals:** heuristic accuracy is disclosed, never claimed;
tables/lists/ActualText/multi-column/outline-derived headings = P3; OCR-first for scanned
pages = P3 candidate (wrong-text hazard without review).

---

## 3. batch-presets P2 (`728e9284`)

**Headline design.** Closes the seven P1 residuals — bates + run-ordered continuity,
`onConflict "rename"`, `onFileFailure "stop"`, batch-scoped failure abort, import/export,
per-step measured-bytes report, hot-folder preset ingest, and the manager + multi-step
editor dialog — with a **zero-schema-churn** claim as the load-bearing discipline: every P2
value is already in the v1 grammar the P1 plan specified (the `bates` param table; both enum
values), the P1 build merely refused them as not-implemented, so `kSchemaVersion` stays **1**,
every P1 golden stays byte-stable, and the fail-closed/handshake policy loosens not at all
(pinned by test). Bates continuity rides a dedicated **ordered lane**: bates-bearing presets
run sequentially in list order (continuity is a loop invariant; the parallelism cost is
disclosed in pre-flight); the two-phase pipeline is recorded as the P3 optimization with its
accounting-complexity reason. `rename` = stem-2/3… resolved through the W1-01 containment
guard (no silent overwrite anywhere — the landed rule is re-pinned, extended to renamed
outputs). `stop` halts at the next file boundary with truthful not-run reporting. Batch-
scoped failures (unwritable dir, SafeSave/candidate failures, commit coordination, vanished
capability = defect) abort at the boundary, KEEP committed files, name the cause once.
Import/export = validated atomic file copies (V8 id==stem enforced; confirm-on-conflict;
`brokenFiles()` disclosure). The per-step report carries measured on-disk bytes per chain
link + durations + Bates ranges, exportable JSON + CSV (M8 measured-readout moat; U07 CSV
discipline). Hot-folder ingest runs the selected preset unattended with a new call of
record: `ask` degrades to `rename` with a log line (the watcher path must never open a
modal; W1-01 forbids silent overwrite). The manager/editor dialogs complete the P1 plan's
§4.2 surface with the same capability-honest design/run-time gating.

**Decision requests:** none. **Residuals:** two-phase bates pipeline = P3 optimization;
`ocr`/`rotate-pages`/`convert` terminal ops = P3 op set (already in the P1 plan's table);
drag-reorder in the editor optional.

---

## 4. Wave notes for the implementation lanes

- **Integration order:** the three plans are mutually independent (form-JS/engines,
  accessibility/engines+UI, batch/modes+core); none blocks another. Within form-JS, P3a
  (worker) precedes P3b (consent+scripts) by design. Batch-presets P2 has no P0-style
  external gate left (the drain-race fix it once required landed with the P1 wave's
  accounting); its own tests ride each unit.
- **House discipline carried:** every plan's tests are fail-first/negative/revert-verify
  shaped; SafeSave fault seams (`FailBeforeCommit`, `StepFaultForTesting`-class,
  `TaggerFaultForTesting`-class, form-JS watchdog fault seams) are specified where mutation
  or execution is involved; ledger rows are expected in the implemented-awaiting-review
  protocol (RQ07) per feature, NOT granted by these documents.
- **Not authorized by these plans:** new dependencies (none requested anywhere), the R24
  allowlist key (form-JS D1 — explicit request), the worker executable as a shipped surface
  (form-JS D2 — explicit request), any src/tests change (this lane).
