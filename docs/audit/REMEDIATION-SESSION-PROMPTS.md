# GlyphPDF — Audit Remediation Session Prompts (self-contained, parallel-ready)

**Generated:** 2026-06-09 | **Methodology:** 7-H prompt engineering (prompt-factory) | **Format:** matches `SESSION_PROMPTS_V2.md`
**Source of truth:** `docs/audit/AUDIT-2026-06-02.md` (plan) + `docs/audit/_workstreams/A-G.md` (per-finding evidence + proposed fixes)
**Branch:** all sessions work on `audit-remediation`.

Each session below is **completely self-contained** — paste one block into a fresh Claude Code session rooted at `C:\Users\User\Projects\pdf` and it runs with zero prior context. Each names the **orchestrator agent(s)** to invoke (`Agents:` line). Run file-disjoint sessions in **parallel** per the matrix.

---

## ALREADY DONE (do NOT redo — committed on `audit-remediation`)

- **WP-0** doc truth-up · **WP-1** remove cloud upload (5 commits; deleted Anthropic/OpenAI/Gemini providers, CollaborationManager, SYNCED indicator, Send-for-Sig; AI = Ollama-only) · **WP-2** crypto/security (14 commits: A-01,E-01,E-03,E-04,E-06,E-08,E-09,E-05,A-02,A-04,A-05,B-03,E-13,E-02) · **WP-3** architecture (6 commits: D-01,D-02,D-04,D-05,D-07,D-10) · **WP-4 partial:** C-02 real `PdfStructureMapper` committed (`362c436`).
- Baseline at HEAD: **32/32 ctest pass.** ⚠️ **Uncommitted in the working tree:** an in-progress **C-03 FormBuilder** change (8 files, unverified) — SP-1 owns it. Also unrelated `graphify-out/`, `.agents/`, `.obsidian/`, `label_communities.py` artifacts — **do NOT commit these into remediation.**

---

## PARALLEL / SEQUENCING MATRIX

