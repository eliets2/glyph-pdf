# Form JavaScript Phase 3 — Consent-Gated Document Scripts + the Worker-Process Boundary (DESIGN ONLY, decision requests)

**Date:** 2026-09-21 · **Repo:** pdf-parity @ `ec9f16f6`, branch `feat/feature-plans` (from `feat/parity-glm`)
**Status:** Design proposal for **Phase 3** of `docs/research/form-js-implementation-plan.md`
(document-level `/OpenAction` + `/Names /JavaScript`, consent-gated) and for the **JS-01
worker-process recommendation** recorded as the residual of the R05/JS-01 review
(`docs/audit/CURRENT-EVIDENCE-LEDGER-2026-09-05.md:539–549`). No production code and no build
files are touched by this document. Section 7 carries **decision requests for the user**;
nothing here self-authorizes a policy-key addition (RQ10 discipline, extended to the R24
bounded allowlist) or a new executable surface.

**Sources for status claims (all read at pinned revision `ec9f16f6` unless noted):**
`src/engines/formjs/FormJsSandbox.{h,cpp}`, `src/engines/formjs/FormJsRunner.{h,cpp}`,
`src/engines/formjs/AFormShim.{h,cpp}`, `src/engines/FormManager.cpp` (`:94–115`
`runInTransactionCalculateCascade`, `:497` keystroke wiring),
`src/modes/FormFieldPropertiesPanel.cpp:228`, `src/core/Capability.{h,cpp}`
(`:50` `CapId::FormJavaScript`, `:610–636` `probeFormJavaScript`, `:661` probe registration),
`src/core/PolicyController.{h,cpp}` (R24 allowlist + enforcement contract),
`src/ui/OcspConsentDialog.{h,cpp}` (R24-W2 per-document consent idiom),
`src/ui/PreferencesDialog.cpp`, `src/GpMainWindow.cpp` (`:778` `openDocument` — the §9.16
open choke point, `:884` `beginDocument` boundary), `src/engines/DocumentSession.h`
(ARC01 identity), `src/engines/podofo/PoDoFoBackend.cpp:3295–3317` (sanitize strips
`/OpenAction`, catalog `/AA`, `/Names /JavaScript`),
`docs/audit/CURRENT-EVIDENCE-LEDGER-2026-09-05.md:539–549` (R05/JS-01),
`docs/research/form-js-implementation-plan.md` (§1.2 event table, §3.1 defaults, §4 Phase 3,
§6.1 runtime pin), `tests/TestFormJsCalc.cpp` (29 slots), `LICENSE-3RD-PARTY.md`.

Confidence legend follows the research-specialist 6-level scale (TRUE … UNVERIFIABLE). Engine
facts cite code at `ec9f16f6`; the plan-of-record facts cite the pinned form-js design doc.

---

## 0. Executive summary (3 sentences)

Phases 1–2 landed honest field-level execution — Calculate, Format, Validate and Keystroke run
in-process inside the R01 transaction under a cooperative-VM budget (16 MiB, whole-operation
deadlines, zero host I/O, recorded egress no-ops) — but document-level entry points
(`/OpenAction`, `/Names /JavaScript`) are still the disclosure state, and the JS-01 review's
residual stands: an in-process budget is not an OS sandbox, and its nine reproduced bypasses
are only *mitigated* (whole-operation deadline bracket) where they are not *impossible*
(worker process + watchdog). This plan ships Phase 3 in two movements that can land
independently: **P3a** moves ALL script execution into a restricted, sacrificial
`gpformjs-worker` process (one JSON-request/JSON-response IPC, parent watchdog with a hard
kill backstop, per-request process lifetime — the script-serve surface dies with the process)
and wires **P3b**'s consent gate behind it: an open-time probe finds `/OpenAction` +
`/Names /JavaScript`, a per-document consent dialog (Allow once / Allow for this document /
Deny, mirroring the R24-W2 `OcspConsentDialog` idiom) gates the run, a new managed policy key
`forms/scriptPolicy` = `"ask" | "never"` (decision request D1) provides the never-script
floor exactly like `signing/ocspNetworkPolicy`, and a "scripts ran / were blocked / failed"
session report closes the honesty loop. Scope is deliberately cut: the catalog `/AA`
lifecycle events (`/WS /DS /WP /DP /DC`) are deferred (§1.3), mark-of-the-web origin rules are
deferred behind the uniform consent dialog (§6), and field-level events stay in-process until
the worker proves itself on the new surface (§4.4).

