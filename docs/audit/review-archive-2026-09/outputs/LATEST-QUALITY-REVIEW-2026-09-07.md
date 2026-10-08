# GlyphPDF latest code-quality review — 7 September 2026

**The late updates improve sanitization safety and recovery, but the branch still has material correctness failures.** This report includes the commits that arrived during the review and distinguishes implemented fixes from independently accepted behavior. The nine findings N01–N09 from the earlier snapshot remain applicable, and this pass adds N10: four claimed regression checks are not registered with Qt Test.

The final review boundary is **`origin/feat/parity-glm` at `0caa45e7d0751caaa36a54a085c41211a422f019`**, fetched successfully during this session. The late comparison from `95676e6` contains six commits and **13 files, 938 insertions, 44 deletions**. The earlier comparison from `4761443` to `95676e6` was already built, tested and reviewed. No subsequent branch movement is included in this fixed snapshot.

Read this report first, then the [detailed N01–N09 instructions and row-by-row review](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/outputs/UPDATE-QUALITY-REVIEW-2026-09-07.md). This report overrides that snapshot's D01/D05/D06/D07 status. The [GLM Flash companion](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/outputs/GLM-FLASH-IMPLEMENTATION-AND-UI-PLAN-2026-09-05.md) points to the current implementation queue.

## Latest verification

- Full application/test-target build at `0caa45e`: **exit 0**.
- Full CTest run: **107/107 passed, 38.24 seconds**. Detailed Qt totals: capability 16, Ollama 60, redaction marking 13, redaction transaction 26; these totals include setup/cleanup. All four detailed suites report no skips. Other model/hook limitations described below remain.
- The executable `-functions` inventories omit all four functions in N10. The capability and redaction-marking totals remain unchanged despite those added functions.
- Rebuilt independent probes still reproduce N01/N02/N03/N05/N06/N07/N08/N09. N04 remains confirmed by its production caller. The default-on/explicit-opt-out probe passes.
- The capability probe now rejects the empty detector and recovers basic widget enablement. With three nonempty `payload` files it returns Available/“Usable model set,” while the real OCR engine fails ONNX parsing. D06 is therefore only partially repaired.

The source for `0caa45e` was obtained by Git archive and overlaid into the existing isolated build directory, whose `parity-95676e6` name is historical. The build source contents are checked against the new archive; the original archive and evidence remain preserved separately. No production repository files, test assertions, branches, or installation were changed. Full-target build/test commands and probe logs are in the [latest evidence archive](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/outputs/latest-quality-evidence-2026-09-07.zip).

The initial `95676e6` build passed all 107 CTest targets in 65.28 seconds and captured 24 detailed Qt logs. Those logs contain three model-dependent RapidOCR skips and one revoked-certificate test-hook skip. No sanitizer, full alternate dependency matrix, veraPDF conformance validation, external Office round-trip, or installed-app walkthrough is claimed for either snapshot. The older installed application was not used as evidence.

## What the late commits actually repair

| Finding/package | Independent current disposition | Evidence and boundary |
|---|---|---|
| D05 — safe sanitized-output replacement | **Verified for the redaction transaction/retry boundary** | Sanitization now writes an operation-owned candidate, reopens it and checks page count, then uses checked `SafeSave` replacement. Existing destinations are covered by injected sanitize/commit failures and successful replacement. Overlay candidate replacement also uses `SafeSave`. This does not certify every unrelated direct caller of the backend sanitize method. |
| D01 — sanitize retry | **Partial; intended-path recovery repaired** | `intendedSanitizedDestination` preserves the requested path separately from the committed one. The real presenter's retry and failure/keep/discard decisions now have modal-driven tests. Redact mode reads the recovered result. Security's callback still builds the final banner from the original partial result; see the residual below. |
| D07 — default-on sanitization | **Verified for initial policy and explicit opt-out** | Both production callers now seed the same `kDefaultSanitizeOn` policy. The independent widget check covers the Redact mode default, a dialog carrying that policy and explicit opt-out. The newly claimed repository test for this is not registered (N10), so it is not regression protection yet. |
| D06 — capability honesty | **Partial; empty/incomplete-model rejection repaired** | Mandatory detector, recognizer and vocabulary files must now be readable/nonempty. Basic unavailable→available widget recovery is implemented. Nonempty corrupt models still receive Available/“Usable” without successful initialization; real-engine resolution and runtime refresh remain separate. Details below. |
| R03/R04 and the new “D02 (AI)” row | **Existing scoped acceptance retained; added owner-shape coverage** | The additional tests model a panel owning provider and watcher, exercise owner destruction and retry, and drain queued delivery. This is useful test coverage, not a production AI change or a repair of redaction D02. No sanitizer or actual AIChatPanel UI walkthrough is claimed. |
| Redaction D02 | **Open** | The UI-owned operation still does not retain the worker state it dereferences. The ledger correctly keeps this distinct and open. |
| V01/V02 — form import/undo | **Open** | `FormsController.cpp` and `EditFormFieldCommand.h` are unchanged. Commit `8bd01d5` uses V01/V02 labels for redaction mark/persistence tests; it does not fix the original form import or failed-undo history findings. |

The new heading in the ledger no longer calls all UI packages open. Other maintenance remains: duplicate U07, stale test totals, R09's wrong fixture attribution, and placeholder “(this commit)” values for D06/D07. Replace those placeholders with `9435b1b` and link actual review evidence.

### D01 residual — Security re-announces failure after successful recovery

At `src/shell/controllers/SecurityController.cpp:665`, the caller invokes `present(..., result)` without the new effective-result output. At lines 681–682 it then calls `bannerText(result)` on the original `PartialRedactedOnly` value. The shared presenter may already have successfully created the sanitized output, yet the status bar still reports the earlier failure. The dedicated Redact mode correctly passes an output result and uses that result for its banner.