| Session | Touches (file cluster) | Parallel-safe with | Must run after |
|---|---|---|---|
| **SP-1** WP-4 finish (UI mock) | FormManager, FormBuilderMode, form-field commands, ThumbnailSidebar, PdfAValidationPanel, BatchMode/PreferencesDialog labels | SP-3, SP-5a | — |
| **SP-2** WP-5 backend selectors | PreferencesDialog, BackendRouter, OcrEngine, AIChatPanel | SP-3, SP-5a | **SP-1** (shares PreferencesDialog) |
| **SP-3** WP-6 perf/concurrency | AutosaveManager, LaneScheduler, PipelineStage, RenderCache | SP-1, SP-2, SP-5a | — |
| **SP-4** WP-7 tests + D-mediums | tests/*, CMakeLists test section, CompareMode, BackendRouter | (run mostly solo) | **SP-1, SP-2, SP-3** (tests their behavior) |
| **SP-5a** WP-8 supply (non-history) | CredentialManager, .github/workflows/ci.yml, third_party/pdfium | SP-1, SP-2, SP-3 | — |
| **SP-5b** WP-8 **history rewrite (B-01)** | git history (filter-repo) | **NONE — run solo** | all code sessions merged + clean tree |
| **SP-6** WP-9 final verify | read-only + docs | none | **everything else** |

**Recommended waves:** Wave A (parallel) = SP-1 + SP-3 + SP-5a → Wave B = SP-2 → Wave C = SP-4 → Wave D = SP-5b (solo) → Wave E = SP-6.

---

## STANDARD EXECUTION PROTOCOL (inherited by every session)

```
PHASE 0 — ORIENT (zero-context start)
  git rev-parse --abbrev-ref HEAD   # must be audit-remediation; else: git checkout audit-remediation
  Read this session's <files_to_read>. Read the cited workstream report for exact evidence.
  Do NOT re-derive findings — they are 100% logged in docs/audit/.

PHASE 1 — PLAN  Identify the order of deliverables that minimizes rework.

PHASE 2 — IMPLEMENT + VERIFY (per deliverable)
  a. Write the fix (match existing C++17/Qt6 style; minimal, surgical).
  b. Build:  (PowerShell)
       $env:PATH = 'C:\msys64\ucrt64\bin;' + $env:PATH
       $env:QT_QPA_PLATFORM = 'offscreen'
       $env:QT_PLUGIN_PATH  = 'C:\msys64\ucrt64\share\qt6\plugins'
       cmake -B build -G Ninja        # ONLY if you changed CMakeLists.txt
       cmake --build build --parallel 8
  c. Test:  ctest --test-dir build --output-on-failure -j4     # MUST stay 32/32 (or higher if you add tests)
  d. If build/test fails → fix before proceeding. Never commit a red tree.
  e. Atomic commit per finding:
       git -c user.name="Claude (audit)" -c user.email="noreply@anthropic.com" `
         commit -m "fix(<ID>): <title>" -m "<detail>" -m "Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"

PHASE 3 — CONTEXT GATE  Past ~50% context → write STATE.md (deliverable progress, files touched,
  build/test status) and stop. Quality over completion.

PHASE 4 — FINAL VERIFICATION
  Run <success_criteria>. Verify ctest result is FRESH (result mtime newer than your edits — CLAUDE.md §7
  Patterns 1-2: never trust a stale result file or a self-report). Update the WP row in
  docs/audit/AUDIT-2026-06-02.md §7 and append commit hashes to the vault note
  C:\Users\User\.claude\memory\projects\glyphpdf\10-audit-2026-06-02.md.
```

**Environment constants (current — MSYS2 ucrt64; the Qt-installer/vcpkg constants in SESSION_PROMPTS_V2 are OBSOLETE):**
- Project: `C:\Users\User\Projects\pdf` · Branch: `audit-remediation`
- Toolchain: GCC 16.1.0, Qt 6.11.0, CMake 4.3.3 at `C:\msys64\ucrt64\bin` (NOT Qt-installer, NOT vcpkg)
- Build: `cmake --build build --parallel 8` · Test: `ctest --test-dir build --output-on-failure -j4` (32 targets)
- Never: link MuPDF/Poppler in-process · use qpdf in the signing path · weaken SHA-256 signing · re-introduce a cloud-upload path (WP-1 removed them).

---

## SP-1 — WP-4 finish: fix all remaining UI mock data

<session_metadata>
Phase: Remediation WP-4 (finish) | Priority: HIGH | Depends on: none (C-02 already committed)
Agents: **/backend** (primary — FormManager/PoDoFo engine ops) + **/frontend** (secondary — FormBuilderMode UI)
Estimated context: ~40% | Risk: MEDIUM — touches FormManager + an uncommitted in-progress change
</session_metadata>

<role>
You are a senior C++17 / Qt 6 engineer finishing the "no fake UI" remediation of GlyphPDF, a native Windows PDF workstation. Every UI surface must be wired to a REAL backend or removed — never a fake-functional control or a "coming soon" disabled button.
</role>

<project_context>
GlyphPDF: C++17 / Qt 6.11 / MSYS2 ucrt64, at C:\Users\User\Projects\pdf. 4 static libs (pdfws_core → pdfws_engines → pdfws_commands → pdfws_ui) + executable. Branch audit-remediation, 32/32 ctest pass. The project's history (CLAUDE.md §7) is "mock data masquerading as real" — your job closes that out.
</project_context>

<current_state>
WP-4 was interrupted mid-flight. C-02 (real PdfStructureMapper) is committed. The working tree has an UNCOMMITTED, unverified C-03 FormBuilder change: src/commands/{Delete,Move,Resize}FormFieldCommand.h, src/core/interfaces/IFormManager.h, src/engines/FormManager.{h,cpp}, src/modes/FormBuilderMode.cpp, tests/TestFormBuilder.cpp. The tree also contains unrelated graphify-out/, .agents/, .obsidian/, label_communities.py artifacts that must NOT be committed.
</current_state>

<objective>
Land C-03 (FormBuilder delete/move/resize that actually persist), remove all remaining "coming soon" stubs and stale sprint labels, and verify/disclose every other mock surface the C report lists — keeping ctest green.
</objective>