---

## 1. Execution model: which document-level events run, and when

### 1.1 The entry-point inventory (ISO 32000-1:2008 §7.7.3, §12.6.4.16)

| Entry point | Where it lives | Acrobat behavior | P3 decision |
|---|---|---|---|
| `/OpenAction` JS action | Catalog `/OpenAction` `/S /JavaScript` `/JS` (string or stream) | Runs once, after document open | **RUNS (P3b)** — after consent, off the GUI thread, in the worker |
| Document-level named scripts | Catalog `/Names` → `/JavaScript` name tree | ALL run in name-tree order at document open (they are "document-level JavaScripts", not callable subroutines) | **RUN (P3b)** — same consent, same run unit, in tree order, after `/OpenAction` (Acrobat order: named scripts then OpenAction? Acrobat executes document-level scripts at open; the /OpenAction JS fires when the document opens. Order between the two is not normative — pdf.js runs document-level scripts first, then OpenAction. We pin: named scripts first, then /OpenAction, and DISCLOSE the choice as a pinned convention (goldens), not as an Acrobat-parity claim) |
| Catalog `/AA` → `/WC` `/WS` `/DS` `/DC` `/WP` `/DP` | Catalog `/AA` dict | WillClose/WillSave/DidSave/DidClose/WillPrint/DidPrint lifecycle events | **DEFERRED (§1.3)** — disclosure stays: probe reports them; they never run |
| Field events `/AA` `/C /F /V /K` | Field dicts | Compute/validate/gate field values | Already RUNNING (P1/P2, landed); in-process; worker migration is P4 (§4.4) |

### 1.2 The document script run unit

One **document script run** = one consent grant → one worker request that executes, in one
quickjs runtime: every named script in name-tree order, then `/OpenAction` (if it is a
JavaScript action — a non-JS `/OpenAction` such as a destination is *not* a script and is
neither consent-gated nor executed; it only counts in the probe disclosure). Within the run
unit the Acrobat "document globals persist across document-level scripts" semantics hold
(shared runtime, one field-value snapshot installed at request start). Across run units
(e.g. a later re-consent after a document reload) a FRESH runtime is used — no global
persistence beyond the file's own `/V` state, which is exactly what Acrobat's "document
reload resets scripting state" does.

What document scripts can DO in our runtime (host-object honesty, inherited from P1/P2):

- READ: the field-value snapshot (`/V` strings), field names/types/required flags — the same
  read-only proxy the field events use (AFormShim).
