# SWEEP-W1 ADVERSARY — end-phase ponytail sweep W1 (native-adversary)

**Date**: 2026-09-19
**Target**: GlyphPDF candidate `feat/parity-glm` @ a3a9317 (send-for-signing P1, batch-presets P1, R24 policy, printable summaries + accessibility lane in parallel)
**Branch**: `feat/sweep-w1-adversary` (repros + this doc only — REVIEW-ONLY round, zero production-code changes)
**Method**: adversarial review per the native-adversary role: threat-enumerate the newest surfaces, grep every caller, name the shared defect (root cause, not symptom), prove with FAILING repros or refute with the covering pin. Verdicts: CONFIRMED (failing repro + impact) / REFUTED (evidence + pin) / INCONCLUSIVE (exact missing piece).

**Environment**: Windows 11, MSYS2 UCRT64, GCC 16.1.0, Qt 6.11.0, PoDoFo 1.1.0 (vendored), build dir `build-r18-noeng`, `QT_QPA_PLATFORM=offscreen`, QtTest `-o FILE.txt,txt`.

---

## Verdict summary (severity-ranked)

| # | Sev | Surface | Verdict | Shared defect (one line) |
|---|-----|---------|---------|--------------------------|
| W1-01 | HIGH | batch-presets | CONFIRMED | Naming-template literals are never containment-checked; sanitize lives at token-value level instead of the resolved result |
| W1-02 | MED-HIGH | send-for-signing | CONFIRMED | The 1-field=1-signer lint exists at prepare + create but not at `fromJson` — the only boundary that consumes untrusted input |
| W1-03 | MEDIUM | send-for-signing | CONFIRMED | precheck validates the ENTRY, not the engine's global precondition; lazy placement mutates before the engine can proceed |
| W1-04 | MEDIUM | printable summaries | CONFIRMED | A per-string encoding fault (TAB, control char, CJK, emoji) aborts the ENTIRE summary export |
| W1-05 | MED-LOW | policy | CONFIRMED (design) | An attacker-writable `%PROGRAMDATA%` policy file is loaded and machine-enforced with no ownership/ACL check |
| W1-R1..R7 | — | several | REFUTED | (pins below) |
| W1-H1..H3 | — | several | HYPOTHESIS / INCONCLUSIVE | (missing pieces below) |

Repro suites (all on the branch; failures ARE the deliverable — a fix flips them green with no test edit):

| Suite | Totals | Meaning |
|---|---|---|
| `TestSweepW1PresetAdversary` | 3 passed, 3 failed | 3 failures = W1-01 repros; the green slot pins the QDir join semantics (exploitability) |
| `TestSweepW1SigningAdversary` | 3 passed, 3 failed, 0 skipped | 3 failures = W1-02 (x2) + W1-03; real P12 signing exercised |
| `TestSweepW1SummaryPolicyAdversary` | 6 passed, 2 failed, 1 skipped | 2 failures = W1-04 repros; green = loop/injection pins + W1-05 squat probe; skip = diagnostic triage slot |

---

## CONFIRMED findings

### W1-01 — Batch-preset naming template escapes the output directory (arbitrary overwrite) — HIGH

