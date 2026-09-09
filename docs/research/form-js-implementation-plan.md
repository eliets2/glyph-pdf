# Form JavaScript Execution — Run-Side Implementation Plan (DESIGN ONLY, decision request)

**Date:** 2026-09-07 · **Repo:** pdf-parity @ `955d61d`, branch `feat/parity-glm`
**Status:** Design proposal. **NOT an authorization to add a dependency.** Per
`.context/RESEARCH-RECONCILIATION-2026-09-08.md` RQ10: *"Do not let a research note silently
authorize new network services, a scripting runtime, pricing changes or a broad print engine."*
Section 2 and Section 6 are therefore written as a **decision request for the user**.

**Sources for status claims in this doc (all read at pinned revision 955d61d):**
`src/engines/FormManager.{h,cpp}`, `src/core/interfaces/IFormManager.h`,
`src/shell/controllers/FormsController.cpp`, `src/modes/FormBuilderMode.cpp`,
`src/core/Capability.h`, `src/engines/SafeSave.h`, `tests/TestFormSafety.cpp`,
`tests/TestFormUndo.cpp`, `PRD.md` §9.6, `CLAUDE.md`, `LICENSE-3RD-PARTY.md`,
`docs/research/synthesis.md` (T1-3), `docs/research/{masterpdf,acrobat,pdfxchange}.md`,
`.context/RESEARCH-RECONCILIATION-2026-09-08.md`.

Confidence legend follows the research-specialist 6-level scale (TRUE … UNVERIFIABLE).
Web sources verified 2026-09-07; all engine facts below cite primary sources (license files,
repo release pages, MSYS2 package DB, official docs).

---

## 0. Executive summary (3 sentences)

GlyphPDF already *writes* AcroForm JavaScript (`/AA /C` calculate actions, `/AA /F` format
actions, `/CO` calculation-order registration — `FormManager::addCalculatedField`) but never
*executes* it, so every third-party form that computes totals, formats currency or validates
dates shows stale or empty computed fields; this is Tier-1 gap T1-3 with demand in 8/16
competitor tools (`synthesis.md` §2). The only realistic execution path is embedding a small
ECMAScript engine plus a hand-written Acrobat-host-object shim; **quickjs-ng (MIT) is the
recommendation** because it is the only candidate that is simultaneously permissively licensed,
actively maintained (v0.16.2, 2026-08-20), ES2023-conformant, and *already packaged for the
project's exact build environment* (`mingw-w64-ucrt-x86_64-quickjs-ng` in the MSYS2 UCRT64
repo — the same pacman channel the project mandates). Because a scripting runtime is a new
dependency and RQ10 forbids research notes from self-authorizing it, this document requests
explicit user sign-off (Section 6) and specifies an honest no-dependency fallback
(CapabilityRegistry disclosure, Section 5) that does not silently miscompute anything.

---

## 1. Scope: what "run-side form JS" means concretely

### 1.1 The gap, in one paragraph

`FormManager::addCalculatedField` (`src/engines/FormManager.cpp:799–865`) writes a field with
`/AA /C` (Calculate) and `/AA /F` (Format) JavaScript actions and registers it in the AcroForm
`/CO` array "so conforming viewers recompute it in document order" — the code comment says it,
the file proves it, and nothing in the codebase ever recomputes it. `fillForm`
(`FormManager.cpp:400`) sets `/V` explicitly and `applyFieldSnapshot`
(`IFormManager.h:98`) applies one field's `/V` transactionally; both are value *writes*, not
*evaluations*. A form authored in Acrobat with `AFSimple_Calculate('SUM', …)` therefore opens
in GlyphPDF with the author's placeholder value frozen in place. Master PDF Editor's delta
table grades our in-house calculated field "not scriptable" (`masterpdf.md` §4, TRUE), and
acrobat.md ranks the JS event model as "the #1 power-user dependency in enterprise forms"
(`acrobat.md:65`).

### 1.2 In scope (run-side only)

