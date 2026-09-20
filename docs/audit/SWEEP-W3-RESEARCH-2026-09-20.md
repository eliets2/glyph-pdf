# SWEEP-W3-RESEARCH — 2026-09-20

END-PHASE sweep, W3 research/tracking-corpus lane (DOCS-ONLY — zero `src/` or `tests/`
changes; no build). Mission: bring the research/tracking corpus to GROUND TRUTH at the
mainline tip so the program's consolidated report does not repeat superseded claims.

- **Branch:** `feat/sweep-w3-research` (from `feat/parity-glm` @ `ec9f16f6`, the mainline tip
  carrying everything through rotate-270). Tip after this lane: see `git log` below.
- **Method:** every reconciled row was checked against (1) the evidence ledger
  `CURRENT-EVIDENCE-LEDGER-2026-09-05.md` (newest sections first), (2) the feature-command
  matrix CSV as updated by the landing lanes, and (3) the code itself (grep-level spot checks
  at tip). Every claimed landing commit was verified as an **ancestor of tip** via
  `git merge-base --is-ancestor`.
- **Vocabulary discipline:** the ledger's states are respected — everything this lane
  reconciles is **implemented-awaiting-review** unless a named independent review (R14,
  SWEEP-W2B) already flipped that specific row to verified in the ledger. This lane flips NO
  evidence status anywhere; it corrects research/tracking prose. The word "verified" appears
  below only where the ledger itself says it.
- **Ledger untouched:** `CURRENT-EVIDENCE-LEDGER-2026-09-05.md` was read, never written.

## 1. Files changed (this lane)

| File | Change | Commit |
|---|---|---|
| `docs/research/RESEARCH-BACKLOG-2026-09-10.md` | status reconciliation: dated preamble, §0 finding resolutions, 16 table-row status cells, counts line, §5 shortlist marks, §6 note | `49c37a9e` |
| `docs/research/synthesis.md` | §2 Tier-1 ground-truth pointer + new §7 Ground-Truth Addendum (Tier-1/Tier-2/Tier-3 statuses, dead-command resolution, moat effect) | `ce6dd83b` |
| `docs/audit/FEATURE-COMMAND-MATRIX-NOTES-2026-09-09.md` | new §G ground-truth addendum: 300→311 rows, +11 new commands, disposition recount, 2 CSV defects, stale in-CSV rows | `bc776905` |
| `docs/research/{send-for-signing,form-js,batch-presets}-implementation-plan.md` | dated status notes: P1 (s4s, presets) / Phases 1–2 (form-js) landed; §1.2 B-B-only finding superseded | `75909130` |
| `docs/audit/SWEEP-W3-RESEARCH-2026-09-20.md` | this reconciliation log | (this commit) |
| `.context/sweep-w3-research-wip.md` | handoff (gitignored, carried in tree) | — |

## 2. Backlog reconciliation log (row → old status → new status → evidence)

Source: `docs/research/RESEARCH-BACKLOG-2026-09-10.md` (written 09-10; every row stale by
10 days of merges). Landing SHAs below are verified ancestors of `ec9f16f6`.