- WRITE (session-display only): `this.getField("x").value = …` sets the field's value in the
  SESSION snapshot and is REPORTED in the script summary. It never writes `/V` to the file by
  itself (the format-event rule generalized: presentation ≠ document state). A prefilled
  field reaches disk only through the normal user fill/save path — the user's commit, the
  R01 transaction. This is disclosed in the consent dialog ("changes scripts make to field
  values are shown but only saved if you save the form").
- EGRESS: `doc.submitForm`, `doc.mailDoc`, `doc.exportData`, `app.launchURL`, `app.mailme`,
  `app.execDialog`, `app.media` — hard no-ops, each recorded in `JsEvalResult::blocked`
  (landed behavior, unchanged).
- NEVER: file/network/OS primitives (no quickjs-libc module exists in the runtime — landed),
  document structure mutation, annotation creation, page navigation (out of the host object
  whitelist = absent, not stubbed — landed rule).

### 1.3 Why the catalog `/AA` lifecycle events are deferred (honest scope cut)

They were never in the Phase 3 bullet ("`/OpenAction` + `/Names /JavaScript`", design doc
§1.2/§4-P3), and each one is a different engineering problem than the one P3 solves:

- `/WS /DS /WP /DP` need lifecycle interception points wired into every save/print dispatch
  (Save/Save As, batch commits, print pipeline) — each is a new decision point in
  latency-critical paths, each needs its own consent semantics (a WillSave script running
  inside a batch worker's save is a different threat shape than an open-time script).
- `/DC` collides with the close-boundary policy family (G14 checked-transition semantics in
  `MainWindow::openDocument`/`closeEvent`): a script that must run before close can veto a
  close, which is a UX commitment P3 does not make.
- Parity value is low (synthesis T1-3 demand was calculated/validated FORMS; open-time
  scripts are the observable class — lifecycle automation is the Acrobat-action class we
  deliberately do not copy; batch-presets plan §1.3 made the same call for Action Wizard).

The checker-style probe (§2.1) reports their PRESENCE in the document disclosure so the
honesty surface still names them ("this document also carries save/print/close scripts;
GlyphPDF does not run them").

---

## 2. Consent surface

### 2.1 The open-time probe (detection, never execution)

At the §9.16 open choke point (`MainWindow::openDocument`, which every open path already
funnels through — File>Open, Welcome card, recents, drag-drop), after the engine load
succeeds and BEFORE `beginDocument` publishes the identity, a raw-dictionary probe walks the
catalog: `/OpenAction` (is it `/S /JavaScript`?), `/Names → /JavaScript` name tree (count +
names; bounded, string-safe walk — the `AccessibilityChecker` walk idiom: `resolve()`,
visited set, depth cap). Result type `DocScriptInventory { openActionJs, namedScripts,
lifecycleAA: bool }`. Cost: dictionary reads only; no strings decoded beyond names; capped
walk (the checker's `kA11yMax*Findings` pattern for the name list).

Anchor honesty: the probe must run against the SAME loaded bytes the viewer shows. The
engine-resident document is PoDoFo-backed (`loadDocumentForEditing` at `GpMainWindow.cpp:880`)
— the probe reads through it (or reopens the path read-only if the resident handle is
coordinated away; the SafeSave handle-coordinator pair is the existing idiom). No second
parse of untrusted bytes beyond what open already performed.

### 2.2 The consent dialog (`FormScriptConsentDialog` — the OcspConsentDialog idiom)

Mirrors `OcspConsentDialog` (R24-W2) structurally so the tests and the disclosure discipline
transfer:

- Buttons (pinned objectNames for offscreen tests): **Allow once** / **Allow for this
  document** / **Deny**. Session-scoped memory keyed by `DocumentSession::documentGeneration()`
  (ARC01 identity — a same-path reopen is a NEW document and asks again, which is the correct
  security posture for scripts, unlike OCSP's convenience memory). **Deny is never
  remembered** (a misclick cannot lock the user out; re-opening or re-loading asks again).
- Disclosure text names, concretely: what will run ("this document contains N document-level
  scripts and an open-action script authored inside the PDF"); what they cannot do ("the
  script engine has no network or file access; submit/email/URL verbs are blocked and
  reported"); what denial means ("the document opens fully usable; calculated field values
  keep their stored values; nothing else changes"); and that field-level calculation
  (landed P1/P2) runs regardless — it cannot act, only compute (design doc §3.1 decision,
  unchanged).
- The dialog is MODAL over the workspace but the document is ALREADY loaded and rendered
  underneath — consent gates the SCRIPT RUN, not the open. A deny (or a crashed worker, or a
  policy floor) leaves a fully usable document minus script effects: the design doc §4-P3
  refusal contract.

### 2.3 Policy + preference: `forms/scriptPolicy` (decision request D1)

- User preference `forms/scriptPolicy`: `"ask"` (default) | `"never"`. `"never"` is the
  fail-closed floor: scripts are refused WITHOUT any dialog, with an honest whyNot naming the
  setting and the way out (exactly the `signing/ocspNetworkPolicy = "never"` shape,
  `OcspConsentDialog.h:17–24`). Unknown values refuse (the
  `padesLevelFromSetting`-style floor mapping). No `"always"` — a never-ask-again always-run
  value is an anti-recommendation for script execution (the OCSP consent design reached the
  same shape for egress).
- Managed-policy key of the same name (R24): adding a key to the bounded R24 allowlist is a
  governance act (PolicyController.h: "A key joins [the allowlist] …" + the RQ10 extension in
  the send-for-signing plan header) — **requested, not taken** (§7 D1). If approved:
  allowlist row, Preferences managed-row rendering, persistSetting write guard and the
  support-bundle policy section come from the EXISTING R24 machinery unchanged
  (`PolicyController::effectiveValue` precedence; `TestPolicyWiring`-style observable pins:
  `"never"` → no dialog, refusal whyNot names the policy).
- Interaction rule: policy wins over the per-document memory at load time; a policy-managed
  `"never"` suppresses the dialog entirely (the ocspNetworkPolicy floor behavior).

### 2.4 Mark-of-the-web: deferred, with the reason on the record

The design doc §3.1 sketched Zone.Identifier-aware default-deny for network-origin files.
No MOTW/ADS reading exists anywhere in the app today (grep-verified at `ec9f16f6`), so
shipping it in P3 adds a brand-new origin-intelligence surface (ADS read, delete-on-save
etiquette, save-as clearing) to a phase whose load-bearing new thing is the worker boundary.
P3 ships the UNIFORM consent dialog instead — the user is in the loop on every script-bearing
open regardless of origin. MOTW-strictness (auto-deny + "unblock" affordance) is recorded as
a P4 candidate, dependent on D1's key (a policy can already express the admin version of the
same intent today: `forms/scriptPolicy: "never"`).

---

## 3. Sandbox inheritance (what P3 does NOT re-decide)

Every cap and rule below is LANDED behavior in `FormJsSandbox` and is inherited verbatim —
P3 adds no new knob:

| Rule | Landed value (FormJsSandbox.h:33–69) | P3 note |
|---|---|---|
| Memory | `JS_SetMemoryLimit` 16 MiB; stack 1 MiB | one runtime per request |
| Deadline | ONE absolute whole-operation deadline bracketing setup → script → exception reads → result collection (R05/JS-01) | per script: 250 ms event class; per RUN UNIT: 1000 ms cascade-class budget shared across named scripts + OpenAction (the cascade deadline is the honest analog: a document with 30 named scripts is one cascade) |
| Host transfer | script text ≤ 4 MiB; any JS→C++ string ≤ 4 MiB | the IPC response cap (§4) re-uses the same number |
| I/O | zero host I/O — no quickjs-libc module ever initialized | unchanged |
| Egress verbs | recorded no-ops (`JsEvalResult::blocked`) | the run report surfaces them (§2.2 badge) |
| Errors | classified: Syntax / Exception / Timeout / Memory (+ new `EngineLost`, §4.3) | failure of one named script skips it and continues the run unit; timeout/memory of any script ABORTS the remaining run unit (engine state untrusted — the cascade policy verbatim) |
| Numbers | `canonicalNumberString` for any `/V`-destined value | session-only writes here; same canonical form so a later commit is byte-stable with P1 goldens |
| Engine pin | quickjs-ng 0.15.0-1, `GLYPHPDF_QUICKJS_PIN` (design doc §6.1) | the worker links the SAME pinned package — one runtime of record, two link sites |
| No-engine build | supported disclosure state (§6.2) | worker absent = doc scripts disclosed-blocked; field events keep their `#else` bodies |

`/AA /DC /WS` etc. (deferred, §1.3): probe-reported, never executed. Sanitize is UNCHANGED:
`PoDoFoBackend.cpp:3311–3317` keeps stripping `/OpenAction`, catalog `/AA` and
`/Names /JavaScript` — the redaction bundle's "remove scripts" contract stays byte-for-byte
the existing behavior, and it remains the user's way to PERMANENTLY remove what consent only
temporarily declines.

---

## 4. The worker-process boundary (JS-01 recommendation, concretized)

### 4.0 The review condition being implemented (citation of record)

Ledger R05/JS-01 residual (`CURRENT-EVIDENCE-LEDGER-2026-09-05.md:549`): *"IN-PROCESS
EXECUTION IS NOT AN OS SANDBOX — this boundary is a cooperative-VM budget, not a security
kernel; the review's NEXT boundary stands as the recommendation: move execution into a
restricted worker process with a parent watchdog (hard kill backstop, host-side resource
control)."* Nine deadline bypasses were reproduced pre-fix and are now bracketed by the
whole-operation deadline; the two residual classes that remain in-principle in-process are
(a) an engine-internal hang the interrupt handler cannot reach (interrupt-cadence latency is
bounded but nonzero) and (b) the fact that hostile bytecode runs mapped into the user's own
process — any engine 0-day yields the app's privilege. A sacrificial short-lived process
converts both into "a worker died; here is an honest error".

### 4.1 What moves out-of-process (and what deliberately stays)

- **Moves:** quickjs runtime creation, shim install, field-snapshot install, script eval,
  exception classification, log/blocked collection — everything that touches hostile
  bytecode. Encoded in `FormJsSandbox`-shaped terms: the ENTIRE engine half of the seam.
- **Stays in-process (host side):** the probe (§2.1), consent state, field-value snapshot
  SERIALIZATION (the QVariantMap→JSON handoff), any `/V` write, the R01 transaction, the run
  REPORT and its UI. The host never runs script text; the worker never touches a file handle
  beyond its stdio.
- **Consequence:** the worker binary needs quickjs-ng + the shim sources and NOTHING else —
  no Qt GUI, no PoDoFo, no OpenSSL. It is a small second executable in the same MSYS2 UCRT64
  toolchain (new CMake target; build-matrix cost noted in §6).

### 4.2 The IPC surface (one request → one response → exit)

- Transport: the worker's stdin/stdout only. No sockets, no shared memory, no temp files
  (a temp-file channel would be a filesystem round-trip for hostile bytes we do not need).
- Framing: first line `gp-formjs-ipc 1\n` (version handshake, both directions; mismatch =
  immediate honest refusal — the preset version-handshake idiom). Then one length-prefixed
  UTF-8 JSON request; one length-prefixed JSON response; process exits (clean exit code 0 =
  response delivered; anything else = `EngineLost`).
- Request (superset for all event kinds — field events reuse it in P4):
  `{ requestId, kind: "openaction"|"named"|"event"|"keystroke", script, fieldName, eventKind,
     currentValue, change, selStart, selEnd, fieldValues: {name→value}, limits: {memoryBytes,
     stackBytes, deadlineMs, maxScriptBytes, maxTransferBytes} }`. `limits` travels FROM the
  parent — the worker does not trust its own defaults for caps, and the parent enforces the
  response cap on receipt regardless of what the worker claims (defense in depth; the landed
  `SandboxLimits` numbers are the only values the parent will send).
- Response: exactly `JsEvalResult` shape + `{ engineUs, lost: bool, lostReason }`.
  `blocked[]` and `logs[]` ride back for the honesty surfaces.

### 4.3 The parent watchdog (the "hard kill backstop")

- The parent arms a watchdog timer per request: `deadlineMs + grace` (grace = 750 ms; the
  whole-run-unit budget governs, not the per-script one). On expiry: `QProcess::kill()`
  (TerminateProcess) — no cooperative teardown is attempted with hostile code, that is the
  lesson of JS-01.
- Windows job object with `JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE` is attached at spawn so an
  orphaned/killed worker can never outlive the app or leave grandchildren (plain win32 via
  the existing pattern of direct system-API use where Qt has no wrapper — design-grade note:
  the job object is created per worker, the handle closed in the parent's destructor path).
- Crash/exit-code accounting: nonzero exit, missing handshake, malformed/oversize response,
  and watchdog expiry ALL classify to one new honest error kind, `EngineLost`
  ("the script engine stopped responding and was stopped; the script's effects are
  discarded") — field-attributed and run-attributed, never silent, never a wrong value.
- Resource control: the worker also sets its own `JS_SetMemoryLimit`/stack/deadline from the
  request (cooperative, for clean classification), while the parent's kill is the ENFORCED
  bound (OS, not cooperative). Memory-ceiling for the process itself is policy-token
  (job object `JOB_OBJECT_LIMIT_PROCESS_MEMORY` at a fixed 256 MiB process cap — the
  interpreter plus its heaps cannot starve the host machine even before the deadline fires).
- Startup probe: the worker binary's presence + handshake is checked ONCE per app run (and
  its absence is the `UnavailableBuild`-class disclosure, reusing `probeFormJavaScript`'s
  honest no-engine copy with worker-specific wording). Absent worker ⇒ document scripts are
  consent-blocked with that disclosure (FAIL CLOSED — there is no in-process fallback for
  document scripts; a silent fallback would re-open exactly the boundary JS-01 closed).

### 4.4 Sequencing: new surface goes first, migrated surface follows

- **P3a (worker + field-event migration):** land the worker binary + IPC + watchdog, and
  switch ALL FOUR landed field-event seams (`runCalculateCascade`, `formatForDisplay`,
  `runValidateEvent`, `runKeystrokeEvent`) to it behind one internal seam
  (`FormJsExecutor` with in-process / worker implementations; the in-process one is RETAINED
  as the test/revert-verify reference and the `GLYPHPDF_FORMJS_INPROCESS=1` escape hatch).
  Rationale for migrating the landed path FIRST rather than adding doc scripts in-process:
  the JS-01 residual names the in-process shape itself as the debt; landing doc scripts
  in-process first would deepen it, and the field-event paths already have 29 green
  TestFormJsCalc slots that become the migration's safety net (the goldens must stay green
  over the IPC — that is the strongest possible validation of the worker before any new
  surface rides on it).
- **P3b (probe + consent + document script run):** rides on the proven worker. The keystroke
  path deserves one honesty note: per-keystroke IPC round-trips add latency to typing; the
  design accepts the worker's cost for validate/format/calculate/open paths, and for
  keystroke events the worker MUST be warm-started per field focus (a persistent worker per
  document session, still behind the watchdog) — the per-request cold worker is for
  calculate/open/named classes; warm workers are reaped at blur/close. If keystroke latency
  over IPC still fails the typing feel bar, the honest fallback is per-field disclosure
  (keystroke scripts run in-process, disclosed in the capability detail) — decided by
  measurement at implementation, pinned by a latency test budget (P95 keystroke commit
  overhead ≤ 30 ms on the reference machine; the measurement, not taste, picks the shape).

### 4.5 Landed surface that does NOT change

Sanitize (strips scripts — unchanged), the no-engine disclosure build (unchanged; worker
absence is its doc-script analog), the capability probe's copy (extended to name the worker),
the R01 transaction policy (a document-script failure NEVER blocks open/save; a validate
event still fails closed), and every user-visible honesty idiom (field-attributed errors,
blocked-verb reporting).

---

## 5. Security & honesty statement

- Consent is a UX control, the policy floor is a preference control, and the worker is the
  enforcement boundary — the plan never claims the dialog "secures" anything (the OCSP
  consent design's discipline: the gate decides WHETHER to run; the sandbox decides WHAT a
  run can do; the watchdog decides WHEN it stops).
- The probe, the consent memory and the run report are all keyed to ARC01 document identity —
  a re-open re-asks; there is no cross-document script state.
- The worker gains no privileges: it reads one request from its parent and writes one
  response. It cannot open files (no file I/O code links into it), cannot listen (no socket
  code), and its job object caps its memory. Its compromise yields at most the request bytes
  (the field values already in the app's process) for a few hundred milliseconds of lifetime.
- Every refusal states the way out (grant consent / change setting / sanitize the document);
  every acceptance states what ran, what was blocked and what failed (the document badge:
  "document scripts: 2 ran, 1 blocked verb, 0 failed" — the design doc §4-P3 summary).

---

## 6. Phased plan (integration anchors at `ec9f16f6`)

### P3a — worker process + executor seam + migration (2.5–3.5 wk)

- `src/formjs-worker/main.cpp` + CMake `add_executable(gpformjs-worker …)` (WinMain-free
  console app; links qjs only): handshake, request parse (fail-closed: unknown kind/limits →
  error response, exit 2), sandbox lifecycle, response write. Reuses `FormJsSandbox.cpp` /
  `AFormShim.cpp` sources (they are engine-host agnostic already).
- `src/engines/formjs/FormJsExecutor.{h,cpp}`: `run(event…) → JsEvalResult` with
  `InProcessExecutor` (today's sandbox, kept for tests + escape hatch) and
  `WorkerExecutor` (QProcess, watchdog, job object, `EngineLost` classification).
  `FormJsRunner` internals switch to an executor instance (small, mechanical diff — the four
  public signatures do not change).
- Tests (`tests/TestFormJsWorker.cpp`, fixture style of `TestFormJsCalc.cpp`):
  handshake mismatch refused; malformed request → error response + exit 2; watchdog expiry →
  `EngineLost` ≤ deadline+grace+slacks (a `while(true){}` script must come back as Timeout
  via the worker's own interrupt handler normally, and as EngineLost via the kill when the
  interrupt is suppressed by a fault seam); worker binary absent → honest disclosure, doc
  scripts blocked, field events keep working through the in-process executor (the escape
  hatch) with the disclosure; ALL 29 landed TestFormJsCalc slots green with
  `WorkerExecutor` forced (the migration proof); oversize response refused at the 4 MiB
  transfer cap; orphan-containment pin (worker killed with the parent — job object).
  Fail-first + negative + revert-verify per D03 (the harness kills that JS-01 used are the
  revert-verify teeth here too).
- **Effort note:** worker+IPC ~1 wk; executor seam + migration + goldens-over-IPC ~1–1.5 wk;
  watchdog/job-object tests incl. fault seams ~0.5–1 wk.

### P3b — probe + consent + document script run (1.5–2 wk)

- `DocScriptInventory` probe (§2.1) at `MainWindow::openDocument` (after `engineReady`,
  before `beginDocument`); `FormScriptConsentDialog` (§2.2, offscreen-testable like
  `OcspConsentDialog`); `forms/scriptPolicy` preference + (pending D1) policy key;
  document script run through `WorkerExecutor` kind `"openaction"`/`"named"` with the
  run-unit budget; session report + document badge wiring (status-bar message class of
  `formatCompletionReport`).
- Tests (`tests/TestFormJsOpenAction.cpp`, `tests/TestFormScriptConsent.cpp`): probe fires on
  seeded `/OpenAction` + name-tree fixtures (raw-dict construction in the test harness, the
  checker-fixture idiom); consent allow-once runs once and re-asks on reopen (ARC01 identity
  pin); deny leaves the document fully usable minus effects (page renders, fields fill); policy
  `"never"` → no dialog + refusal whyNot naming the key; unknown policy value → floor;
  blocked `submitForm` recorded and surfaced; named-script syntax error skips that script,
  discloses, runs the rest; timeout aborts the remaining run unit with the "skipped" honesty
  list (the cascade policy); doc-level field writes show in the fill UI and reach disk only
  via user save (transaction pin: saved bytes carry `/V` only after user commit);
  no-engine/no-worker build → disclosure state, self-skip with the honest message.
- **Effort note:** probe+dialog+policy ~4–5 d; run unit + report + tests ~4–5 d.

### Explicit scope cuts (gates, not todos)

No `/AA` lifecycle events (§1.3); no MOTW (§2.4); no `"always"` consent value; no JS
authoring/console UI; no cross-document or persistent script state; no E4X; no worker for
anything but script execution (never a general "run user code" service); no in-process
fallback for document scripts (fail closed); doc-global persistence is per-run-unit only
(disclosed — Acrobat's persistence is per-session too, and our run unit IS the session).

---

## 7. Decision requests

- **D1 — policy key `forms/scriptPolicy`** in the R24 allowlist (`"ask" | "never"`): required
  for the managed-floor half of §2.3. The user-preference half ships regardless; the policy
  key is the admin surface. Silence = preference-only (the floor still exists, just not
  machine-manageable).
- **D2 — worker executable approval:** shipping a second, script-only executable is a new
  distribution surface (packaging/signing footprint; `LICENSE-3RD-PARTY.md` unchanged — no
  new dependency, same pinned quickjs-ng). The alternative (doc scripts in-process) is
  explicitly the shape JS-01 rejected. Recommendation: approve the worker (P3a) — it is the
  review's own recommendation and the only shape that makes the JS-01 residual go from
  "mitigated" to "closed".
- **D3 — keystroke over IPC vs disclosed in-process keystroke** (§4.4): decided by the P95
  latency budget at implementation; flagged here because it is a user-visible honesty trade
  either way.

---

## 8. TL;DR (10 lines)

1. P1+P2 landed field-level execution under a cooperative budget; JS-01's residual names the
   worker process as the next boundary — this plan implements exactly that, then Phase 3 on top.
2. P3a: `gpformjs-worker` (script-only executable: quickjs + shim, nothing else), one JSON
   request/response on stdio, parent watchdog + hard kill + job object, all four landed
   field events migrate behind one executor seam with the 29 goldens as the safety net.
3. P3b: open-time probe (`/OpenAction` + `/Names /JavaScript`, raw-dict, never executes),
   per-document consent (Allow once / this document / Deny — OcspConsentDialog idiom,
   ARC01-keyed memory, deny never remembered).
4. New R24-style key `forms/scriptPolicy` `"ask"|"never"` = the never-script floor
   (decision request D1); no `"always"` value, ever.
5. Document scripts run once per consent in one run unit (named scripts then OpenAction,
   pinned convention), under the inherited caps: 16 MiB, whole-operation deadlines
   (250 ms/script, 1 s/run unit), 4 MiB transfer, zero I/O, recorded egress no-ops.
6. Doc scripts may set field values in the SESSION only; disk `/V` changes still require the
   user's own save (the format-event rule generalized).
7. Catalog `/AA` lifecycle scripts (/WS /DS /WP /DP /DC): probe-reported, never run —
   deferred with reasons (§1.3); sanitize keeps stripping ALL script entry points unchanged.
8. MOTW/Zone.Identifier: deferred; the uniform consent dialog is the P3 origin story.
9. Honesty loop: every run reports ran / blocked verbs / failures on the document badge;
   every refusal names its key, its reason and the way out; `EngineLost` covers the kill path.
10. Decision requests: D1 policy key, D2 worker executable, D3 keystroke-over-IPC latency call.

---

## 9. Source register

**Codebase (pinned `ec9f16f6`):** `src/engines/formjs/FormJsSandbox.h:14–48` (error kinds +
`SandboxLimits`), `:50–121` (contract: whole-operation deadline, zero I/O, egress no-ops,
runEvent/runKeystrokeEvent), `FormJsRunner.h:27–57` (failure policy), `:82–140`
(validate/keystroke host decision tables), `:152–155` (the P3 hooks, deliberately
unimplemented), `FormManager.cpp:94–115` (in-transaction cascade + failure reporting),
`FormFieldPropertiesPanel.cpp:228` (keystroke wiring), `Capability.cpp:610–636`
(`probeFormJavaScript` both arms), `Capability.h:50`, `PolicyController.h`
(allowlist + enforcement contract), `OcspConsentDialog.h:8–46` (consent idiom + policy key
pattern), `PreferencesDialog.cpp` (settings surface), `GpMainWindow.cpp:778–895`
(`openDocument` choke point, `engineReady` gate, `beginDocument` boundary),
`DocumentSession.h` (ARC01 identity, `documentGeneration()`), `PoDoFoBackend.cpp:3295–3317`
(sanitize strips scripts), `VeraPdfValidator.h` (subprocess-validator precedent for the
worker's process shape), `LICENSE-3RD-PARTY.md` (no new rows needed — same pinned engine),
`docs/audit/CURRENT-EVIDENCE-LEDGER-2026-09-05.md:539–549` (R05/JS-01: nine bypasses,
fix, residual), `docs/research/form-js-implementation-plan.md:61–78` (event table +
Phase-3 entry points), `:228–238` (§3.1 defaults incl. the MOTW sketch), `:319–325`
(Phase 3 bullets), `:383–399` (§6.1 runtime pin), `:401–437` (§6.2 no-engine proof),
`tests/TestFormJsCalc.cpp` (the 29 slots that gate P3a's migration).

**Format note:** follows the house design-doc format of
`docs/research/batch-presets-implementation-plan.md` and
`docs/research/send-for-signing-implementation-plan.md` (pinned-revision source register,
graded claims, explicit decision-request section, honest scope cuts).