ISO 32000-1:2008 (publicly hosted by Adobe,
<https://opensource.adobe.com/dc-acrobat-sdk-docs/pdfstandards/PDF32000_2008.pdf>) defines
JavaScript actions in **§12.6.4.16** and the field additional-actions in **Table 220**. The
four field event classes, in the order Acrobat defines them:

| Event | `/AA` key | Fires when | Typical content | Phase |
|---|---|---|---|---|
| Calculate | `C` | another field's value changes; order from AcroForm `/CO` | `AFSimple_Calculate`, `event.value = …` | **Phase 1** |
| Format | `F` | value is formatted *for display only* | `AFNumber_Format`, `AFDate_FormatEx` | **Phase 1** |
| Validate | `V` | value changes (user commit or script) | `AFRange_Validate`, `event.rc = false` | Phase 2 |
| Keystroke | `K` | user types; `event.change` holds the pending keystrokes | `AFDate_KeystrokeEx`, custom masks | Phase 2 |

Document-level entry points:

- Catalog `/OpenAction` holding a JavaScript action (ISO 32000-1:2008, document catalog
  §7.7.3 + §12.6.4.16) — **Phase 3**, consent-gated.
- Document-level named scripts in the catalog `/Names` → `/JavaScript` name tree
  (ISO 32000-1:2008, §7.7.3 / §12.6.1) — Phase 3, consent-gated.

Everything above executes against **existing AcroForms only**. The write-side that already
exists stays as-is.

### 1.3 What the dialect requires (engine ≠ compatibility)

Acrobat's dialect is not plain ECMAScript; an engine provides syntax only. Two layers must be
authored:

**a) The `event` object.** Per-event transient object with at minimum: `value`, `rc`,
`target`, `targetName`, `source`, `name`, `type`, `change`, `changeEx`, `commitKey`, `keyDown`,
`modifier`, `selStart`, `selEnd`, `shift`, `willCommit`, `richChange/-Ex/value`
(shape verified against Mozilla's normative-subset implementation,
`pdf.js src/scripting_api/event.js` — Apache-2.0). The EventDispatcher that drives calculate
cascades from the AcroForm `/CO` array — with a re-entrancy guard against malformed cyclic
`/CO` (pdf.js keeps `_isCalculating`) — is the behavioral reference.

**b) The AF function library (the "AF shim").** Acrobat predefines ~25 `AF*` functions
(AFSimple, AFSimple_Calculate, AFNumber_Format, AFNumber_Parse, AFPercent_Format,
AFDate_Format/FormatEx/Keystroke/KeystrokeEx, AFTime_*, AFSpecial_*, AFRange_Validate,
AFMergeChange, AFMakeNumber, AFMakeArrayFromList, AFExtractNums, AFParseDateEx, AFExactMatch)
plus the host objects `event`, `this` (Document/Field), `util` (printf/printd/scand),
`app`, `color`, `console` (authoritative reference: *Acrobat JavaScript API Reference*,
publicly reachable at <https://opensource.adobe.com/dc-acrobat-sdk-docs/library/jsapiref/index.html>;
companion *JavaScript Development Guide* at `…/library/jsdevguide/index.html`). **Mozilla's
pdf.js ships a complete, license-compatible reference shim** of exactly this layer:
`src/scripting_api/aform.js` implements all 24 AF functions listed in §2.4 below (file
fetched 2026-09-07; Apache-2.0 — the same license as this project's SPDX headers, so
porting with attribution is license-clean). This collapses the shim's scoping risk from
"reimplement Acrobat's undocumented library" to "port a tested Apache-2.0 reimplementation
and pin behavior with golden tests."