| Row | Item | Old (09-10) | New (09-20) | Evidence (commit → test/ledger) |
|---|---|---|---|---|
| 1 | Redaction Proof Mode | SHIPPED (`15f3f1c`) | SHIPPED — unchanged; R14-verified sub-rows recorded (L5/L8/F1 fixes) | `15f3f1c` + `1e2ab02`/`f2a9d06`/`938401b`/F1; TestRedactionProof 20/0 |
| 2 | Measurement toolset | SHIPPED (`e73a446`) | SHIPPED — unchanged | `de77d46`/`e73a446`/`83706cf`/`69f8a94`; T1 gate 119/120 |
| 3 | N1 measurement CSV | **NEW (N1)** | **SHIPPED** | `d4b45b24` (quick lane); TestMeasureCsvExport; matrix row `measure-export-csv`. Manage-measurements import/filter dialog NOT in scope of what landed — open |
| 4 | Form-JS execution | QUEUED-PACK-B | **SHIPPED-P1+P2; P3 open** | P1 `baf031e`/`86f8637`/`ac3698f` + R05 `77bc50b`/`8afccc5` (TestFormJsCalc 29 slots); Keystroke `bcd34eb` **verified (R14)** (TestFormKeystroke 8/8); Validate /AA /V wired in fillForm transaction (`FormManager.cpp:598`, same family). Open: P3 OpenAction + doc-level scripts + consent (`FormJsRunner.h:153` hooks-only); worker-process sandbox residual |
| 5 | Send-for-signing | QUEUED-PACK-C | **SHIPPED-P1** | `57cca6d`/`0591693`/`97c59a1`/`bc0ade4`; TestSendForSigning 12/0 (two REAL P12 signs). Open: P2–P4 (routing, reminders, audit PDF, DocMDP prepare, order enforcement advisory-only) |
| 6 | Batch presets (T2-1) | QUEUED-PACK-A | **SHIPPED-P1** | R26 `f17f47f`/`eb42ab1`; TestBatchPresets 14/0 + NCs; W1-01 naming **verified** (SWEEP-W2B). Open: Bates step, onConflict rename/stop, import/export dialogs, hot-folder ingest |
| 7 | T2-2 Find & Replace | QUEUED-T2-2 | **SHIPPED** | `7e32093`; TestFindReplace 20/20 (R14 carried revert-verify). Residual: replacement writer height-only flip on /Rotate (sweep-legacy) |
| 8 | T2-3 statuses + summary | QUEUED-T2-3 | **SHIPPED** | `7e32093` (TestReviewSummary 9/9) + printable surface `2eb4ff6`/`8184a95` (TestPrintableSummary 11/11) |
| 9 | T2-4 accessibility | QUEUED-T2-4 | **SHIPPED-P1** | `41300f0` + `c6a56b2` (checker 10/0, fixes 9/0, panel 7/0+2; SWEEP-W2B A11y probe verified). Open: tag-tree authoring, auto-tag, PDF/UA (disclosed not-built) |
| 11 | T2-6 stamps | QUEUED-T2-6 | **SHIPPED** | `7e32093`; TestDynamicStamps 10/10; dead Stamps-menu fix `MenuBar.cpp:104–111`; sweep-legacy reviewed-clean at tip |
| 13 | T2-9 auto-bookmarks | QUEUED-T2-9 | **SHIPPED** | `7e32093`; TestAutoBookmarks 9/9 |
| 18 | N2 XFA banner | **NEW (N2)** | **SHIPPED** | `3cb1b424` (ancestor; twin `8cff57a6` not on this line); TestXfaHonestyBanner; matrix `xfa-disclosure` |
| 19 | N3 skip-OCR | **NEW (N3)** | **SHIPPED** | `2a82738` + Q-lane `7cc85e9`/`0411826`/`d88471d`/`ff3cc68`; TestBatchOcrSkipText (Q1, Q3 **verified by R14**). Q4 contract correction: text-equality + page-object preservation, NOT byte preservation; /AcroForm-not-surviving-page-copy finding XFAIL-pinned |
| 20 | N4 OCSP-offline toggle | **NEW (N4)** | **SHIPPED — in a different shape** (consent switch) | R24-W2 `d574530`: OcspConsentDialog + `signing/ocspNetworkPolicy` "never" (TestOcspConsent 9/9, TestNetworkDisclosure 7/7; SWEEP-W2B F4-adjacent). Delta: never = refuse dispatch with whyNot, NOT offline-degraded validation with a "revocation not checked" panel state — that wording stays open |
| 33 | N17 cert-encryption UI | **NEW (N17)** | **SHIPPED** | `d423ba3` (TestCertEncryptPicker 12/12 + NC) + FU-1 `15c0d11`; matrix `certEncrypt`. Per-recipient permission sets not in scope of what landed |
| 34 | N18 DocMDP certify UI | **NEW (N18)** | **SHIPPED** | `e6e2276` (TestCertifySelector 11/11 + NC) + FU-1 `15c0d11` (production seam; /DocMDP /P==2 pin); matrix row added |
| 54 | N38 deployment trust pack | **NEW (N38)** | **SHIPPED-CORE** (policy mechanism) | R24 `8ea3876`/`c766623`/`60212ca` + wiring closure `03f4606`/`d574530`/`cbd599b` (all six keys enforced at real decision points; W1-05/F1/F4 verified in SWEEP-W2B). Open: GPO/ADMX templates, silent-MSI doc, offline-license statement |
| 71 | N54 measurement follow-ups | **NEW (N54)** | **PARTIAL** | CSV half closed by N1 `d4b45b24`; per-viewport scales / AP-stream captions / data-driven captions still deferred (T1/UI disclosures stand) |

