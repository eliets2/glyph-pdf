# M3-PROMPT-1 Vault Summary

## Anti-pattern reminders (Patterns 1-4 + relevant others)

**Pattern 1: Agent self-reports are unreliable; verify on disk**
- After agent claims tests pass, independently check `build/*_results.txt` timestamps (must be newer than run start) AND tail actual content
- For C++/CMake/CTest: `Get-ChildItem build\*_results.txt | Sort LastWriteTime` then `tail -30 build/X_results.txt`

**Pattern 2: Stale test result files = false confidence**
- Build + test as ONE atomic verification (`build && test`)
- Result file `mtime` must be newer than source edits

**Pattern 3: Scratch file pollution**
- `.gitignore` must include scratch patterns
- Never use `git add .` / `git add -A` — list specific files
- Agents must clean up intermediate files

**Pattern 4: Forgetting commits after edits**
- Atomic commits per deliverable (if D1-D11, expect 11 commits)
- Before each new deliverable, confirm `git status --short` is clean

**Pattern 9: Engines built, UI not wired**
- "Engine done" ≠ "feature shipped"
- Every backend feature must have a working UI/API path
- Walkthrough.md must list BOTH (a) engine methods + tests exist, (b) UI/API surfaces consume them

**Pattern 14: Architectural ambiguity surfaced mid-session**
- Prompts should pre-decide ambiguous design choices when possible
- If unavoidable, explicit "STOP and ask" instructions for ambiguity classes

**C++/Qt-specific (Pattern 17 + OpenSSL UAF)**
- Wrap every OpenSSL handle in RAII (`std::unique_ptr<X, XDeleter>`)
- Validate every `i2d_*` / `d2i_*` return value
- AddressSanitizer build in CI for OpenSSL-heavy modules

## Non-negotiables relevant to M3-PROMPT-1 (UI/mode-page work)

**License contamination protection**
- GPL/AGPL transitive deps forbidden (would relicense the entire project)
- MuPDF (AGPL), Poppler (GPL) have FATAL_ERROR guards in CMakeLists.txt
- PoDoFo headers NEVER leak into public API; opaque IPdf* interfaces only

**Content-stream injection fix (M1 B6)**
- All user-input content-stream operators MUST use `pdfEscapeLiteralString`
- Prevents operator injection via `(text) Tj` literal-string escape
- Applied in 5 sites: watermark, annotation, header/footer, Bates, editTextInline

**Black-rectangle redaction (M1 + M2)**
- NEVER use visual overlay
- Content-stream excision + Edact-Ray glyph-advance normalization mandatory
- Edact-Ray prevents char recovery from gap widths

**Mock data masquerading forbidden (Pattern 5)**
- Every UI output must be REAL (wired to engine) OR EXPLICITLY DISCLOSED
- Use `setEnabled(false)` + tooltip ("Coming in v1.1") for stubs
- UI labels must describe actual behavior (no "AUTOSAVED" without autosave timer)

**Atomic commits per deliverable**
- Expected format: 1 commit per D (D1, D2, ..., Dn)
- Walkthrough.md must list explicitly: complete items + deferred items + test count

**PoDoFo vendor integration (post-MSYS2)**
- PoDoFo 1.1.0 vendor headers in `third_party/PoDoFo/`
- API migration complete (SetFieldFlag → protected, PdfSigner virtual methods updated)
- No Qt installer or vcpkg mixing; MSYS2 ucrt64 only

## Current state

- **Commit:** `e3c9e24` (May 29, 2026)
- **Test count:** 17/17 ctest suites passing
  - Includes: UnitTests, TestInterfaces, SmokeTest, TestSanitization, TestSignatureValidation (real-crypto 9), TestSignatureValidationMock (21), TestRedaction, TestThreadSafety, TestEncryption, TestResourceLimits, TestControllers, TestIntegration, TestPerformance, TestAutosave (M1), TestSignatureRealCrypto (M1+M2-P4 with pre-existing CMS verify tracking), TestVeraPdf (M2-P3 QSKIP without CLI), TestLaneScheduler (M2-P5 6×5 flakiness-verified)
- **Known broken:** None; working tree clean
- **M2 completion:** M2-PROMPT-4 (D1-D4 adversarial crypto + RSA enforcement) and M2-PROMPT-5 (D1-D6 LaneScheduler) landed; WS1 infrastructure dependency complete

## Prerequisites confirmed for M3-PROMPT-1

**M3-PROMPT-1 = FormBuilderMode wired drag-and-drop:**
- Target: Connect canvas to live document; click-to-place workflow; field properties panel; tab-order editor

**M1 prerequisites (VERIFIED DONE):**
- ✓ AutosaveManager + RecoveryDialog (B1+B2)
- ✓ SignatureManager hardening (B3-B11)
- ✓ CMS_verify real trust policy, OCSP verified, content-stream injection fix
- ✓ RenderCache TOCTOU race fix
- ✓ encryptWithCertificate real `/Filter /PubSec` output
- ✓ CMake Poppler FATAL_ERROR guard
- ✓ RapidOCR runtime gate (isMockImplementation) in OCRMode

**FormManager prerequisite (VERIFIED REAL):**
- ✓ FormManager exists in codebase (post-DI modernization, M1)
- ✓ IPdfEditorEngine has form-related methods (field creation/properties)
- ✓ FormDialog + FormPropertiesPanel UI stubs present
- ✓ FormMode exists (currently disabled with "preview banner")

**WS1 LaneScheduler dependency (VERIFIED COMPLETE):**
- ✓ M2-PROMPT-5 landed `c1157de` → `c3eb22a`
- ✓ ILaneScheduler, LaneScheduler, GPU lane warm worker, CPU pool all implemented
- ✓ R10 closed; infrastructure ready for M5 OCR ensemble consumer

**Build + distribution state:**
- ✓ MSYS2 ucrt64 only; no hybrid toolchain
- ✓ PoDoFo 1.1.0 vendored; API migration complete
- ✓ All 17 ctest targets passing
- ✓ No stale test result artifacts
- ✓ No TODO(audit-*) or FIXME in source
