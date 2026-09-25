# formjs Threat Model — the never-reviewed JavaScript runtime for PDF forms

**Lane:** Phase D review, `feat/formjs-review` (base `8a0a8b3d`, PR head of
`review/consolidated-parity`), 2026-09-23/24.
**Scope:** `src/engines/formjs/` — `FormJsSandbox.cpp` (605), `FormJsRunner.cpp`
(680), `AFormShim.cpp` (845) — plus every surface its output reaches
(`FormFieldPropertiesPanel`, `FormsController`, `FormManager`).
**Doctrine:** every document is hostile; every checklist item is attacked as an
adversary would (prototype-pollution chains, host-object reaches,
budget-interaction exploits); findings ranked by exploitability; this document
is written as attack stories, not audit bullets.
**Prior coverage built on (not redone):** JS-01 deadline work, W1-sep13
sandbox checks (16 MiB / 1 MiB stack / 4 MiB transfer / 250 ms–1 s budgets /
zero-I/O / egress no-ops), `TestFormJsCalc` 49 goldens, `TestFormKeystroke`.

---

## 1. The adversary and the assets

The adversary is the **document author**. They control, with full precision:

- every `/AA` script body (`/C` calculate, `/F` format, `/V` validate, `/K`
  keystroke — all reachable through ordinary user actions: opening a field's
  properties, typing, committing a value, saving);
- every field name (any printable PDF name, including `__proto__`,
  `<img>`, control characters, 1 KiB emoji strings);
- every field value;
- the `/CO` calculation-order array (any references, any length, duplicates,
  dangling, self-referential).

The assets at stake, ranked:

1. **The user's machine while the UI thread runs scripts** — the app executes
   document-supplied code synchronously on the Qt main thread. Any hang or
   freeze is directly user-visible; any escape is code execution.
2. **The integrity of stored field values** — `/V` writes must never be
   silently wrong; the lane's own contract is "a script failure never
   produces a wrong value."
3. **The honesty of the disclosure channel** — failure reasons, logs and
   previews are the user's only view into what a script did. Whatever a
   script can place there must be *rendered as text*, never interpreted.
4. **Host resources** — memory, CPU, wall-clock, transfer surfaces.

Structural facts the defense rests on (all re-verified this lane):

- The quickjs-ng 0.15.0 engine is initialized **without quickjs-libc**: no
  stdin/stdout/filesystem/network/process primitive exists at any scope. The
  behavioral pin is `shimSurfaceIsExactlyTheDocumentedSet` +
  `noHostCapabilityReachableThroughAnyChain`.
- The host never pumps promise jobs, so asynchronous code scheduled by a
  script **never runs at all** (`promiseJobsNeverRunAfterEvalReturns`).
- Values cross the C↔JS boundary as two JSON channel calls per event
  (`__gpBeginEvent` / `__gpEndEvent`); there are no native function objects
  inside the sandbox for a script to grab.
- One absolute whole-operation deadline (R05/JS-01) covers every engine entry
  — setup eval, script, exception property reads, end-event collection, and
  every getter/coercion they trigger.

---

## 2. Attack stories

Each story is an adversary goal, the best attack we could construct, and what
actually happened. Where the attack worked, it became a numbered finding.

### S-1 · "Make the user's screen lie to them" — WORKED (PGR-35, HIGH, fixed)

The strongest script-controlled text channel is not the log — it is the
**properties panel**. A format script's *output* (`event.value`) is displayed
verbatim in the display-preview label; a keystroke/validate script's
*exception message* is displayed verbatim in the keystroke-status label; and
cascade failure reasons (field names + script-authored `Error` messages) are
interpolated into the calculation-failure dialogs in the panel and in
`FormsController`'s import path.

Pre-fix, every one of those surfaces used `QLabel`'s default `Qt::AutoText` /
static `QMessageBox::warning` — Qt's `mightBeRichText` heuristic **renders
markup**. A script as small as

```js
throw new Error('<img src=//attacker/x><h1>Session expired — re-enter your password</h1>');
```

gets arbitrary HTML rendered inside the trusted app chrome: UI spoofing
(forged banners, fake warnings), and `<img src="\\fileserver\share\x.png">`
or `file:` references, which Qt's rich-text engine loads — a content beacon
to an attacker-controlled host, fired by merely viewing a field's properties.