Rows verified NOT changed and left honest as written: 10 (T2-5 — cross-doc Bates `69cb6e4`
shipped, batch split/password-strip still open), 12, 14–17, 21–32 (N5 reverse confirmed still
unwired — no ToolId/reorderAllPages permutation at tip; N6–N16, N19–N32 unchanged), 35–53,
55–82, all REJECTED rows, all Tier-3-synthesis rows not named above.

**Reconciliation counts:** 12 rows now fully SHIPPED (2 pre-existing + 10 flipped), 6 partial
ships (form-JS P1+P2, s4s P1, presets P1, T2-4 P1, policy core, N54 CSV half), 0 rows
downgraded, 0 REJECTED rows touched, 0 evidence-status flips.

## 3. §0 "load-bearing findings" — resolution log

| §0 | Finding (09-10) | Resolution at tip |
|---|---|---|
| 0.1 | T3-1 cert encryption engine-done, UI-only → N17 | CLOSED: N17 landed (`d423ba3` + `15c0d11`) |
| 0.2 | T2-7 DocMDP engine-done, UI-only → N18 | CLOSED: N18 landed (`e6e2276` + `15c0d11`) |
| 0.3 | "signs effectively B-B only"; "Timestamp document is a dead command" | **SUPERSEDED**: R19 `abc87de2` settings-driven PAdES/TSA — production call sites now exist (`SecurityController.cpp:1114` ← `readSigningConfig`; `:255` request path; `SigningRequestRunner.cpp:257`); `timestampDocument()` live (`:1097`, dispatched `:428`); failed-B-T honesty (L1 `d58896f` verified + RES-1 `2a334ca`); policy-effective TSA (R24-W1 `03f4606`; W2B F4 verified). Honest caveat downgraded from "dead" to "implemented-awaiting-review" |
| 0.4 | QUEUED-PACK-A P0 already satisfied by G12 | Still accurate; superseded further by presets P1 landing |
| 0.5 | T1 deferred CSV export → tonight #1 | RESOLVED: N1 `d4b45b24` |
| 0.6 | Dead Stamps menu (3 unconnected QActions) | RESOLVED: T2-6 `7e32093` registry wiring (`MenuBar.cpp:104–111`) |

## 4. Stale-claims cross-check (contradictions vs newer waves)

### 4.1 Fixed in this lane's scope (docs/research/)

| Where | Stale claim | Superseded by | Fix |
|---|---|---|---|
| RESEARCH-BACKLOG §0.3 + counts + table statuses | B-B only / dead command / rows NEW or QUEUED that landed | R19, R24 wiring, quick lane, Pack A, R26, S4S P1, T2-4 P1, n17n18+FU-1 | `49c37a9e` (dated notes + cell rewrites) |
| send-for-signing-implementation-plan §1.2 (lines ~31–32, ~101) | "`setTsaUrl`/`setSignatureLevel` never called anywhere under src/"; "Timestamp document menu action cannot succeed" | R19 `abc87de2` (+ R24-W2 consent) | `75909130` (dated STATUS banner; snapshot text preserved) |
| form-js-implementation-plan header/§1 table | P2 (Validate/Keystroke) and Phase 3 future; "NOT an authorization to add a dependency" standing | Option A authorized; P1+P2 landed (`baf031e` family, `bcd34eb` R14-verified, Validate wired `FormManager.cpp:598`); P3 still open | `75909130` (dated STATUS banner) |
| batch-presets-implementation-plan header | "DESIGN ONLY, no code"; decision requests pending | P1 landed R26 with recorded deviations; W1-01 verified | `75909130` (dated STATUS banner) |
| synthesis.md Tier-1/Tier-2/Tier-3 MISSING/PARTIAL grades | T1-1..T1-4, T2-1/2/3/4/6/7/9, T3-1/T3-9 graded missing/partial | all landed in core per ledger | `ce6dd83b` (§2 pointer + §7 addendum; historical text preserved) |

### 4.2 Found in docs/audit/ — REPORTED here, NOT edited (coordinator consolidates)