**Implement:** pass and consume the recovered result in both callers. Keep mark decisions driven by the presenter's actual choice. Add a test that captures the Security callback's final status after successful retry, not only a presenter test using a manually supplied output result. This remaining caller mismatch is source-traced; the latest presenter tests establish the underlying recovery behavior.

### D06 residual — file presence is still reported as a usable engine

`src/core/Capability.cpp:587–615` checks file readability and size, then returns Available and “Usable model set.” Three files containing only `payload` meet that test. They are not ONNX models. The independent probe returns Available from the registry and `false` from `RapidOcrEngine::initialize` on that same directory, with an ONNX protobuf-parsing error.

The engine still resolves its directory by detector existence (`RapidOcrEngine.cpp:45–51`), whereas the registry can fall through from an incomplete preferred directory to a complete directory beside the executable. The registry can consequently describe a different model set from the one initialization chooses. Interactive controller/preferences retain their separate detector checks, and this commit does not wire production invalidation when model availability changes.

`applyToWidget` also records `capOwnedDisable=true` even when a widget was already disabled for another reason. A later Available state unconditionally enables it; Unavailable→Degraded does not undo its own disable. Its comment promises broader reversibility than the implementation provides.

**Implement:** resolve model assets once at the engine boundary and reuse the selected directory in the registry and callers. Either describe the result as “assets present, initialization unverified,” or publish ready status only after successful initialization, with expensive loading off the GUI thread. Reuse the existing registry. Refresh at the relevant preference/install/operation boundaries and test transitions with independent disabling reasons. Do not replace this with another parallel registry.

**Acceptance:** empty, partial, unreadable and corrupt sets; usable sets; optional classifier absent; conflicting app-data/executable directories; install/remove within one session; and Available/Degraded recovery without overriding another disabled state. The prior zero-byte failure is repaired, but full D06 acceptance remains open.

### N10 — P2: four new regression checks never run

**Source confirmed; executable test inventory checked in the evidence.** `tests/TestCapabilityRegistry.cpp:385–438` defines three new `rapidModelsProbe...` functions after the test class's closing `};`. They are free functions, not private slots of `TestCapabilityRegistry`. `tests/TestRedactMarkAll.cpp:589–607` similarly defines `defaultSanitizePolicyIsSharedAndOn` outside the class without a slot declaration or class-qualified definition. `QTEST_MAIN` therefore never invokes these functions.

The functions compile and may contain `QVERIFY`/`QCOMPARE`, but those assertions do not run merely because the file builds. This invalidates the commit/ledger claim that the new directory/default-policy cases are passing regression tests. It does not by itself disprove the production repair; this review used separate probes for that behavior.

**Implement:** place the three capability methods inside `private slots:`. Add the redaction method to the class's slot declarations and qualify its out-of-class definition. Run each named function explicitly, check `-functions` includes it, then execute the suites with detailed logs. Ensure a deliberately wrong expected value fails the selected case before reverting that test-only mutation. The corrupt-model case must not assert that arbitrary payload bytes are a usable OCR engine.

## Earlier findings still needing work

The latest changes do not touch the logic behind N01–N09. Their exact triggers, reproduction limits and implementation steps are in the [detailed snapshot report](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/outputs/UPDATE-QUALITY-REVIEW-2026-09-07.md).

| Priority | Finding | User-visible failure |
|---|---|---|
| P1 | N01 — signature tab mapping | Visible Initials and Upload pages dispatch the opposite kinds and validation. |
| P1 | N03 — PDF/A-3U version | Export reports PDF/A-3U while writing PDF 2.0; actual conformance is unvalidated. |
| P1 | N09 — split integration | The source-loaded editor rejects writes to each output path; default split returns no files. |
| P2 | N02 — alphabetic page labels | Label 28 is `ab` instead of PDF's `bb`. Writer/UI is still deferred. |
| P2 | N04 — missing overlay request field | Security entry drops the label chosen in the shared redaction dialog. |
| P2 | N05 — signature cache lifecycle/gate | A→B→A can retain a supposedly per-document signature; checked reuse can still leave OK disabled. |
| P2 | N06 — signing retry appearance | The image is consumed once and absent from the complete retry request. |
| P2 | N07 — redaction exit | The screen closes but the redaction marking tool stays active. |
| P2 | N08 — overlay fit | White label glyphs extend outside the minimum-height black box. |

V01–V06, D02–D04 remain open with the preceding evidence. D01 and D06 are partially repaired as described above. D05 and D07 are accepted only within the stated boundaries. All other earlier ledger dispositions carry forward: the new fixes do not accept all of U05/U08, and the three narrow July follow-up acceptances do not imply full parity. E-1 still needs the exact same-stream corruption reproducer; it was not independently reproduced in the earlier two synthetic fixtures.

## Implementation handoff

1. Correct the ledger's IDs and test registration first so future progress reports refer to the actual findings and executed checks. Redaction marks are not form undo history; AI ownership tests are not a redaction lifetime fix.
2. Preserve the new D05 candidate/commit implementation, recovered-path handling and shared default policy. Finish D01's Security caller and D06's readiness/caller integration.
3. Prioritize the unresolved P1 output/lifetime/history failures and N01/N03/N09. Implement one bounded repair at a time and verify through the real caller, backend and saved artifact where relevant. Follow the detailed companion for V/D repairs.
4. Keep “implemented-awaiting-review” until the independent reviewer accepts the precise contract at an exact commit. Record skipped cases and distinguish a helper test from the user's end-to-end workflow.
5. After code acceptance, reinstall the exact accepted commit and conduct the planned computer-use/UI review. No installed-app conclusions have been substituted for this code review.
