# GlyphPDF — Round-2 Remediation Session Prompts (self-contained, parallel)

**Branch:** `audit-remediation` (HEAD `0564d34`). Each block is self-contained — paste into a fresh Claude Code session rooted at `C:\Users\User\Projects\pdf`. Findings detail: `docs/audit/round2/{SUMMARY,SEC-reverify,UX,UI,CODE,REPO}.md`.

## POLICY (apply in EVERY session — owner-locked 2026-06-09)
- **Keep the buttons. Delete the mock.** For any feature not implemented yet: **keep the control visible but `setEnabled(false)` + `setToolTip("Planned for a future release")`** (project Pattern 6). **NEVER** a silent `qWarning`, fake success, or mock return.
- **Triage handler-less tools:** if a **real engine method already exists**, WIRE it now; only **genuinely-unbacked** tools get the "Planned" disclosure.
- **Remove cloud-orphans** (`sendForm`,`collect`,`submit`,`auditLog`,`dlp`,`policy`) — offline-first killed them; they are NOT deferred.
- **Bugs get real fixes** (security / silent-failure / test / theme), never disclosure.

## ALREADY DONE (verified — do not redo)
WP-0…WP-3, WP-4/C-02, WP-6, WP-7, SP-2 selectors, WP-8a — committed; **build clean under `-Werror`, 33/33 ctest**. All 8 Pattern-5 mock surfaces gone; cloud removal clean; vault drift fixed.

## PARALLEL / SEQUENCING MATRIX
| Session | Touches | Parallel-safe with | After |
|---|---|---|---|
| **R2-1** CHAIN-1 closure | SignatureManager, PoDoFoBackend, HomeController save | R2-4, R2-5 | — |
| **R2-2** silent-failure sweep | SecurityController, HomeController | R2-4, R2-5 | R2-1 |
| **R2-3** redaction + parser harden | PoDoFoBackend, SignatureManager(CRL), PdfiumBackend | R2-4, R2-5 | R2-1 (shares SignatureManager) |
| **R2-4** ribbon triage + disclosure | RibbonModel, ToolRegistry, controllers | R2-1, R2-3, R2-5 | — |
| **R2-5** theme fixes | widgets + QSS resources | everything | — |
| **R2-6** FormBuilder persistence | FormManager, FormBuilderMode | R2-5 | R2-4 (shares Forms controller) |
| **R2-7** test integrity | tests/ + fixtures + CMake | R2-5 | R2-1/2/3/6 (tests their behavior) |
| **R2-8** B-01 key purge | git history | **NONE — solo** | all merged + clean tree |
| **R2-9** capstone + release + push | read-only + docs | none | everything |

**Waves:** A = R2-1 ‖ R2-4 ‖ R2-5 → B = R2-2, R2-3, R2-6 → C = R2-7 → D = R2-8 (solo) → E = R2-9.

## STANDARD PROTOCOL (every session)
0. `git rev-parse --abbrev-ref HEAD` == `audit-remediation` (else checkout). Read this session's `<files_to_read>` + the cited round-2 report. Don't re-derive.
1. Implement (match C++17/Qt6 style; minimal). Apply the POLICY above.
2. Build+test (PowerShell): `$env:PATH='C:\msys64\ucrt64\bin;'+$env:PATH; $env:QT_QPA_PLATFORM='offscreen'; $env:QT_PLUGIN_PATH='C:\msys64\ucrt64\share\qt6\plugins'; cmake -B build -G Ninja; cmake --build build --parallel 8; ctest --test-dir build --output-on-failure -j4` — must stay **green under `-Werror`** (verify result is fresh, CLAUDE.md §7 P1-2).
3. Atomic commit per finding: `git -c user.name="Claude (audit)" -c user.email="noreply@anthropic.com" commit -m "fix(<ID>): <title>" -m "<detail>" -m "Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"`.
4. Update `docs/audit/round2/SUMMARY.md` + the vault note `…/glyphpdf/10-audit-2026-06-02.md`.

---