| Where | Stale/contradicted content | Ground truth | Suggested disposition |
|---|---|---|---|
| `COMPARISON-TABLES-2026-07-01.md:133` | "SignOutcome::PartialLtvMissing … enum-only — no evidence of a user-facing dialog/toast explaining the degradation" (🔴 Behind) | Degradation is user-facing since L1 `d58896f` + RES-1 `2a334ca` (buildSigningOutcomeWarning + attainedLevelLabel floor + plain-language warning), R14-verified | July table is a dated snapshot; add a dated correction note or footnote when the consolidated report cites it |
| `SEP13-LEADS-CONFIRMATION-2026-09-14.md:423` | L12 described as open/unconfirmed perf lead ("the perf half is already L12") | L12 flipped CONFIRMED → fixed (FU-3 `4d0b606`, memoized page→anchor index, measured ~70x per-toggle) | Confirmation doc is a historical snapshot; ledger already carries the flip — consolidated report should cite the ledger row, not the confirmation doc |
| `FEATURE-COMMAND-MATRIX-2026-09-09.csv` line 302 (`measure-export-csv`) | Row parses as 28 fields (unquoted commas in `output_or_state_change`); `review_status` reads garbage to RFC-4180 parsers | Intended review_status = implemented-awaiting-review (visible in trailing fields) | Mechanical CSV repair (quote the cell) — matrix owner |
| `FEATURE-COMMAND-MATRIX-2026-09-09.csv` lines 141 vs 306 | Two VISIBLE ribbon rows share `stable_command_id` `certify` (old inventory row + new N18 row) | Needs alias mapping or id split (§C discipline) | Matrix owner at next rebaseline |
| `FEATURE-COMMAND-MATRIX-2026-09-09.csv` rows 52–54 (`measure`/`distance`/`area`) | Still describe MeasureMode as UNTRACKED WIP, no .cpp, menu Disabled | MeasureMode landed `e73a446` (T1/UI); panel wired, menu mirrors live | Matrix owner rebaseline (recorded in notes §G.3) |
| `FEATURE-COMMAND-MATRIX-NOTES-2026-09-09.md` §E | "Menu Stamps submenu: UNCONNECTED QActions — silent no-ops" | Fixed by T2-6 `7e32093` | Corrected in §G.3 of the same file by this lane (`bc776905`) |

Not found (checked, no longer claimed anywhere): standalone "OCSP consent missing" and
"policy keys unenforced" claims — R24-C's own disclosure row was the honest wording, and the
R24 wiring closure + W2B F4/S4S rows superseded it; the ledger self-corrects, no external doc
repeats the gap as current.

## 5. What this lane verified but did NOT change

- Code spot-checks at tip (grep-only, no builds): `setTsaUrl`/`setSignatureLevel` production
  call sites; `timestampDocument()` wiring; `runValidateEvent` call path + `OpenAction`
  hooks-only state; stamps menu registry entries; absence of a `reverse` command id (N5 still
  open); TestFormJsCalc/TestFormKeystroke presence.
- Matrix CSV recounts (Python csv): disposition classes, hidden-ribbon composition
  (16/8/28 unchanged), ribbon visible 92→95, menu Planned 9 unchanged.
- All landing SHAs cited above verified as ancestors of `ec9f16f6`.

## 6. Residuals / hand-off to the consolidated report

1. Everything reconciled remains **implemented-awaiting-review** at the program level except
   the R14/SWEEP-W2B-verified rows the ledger names; the consolidated report should phrase
   shipped-as "landed (implemented-awaiting-review unless independently reviewed)".
2. Open items the corpus should not silently upgrade: form-JS P3 (OpenAction/doc-level +
   consent), s4s P2–P4, presets P2/P3, T2-4 tagging/auto-tag/PDF-UA, T2-5 batch
   split/password-strip, N5 reverse wire-up, N4's offline-degraded-validation wording,
   N38's GPO/ADMX/MSI/license-statement tail, Tier-3 pool.
3. The two matrix CSV mechanical defects + stale measure rows are the matrix owner's to fix;
   this lane's notes addendum §G documents them precisely.
4. The 2026-09-20 reconcile pass did not re-audit the 19 per-competitor sheets in
   `docs/research/` (acrobat.md … updf.md) — they are competitor snapshots, not status
   claims, and no stale program-status claims were found in them by the targeted greps
   (L12/consent/B-B/dead-command/policy-enforcement patterns).

## 7. Verification statement

DOCS-ONLY lane: `git diff ec9f16f6..HEAD --stat` shows only files under `docs/` (plus the
gitignored handoff). No `src/`, `tests/`, build files, or ledger rows touched. No push; no
reset/clean/force/gc/prune; foreign worktrees and the untracked sibling-lane files
(`ad_diag.txt`, `rma_diag.txt`, `tests/W2Probe*.cpp`) untouched.