<files_to_read>
docs/audit/_workstreams/C-claims-vs-reality.md   (the full mock/stub inventory with file:line)
docs/audit/AUDIT-2026-06-02.md                    (§5 WP-4 row)
src/engines/FormManager.cpp + src/modes/FormBuilderMode.cpp   (the uncommitted change)
</files_to_read>

<deliverables>

### D1: Land or redo the uncommitted C-03 FormBuilder change
**Files:** src/engines/FormManager.{h,cpp}, src/commands/{Delete,Move,Resize}FormFieldCommand.h, src/core/interfaces/IFormManager.h, src/modes/FormBuilderMode.cpp, tests/TestFormBuilder.cpp
**Acceptance:** delete/move/resize of a form field PERSISTS across save+reload (a deleted field does NOT reappear); undo/redo correct; build + 32/32 ctest green.
**Implementation:** Build the tree as-is first. If green → extend TestFormBuilder to assert persistence after save+reload, then commit `fix(C-03): FormBuilder delete/move/resize persist via FormManager engine ops`. If it fails to build/test → fix it; if unsalvageable, `git checkout -- <the 8 files>` and reimplement the engine ops in FormManager (AcroForm field CRUD via PoDoFo) wired to the QUndoCommands.

### D2: Remove "Coming in v1.1" disabled stubs
**Files:** src/modes/FormBuilderMode.cpp (≈ lines 96,111,322,344 — Auto-detect, Preview, tab-order-to-PDF)
**Acceptance:** no `setEnabled(false)` paired with a "Coming"/"v1.1" tooltip remains. Each control is either wired to its real engine method or removed from the UI entirely.

### D3: Strip stale sprint codenames from user-facing text (G-11/G-12)
**Files:** src/modes/BatchMode.cpp ("coming in M4" / "coming in M3-P4"), src/ui/PreferencesDialog.cpp ("real LLM calls scheduled for v1.1" — stale; AI is Ollama-only now)
**Acceptance:** `grep -rniE "coming in M[0-9]|scheduled for v1\\.1|M[0-9]-P[0-9]" src/` returns nothing user-facing.

### D4: Verify/close remaining mock surfaces from the C report
**Files:** per C report — ThumbnailSidebar, PdfAValidationPanel, src/shell/Sidebar.cpp (placeholder bytes), any other undisclosed MOCK/STUB
**Acceptance:** each is verified against CURRENT code (some are already real after prior sprints) and is either wired-to-real or honestly disabled/removed. Document the disposition of each in the commit body.

### D5: Keep the tree clean
**Acceptance:** none of graphify-out/, .agents/, .obsidian/, label_communities.py, .context/ref_ppocr.py are staged or committed. Use explicit `git add <file>` per finding; never `git add -A`.
</deliverables>

<success_criteria>
- ctest 32/32 (or higher) green, fresh.
- No `setEnabled(false)`+"coming" stubs; no sprint codenames in user-facing strings.
- FormBuilder field delete/move/resize persist across reload (proven by a test).
- AUDIT-2026-06-02.md §7 WP-4 row marked DONE with commit hashes; vault note appended.
</success_criteria>

---

## SP-2 — WP-5: make every available backend selectable in the UI

<session_metadata>
Phase: Remediation WP-5 | Priority: MEDIUM-HIGH | Depends on: SP-1 (shares PreferencesDialog)
Agents: **/frontend** (primary — Preferences UI + selectors) + **/backend** (secondary — BackendRouter/engine plumbing)
Estimated context: ~35% | Risk: MEDIUM
</session_metadata>

<role>
You are a senior Qt 6 UI engineer exposing GlyphPDF's swappable engines as first-class, honest user choices (no option that silently does nothing).
</role>

<project_context>
GlyphPDF (C++17/Qt 6.11/MSYS2 ucrt64) routes PDF operations through `BackendRouter` over three backends (PoDoFo, PDFium, qpdf), OCR through Tesseract + RapidOCR (+ ensemble), and AI through a local Ollama provider (cloud providers were removed in WP-1). Branch audit-remediation, 32/32 ctest green.
</project_context>

<current_state>
Backend choice is implicit/hardcoded per operation (BackendRouter). OCR engine and AI model are not user-selectable in Preferences. After WP-1 the AI tab is Ollama-only.
</current_state>