## R2-1 — Close CHAIN-1 (silent un-signing)  [HIGH · security]
<session_metadata>Agents: **/security** (primary) + **/backend** | Risk: HIGH (signing path)</session_metadata>
<role>Senior C++/PAdES engineer making the "a signed document can't be silently altered" guarantee real.</role>
<current_state>Round-2 (SEC-reverify NF-3) found CHAIN-1 is NOT closed: `ProvenanceGuard::checkEditVia()` is a no-op (hardcoded `DirectStructural` it always allows), AND the incremental `writeUpdate` logic has **no production caller** — every save goes through full `saveDocument`, invalidating a signed doc's `/ByteRange`. `HomeController.cpp:117` TODO confirms enforcement deferred.</current_state>
<files_to_read>docs/audit/round2/SEC-reverify.md; src/pdfws_djot/ProvenanceGuard.*; src/shell/controllers/HomeController.cpp; src/engines/SignatureManager.cpp + src/engines/podofo/PoDoFoBackend.cpp (writeUpdate)</files_to_read>
<deliverables>
D1 — ProvenanceGuard a REAL gate: evaluate the actual edit path + signed/born-PDF state; REFUSE semantic/Djot save-back on a signed or born-PDF document and route to "Save as copy". Not a hardcoded allow.
D2 — Route saves of signed documents through the incremental `writeUpdate`/`SaveUpdate` path (E-08 logic); never full-`saveDocument` a doc that has signatures.
D3 — Test: sign a fixture, make an edit, save → assert the signature is STILL valid (and a Djot save-back is refused). Add to TestSignatureRealCrypto or a new test.
</deliverables>
<success_criteria>A signed doc survives edit+save with its signature intact; Djot save-back on signed/born-PDF is refused; a test proves both. Build+33/33 green.</success_criteria>

---

## R2-2 — Silent-failure sweep  [HIGH · security-adjacent]
<session_metadata>Agents: **/security** (primary) | After R2-1</session_metadata>
<current_state>UX-03/04: `permissionsDocument` + `removeSecurity` (`SecurityController.cpp:480-514`) discard `saveDocument` return → claim success on a failed write (same E-04 pattern). UX-14: `HomeController::onSave` shows save-failure only as a 5-second status message.</current_state>
<files_to_read>docs/audit/round2/UX.md; docs/audit/_workstreams/E-silent-failures.md; src/shell/controllers/SecurityController.cpp + HomeController.cpp</files_to_read>
<deliverables>
D1 — `permissionsDocument` + `removeSecurity`: check the save return; surface failure; never show success on failure.
D2 — `onSave` failure → `QMessageBox::critical` (match `onSaveAs`).
D3 — Grep ALL controllers/helpers for other discarded `saveDocument`/engine-op returns on user-facing actions; fix each.
</deliverables>
<success_criteria>No success UI on a failed write anywhere; build+ctest green; add an assertion where practical.</success_criteria>

---

## R2-3 — Redaction completeness + parser hardening  [HIGH/MED · security]
<session_metadata>Agents: **/security** (primary) | After R2-1 (shares SignatureManager)</session_metadata>
<current_state>SEC-reverify: NF-2 redaction misses images in Form-XObject-local resources, annotation `/AP` streams, and `/SMask`s (lookup always uses page resources). NF-1 forged CRL (`CRL_SIGNATURE_FAILURE`/`UNABLE_TO_DECRYPT_CRL_SIGNATURE`) soft-fails as UntrustedChain. NF-4/A-03 PdfiumBackend `renderPage/renderTile` allocate `QImage(w*scale,…)` unclamped → OOM/overflow on a hostile PDF.</current_state>
<files_to_read>docs/audit/round2/SEC-reverify.md + docs/audit/_workstreams/A-security-crypto-pdf.md; src/engines/podofo/PoDoFoBackend.cpp; src/engines/SignatureManager.cpp (~1197); src/engines/pdfium/PdfiumBackend.cpp</files_to_read>
<deliverables>
D1 — Redaction excises intersecting images in Form-XObject-local resources + annotation `/AP` streams + `/SMask`s (recurse resource dicts, not just the page).
D2 — Move `CRL_SIGNATURE_FAILURE` + `UNABLE_TO_DECRYPT_CRL_SIGNATURE` OUT of the soft `UntrustedChain` bucket → `Invalid` (a forged CRL is hostile, not merely unreachable).
D3 — Clamp PdfiumBackend render allocation (max dimension + max area; reject gracefully — no OOM/`double→int` overflow).
D4 — Extend TestRedaction (image-in-XObject case) + TestRobustness (oversized-dimension case).
</deliverables>
<success_criteria>Redacted images don't survive in any resource scope; forged CRL hard-fails; oversized render rejected gracefully; tests prove it; build+ctest green.</success_criteria>