**Component / asset**: `src/core/BatchPreset.cpp` — `BatchPresetSchema::resolveNaming` (root cause); consumers `src/modes/BatchMode.cpp` `resolveOutputPath` (GUI pre-check + captured worker output) and `SafeSave::commitFileToDestination` (commit).
**Entry point**: a hostile `*.glyphpreset.json` (import / shared preset). `parse()` validates `output.naming` for token syntax and the `.pdf` suffix only (BatchPreset.cpp:634-643 resolves with empty basename to check syntax — not containment).
**Bug class / CWE**: CWE-22/CWE-23 (path traversal), CWE-73 (external control of file name/path).
**Shared defect**: plan §3.6's "no path separator can be smuggled through a token" rule is enforced ONLY on token *values* (`sanitizeNameComponent`, BatchPreset.cpp:64-75). The template *literal* is copied verbatim — `..`, `/`, `\`, `:` pass. The resolved name is then joined with `QDir(outDir).filePath(name)`, which neither normalizes `..` nor rejects absolute operands, and **no caller applies a containment check**. Patch once in `resolveNaming` (validate the RESOLVED result: reject separators, `..` segments, drive-absolute forms) and every caller is fixed — the ponytail root-cause shape.
**Chain (verified by code reading; the codec half is failing-repro-proven)**:
1. `parse("output.naming": "../evil_{n}.pdf")` → **accepted** (repro failure #1).
2. Absolute form `C:/Users/Public/evil_{n}.pdf` → **accepted**; `QDir::filePath` returns absolute operands as-is (repro failure #2; green join-semantics pin documents Qt behavior).
3. Preset `output.onConflict: "overwrite"` is schema-legal → `presetOverwriteConfirmed` (BatchMode.cpp:1247-1251) skips **every** interactive overwrite confirmation, including the multi-file summary dialog.
4. Worker commits through `SafeSave::commitFileToDestination(current, outputPath)` — no containment at the commit seam either.
**Attacker narrative**: distribute `web-optimize.glyphpreset.json` (id must equal the file stem — attacker controls both). Victim imports, selects it with their output dir, runs the batch. Outputs land at attacker-chosen relative (`../../…`) or absolute paths, silently overwriting same-named files with attacker-influenced PDF bytes (the `watermark` op's `text` param is schema-bounded but attacker-chosen). Impact: silent data destruction / planting under the victim's own user rights; not elevation.
**Evidence**: `tests/TestSweepW1PresetAdversary.cpp`
- `parseRefusesParentTraversalNamingTemplate` FAIL: parse accepted `../evil_{n}.pdf`
- `parseRefusesAbsoluteNamingTemplate` FAIL: parse accepted `C:/Users/Public/evil_{n}.pdf`
- `resolveNamingRefusesTraversalAndAbsoluteResults` FAIL: resolver accepted `../` template (resolved `../evil_1.pdf`)
- `joinSemanticsPreserveTraversalProbe` PASS: `QDir(outDir).filePath("../escaped.pdf")` resolves outside outDir; absolute operand returned as-is.
**Repro command**: `cd build-r18-noeng && QT_QPA_PLATFORM=offscreen ./TestSweepW1PresetAdversary.exe -o out.txt,txt` (expect exit 3 until fixed).
**Existing pin coverage**: `TestBatchPresets::namingTokensResolveAndSanitize` covers separator smuggling through token VALUES only — the template literal was unpinned.
**Exploitability**: high (attacker-controlled file write, silent with legal schema value `onConflict: "overwrite"`; bounded by the victim's user rights).
**Blast radius**: data destruction/planting in user-writable locations; no code execution vector identified (writes are `.pdf`-suffixed PDF bytes; absolute paths allow overwriting any user-writable file type-insensitive target).
**Remediation (describe-only)**: validate the RESOLVED result inside `resolveNaming` — reject any result containing `/`, `\`, `:`, or `..` segments (one guard; all three call sites — GUI pre-check, worker capture, parse-time validation — inherit it). Optionally have `parse()` re-run the same resolver post-fix so hostile presets are refused at import.
**Regression check**: the three failing slots above flip green; `TestBatchPresets` naming pins must stay green.

### W1-02 — Aliased sidecar fieldName bindings defeat coverage verification (false success) — MEDIUM-HIGH

**Component / asset**: `src/core/SigningRequestModel.cpp` `fromJson` (the defect), vs `src/ui/SigningRequestDialog.cpp` `saveRequest` (line ~341 duplicate lint) and `src/engines/SignatureFieldCreator.cpp` create-time lint (the two places that DO enforce).
**Entry point**: attacker-crafted `<doc>.signrequest.json` opened alongside a document (the fill flow loads the sidecar from disk every step — `SendForSigningController::runSignStep`).
**Bug class / CWE**: CWE-20 (improper input validation at trust boundary) leading to false-success workflow attestation (honesty-contract violation, moat M8).
**Shared defect**: the 1-field == 1-signer invariant is enforced in two of the three places that construct/consume the model. `fromJson` — the only place that consumes UNTRUSTED bytes — accepts duplicate `fieldName` entries. `verifyAgainstDocument` then cannot see the alias: coverage is matched per-FIELD and the count check passes whenever the document carries as many real signatures as entries.
**Spoof proven end-to-end (real signatures)**: document with two REAL ByteRange-intact signatures (attacker's own cert on sig_A and sig_B); crafted sidecar binds BOTH entries to sig_A with full engine-style attestation (`signed`, `signedFieldName`, `fieldMatch`, `attainedLevel`, `signatureSummary` all copied from what a real step writes). `verifyAgainstDocument` returns `consistent = true`, zero warnings; the progress panel renders "Signing request complete — all 2 signer(s) signed. Every recorded signature matches the document." — for a request that was never fulfilled (signer 2 never signed a field they were bound to).
**Evidence**: `tests/TestSweepW1SigningAdversary.cpp`
- `fromJsonRefusesDuplicateFieldBindings` FAIL: `fromJson` accepted two signers bound to `sig_A` (error=0/None).
- `aliasedSidecarReportsConsistentWhenItMustNot` FAIL: aliased crafted sidecar verified CONSISTENT against a 2-signature document (warnings empty).
**Repro command**: `cd build-r18-noeng && QT_QPA_PLATFORM=offscreen ./TestSweepW1SigningAdversary.exe -o out.txt,txt` (expect failures until fixed; requires tests/fixtures/signing).
**Exploitability**: the sidecar lives next to the PDF (untrusted-by-design transport: e-mail/cloud). The victim's UI attests completion they cannot distinguish from the honest state. No crypto bypass — the signatures on the document are real; the lie is the workflow attestation.
**Blast radius**: false-success (a reviewer ships a "fully signed" document missing a real signer's signature).
**Remediation (describe-only)**: one lint at the model boundary — `fromJson` returns `SchemaInvalid` when two entries share a trimmed `fieldName` (mirror `saveRequest`'s wording). Sibling call sites already enforce it; the consumers (`precheck`, `runFillStep`, `verifyAgainstDocument`) then inherit the invariant instead of re-checking it.
**Regression check**: both slots flip green; `TestSendForSigning` (handshake, fill flow, verification honesty) must stay green.

### W1-03 — Step mutation on failure + permanent request deadlock when a foreign unsigned field exists — MEDIUM

**Component / asset**: `src/core/SigningRequestRunner.cpp` — `precheck` (binding gate, lines ~99-132) and `runFillStep` (lazy placement, lines ~146-176); engine seam `SignatureManager` D6 post-condition (~line 1754) which fails the sign whenever ANY post-validation entry lacks `integrityIntact` — and an unsigned field reports `integrityIntact=false` (SignatureManager.cpp:2165-2192 defaults + "Unsigned" classification).
**Entry point**: document carrying a foreign UNSIGNED signature field (attacker-placed, or simply a stale artifact) + sidecar entry bound to a lazy anchor.
**Bug class / CWE**: CWE-460 (inconsistent cleanup on failure) + honesty-contract violation (false error text) + workflow DoS (CWE-400 class).
**Shared defect**: `precheck` validates only THE BOUND entry (exists-unsigned-or-creatable) and never consults the engine's GLOBAL precondition — "the document must not contain any unsigned field other than the one about to be signed". `runFillStep` then mutates the document (lazy placement writes the anchored field to `docPath` via `createSignatureFields(src, specs, src, …)`) BEFORE learning the engine cannot proceed. Result, reproduced:
1. precheck passes (foreign field never checked);
2. document mutated (`fieldCreated=true`);
3. engine signs the FIRST unsigned field — the foreign one (`U_foreign`), per the documented first-field rule;
4. post-validation still sees an unsigned field → `SignOutcome::Failed`, candidate dropped;
5. step error text claims "the document is unchanged" — FALSE (the document gained an unsigned field);
6. every retry fails identically → the request can NEVER complete; the document stays polluted.
(Side note: SignatureManager's post-condition log wording "A signature's integrity was broken by this update" is also misleading here — the actual cause is the surviving unsigned field, not broken integrity. Developer-facing only.)
**Evidence**: `tests/TestSweepW1SigningAdversary.cpp::foreignUnsignedFieldPollutesDocumentAndDeadlocksRequest` FAIL with both halves: `mutation-on-failure=YES` (`fieldCreated=true` on a failed step) and `permanent-deadlock=YES` (retried step failed: "The signing engine could not write a signature for signer 1 — the document is unchanged.").
**Repro command**: as W1-02.
**Impact**: fail-closed (no false success) but: permanent workflow DoS, unreviewed document mutation on failure (contradicts the runFillStep failure contract wording), dishonest user-facing error. Fail-closed UX debt from the honest D6 post-condition — the gate is right; the step just never asks it before mutating.
**Remediation (describe-only)**: extend `precheck` with the engine's global condition — enumerate `signatureFieldAnchors` + `validateSignatures`; if any unsigned field other than (the bound one when it exists) will survive the step, refuse `MissingField`-style BEFORE mutation. Alternatively make lazy placement stage onto a candidate instead of `docPath` so a failed step leaves the document byte-identical. Either fix is single-point; no per-caller patches.
**Regression check**: the slot flips green (refusal-before-mutation OR honest completion); `TestSendForSigning::fillFlowRealSignTwoSigners` / `orderAdvisoryAndEngineFirstFieldHonestyPin` must stay green (lazy placement must keep working when the request itself owns the only unsigned field).

### W1-04 — Printable summary export dies on ordinary annotation content — MEDIUM

**Component / asset**: `src/engines/ReviewSummaryWriter.cpp` `renderSummaryDocument` (draw path) + PoDoFo 1.1.0 standard-14 Helvetica (WinAnsi) via `GetStandard14Font`.
**Entry point**: ANY annotation whose /T (author) or /Contents (text) carries a character outside the WinAnsi-mappable set — including a TAB (0x09), control chars 0x01-0x1F, CJK, emoji. Nothing malicious required.
**Bug class / CWE**: CWE-400-class availability (feature denial), fail-closed.
**Shared defect**: the draw path uses a non-Unicode standard-14 font and lets a PER-STRING encoding fault abort the ENTIRE export. PoDoFo throws `PdfErrorCode::InvalidFontData` ("The provided string can't be converted to CID encoding", PdfEncoding.cpp:106); `writePrintable` catches and fails the whole file.
**Trigger matrix (diagnostic slot `encodingTriggerTriage`)**:
- FAIL: TAB (0x09), 0x01, 0x1F, CJK text, CJK author, emoji (surrogate pair)
- OK: plain Latin, HTML-lookalike text, operator-lookalike text, NUL (0x00 — C-string truncation), Latin-1 é, empty author/text
**Evidence**: `tests/TestSweepW1SummaryPolicyAdversary.cpp`
- `writePrintableSurvivesNonWinAnsiComments` FAIL (CJK comment kills export)
- `writePrintableSurvivesTabInCommentText` FAIL (a single TAB kills export)
- green pins: 800k-char adversarial set (HTML/operator/control/NUL/space-pathology payloads) terminates ~1.3 s, paginates boundedly, re-opens via PoDoFo — operator injection, unbounded loops and layout amplification are all REFUTED (see W1-R6).
**Repro command**: `cd build-r18-noeng && QT_QPA_PLATFORM=offscreen ./TestSweepW1SummaryPolicyAdversary.exe -o out.txt,txt`.
**Attacker narrative**: send a victim a document with one CJK (or even just TAB-bearing) comment; their printable review summary and print surface are dead — deterministic, no crash, error message discloses the PoDoFo fault text.
**Blast radius**: feature denial; no corruption, no leak.
**Remediation (describe-only)**: draw with a CID-capable/Unicode font (PoDoFo font loading with Identity-H), or sanitize per-string before `drawLine` (replace unmappable codepoints with U+FFFD or drop) — the sanitize belongs at the `drawLine` boundary so every draw site inherits it; a per-string fault must degrade the STRING, never the FILE.
**Regression check**: both slots flip green; `TestPrintableSummary` / `TestReviewSummary` must stay green.

### W1-05 — Machine-policy squat: attacker-writable `%PROGRAMDATA%` file is machine-ENFORCED — MEDIUM-LOW (design/ACL posture)

**Component / asset**: `src/core/PolicyController.cpp` `defaultPolicyPath` (GenericDataLocation = `C:\ProgramData\GlyphPDF\policy.json`) + `load` (no ownership/ACL/integrity check); enforcement `src/shell/controllers/SecurityController.cpp` `readSigningConfig` (the ONE production signing-settings reader) via `effectiveValue` for `signing/tsaUrl` + `signing/padesLevel` (`isEnforcedKey`).
**Mechanism (proven by probe, green)**: on a default Windows install a standard-user process can create `C:\ProgramData\GlyphPDF\` and plant `policy.json` (auto-creatable dir, creator owns the file). GlyphPDF loads it (`State::Loaded`) and the two enforced keys override the user's stored values at EVERY sign/certify dispatch. Probe `attackerPlantedPolicyFileIsLoadedAndEnforced` demonstrates: attacker `tsaUrl` (`https://attacker.example/tsa`) and `padesLevel` (`B-B` downgrade) flow through `effectiveValue` over the user's values; the unrecognized-key disclosure and the status line naming the attacker path are the only disclosures.
**What is NOT reachable**: plain-HTTP TSA. The NetworkTouchpoints disclosure ("HTTPS enforced — HTTP URLs are refused") is TRUE — `SignatureManager::httpPost` refuses `http://` at the single network seam (SignatureManager.cpp:255-258). An https TSA redirect requires a CA-trusted attacker endpoint (admin rights) — not available to the same unprivileged attacker.
**Actual payload**: silent machine-wide signing downgrade (`padesLevel: B-B` — no timestamp, weaker assurance) and/or TSA redirect to an attacker-chosen https host, plus preference-lock of the remaining allowlist keys. Disclosure exists only in the Preferences status line / support bundle.
**Remediation (describe-only)**: at load, verify the policy file's ACL/owner is admin-tier (GetNamedSecurityInfo; refuse when a non-admin SID owns it) or pin a first-run recorded hash. Consider disclosing the enforcement as a persistent badge, not a transient status line.
**Regression check**: the probe stays green; add the ACL-refusal case once implemented.

---

## REFUTED hypotheses (evidence + covering pin)

| # | Hypothesis | Refutation / pin |
|---|---|---|
| W1-R1 | Read→enforce race on policy.json (swap the file between read and enforcement) | `PolicyController::load` sets `m_loadedOnce` (PolicyController.cpp:66); `ensureLoaded` never re-reads (lines 114-119). Enforcement reads the cached map. Symlink/hardlink angle adds nothing over direct file creation (same write access required) — folded into W1-05's posture. |
| W1-R2 | Support-bundle leak via hostile recents paths | Recents reduced to COUNT only (SupportBundle.cpp:177-183). Scrub pass over every string; capability `detail` (the only path-bearing field — Capability.cpp:514) is deliberately omitted from the bundle (SupportBundle.cpp:233); `whyNot`/`alternative` are path-free by construction (verified across Capability.cpp). |
| W1-R3 | Hostile TSA URL scheme (`http://` from user prefs or policy) | Refused at the single network seam: `SignatureManager::httpPost` rejects `http://` (SignatureManager.cpp:255-258). The disclosure claim is honest. |
| W1-R4 | Hostile preset op parameters (huge counts, negative indices) poisoning the chain | Schema lints everything post-parse: quality 10-100, targetDpi 36-600, opacity 1-100, text ≤120 chars, steps ≤ kMaxSteps=16, strings ≤1 MiB, file ≤256 KiB; unknown keys/params fail-closed. `jsonToInt`'s out-of-range int conversion garbage cannot survive the post-conversion range checks. |
| W1-R5 | Capability-check TOCTOU (veraPDF vanishing between blocker check and run) | Fail-closed: `runPresetCheckStep` fails the FILE when `report.validatorAvailable` is false (BatchMode.cpp) — "never a silent skip". |
| W1-R6 | Printable-summary annotation content injection: PDF operator injection, unbounded wrap/pagination loops, layout amplification | DrawText escapes PDF strings (no operator injection — operator-lookalike payloads render as text, probe green); `wrapText` advances ≥1 char per iteration, `ensureRoom` is monotonic; 800k-char adversarial set: OK in ~1.3 s, bounded sheet count, re-opens (P1a/P1b/P1c green). |
| W1-R7 | Sidecar path traversal via field names/anchors ({basename}-analogues) | Sidecar path is `docPath + ".signrequest.json"` — no tokens, no user text; field names never touch the filesystem (by-name engine targeting only). Anchor rects are linted at create (`viewerRect` positivity, page range checked against the real page count — SignatureFieldCreator.cpp:95-123). |

## HYPOTHESES / INCONCLUSIVE (exact missing pieces)

| # | Hypothesis | Status | Missing piece |
|---|---|---|---|
| W1-H1 | Two GlyphPDF instances signing the same document concurrently → lost-update on the sidecar / interleaved candidate commits (last writer wins; a committed signature can be silently overwritten) | HYPOTHESIS, medium confidence | A two-process harness driving `runSignStep` concurrently on one doc+sidecar pair. P1 is explicitly single-user/single-instance; out of the P1 threat model, noted as a P2 multi-session concern. |
| W1-H2 | Re-confirm flow asks the user to accept CHANGED bytes without re-rendering them first (SendForSigningController.cpp:179-200): user consents to bytes they never saw; a swap between viewer load and sign gets consented and signed | HYPOTHESIS (honesty gap), medium confidence | GUI-flow harness that swaps the on-disk file after session load and screenshots the state at the question dialog. The gate DOES detect the change and requires explicit consent — the gap is consent-before-reload. Suggested fix shape: `markReload()` before asking, or render the new bytes' hash/summary in the dialog. |
| W1-H3 | Preset candidate-chain intermediates leak when an engine step THROWS (the `intermediates` cleanup in `runPresetPipelineStep` sits after the loop; the E-13 catch-all sits in the mapped lambda ABOVE the function — an exception skips the cleanup, orphaning candidates in the shared `%TEMP%/glyphpdf-candidates`, the FU-2 shared-dir collision class) | INCONCLUSIVE by dynamic evidence; mechanism confirmed by reading | A crafted PDF that makes a `PdfEditorEngine` seam (`optimizeDocument`/`exportPdfA`/`sanitizeDocument`) throw mid-chain. Consequence if confirmed: temp-dir pollution / disk growth only; original file untouched; no FU-2 collision observed in this run (`candidatesDirClean` pin green). |

## Cross-cutting notes (scope 5)

- Sign-while-placing races in-app: REFUTED — `runSignStep` is GUI-serialized (WindowModal progress dialog blocks the main window while the worker runs); the model snapshot reloads from DISK every step; the controller advances `preparedSha256` over the workflow's own lazy creation on the failure path (SendForSigningController.cpp:234-248); autosave writes `<file>.autosave.pdf`, never the document path. The remaining sign-while-placing defect is W1-03 (engine-precondition deadlock), not a hash-gate race.
- SafeSave candidate lifecycle across the new call sites: all four audited seams (Runner `runFillStep`, `SignatureFieldCreator`, preset candidate chain, `ReviewSummaryWriter::writePrintable`) drop their candidates on every outcome including success (`5c8fd08` discipline); `TestSweepW1SigningAdversary::candidatesDirClean` green.

## Repro inventory & commands

Files (branch `feat/sweep-w1-adversary`):
- `tests/TestSweepW1PresetAdversary.cpp`
- `tests/TestSweepW1SigningAdversary.cpp`
- `tests/TestSweepW1SummaryPolicyAdversary.cpp`
- `CMakeLists.txt` — three appended `if(EXISTS)` registration blocks (end of file). Note: the signing target links `pdfws_engines pdfws_ui pdfws_engines pdfws_core` — `SigningRequestRunner` (pdfws_engines) references `gp::SecurityController` (pdfws_ui); the plain `pdfws_ui pdfws_engines` order does not link a fresh target.

Build + run (from the repo root):
```
MSYSTEM=UCRT64 /c/msys64/usr/bin/bash.exe -lc \
  'cd /d/pdf/pdf-r18/build-r18-noeng && ninja -j 2 TestSweepW1PresetAdversary \
   TestSweepW1SigningAdversary TestSweepW1SummaryPolicyAdversary'
QT_QPA_PLATFORM=offscreen ./TestSweepW1PresetAdversary.exe -o o1.txt,txt          # exit 3 (W1-01)
QT_QPA_PLATFORM=offscreen ./TestSweepW1SigningAdversary.exe -o o2.txt,txt         # exit 3 (W1-02, W1-03)
QT_QPA_PLATFORM=offscreen ./TestSweepW1SummaryPolicyAdversary.exe -o o3.txt,txt   # exit 2 (W1-04)
```
Fixing the four findings flips all suites green with zero test edits.

## Residuals

- W1-H1..H3 (above) — need the named missing pieces.
- The preset-chain throw-leak (W1-H3) may deserve a pin regardless (move the cleanup into a scope guard) — cheap hardening.
- Accessibility lane landed in parallel and was not re-attacked here (per scope: newest surfaces only).
- Known-flake suites (`TestOllamaProvider`/`TestBatchMode`/`TestLaneScheduler`/`TestReadOnlyGate`/`TestBatchOpsCoverage`, `TestEngineSave×TestRedactTransaction`) not run — no production code changed in this round, so no regression surface exists.