Engine fidelity note: Acrobat DC embeds Mozilla SpiderMonkey 24.2 with E4X forward-ported
(MOSTLY_TRUE — secondary sources: encyclopedia.pub SpiderMonkey entry, Adobe community
threads; Adobe's own docs do not state a version number); pre-DC Acrobat was ECMA-262
Edition 3 + E4X (ECMA-357). Practical consequence: **the Acrobat dialect is a strict subset
of ES2023**, so a modern ES2023 engine never rejects Acrobat-authored scripts for *language*
reasons; the only compatibility surface is the host-object shim, which the engine does not
provide at all. (E4X is dead in practice and may be omitted — UNVERIFIED how many live forms
still use it; treat E4X as out of scope.)

### 1.4 Explicitly OUT

- **Write-side authoring** — no JS editor UI, no calculation-order authoring UI (Acrobat's
  "calculation-order UI" stays an acrobat.md recommendation; we only *traverse* `/CO`).
  The existing `FormBuilderMode` expression input (`FormBuilderMode.cpp:317–326`) stays.
- **XFA** — including static XFA fill. XFA is ISO-deprecated (removed in PDF 2.0);
  synthesis T3-4 ranks it niche; the Okular-style honest banner via CapabilityRegistry
  remains the pattern (`synthesis.md` T3-4).
- **Full ECMAScript host objects** — no `search`, `SOAP`, `database`, `Net.stream`,
  `AForm`-authoring objects, no `global` persistence across documents, no Right-to-Left
  rich-text objects.
- **Anything that makes JS an egress or filesystem channel** — see §3 (submitForm, mailDoc,
  launchURL, app.execDialog, app.media are all hard-blocked).
- **PDF XChange-style JS console** (`pdfxchange.md` §4 documents one) — a debugging console
  is authoring-adjacent; defer indefinitely.

---

## 2. Dependency decision (LOAD-BEARING SECTION — requires user sign-off)

### 2.0 The rule that makes this a decision, not a choice

- `CLAUDE.md` fixes the build environment: MSYS2 **ucrt64**, GCC 16.1.0, Qt 6.11.0,
  dependencies "via MSYS2 pacman", NOT vcpkg (`CLAUDE.md:15,47`).
- `LICENSE-3RD-PARTY.md` is the governance ledger: every linked dependency is recorded there
  with a compatibility rationale; MuPDF (AGPL) and Poppler (GPL) are **FORBIDDEN** rows;
  veraPDF is accepted **subprocess-only**; Lua 5.4 (MIT) and the Djot parser (MIT) were
  *added by prior explicit decision* for the Djot workstream. Precedent therefore exists for
  adding a permissive engine — but each addition was an accepted decision, not an agent one.
- RQ10 (reconciliation) bars this document from authorizing a scripting runtime itself.

**Consequently: implementing form-JS execution at all implies exactly one dependency decision,
made by the user.** The sections below size that decision.

### 2.1 Candidate matrix

| Criterion | **quickjs-ng** | QuickJS (Bellard) | MuJS (Artifex) | Duktape | Hermes (Meta) | V8 (Google) |
|---|---|---|---|---|---|---|
| License (verified primary source) | **MIT** — LICENSE file fetched 2026-09-07, copyright Bellard/Gordon/Noordhuis/Ibarra Corretgé 2017–2026. ⚠️ *Tasking premise said "Apache-2.0" — corrected: the engine is MIT.* | MIT (same lineage) | **ISC** (mujs.com/license.html, © 2013–2020 Artifex). ⚠️ *Tasking said "ISC/AGPL dual — check": resolved — MuJS launched AGPL in 2015 and was relicensed **ISC**; current releases are pure ISC (mujs.com, GitHub LICENSE; HN #9213248 corroborates the history).* | **MIT** (duktape.org). ⚠️ *Tasking said "Zlib" — corrected: MIT.* | MIT (github.com/facebook/hermes) | BSD-3-style + MIT components (v8.dev) — permissive but multi-file |
| Maintenance (last release) | **v0.16.2, 2026-08-20**; ~10 releases/2yr (GitHub releases page) | Alive but slow; MSYS2 package `quickjs 2026.06.04-1` implies a 2026-06-04 tarball (inference from package version; bellard.org is canonical) | **1.3.10, 2026-08-11** (GitHub tags) | Dormant: last major 2.7.0, Aug 2021; older branches labeled "no longer maintained" (duktape.org/download) | Active, but scheduled around React Native | Active, Chromium cadence |
| MSYS2 UCRT64 / MinGW-w64 feasibility | **In-repo: `mingw-w64-ucrt-x86_64-quickjs-ng` 0.15.1-1** (packages.msys2.org, fetched 2026-09-07); builds with CMake+MinGW; MSVC also supported | In-repo: `mingw-w64-ucrt-x86_64-quickjs`; **conflicts with the -ng package** (can only install one) | Plain C89-ish, builds anywhere with make/CMake; **no MSYS2 package found** → would be vendored | Single amalgamated C file — trivially portable (duktape.org) | Windows path is MSVC/clang-cl via CMake; MinGW not a supported configuration; RN-coupled build system | **MinGW not supported**: GN expects VS/clang toolchains; MSYS2 MINGW-packages issue #12568 documents GN failing ("No supported Visual Studio can be found"); v8.dev/build-gn |
| Binary impact, this app | **Measured: 0.63 MB download / 1.81 MB installed** for the whole UCRT64 package (qjs.exe, qjsc.exe, libqjs-0.dll, headers, CMake config); only runtime dep `mingw-w64-ucrt-x86_64-cc-libs` — already shipped. Static-link delta of the engine is the ~1 MB class (order of magnitude; exact .a size measured at integration). | Same class as ng | Smallest class (~100s of KB static, UNVERIFIED — no published figure; measurable at integration) | Small class (~100s of KB, UNVERIFIED) | Multi-MB (UNVERIFIED exact) | Tens of MB (UNVERIFIED exact; Chrome-class) |
| Language level | **ES2023-class** ("latest ECMAScript spec" — repo README) | ES2023-class | ES5.1-class subset (mujs.com describes lightweight interpreter; exact conformance matrix UNVERIFIED) | E5/E5.1 + partial ES2015 (duktape.org/guide) | Modern but **`eval`/`new Function` restricted** (official Features.md; issues #785/#957/#120) | Full ES2024+ |
| Acrobat-dialect risk | Syntax: none (superset). Host objects: shim required (any engine) | Same | Same, plus date-format edge cases on an ES5.1 core | Same | **Higher**: dynamic `eval`/Function-constructor limits break forms that build expressions at runtime | Same, but irrelevant given build infeasibility |
| Attack surface | Small C codebase, ASAN/fuzzing in project CI (repo claims; MOSTLY_TRUE from repo docs), no threads, no JIT → no JIT-spray class | Same core lineage | Very small C codebase | Very small, expressly embedded-oriented, no I/O by default | Large, RN-coupled | Largest by far; designed for exactly the threat model we would face, but heavyweight |
| Egress risk in engine core | Zero I/O unless quickjs-libc modules registered (opt-in by design) | Same | Zero (no stdlib) | Zero (I/O opt-in via duk_config) | RN host provides I/O — must be stripped | Node/embedder provides I/O — must be stripped |

### 2.2 Reading of the matrix

- **quickjs-ng is the only candidate that satisfies every constraint simultaneously:**
  permissive license verified from the license file, actively maintained, modern-ES (so the
  Acrobat ES3-era dialect is a syntactic subset), first-class MinGW support, and — decisively —
  **already packaged in the UCRT64 repo this project mandates**, which reduces the
  "new dependency" increment to a one-line pacman install + one `find_package`/link line + a
  LICENSE-3RD-PARTY.md row. The project already trusts this channel for Qt, PoDoFo (install
  tree), qpdf, OpenSSL, Tesseract, OpenJPEG (`CLAUDE.md:47,83`).
- **MuJS is the genuine fallback** (smaller, ISC, active) but has no MSYS2 package → vendored
  C sources → exactly the "new vendored dependency" weight the no-deps instinct guards
  against, plus an ES5.1 core whose date/number formatting corners are where tax-form bugs
  hide.
- **Bellard QuickJS** is functionally close to ng but the MSYS2 packages mutually conflict,
  and its release cadence is slower; ng is the maintained fork of it — no reason to prefer
  upstream here.
- **Duktape is disqualified by dormancy** (2021) despite perfect portability; shipping a new
  security-sensitive interpreter that no longer receives fixes is how the "old vendored JS
  engine CVE" story starts.
- **Hermes/V8 are disqualified on feasibility** (MinGW unsupported / RN-coupled; V8 GN
  toolchain failure documented in MSYS2 issue #12568) and proportionality.

### 2.3 The AF shim work an engine does NOT remove

Whichever engine is chosen, the project writes/ports (Apache-2.0 pdf.js as reference):

- `event` object + EventDispatcher over `/CO` (≈300 LoC, ref `event.js`);
- 24 AF functions (ref `aform.js`, ≈700 LoC): `AFSimple`, `AFSimple_Calculate`,
  `AFNumber_Format`, `AFNumber_Keystroke`, `AFPercent_Format`, `AFPercent_Keystroke`,
  `AFDate_Format`, `AFDate_FormatEx`, `AFDate_Keystroke`, `AFDate_KeystrokeEx`,
  `AFTime_Format`, `AFTime_FormatEx`, `AFTime_Keystroke`, `AFTime_KeystrokeEx`,
  `AFSpecial_Format`, `AFSpecial_Keystroke`, `AFSpecial_KeystrokeEx`, `AFRange_Validate`,
  `AFMergeChange`, `AFParseDateEx`, `AFExtractNums`, `AFMakeNumber`,
  `AFMakeArrayFromList`, `AFExactMatch`;
- `util` subset (`printf`, `printd`, `scand` — the date/print layer pdf.js keeps in
  `scripting_utils`), `console` (session log sink), minimal `app` (`alert` → Qt message,
  `calculate` flag), a Document/Field proxy exposing only read of `/V` per field, set of
  `event.value`, and read-only field metadata (name, type, required).

### 2.4 Honest assessment of the "no new engine" engineering alternatives

- **Write our own mini-interpreter for the AF subset:** rejected. The dialect is ES3
  *expressions plus arithmetic plus locale-flavored number/date formatting*; the long tail
  (util.printf-style formatting, date scanning, string/number coercion rules, `new Array`
  vs literal, prototype behaviors) is exactly where silent miscalculation of **tax and
  expense forms** happens. A subset engine that computes a total *slightly wrong* is a worse
  product than one that refuses to compute it — it converts a disclosed limitation into a
  data-corruption bug and breaks the honesty moat (M8). Verdict: **not realistic, do not
  attempt** (assessment, not a graded claim).
- **Ship without execution + disclose:** viable and specified in §5; it is the correct
  outcome if the user declines the dependency.

---

## 3. Security model (PDFs are hostile input)

Precedent in this codebase: the app is zero-egress (loopback-only AI lane; cloud AI is an
anti-recommendation, `synthesis.md` §3.1–3.2; reconciliation RQ05 warns CapabilityRegistry is
disclosure, not network enforcement). Form JS must not become the first scripted egress or
filesystem channel. Competitors' gating precedents: Foxit Safe Reading Mode / Action Inspector
(`foxit.md` §1.7 via synthesis T1-3), PDF-XChange per-document JS user consent since 10.4
(`pdfxchange.md:88`, GRADE TRUE).

### 3.1 Defaults

| Layer | Rule |
|---|---|
| Engine I/O | The engine core has **no** file/network/OS API (quickjs-ng: I/O lives in opt-in quickjs-libc modules — never registered; CI link-symbols test pins this: binary must not reference `fopen`/`socket`/`connect` from the JS runtime object file). |
| Host objects | Whitelist only (§2.3). Everything unrecognized is absent, not stubbed. |
| Egress verbs | `doc.submitForm`, `doc.mailDoc`, `app.launchURL`, `app.mailme`, `app.execDialog`, `app.media`, `doc.exportData` → **hard no-op** that records a blocked-action entry in the per-document session log, surfaced in the UI (honesty surface, not silent). |
| CPU | `JS_SetInterruptHandler` with a monotonic deadline: **250 ms per single event, 1 s per calculate cascade** (config ceiling in settings; never persisted into the document). `while(true){}` must abort the cascade, mark the field "calculation failed", and continue the save. |
| Memory | `JS_SetMemoryLimit` default **16 MiB** per runtime; `JS_SetMaxStackSize` default 1 MiB. |
| Lifecycle | One `JSRuntime` per open document session (Acrobat semantics: doc-level globals persist across field events), destroyed on close; manual "reset form scripting state" action. Fresh runtime per unit-test case. |
| Document-level scripts (Phase 3) | `/OpenAction` + `/Names /JavaScript`: **disabled by default for documents opened from network locations/browsers** (mark-of-the-web via `Zone.Identifier` ADS on Windows), per-document consent dialog otherwise (PDF-XChange per-doc consent pattern). Field-level events (C/F/V/K) need no consent — they cannot act, only compute. |

### 3.2 Engine attack-surface notes

quickjs-ng: small C11 core, interpreter-only (no JIT → no W^X/JIT-spray class), upstream runs
sanitizers/fuzzing (MOSTLY_TRUE from project docs/CI claims). Residual risk: parser/runtime
memory-safety bugs from hostile script text — bounded by the same mitigations that bound a
malformed PDF today (process-level crash = candidate-save abort; no persistence of
attacker state). The veraPDF precedent (AGPL code kept **out of process**) is not needed
here because the dependency license is permissive and the engine runs no untrusted *I/O* —
only untrusted *computation*; a compromise of the interpreter yields the privilege of the
process (read the open document), which is the privilege the user already granted by opening
it. This is stated so the user can weigh it — it is the honest residual risk of ANY in-process
JS engine, including MuJS/Duktape.

---

## 4. Phased plan

### Integration architecture (all phases)

The field-commit pipeline is: UI commit → `FormsController` / panel → `FormManager` mutator →
`runFormSaveTransaction` (`FormManager.cpp:115` candidate → mutate → reopen-validate →
`gp::SafeSave::commitFileToDestination` at `:157`). Form-JS execution slots in as a new
engine-side step **inside the same transaction**:

1. User field value lands (`fillForm` / `applyFieldSnapshot` — the two existing `/V` writers).
2. **Before candidate serialization**, traverse AcroForm `/CO` in order; for each field with
   `/AA /C`: construct `event` (value = current computed `/V`), evaluate, write result to the
   in-memory `/V` of that field. Re-entrancy: pdf.js-style `_isCalculating` guard + depth cap
   (malformed cyclic `/CO` must terminate).
3. Commit once — **all** recalculated `/V`s persist atomically with the user's change. This
   preserves the R01 discipline (one transactional boundary; a JS failure aborts the whole
   commit, never half-writes) and means calculated values are real PDF data on save — visible
   in every other viewer.
4. **Format events are display-only** and never write `/V` (Acrobat semantics: formatting
   changes presentation, not value — writing formatted strings into `/V` would corrupt
   round-tripping). They belong to the widget/presentation layer (viewer form widgets and the
   properties panel read `event.value` through a Format pass; Phase 1 can land with format
   applied only in the fill UI).

New seam, following the house test-fault pattern (`SaveFault` at `FormManager.h:25`,
`CommitFaultForTesting` at `SafeSave.h:32`): a `JsFaultForTesting` injection point (timeout,
syntax error, memory cap, exception) plus a clock seam for deterministic date tests.

### Phase 1 — run-side Calculate + Format (the 80% case: tax/expense totals)

- Scope: `/AA /C` cascade over `/CO` after each commit; `/AA /F` formatting in the fill UI;
  AF subset `AFSimple(_Calculate)`, `AFNumber_Format/Parse`, `AFPercent_Format`,
  `AFDate_FormatEx/AFParseDateEx`, `AFMergeChange`, `AFMakeNumber`,
  `AFMakeArrayFromList`, `AFExtractNums`; `event.value/rc/target`.
- Dependency step (if authorized): `pacman -S mingw-w64-ucrt-x86_64-quickjs-ng`, CMake
  `find_package(qjs)`, LICENSE-3RD-PARTY.md row (MIT), version pinned to the MSYS2 package
  (0.15.1 at writing; note upstream 0.16.2 exists — pin to pacman version for
  reproducibility, matching how Qt 6.11.0 is pinned).
- Tests (`tests/TestFormJsCalc.cpp`, fixture style of `TestFormSafety.cpp`):
  - Golden values: hand-authored forms with `AFSimple_Calculate('SUM'/'PROD')`, line-item ×
    quantity − discount, percent formatting — expected values computed by hand and
    cross-checked once in Firefox (pdf.js) as the reference implementation.
  - Negative: `while(true){}` → interrupt fires ≤ deadline, transaction aborts cleanly,
    source file byte-identical (R01 invariants reused); syntax error in one field's script →
    other fields still calculate, error surfaced with field name (never silent); memory bomb
    → cap hit, honest error; cyclic `/CO` → depth cap; script that throws → rc=false path.
  - Honest-error contract: every JS failure yields a user-visible, field-attributed message
    (extends the U08 disclosure idiom).
- **Effort: 3–4 weeks** (engine+build 1–2 d; runtime/host/event layer ~1 wk; AF shim ~1–1.5 wk
  porting + pinning goldens; transaction integration + tests ~1 wk). Highest uncertainty:
  `util.scand`/date-format fidelity.

### Phase 2 — Validate + Keystroke

- Validate on commit-path (before the cascade): `AFRange_Validate` + custom scripts;
  `event.rc = false` blocks the commit with the field's error surfaced. Keystroke scoped
  honestly: full `event.change` merging is implementable in the Qt line-edit layer
  (`AFMergeChange` semantics), but Acrobat parity for every per-keystroke corner is not
  promised — the capability text says which keystroke classes run (date/number masks vs
  arbitrary scripts).
- Tests: validation-block commit keeps file untouched (R01 reuse); keystroke goldens for the
  AF*Keystroke family; format/validate ordering vs Acrobat's documented event order.
- **Effort: 1.5–2 weeks.**

### Phase 3 — Document OpenAction + document-level named scripts

- Consent UX (per §3.1 defaults), execution in the document-session runtime, blocked-verb
  audit surfacing, "scripts ran / were blocked / failed" summary in the document badge.
- Tests: OpenAction runs once post-open; blocked `submitForm` recorded not executed; consent
  refusal leaves document fully usable minus script effects.
- **Effort: 1–1.5 weeks** including consent UI.

### Cross-phase non-goals (restated as gates)

No authoring UI, no XFA, no new network surface, no persistence of JS state into the file
beyond standard `/V` writes. Every phase ships behind the existing ledger protocol:
implemented-awaiting-review until independently reproduced (per RQ07 discipline).

---

## 5. Honest alternative if the dependency is declined

The CapabilityRegistry path (`src/core/Capability.h`: `whyNot` + `alternative` are enforced
non-empty at `query()`; the UI idiom already surfaces them) — 2–3 days of work:

- A document-open probe (PoDoFo walk, no execution) detects: any `/AA` JavaScript action,
  non-empty `/CO`, `/OpenAction` JS, `/Names /JavaScript`.
- Disclosure copy (per-field and document badge): *"This form contains JavaScript
  calculations/validation. GlyphPDF does not execute form scripts, so computed fields (e.g.
  totals) will not update. Your typed values are saved correctly. Alternative: open in
  Firefox or Acrobat to see computed results; GlyphPDF's own calculated fields (Form
  Builder) remain fully editable and saveable."* — never promise what won't run; computed
  fields are shown with their stored value and flagged, not blanked, not recomputed.
- What this path does NOT buy: the 8/16-tool parity item stays MISSING; the value is
  integrity — GlyphPDF remains the tool that doesn't lie.

(If the user declines a *linked* dependency but accepts process isolation, the veraPDF
precedent offers a third shape — an out-of-process interpreter subprocess — but no suitable
existing subprocess binary exists in the MSYS2 channel; it would mean shipping one, which is
a larger footprint than the linked engine. Listed for completeness, not recommended.)

---

## 6. Recommendation + decision request

| Option | License risk | Build risk (UCRT64) | Fidelity | Size | Maintenance | Verdict |
|---|---|---|---|---|---|---|
| **A. quickjs-ng via MSYS2 pacman** | MIT, clean | **Minimal — packaged in-repo** | ES2023 core + ported AF shim | ~1 MB class | Active (Aug 2026) | **RECOMMENDED** |
| B. MuJS vendored | ISC, clean | Low (portable C) but vendored | ES5.1 core; shim | Smaller | Active | Fallback |
| C. No execution + CapabilityRegistry disclosure | none | none | n/a | none | n/a | Safe default if declined |
| D. Duktape / Hermes / V8 / hand-rolled interpreter | — | infeasible/dormant/misaligned | — | — | — | Rejected (§2.1–2.4) |

**RECOMMENDATION (needs user sign-off under the standing no-new-deps rule, recorded as a new
row in `LICENSE-3RD-PARTY.md`):** adopt Option A — add `mingw-w64-ucrt-x86_64-quickjs-ng`
(MIT, packaged in the UCRT64 repo the project already standardizes on) as the execution
engine, port the Apache-2.0 pdf.js AF shim as the host layer, and implement run-side
Calculate+Format first inside the existing R01 transaction boundary, then
Validate/Keystroke, then consent-gated document-level scripts. The increment is one pacman
package (~1.8 MB installed), one link line, and one license-table row; the payoff converts
GlyphPDF's only Tier-1 functional gap in forms from "writes scripts it never runs" into
honest execution with measured caps. If the user declines, implement Option C exactly as
specified in §5 — and ship the disclosure rather than a partial interpreter that could
silently miscalculate tax forms.

**Decision requested:** approve A (link quickjs-ng), B, or C. Silence = no dependency
(Option C stands).

---

## 7. Source register

**Codebase (pinned 955d61d):** FormManager.cpp:799–865 (calculated-field write path),
:115/:157 (save boundary), :400 (fillForm), :1439+ (setTabOrder /CO); IFormManager.h:70–98;
FormsController.cpp:92,161–190; FormBuilderMode.cpp:95–98,317–326; Capability.h:57–116;
SafeSave.h:32; FormManager.h:25; TestFormSafety.cpp; TestFormUndo.cpp; CLAUDE.md:15,47,83;
LICENSE-3RD-PARTY.md; PRD.md §9.6 (:171–179, :380); docs/research/synthesis.md §2 T1-3;
docs/research/masterpdf.md §4; docs/research/acrobat.md:65,236,257;
docs/research/pdfxchange.md:87–88; .context/RESEARCH-RECONCILIATION-2026-09-08.md (RQ05,
RQ07, RQ08, RQ10).

**Web (verified 2026-09-07):**

- ISO 32000-1:2008 full text (Adobe-hosted): <https://opensource.adobe.com/dc-acrobat-sdk-docs/pdfstandards/PDF32000_2008.pdf> (§12.6.4.16 JavaScript actions; Table 220 field events; AcroForm /CO)
- Acrobat JavaScript API Reference: <https://opensource.adobe.com/dc-acrobat-sdk-docs/library/jsapiref/index.html>; JS dev guide: <https://opensource.adobe.com/dc-acrobat-sdk-docs/library/jsdevguide/index.html>
- quickjs-ng: <https://github.com/quickjs-ng/quickjs> (LICENSE = MIT, fetched raw 2026-09-07); releases page (v0.16.2 2026-08-20); sandbox APIs: <https://quickjs-ng.github.io/quickjs/developer-guide/intro/> (JS_SetMemoryLimit / JS_SetInterruptHandler / JS_SetMaxStackSize)
- MSYS2 packages: <https://packages.msys2.org/package/mingw-w64-ucrt-x86_64-quickjs-ng> (0.15.1-1, 0.63 MB dl / 1.81 MB installed, dep = cc-libs only, conflicts with quickjs); <https://packages.msys2.org/search?t=binpkg&q=quickjs> (both engines, all 4 environments)
- MuJS: <https://mujs.com/> and <https://mujs.com/license.html> (ISC, © Artifex); tags <https://github.com/ArtifexSoftware/mujs/tags> (1.3.10, 2026-08-11); AGPL→ISC history: <https://news.ycombinator.com/item?id=9213248>
- Duktape: <https://duktape.org/download> (dormancy labels), <https://duktape.org/guide> (E5/E5.1 + partial ES2015), <https://github.com/svaarala/duktape>
- Hermes: <https://github.com/facebook/hermes> (MIT); Features.md (eval/`new Function` limits; issues #785, #957)
- V8 on MinGW: <https://v8.dev/docs/build-gn>; <https://github.com/msys2/MINGW-packages/issues/12568>; <https://stackoverflow.com/questions/73219707/build-v8-with-gn-and-mingw>
- pdf.js AF shim (Apache-2.0): <https://github.com/mozilla/pdf.js/blob/master/src/scripting_api/aform.js> (24 AF functions); event object/dispatcher: `src/scripting_api/event.js`
- Acrobat engine = SpiderMonkey 24.2 / E4X (MOSTLY_TRUE, secondary): <https://encyclopedia.pub/entry/33230>; <https://community.adobe.com/questions-9/which-ecma-version-is-used-in-acrobat-x-1272841>

**Premise corrections vs. tasking brief (graded):** quickjs-ng license is MIT not
Apache-2.0 (FALSE→corrected, license file); Duktape license is MIT not Zlib
(FALSE→corrected, duktape.org); MuJS is ISC-only today, not dual (resolved via mujs.com);
FormManager lives in `src/engines/` not `src/core/` (corrected in §0 source list).