---

## R2-4 — Ribbon triage + "Planned" disclosure + cloud-orphan removal  [CRITICAL · UX]
<session_metadata>Agents: **/frontend** (primary) + **/backend** | Large — may split per controller (View/Edit/Convert/Forms/Protect) into parallel sub-sessions</session_metadata>
<current_state>UX-01: ~52 ribbon tools have NO `IToolController` handler → click = silent `qWarning`. UX-02: cloud-orphans (Forms›Distribute, Protect›Compliance) still enabled.</current_state>
<files_to_read>docs/audit/round2/UX.md (the full ~52 list); src/shell/RibbonModel.cpp + ToolRegistry.cpp + ToolId.*; src/shell/controllers/*Controller.cpp (handledTools())</files_to_read>
<deliverables>
D1 — Enumerate every RibbonModel tool with no handler (cross-ref each controller's `handledTools()`).
D2 — **TRIAGE / WIRE:** for each, if a real engine method exists (IPdfEditorEngine/IFormManager/IConversionEngine/IOcrEngine…), register a handler that calls it. (e.g. extractTables/toText/toImage/eyeCare/rtl etc. where the engine supports it.)
D3 — **DISCLOSE the rest:** add genuinely-unbacked tools to a central "planned" ToolId set; Ribbon renders them `setEnabled(false)` + `setToolTip("Planned for a future release")`. **Delete the silent `qWarning` no-op path.**
D4 — **REMOVE cloud-orphans** from RibbonModel: `sendForm`,`collect`,`submit`,`auditLog`,`dlp`,`policy`.
D5 — Add a test: no ENABLED ribbon tool lacks a handler (guards against future silent buttons).
</deliverables>
<success_criteria>Every enabled button does something real; unimplemented features are visibly disabled + "Planned"-tooltipped; cloud-orphans gone; no silent qWarning; test enforces it; build+ctest green.</success_criteria>

---

## R2-5 — Theme correctness (dark / light / high-contrast)  [HIGH · UI]
<session_metadata>Agents: **/frontend** | Parallel-safe with everything</session_metadata>
<current_state>UI T-01/02/03: hardcoded dark hex in `CommentsWidget`, `CompressDialog`, `SignaturesWidget` → broken in Light/High-Contrast. VS-02/03/04: no QSS for QTabWidget/QGroupBox/QSpinBox. T-10: AnnotationLayer selection uses `Qt::blue` (invisible in HC).</current_state>
<files_to_read>docs/audit/round2/UI.md; src/util/GpTheme.*; resources/*.qss; src/ui/CommentsWidget.cpp, CompressDialog.cpp, SignaturesWidget.cpp, AnnotationLayer.cpp</files_to_read>
<deliverables>
D1 — Route the hardcoded-color widgets through GpTheme (no inline dark hex).
D2 — Add QSS coverage for QTabWidget/QGroupBox/QSpinBox in all 3 theme files.
D3 — AnnotationLayer selection colors → `Theme::accent()` (HC-visible).
</deliverables>
<success_criteria>All 3 themes render consistently — no dark islands in Light, HC contrast adequate; build+ctest green.</success_criteria>

---

## R2-6 — FormBuilder field persistence (C-03 — wire it)  [HIGH · forms]
<session_metadata>Agents: **/backend** (primary) + **/frontend** | After R2-4 (shares Forms controller)</session_metadata>
<current_state>C-03/UX-08: FormBuilder delete/move/resize are UI-only (`FormBuilderMode.cpp:143,230,421` delete button `setEnabled(false)`); a deleted field reappears on reload. This is WIRE-IT (real backend exists in PoDoFo AcroForm), not disclose.</current_state>
<files_to_read>docs/audit/_workstreams/C-claims-vs-reality.md (C-03); src/engines/FormManager.* + src/modes/FormBuilderMode.cpp + src/commands/{Delete,Move,Resize}FormFieldCommand.h</files_to_read>
<deliverables>
D1 — Implement engine-side delete/move/resize in FormManager (PoDoFo AcroForm field CRUD) so the QUndoCommands persist.
D2 — Enable the now-backed delete button (remove the `setEnabled(false)` stub).
D3 — Test: add+delete a field → save → reload → field stays deleted; move/resize persist.
</deliverables>
<success_criteria>Form-field delete/move/resize persist across save+reload, proven by a test; build+ctest green.</success_criteria>