<objective>
Add persisted, honest UI selectors for PDF engine, OCR engine, and Ollama model — each reflecting real availability and actually changing the engine used.
</objective>

<files_to_read>
src/engines/BackendRouter.{h,cpp}
src/ui/PreferencesDialog.{h,cpp}
src/engines/OcrEngine.* + src/engines/ocr/RapidOcrEngine.* + src/engines/ai/OllamaProvider.*
docs/audit/_workstreams/D-architecture-quality.md  (D-08 BackendRouter raw-ptr note)
</files_to_read>

<deliverables>

### D1: PDF engine selector
**Files:** src/ui/PreferencesDialog.{h,cpp}, src/engines/BackendRouter.{h,cpp}
**Acceptance:** a Preferences control lets the user pick the backend for render/edit operations where more than one is valid; the choice persists (QSettings) and is actually honored by BackendRouter. Operations that REQUIRE a specific backend (e.g. signing = PoDoFo, never qpdf) stay locked with a tooltip — never offered as a fake choice.

### D2: OCR engine selector
**Files:** src/ui/PreferencesDialog.{h,cpp}, OCR wiring
**Acceptance:** user selects Tesseract / RapidOCR / Ensemble; persisted; honored by the OCR path. If a model/dep is missing, the option is greyed with an honest tooltip (no silent no-op).

### D3: Ollama model selector
**Files:** src/ui/PreferencesDialog.{h,cpp}, src/engines/ai/OllamaProvider.*, src/modes/AIChatPanel.*
**Acceptance:** endpoint + model fields persist and are used by the live Ollama call; if Ollama is unreachable the UI says so honestly.

### D4 (optional): tidy BackendRouter ownership (D-08)
**Acceptance:** if quick, replace raw-pointer backend handling with clear ownership; otherwise leave for SP-4 and note it.
</deliverables>

<success_criteria>
- Each selector persists across restart and demonstrably changes the engine used (or is honestly disabled).
- No selector is a no-op. Build + 32/32 ctest green, fresh.
- AUDIT §7 WP-5 row DONE + vault note appended.
</success_criteria>

---

## SP-3 — WP-6: performance & concurrency correctness

<session_metadata>
Phase: Remediation WP-6 | Priority: HIGH (UAF/deadlock) | Depends on: none — PARALLEL-SAFE with SP-1/SP-2/SP-5a
Agents: **/performance** (primary) + **/backend** (secondary)
Estimated context: ~35% | Risk: MEDIUM-HIGH (threading)
</session_metadata>

<role>
You are a senior C++17 concurrency engineer fixing native threading defects in GlyphPDF's render/OCR scheduler — use-after-free, deadlock, and a GUI-thread stall.
</role>

<project_context>
GlyphPDF (C++17/Qt 6.11/MSYS2 ucrt64). Heterogeneous LaneScheduler (persistent warm GPU worker + CPU pool + cross-page pipeline), 3-tier RenderCache (256 MB LRU), AutosaveManager. Branch audit-remediation, 32/32 ctest green (TestLaneScheduler, TestThreadSafety, TestPerformance exist but total suite runs in <5 s, so heavy-load paths are under-exercised).
</project_context>

<current_state>
Five concurrency findings open (see F report). GPU warm-worker design is correct; the defects are lifetime/backpressure/GUI-thread issues.
</current_state>

<objective>
Fix F-01..F-05 (and remaining F mediums) so the scheduler is UAF-free, deadlock-free, and never blocks the GUI thread — verified by build + ctest and, where feasible, a stress assertion.
</objective>

<files_to_read>
docs/audit/_workstreams/F-performance-concurrency.md   (full evidence + proposed fixes)
src/engines/scheduling/LaneScheduler.{h,cpp} + PipelineStage.h
src/engines/RenderCache.cpp + src/engines/AutosaveManager.cpp
</files_to_read>

<deliverables>

### D1: F-01 — no msleep on the GUI thread
**Files:** src/engines/AutosaveManager.cpp (≈103)
**Acceptance:** the failed-rename retry no longer calls `QThread::msleep` inside a `QFutureWatcher::finished` lambda; use a `QTimer`/off-thread retry. GUI never blocks 250 ms.