**Verdict:** HIGH — highest exploitability × reachability in the lane: any
hostile form document triggers it through ordinary viewing/typing.
**Fix:** `Qt::PlainText` on all six disclosure labels and explicit
`setTextFormat(Qt::PlainText)` on the three interpolated message boxes.
The engine still reports hostile text verbatim (pinned), which is exactly
what an honest disclosure needs — rendered as source.
**Evidence:** `pgr35PanelDisclosuresRenderAsPlainText` (drives the real panel
offscreen through a hostile `/AA /K` rejection and a hostile `/AA /F`
preview); commit `155f3bb7`.

### S-2 · "Burn the user's CPU where the budget can't reach" — WORKED (PGR-40, HIGH, deferred: dependency)

The 250 ms/1 s budgets are enforced by the interrupt handler. quickjs-ng
polls it in interpreted loops (baseline probe: a 1e8-iteration JS loop aborts
at exactly 150 ms) and in the regexp engine (four catastrophic-backtracking
probes all abort at the deadline — see S-7, refuted as an attack). But
**native builtins that scan sparse arrays element-by-element never poll**,
and scanning holes never allocates, so the memory cap never fires either.
Measured with the exact sandbox contract (probe harness
`evidence-formjs-2026-09-23/qjs-probe.c`):

| one expression | wall time | outcome |
|---|---|---|
| `Array(2147483648).indexOf('x')` | **44.8 s** | completes, deadline never fires |
| `Array(2147483648).includes('x')` | **53.2 s** | completes |
| `Array(2147483648).lastIndexOf('x')` | **56.6 s** | completes |
| `Array(2147483648).flat()` | **61.8 s** | completes |
| `Array(268435456).sort()` | **12.8 s** | completes |
| `Array(2147483648).join('a')` | **37.0 s** | eventually OOM at the string-buffer cap |

Re-measured 2026-09-24, same-machine version-stamped A/B (probe now prints
`JS_GetVersion()`; captures in `evidence-formjs-2026-09-23/`:
`probe-output-0.15.0-2026-09-24.txt` / `probe-output-0.15.1-2026-09-24.txt`,
source `qjs-probe-2026-09-24.c`; 0.15.1 = MSYS2
`mingw-w64-ucrt-x86_64-quickjs-ng 0.15.1-1`, runtime `libqjs-0.dll` sha256
`cc92ba7e…583ce`) — **still unfixed**:

| one expression | 0.15.0 (this machine) | 0.15.1 (this machine) | outcome |
|---|---|---|---|
| `Array(2147483648).includes('x')` | 26.6 s | 39.4 s | ok — deadline never fires |
| `Array(2147483648).indexOf('x')` | 25.5 s | 41.5 s | ok — deadline never fires |
| `Array(2147483648).join('a')` | 30.9 s | 45.6 s | InternalError, late |
| `Array(16777216).includes('x')` (the pin's probe) | 188 ms | 303 ms | ok, past the 150 ms deadline |

(Wall times swing ±50% with machine load across runs — e.g. the pin probe
measured 156/188/303 ms in three runs — so only the deadline behavior is
comparable, and it is IDENTICAL on both versions. The interrupt handler is
still never polled inside the native scan; the auto-arming pin correctly
skipped in-suite on 0.15.1 (19 pass / 0 fail / 1 skip). The baseline loop
and all four ReDoS probes abort at exactly ~151 ms on both versions — the
deadline itself is healthy. Note the original 0.15.0 table above was taken
on a different machine/load; this A/B is the controlled comparison.)

One line in any `/AA` script freezes the UI thread unkillably for about a
minute, repeatable on every save/keystroke — a 250× budget overrun per event,
with no honest classification possible because the host thread is stuck
inside the native call.

**Verdict:** HIGH — trivially exploitable, fully reproducible, but the fix is
a dependency bump, not a product patch: upstream quickjs-ng added
interrupt checks to the array builtins **after 0.15.0** (verified in master
source: `js_poll_interrupts` now inside `js_array_indexOf` and friends), but
**not in the 0.15.1 release**: MSYS2 packages `0.15.1-1` as of 2026-09-23
(`pacman -S` taken, hashes in the R18c pin comment in CMakeLists.txt) and
re-measurement shows the scans still unpollled — the checks are on master,
not yet in any tagged release MSYS2 carries. **Remediation:** bump the
pinned `mingw-w64-ucrt-x86_64-quickjs-ng` package to a release carrying the
array-method interrupt checks (or vendor+patch — a
supply-chain decision above this lane's authority, per the Option-A
pacman decision in the Phase-1 design). **Regression pin:** the suite's
`nativeSparseArrayScansAbideTheDeadline` auto-arms once a fixed package
lands (on 0.15.0 and 0.15.1 it skips with the finding reference; on a fixed
engine it asserts the 150 ms deadline fires).

### S-3 · "Rewire the machine the next script runs on" — PARTIALLY WORKED (PGR-41, LOW, deferred: platform-inherent; PGR-39, LOW, fixed)

Because one sandbox serves a whole cascade, a script can tamper with the
shared machinery — and a script whose event ends **without a committed
write** (rc=false, syntax error is impossible here, but rejection is
ordinary) skips the post-write snapshot refresh, leaving its tampering in
place for the *next* field:

```js
globalThis.__gpFieldValues = { b: '999' }; event.rc = false;
```

— the next event's `getField('b')` reads the tampered `999`, and the wrong
result is written to `/V` with no failure anywhere. Characterized and pinned
by `crossEventTamperWindowIsCharacterized`.

Why deferred: Acrobat and pdf.js share the exact property (all of a
document's scripts run in ONE context, forever — in pdf.js a Format script
can rewire a later Calculate script's `util.printf` with no bound at all).
Our per-cascade sandbox with post-write refreshes is already strictly
tighter. Closing the window fully requires a fresh runtime per event — a
design decision (per-event shim-install cost vs. semantics) above a review
lane. The pinned test flips deliberately if that decision lands.

What COULD be contained, was fixed: the shim's internal helpers leaked as
writable globals (PGR-39, below), which made the tamper surface larger than
documented — a script could rebind `__scand`/`__printf` and corrupt the date
and number machinery for every later event, not just the value map.

### S-4 · "Poison the prototype chain" — WORKED at the operation table (PGR-36, MEDIUM, fixed)

`AFSimple_Calculate('toString', new Array('a'))` — the operation membership
test was `op in actions`, which walks the prototype chain. `toString`,
`constructor`, `hasOwnProperty`, `valueOf` all pass, the inherited function
is called as an arithmetic operation, and the result flows through
`Math.round(1e6*res)/1e6` to NaN — which, combined with S-5, **silently
wiped the field's /V**. Fix: own-property membership; honest TypeError;
cascade discloses a field-attributed failure and keeps the committed value.
Evidence: `afSimpleCalculateInheritedOpsRefused`; commit `dc3240e9`.
The rest of the pollution surface is hardened: `getField` uses
`Object.prototype.hasOwnProperty.call`, the alias table already used own-key
lookup, `__printd`/`__scand` handler tables are fed by fixed regex
alternations, and `__proto__` as a *field name* is handled by S-8/PGR-38.

### S-5 · "Erase a value with NaN" — WORKED (PGR-37, MEDIUM, fixed)

`event.value = 0/0` (or `qty * price` with an empty input — division by a
field that parses to 0 is the honest-script variant). `__gpEndEvent`
reported `hasValue=true` because `typeof NaN === "number"`, while
`JSON.stringify` serialized the payload as `null` — the host read an empty
string and wrote it over the committed `/V` at every save, silently. A
silent wrong value is precisely what the lane's honesty contract forbids.
Fix: a non-finite number is "no usable value" (the existing unset-value
policy keeps the committed `/V`). Evidence:
`nanOrInfinityKeepsTheCommittedValue`; commit `77b57a7e`.

### S-6 · "Hide a value in a field named `__proto__`" — WORKED (PGR-38, LOW, fixed)

JSON is a *syntactic* subset of JS, not a semantic one: embedding the
snapshot as an object literal makes the key `"__proto__"` invoke the
inherited setter, and a string value is silently dropped — the field
vanished from every script's view (and the code comment claimed the embed
was safe). Fix: the snapshot is embedded as an escaped JS string literal
and parsed by the engine's own `JSON.parse` (own-property semantics), under
the same deadline and transfer cap. Evidence:
`protoFieldNameSurvivesTheSnapshot`, `protoFieldComputesInTheCascade`;
commit `f6e1953c`.

### S-7 · "Catastrophic regex backtracking" — REFUTED

Four failing-match ReDoS probes (`/(a+)+$/`, `^(a|a)*$` families, 24–40
character adversarial inputs) all abort **exactly at the 150 ms deadline** —
quickjs-ng's regexp engine polls the interrupt handler. Good news, pinned by
`reDoSPatternAbortsAtDeadline`.

### S-8 · "Reach the host through any chain" — REFUTED (pinned)

Exhaustive probing of the isolation perimeter, all refuted:

- 25 libc/network/process/timer globals: absent outright, and unreachable
  through the `Function` constructor, indirect `(0,eval)`, or the
  async-function constructor (whose body runs synchronously up to its first
  await — still in the bare sandbox).
- Promise jobs are never pumped by the host: a `.then` spin loop scheduled
  by a script never executes a single step (and `async` is therefore a dead
  end, not a deadline escape).
- Egress verbs (`doc.submitForm/mailDoc/exportData`, `app.launchURL/mailme/
  execDialog/media`) are pure JS closures: return `false`, append to the
  audit sink, never reach the host.
- The engine global surface is EXACTLY the documented shim set — pinned by
  `shimSurfaceIsExactlyTheDocumentedSet` (after PGR-39, below, tightened to
  reject the 9 leaked internals).
- The JSON name channel is injection-proof: a field name containing markup,
  quotes, backslashes, `__proto__` substrings, Latin-1/CJK/astral-plane
  Unicode round-trips byte-exactly (`hostileFieldNamesRoundTripThroughTheEngine`).

### S-9 · "Exhaust every budget, stacked and combined" — HELD (pinned)

Infinite recursion → 1 MiB stack cap, classified Memory. Gradual allocation
(`new Array(64).fill('x')` in a loop) → 16 MiB cap, classified Memory/Timeout.
`'A'.repeat(5MiB)` through `app.alert` → refused at the 4 MiB host transfer
cap with the flood never delivered. Six /CO spinners → the cascade budget
bounds the whole run to budget + one event overshoot. A hostile setter on
`__gpFieldValues` wedges only its own 250 ms refresh, the cascade aborts
with the skipped-field disclosure, and the field's own committed write
stands. 5000-entry self-referential `/CO` → one run. Dangling `/CO` →
skipped, honest fields still compute. A 13-item malformed-script battery
(Proxy throws, hostile `toString` throws, lone surrogates, `new Array(-1)`,
nested eval junk, getter-poisoned `event`, …) — every probe terminates
bounded and honestly classified. The kill-switch disables all four engine
entries. Read-only sessions are gated at both UI entries (E-1, prior
coverage) and the engine honors the global kill-switch regardless.

### S-10 · "Forge the audit trail" — noted, accepted (LOW)

A script can fabricate `logs`/`blocked` entries (it owns the sandbox's log
array until the next event). They reach only the system log via `qWarning` —
never the UI — and fabricating "I attempted egress" entries costs the
adversary nothing. Accepted with a note; the transfer cap bounds the flood
(S-9).

---

## 3. Findings (PGR-35+)

| ID | Sev | Where | Defect (attack story) | Status |
|----|-----|-------|------------------------|--------|
| PGR-35 | HIGH | `FormFieldPropertiesPanel.cpp` (6 labels + apply dialog), `FormsController.cpp` (2 import dialogs) | Document-derived text (script failure reasons, format-script **output**, field-name lists) rendered as rich text → UI spoofing + local-file/UNC `<img>` beacons (S-1) | **Fixed** `155f3bb7` — Qt::PlainText on every disclosure surface; test-backed |
| PGR-36 | MED | `AFormShim.cpp` `AFSimple_Calculate` | `op in actions` prototype-chain hit: `toString`/`constructor`/`hasOwnProperty` accepted as operations → garbage silently written to /V (S-4) | **Fixed** `dc3240e9` — own-property membership; test-backed |
| PGR-37 | MED | `AFormShim.cpp` `__gpEndEvent` | NaN/±Infinity value → JSON null with hasValue=true → committed /V silently wiped (S-5) | **Fixed** `77b57a7e` — non-finite = no usable value; test-backed |
| PGR-38 | LOW | `FormJsSandbox.cpp` `installFieldSnapshot` | JSON-as-JS-literal embed: a field named `__proto__` vanishes from every script (S-6) | **Fixed** `f6e1953c` — JSON.parse embed; test-backed |
| PGR-39 | LOW | `AFormShim.cpp` | Shim leaks 9 internal helpers as writable globals — documented surface exceeded; rewireable for later cascade events (S-3) | **Fixed** `f6e1953c` — IIFE wrap; surface pin tightened |
| PGR-40 | HIGH | quickjs-ng 0.15.0 (dependency of `FormJsSandbox`) | Native sparse-array scans never poll the interrupt handler: `indexOf` 44.8 s / `includes` 53.2 s / `lastIndexOf` 56.6 s / `flat` 61.8 s / `sort` 12.8 s / `join` 37 s, unkillable, per event (S-2) | **Deferred — dependency bump** (upstream fixed after 0.15.0). **Checked 2026-09-24: MSYS2 0.15.1-1 does NOT carry the fix** — same-machine A/B re-measurement unkillable on both (pin probe 188 ms / 303 ms ok past 150 ms); 0.15.1 taken as a clean patch bump (zero golden drift), pin still auto-arming. Probe `evidence-formjs-2026-09-23/qjs-probe-2026-09-24.c` |
| PGR-41 | LOW | `FormJsRunner.cpp` cascade | Cross-event tamper window: a script ending without a committed write leaves its rewiring in place for the next event's inputs (S-3) | **Deferred — platform-inherent** (Acrobat/pdf.js share it); characterized by a pinned test; proposed fix: fresh runtime per event |
| — | LOW | `FormJsRunner.cpp` log sink | Scripts can fabricate audit `logs`/`blocked` entries; reach `qWarning` only, never the UI; flood bounded by the transfer cap (S-10) | Accepted, documented |

## 4. Coverage

Attacked and held (test-pinned in `tests/TestFormJsAdversarial.cpp`, 24 pass
/ 0 fail / 1 disclosed skip, ~2–9 s offscreen): global-surface exactness,
host-capability reaches (5 chains), async/job-pump, egress no-ops + honest
returns, recursion, gradual exhaustion, ReDoS, stacked cascade budgets, log
flood, 5000× self-reference, dangling /CO, hostile snapshot setter,
cross-event tamper window (characterization), malformed battery (13),
hostile field names through the JSON channel, verbatim-failure pin,
kill-switch over all four entries, plain-text disclosures, `__proto__`
snapshot round-trip (unit + cascade).

Also re-run green after the fixes: `TestFormJsCalc` 49/49 (goldens),
`TestFormKeystroke` 9/9 (panel wiring).

Not covered by this lane (with reason): document-open `/OpenAction` and
`/Names /JavaScript` scripts (Phase-3 hooks, deliberately absent — no entry
point exists to attack); the viewer render path (format display in page
widgets is a Phase-2+ disclosure; the panel is the only format consumer
today); LLM-facing surfaces (none in formjs).

## 5. Residuals / handoff to the integrator (R12)

1. **PGR-40 remediation is a dependency bump.** Track
   `mingw-w64-ucrt-x86_64-quickjs-ng` upstream packaging; **checked 2026-09-23:
   MSYS2 0.15.1-1 does NOT carry the array-method interrupt checks** (re-measured
   unkillable; see S-2). Next check when MSYS2 ships a later release; when one
   carries the fix, bump `scripts/bootstrap-vendor-deps.sh` + CI; the regression
   pin (`nativeSparseArrayScansAbideTheDeadline`) flips from skip to assert with
   no code change. Alternatively vendor+patch quickjs — supply-chain
   decision, owner call.
2. **PGR-41** (cross-event tamper window) — if the platform ever adopts
   per-event runtimes, flip `crossEventTamperWindowIsCharacterized`
   deliberately.
3. **Link-symbol CI check** for quickjs-libc symbols (Phase-1 residual) —
   still open; the behavioral no-I/O pin now additionally covers the shim
   surface.
4. The log-flood and audit-fabrication notes (S-10) are recorded, no action.

**Commits (this lane, in order):** `697e7dcf` adversarial suite ·
`155f3bb7` PGR-35 fix · `dc3240e9` PGR-36 fix · `77b57a7e` PGR-37 fix ·
`f6e1953c` PGR-38+39 fixes · this document.