---

## R2-7 — Test integrity (make the real-crypto gate actually run)  [HIGH · tests]
<session_metadata>Agents: **/testing** | After R2-1/2/3/6; coordinate fixtures with R2-8</session_metadata>
<current_state>CODE.md: `TestSignatureRealCrypto` QSKIPs because `tests/fixtures/signing/` is absent on disk → `REQUIRE_FIXTURES()` skips every method (so "33/33" includes a skip; the E-01/E-06/E-08 gate never runs). `TestSignatureRealCrypto.cpp:148` hardcodes an absolute build path. G-07 TestDjotFuzz was never written (`djotToDocument` is REAL, not a stub).</current_state>
<files_to_read>docs/audit/round2/CODE.md; tests/TestSignatureRealCrypto.cpp + tests/fixtures/signing/generate.bat; src/docmodel/DocumentFuzzer.* + src/pdfws_djot/LuaDjotCodec.cpp</files_to_read>
<deliverables>
D1 — Regenerate FRESH **test-only** signing fixtures (`generate.bat`) so `REQUIRE_FIXTURES` passes and TestSignatureRealCrypto RUNS its assertions. (Use freshly-generated keys — do NOT restore the compromised B-01 keys; R2-8 purges those.)
D2 — Fix `TestSignatureRealCrypto.cpp:148` hardcoded path → use `m_tmpDir`.
D3 — Write `tests/TestDjotFuzz.cpp` (G-07): DocumentFuzzer → `documentToDjot` → `djotToDocument` → assert structural equivalence (lossless round-trip). Wire into CMake.
D4 — Verify both actually EXECUTE (not skip) in ctest.
</deliverables>
<success_criteria>TestSignatureRealCrypto runs (not QSKIP) and passes; TestDjotFuzz green; ctest target count rises; build+ctest green.</success_criteria>

---

## R2-8 — B-01: purge private keys from git history  [HIGH · SOLO, LAST before any push]
<session_metadata>Agents: **/security** + **/devops** | RUN SOLO — rewrites history | After all code sessions merged</session_metadata>
<current_state>REPO.md CRITICAL: `ca.key`/`signer.key`/`test_signer.p12` (real key material) are in git history (commit `a6ea6aa`) across all refs. `.gitignore` only stops future commits.</current_state>
<deliverables>
D1 — Confirm clean tree + a full backup/clone exists + no other session active.
D2 — `git filter-repo --invert-paths --path tests/fixtures/signing/ca.key --path …/signer.key --path …/test_signer.p12` across all refs; verify `git log --all -- tests/fixtures/signing/ca.key` is empty.
D3 — Ensure `.gitignore` covers them; confirm R2-7's fresh test-only fixtures are present and the signing tests pass.
D4 — Document the history rewrite in CHANGELOG (collaborators must re-clone). **Do NOT force-push without owner confirmation.**
</deliverables>
<success_criteria>No private-key bytes in any historical commit (verified); suite green; rewrite documented. HARD GATE before public push.</success_criteria>

---

## R2-9 — Emergence capstone + release gate + i18n + push/PR  [LAST]
<session_metadata>Agents: **/security** + **/devops** | After everything</session_metadata>
<deliverables>
D1 — Run the **emergence-engine** over the remediated code → write `docs/audit/round2/H-emergent-risk.md`.
D2 — Full build + `ctest … --repeat-until-fail 3` → green; re-verify the 4 catastrophe chains are CLOSED (esp. CHAIN-1 after R2-1).
D3 — **i18n honesty:** translation needs humans, not an agent. If translators aren't coming for v1.0, set the app **English-only** and make README/CHANGELOG say so (remove the multilingual claim) — implement the chosen honest path. (Owner decides translate-vs-English-only.)
D4 — `gh auth login` → push `audit-remediation` → open PR to `main` → live repo audit (CI status, branch protection, secret scanning, Dependabot). Confirm CI green. (Only after R2-8 purge.)
</deliverables>
<success_criteria>Capstone written; chains verified closed; i18n claim honest; branch pushed + PR open + CI green; release-gate (CLAUDE.md §9) status reported.</success_criteria>

---

## Not an agent task
- **i18n actual translation** — needs human/translation-service (R2-9 only does the English-only honest-disclosure path if chosen).
- **EV code-signing cert** for the MSI (M8) — procurement.