### D2: F-03/F-04 — LaneScheduler lifetime
**Files:** src/engines/scheduling/LaneScheduler.{h,cpp} (≈43-47, ≈180)
**Acceptance:** destructor cannot return while a GPU task lambda capturing `this` still runs (join/flag, not a bare 5 s wait); submit-after-shutdown returns/throws instead of deadlocking on `m_gpuSemaphore.acquire()`.

### D3: F-02 — pipeline backpressure deadlock
**Files:** src/engines/scheduling/PipelineStage.h (≈54-80)
**Acceptance:** with cpuCapacity==backpressure (e.g. 4 on a 4-core box) stage-3 cannot be starved; add a bounded test or reasoning in the commit body.

### D4: F-05 — RenderCache prefetch future
**Files:** src/engines/RenderCache.cpp (≈271)
**Acceptance:** the `QtConcurrent::run` future is tracked/cancelable; prefetch cannot touch cache structures after `clear()`/destruction; the `[[nodiscard]]` is honored.

### D5: remaining F mediums if time; else note as deferred.
</deliverables>

<success_criteria>
- Build + 32/32 ctest green, fresh (ideally `--repeat-until-fail 3` for the threading tests).
- No UAF/deadlock path remains for F-01..F-05. AUDIT §7 WP-6 row DONE + vault note appended.
</success_criteria>

---

## SP-4 — WP-7: make the security tests REAL + clean deferred D-mediums

<session_metadata>
Phase: Remediation WP-7 | Priority: CRITICAL (tests currently can't catch the bugs WP-2 fixed) | Depends on: SP-1, SP-2, SP-3
Agents: **/testing** (primary) + **/refactor** (secondary — deferred D-mediums)
Estimated context: ~45% | Risk: MEDIUM
</session_metadata>

<role>
You are a senior C++ test engineer. You judge tests by whether they can actually FAIL when the behavior is wrong — the project's history is tests that pass while the feature is broken (CLAUDE.md §7). You will make GlyphPDF's security tests genuinely prove the security property.
</role>

<project_context>
GlyphPDF (C++17/Qt 6.11/MSYS2 ucrt64), branch audit-remediation. 34 test targets defined, 32 in the default ctest run, all "green" — but several are hollow (QSKIP, raw-byte greps, tautologies). WP-2 just landed real crypto fixes (A-01 cert encryption, E-01 DocMDP, E-06 timestamp, E-08 incremental save) that currently lack gating tests.
</project_context>

<current_state>
See G report. TestResourceLimits is 100% QSKIP; the redaction-unrecoverability test greps raw bytes (FlateDecode hides a leak); TestEncryption never opens without a password; TestSignatureRealCrypto is silently QSKIPped (relative `kFixtureDir` resolves to build/) and has 8 pre-existing failures (validateSignatures reports a valid signed file as trust="Unsigned").
</current_state>

<objective>
Convert the hollow security tests into real assertions that gate WP-2's fixes, fix TestSignatureRealCrypto so it actually runs and validates, and clean the deferred D-mediums.
</objective>

<files_to_read>
docs/audit/_workstreams/G-tests-release-readiness.md  +  A-security-crypto-pdf.md  +  E-silent-failures.md  +  D-architecture-quality.md
tests/TestResourceLimits.cpp, TestRedaction.cpp, TestEncryption.cpp, TestSignatureRealCrypto.cpp, TestSanitization.cpp
</files_to_read>

<deliverables>
### D1: G-01 TestResourceLimits — real bounded-input assertions (remove the 100% QSKIP at ≈29,36,43).
### D2: G-02 Redaction unrecoverability — DECODE content streams (or re-extract text via PDFium) and assert the secret string is absent; a FlateDecode-compressed secret must FAIL the test. `tests/TestRedaction.cpp`.
### D3: G-03 Encryption enforcement — open the encrypted file WITHOUT the password and assert it fails; add an AES round-trip decrypt (covers A-01). `tests/TestEncryption.cpp`.
### D4: Fix TestSignatureRealCrypto — make `kFixtureDir` absolute (or copy fixtures next to the exe) so it RUNS in ctest; fix the 8 failures so validateSignatures correctly reports a valid signed file as trusted; assert WP-2's E-01 (/DocMDP present), E-06 (no null-byte timestamp), E-08 (incremental save keeps prior sig valid).
### D5: G-04 Sanitization — extend coverage to the ~20 implemented vectors (JavaScript, OCProperties, RichMedia, StructTreeRoot, per-page PieceInfo). `tests/TestSanitization.cpp`.
### D6: G-05/G-06 — remove the `P || !P` tautology (`TestSignatureRealCrypto.cpp:203`); make the OCSP-revocation test real (no permanent `QEXPECT_FAIL` soft-pass).
### D7: G-07/G-08 — wire `src/docmodel/DocumentFuzzer` into a hostile/malformed-PDF robustness test in the suite.
### D8: G-09/G-10 — add an ASan/UBSan-instrumented test build option in CMake; enable `-Wall -Wextra` on the project's own sources (consider `-Werror` for `src/`).
### D9: Deferred D-mediums — D-03 (CredUI PUBLIC link dep), D-06 (CompareMode `tr` shadow), D-08 (BackendRouter raw ptrs — coordinate with SP-2), D-09 (downcast to PoDoFoBackend).
</deliverables>

<success_criteria>
- The redaction-unrecoverability and encryption-enforcement tests are REAL (they fail on a deliberately-broken build) and gate in ctest. TestSignatureRealCrypto RUNS (not QSKIP) and passes. Full suite green, fresh. AUDIT §7 WP-7 DONE + vault appended.
</success_criteria>

---

## SP-5a — WP-8: supply-chain hardening (non-history)

<session_metadata>
Phase: Remediation WP-8a | Priority: MEDIUM | Depends on: none — PARALLEL-SAFE
Agents: **/security** (primary) + **/devops** (secondary — CI)
Estimated context: ~25% | Risk: LOW
</session_metadata>

<role>You are a supply-chain security engineer hardening GlyphPDF's credential storage, CI, and vendored-binary provenance.</role>
<project_context>GlyphPDF (C++17/Qt 6.11/MSYS2 ucrt64), branch audit-remediation, 32/32 green. License-contamination guards (MuPDF/Poppler/veraPDF/DjVu) already verified correct.</project_context>
<current_state>See B report. CredentialManager uses CRED_PERSIST_LOCAL_MACHINE; CI fetches ONNX Runtime 1.17.3 zip with no checksum; third_party/pdfium/lib/libpdfium.dll.a is committed with no version/checksum/provenance.</current_state>
<objective>Close B-04, B-05, B-06 (do NOT touch git history — that is SP-5b).</objective>
<files_to_read>docs/audit/_workstreams/B-supplychain-secrets-build.md; src/core/CredentialManager.cpp; .github/workflows/ci.yml; third_party/pdfium/</files_to_read>
<deliverables>
### D1: B-04 — `CredentialManager.cpp:29` CRED_PERSIST_LOCAL_MACHINE → CRED_PERSIST_ENTERPRISE (user-scoped).
### D2: B-05 — `.github/workflows/ci.yml:60` verify the ONNX Runtime 1.17.3 archive SHA-256 before extraction.
### D3: B-06 — add `third_party/pdfium/PROVENANCE.md` (exact source URL, version/commit, SHA-256 of libpdfium.dll.a) and verify the checksum in CMake or CI.
</deliverables>
<success_criteria>Cred persistence is user-scoped; CI fails on a bad ONNX checksum; pdfium provenance + checksum documented. Build + 32/32 green. AUDIT §7 WP-8a DONE + vault appended.</success_criteria>

---

## SP-5b — WP-8: purge committed private keys from git history (RUN SOLO, LAST)

<session_metadata>
Phase: Remediation WP-8b | Priority: HIGH (release blocker before any public push) | Depends on: ALL other sessions merged + working tree clean
Agents: **/security** (primary) + **/devops** (secondary)
Estimated context: ~30% | Risk: HIGH — REWRITES GIT HISTORY
</session_metadata>

<role>You are a security engineer performing a careful, irreversible git-history purge of secrets. Treat every step as destructive.</role>
<project_context>GlyphPDF, branch audit-remediation. `tests/fixtures/signing/ca.key`, `signer.key`, `test_signer.p12` were committed in history (commit `a6ea6aa`) and are live private keys.</project_context>
<current_state>`.gitignore` excludes them going forward but history still contains them. They are test-only keys, but must not ship in the public OSS repo.</current_state>
<objective>Remove the keys from ALL history and regenerate fresh test-only fixtures, without losing any audit-remediation commits.</objective>
<deliverables>
### D1: Pre-flight — confirm the working tree is CLEAN and a full backup/clone exists; confirm no other session is mid-flight.
### D2: Purge — use `git filter-repo --path tests/fixtures/signing/ca.key --path tests/fixtures/signing/signer.key --path tests/fixtures/signing/test_signer.p12 --invert-paths` across all refs. Verify: `git log --all -- tests/fixtures/signing/ca.key` is empty.
### D3: Regenerate — run `tests/fixtures/signing/generate.bat` to create fresh TEST-ONLY keys; ensure `.gitignore` keeps real keys out; rebuild + ctest green (signing tests use the regenerated fixtures).
### D4: Document the history rewrite (collaborators must re-clone) in CHANGELOG + a note; do NOT force-push without owner confirmation.
</deliverables>
<success_criteria>No private key bytes in any historical commit (verified); fixtures regenerated; suite green; rewrite documented. AUDIT §7 WP-8b DONE + vault appended.</success_criteria>

---

## SP-6 — WP-9: final verification + emergence capstone + release gate (RUN LAST)

<session_metadata>
Phase: Remediation WP-9 | Priority: HIGH | Depends on: SP-1..SP-5 complete
Agents: **/security** (primary) + **/testing** (secondary) — and run the **emergence-engine** capstone that the cap blocked
Estimated context: ~40% | Risk: LOW (mostly read-only + docs)
</session_metadata>

<role>You are the final verifier. You prove the remediation actually closed the dangerous findings and the cross-component catastrophe chains — not just that tasks were marked done.</role>
<project_context>GlyphPDF, branch audit-remediation, all remediation WPs merged. Original audit + plan in docs/audit/. The emergence-engine capstone (workstream H) never ran (cap-blocked).</project_context>
<current_state>WP-0..WP-8 fixes committed. Catastrophe chains from AUDIT §4 must be re-checked against the remediated code.</current_state>
<objective>Confirm green-on-disk, prove the 4 catastrophe chains are broken, run the emergent-risk capstone, and report release-gate status.</objective>
<files_to_read>docs/audit/AUDIT-2026-06-02.md (all) + all _workstreams/A-G.md; CLAUDE.md §9 (release gate)</files_to_read>
<deliverables>
### D1: Full clean build + `ctest --test-dir build --output-on-failure -j4 --repeat-until-fail 3` → all green; capture the result.
### D2: For each catastrophe chain (AUDIT §4: silent-un-signing, confidential-but-plaintext, redaction-leak+blind-test, update-RCE→key-theft) map the commits that close it and add a test or argument that it cannot recur.
### D3: Run the **emergence-engine** agent over the remediated code + all reports + git history → write `docs/audit/_workstreams/H-emergent-risk.md` (residual emergent risk after remediation).
### D4: Re-check CLAUDE.md §9 release gate — no Mock returns in IOcrEngine/ISignatureManager/IFormManager; no `setEnabled(false)`+"coming"; `grep TODO(audit-*)` == 0; CHANGELOG honest; translations status (see note below).
### D5: Write `docs/audit/AUDIT-2026-06-02-POSTREMEDIATION.md` (before/after, remaining items) and finalize AUDIT §7 + vault note.
</deliverables>
<success_criteria>Suite green under repeat-until-fail; every catastrophe chain documented closed; H capstone written; release-gate status reported with evidence.</success_criteria>

---

## NOT an agent task — owner action required

- **i18n (release blocker):** `translations/glyphpdf_{fr,ar,de}.ts` have 1394 `type="unfinished"` empty strings → `.qm` contain zero translations. A coding agent cannot *translate*; either commission human/translation-service translators, or ship **English-only for v1.0** and update README/CHANGELOG to say so honestly (then it is no longer a false claim). Decide before tagging v1.0.0.
- **EV code-signing cert** for the MSI (M8) — procurement, not code.
