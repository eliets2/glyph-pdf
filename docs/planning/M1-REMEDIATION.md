# GlyphPDF — Month 1 Remediation Prompt

**Purpose:** Paste this entire file as the first message in a fresh Claude Code session rooted at `C:\Users\User\Projects\pdf`. It commits the uncommitted Month 1 work in 5 atomic chunks, fixes 3 verified TestSanitization failures, generates signing test fixtures + runs the real-crypto test, runs the autosave test, cleans up scratch pollution, and annotates CHANGELOG with the internal-build marker.

**Estimated effort:** 1 person-day (4-8 hours).

**Pre-conditions:**
- Repo at `C:\Users\User\Projects\pdf`, branch `main`, working tree has 29 modified + 13 untracked files from prior Month 1 prompts.
- Build directory exists at `build/` with all 12 test executables freshly compiled (May 27 16:26 timestamp).
- `git status` confirms the uncommitted state; do not pull or rebase before this remediation.

---

<session_metadata>
Phase: Month 1 closure (gate before Month 2)
Priority: BLOCKING for Month 2 start
Depends on: PROMPT-1 through PROMPT-6 already executed (uncommitted in working tree)
Agents: /backend (primary), /testing (verification)
Estimated context: ~30% | Risk: LOW — well-scoped remediation
</session_metadata>

<role>
You are a senior release-engineering+QA C++ engineer closing the Month 1 verification gate for the GlyphPDF v1.0.0 SCOPE LOCK (Branch C). Six prior prompts (PROMPT-1 autosave + PROMPT-2 SignatureManager + PROMPT-3 content-stream escape + PROMPT-4 RenderCache race + PROMPT-5 encryptWithCertificate + PROMPT-6 CMake guards) executed and produced working-tree changes but were not committed. Three TestSanitization tests fail, one new test never ran, and the real-crypto signing test skipped 6/8 cases due to missing fixtures. Your job: commit what's done in clean atomic chunks, fix the 3 sanitization failures by completing the header/footer + Bates + annotation escaping that PROMPT-3 D3 missed, generate the signing fixtures, run the missing tests, and clean up scratch pollution.
</role>

<project_context>
GlyphPDF v1.0.0 at `C:\Users\User\Projects\pdf`. C++17 / Qt 6.10.2 / MinGW 13.1.0 / PoDoFo 0.10.3+ / OpenSSL 3.x / vcpkg `x64-mingw-dynamic`. Project is OSS under Apache-2.0 (recommended) — license guards in CMake are now existential. Real public v1.0.0 ships in 6-8 months under Branch C SCOPE LOCK; current MSI is private/internal until then.
</project_context>

<current_state>
**Uncommitted changes in working tree (verified by `git status`):**
- 29 modified files spanning: `CMakeLists.txt`, `CHANGELOG.md`, `LICENSE-3RD-PARTY.md`, `ROADMAP.md`, plus engine/UI/test sources for B1, B3, B4, B5, B6, B7, B8, B9, B10, PROMPT-1 autosave + recovery dialog.
- 13 untracked files: `src/engines/AutosaveManager.{h,cpp}`, `src/engines/podofo/PdfStringEscape.{h,cpp}`, `src/ui/RecoveryDialog.{h,cpp}`, `tests/TestAutosave.cpp`, `tests/TestSignatureRealCrypto.cpp`, plus 7 scratch files in repo root: `scratch_pdf_editor.cpp`, `scratch_podofo.cpp`, `test_anno.txt`, `test_autosave_out.txt`, `test_autosave_success.txt`, `test_hf.txt`, `test_wm.txt`.

**Test status (last ctest run, May 27 03:26):**
- TestEncryption: 5/5 PASS (includes testCertificateEncryption — confirms `/Filter /PubSec` output)
- TestThreadSafety: 6/6 PASS (includes testRenderCacheConcurrency — confirms B7 fix)
- TestControllers: 12/12 PASS
- TestSanitization: **14 PASS, 3 FAIL** (see below)
- TestSignatureRealCrypto: 2 PASS, **6 SKIP** (signing fixtures missing)
- TestAutosave: **never ran** (no results file exists; .exe built but not in last ctest pass)
- Other tests: passing per ctest summary

**The 3 TestSanitization failures (verified by reading `tests/TestSanitization.cpp:260-369`):**

1. `testAnnotationInjectionIsPrevented` (line 287) — `backend.embedAnnotations(pdf, m_tmpDir.filePath("anno_out.pdf"), {anno})` returns FALSE with `PdfErrorCode::InvalidDataType` when `anno.text = "Here is \\, ( and )"` and `anno.mode = ToolMode::Callout`. The injection-prevention test wants the resulting `/AP /N` stream to contain the escaped form `"Here is \\\\, \\( and \\)"`. Failure happens BEFORE the escaping check — `embedAnnotations` itself is bailing on backslash-containing input. Investigate `PoDoFoBackend::embedAnnotations` (search for the method) and find why `\\` triggers `InvalidDataType` from PoDoFo. Likely: a `PdfString` construction is rejecting embedded non-printable bytes when the text is later wrapped, OR the escape is being applied at the wrong layer and producing invalid PDF.

2. `testHeaderFooterInjectionIsPrevented` (line 354) — `HeaderFooterOptions::textTemplate = "File: file(.pdf"` is written into the page content stream by `addHeaderFooter` without escaping. The test expects the stream to contain the escaped form (assertion at line 307 `streamStr.find(expectedEscaped) != std::string::npos` returns false). Root cause: PROMPT-3 D3 was supposed to audit + fix "any other content-stream string writes" — including `addHeaderFooter` — but only the watermark + annotation `Tj` paths got escaped. Header/footer template + Bates `prefix`/`suffix` substitution still write raw user input to the content stream.

3. The third failure was truncated in the ctest output; reading `tests/TestSanitization.cpp` shows the file likely also has a Bates-numbering injection test. Find and verify it, then fix.

**RenderCache TOCTOU verified fixed** at `src/engines/RenderCache.cpp:148-158` and `:189-199` — single `WriteLockGuard` wraps both hit-check and LRU update.

**Encrypt-with-cert verified working** — TestEncryption logs "Actual Filter Name: PubSec", confirming the redesigned PdfEncrypt delegating subclass at `src/engines/PdfEditorEngine.cpp:340-365`.
</current_state>

<objective>
Close the Month 1 verification gate so Month 2 can begin: (1) commit the working-tree changes in 5 atomic chunks with clean Conventional Commits messages, (2) complete PROMPT-3 D3 to fix the 3 TestSanitization failures, (3) generate signing test fixtures and re-run TestSignatureRealCrypto so the crypto hardening has actual test coverage, (4) run TestAutosave to verify PROMPT-1 works end-to-end, (5) delete scratch pollution and add `.gitignore` entries, (6) annotate CHANGELOG `[1.0.0]` header with `[INTERNAL-BUILD — NOT FOR PUBLIC DISTRIBUTION]` per Branch C SCOPE LOCK.
</objective>

<files_to_read>
git status output (run `git -C . status` first)
tests/TestSanitization.cpp (full file — already partial knowledge from above)
src/engines/podofo/PdfStringEscape.h
src/engines/podofo/PdfStringEscape.cpp
src/engines/podofo/PoDoFoBackend.cpp — addHeaderFooter (search for "addHeaderFooter"), applyBatesNumbering (search for "applyBatesNumbering"), embedAnnotations (search for "embedAnnotations")
src/engines/PdfEditorEngine.cpp — header/footer + Bates wiring (search "addHeaderFooter", "applyBatesNumbering")
src/engines/AutosaveManager.h + .cpp
tests/TestAutosave.cpp
tests/TestSignatureRealCrypto.cpp (already partially known)
CHANGELOG.md (already partially known)
.gitignore (verify what's currently excluded)
CMakeLists.txt — tests section (look for `tests/fixtures/signing/generate_fixtures.cmake` reference)
</files_to_read>

<deliverables>

### D1: First commit — content-stream escaping (B6, PROMPT-3)
**Files to stage:** `src/engines/podofo/PdfStringEscape.h`, `src/engines/podofo/PdfStringEscape.cpp`, plus the relevant diff lines in `src/engines/podofo/PoDoFoBackend.cpp` (lines around 2148 + 2259 — the watermark + annotation Tj escaping that's already in place). DO NOT include header/footer or Bates changes here — that's D6 below.
**Acceptance:** `git diff --cached` shows ONLY content-stream escape changes. Commit message:
```
fix(security): escape literal strings in PDF content streams (B6)

Introduce pdfEscapeLiteralString to prevent content-stream injection
in user-controlled text written via (text) Tj operators.

- New PdfStringEscape helper handles ( ) \ + control chars + idempotency
- Wired into watermark text writes at PoDoFoBackend.cpp:2259
- Wired into annotation appearance streams at PoDoFoBackend.cpp:2148

PoC: watermark text `Foo) Tj 1 0 0 1 100 200 cm (` no longer terminates
the literal early. Resulting page content stream contains escaped form
only.

Refs: §1 Blocker B6, §3 PROMPT-3 in GLYPH-PDF-AUDIT-v1.0.0.md
```

### D2: Second commit — CMake license guards + sanitization /ID fix (B9, B10, PROMPT-6)
**Files to stage:** `CMakeLists.txt`, `LICENSE-3RD-PARTY.md`, plus the qpdf_set_static_ID removal in `src/engines/podofo/PoDoFoBackend.cpp` (the sanitize-pipeline section).
**Acceptance:** `git diff --cached` shows only license-guard + sanitization changes. Commit message:
```
build(cmake): add Poppler FATAL_ERROR + tighten MuPDF guard (B9)
fix(sanitize): remove qpdf_set_static_ID so trailer /ID randomization persists (B10)

Under Apache-2.0 OSS license, accidentally linking GPL/AGPL deps
forcibly relicenses the project. Symmetric Poppler block added;
MuPDF guard extended to vcpkg + find_package + pkg-config detection.

sanitizeDocument's trailer /ID[1] randomization was previously
defeated by a later qpdf_set_static_ID(pdf, 1) call. Removed; qpdf
now generates a random /ID on write, preserving the randomization.

LICENSE-3RD-PARTY.md updated: removed "(future)" tags from PDFium +
qpdf rows; added ONNX Runtime (MIT).

Refs: §1 Blockers B9 + B10, §3 PROMPT-6 in GLYPH-PDF-AUDIT-v1.0.0.md
```

### D3: Third commit — RenderCache TOCTOU + concurrency (B7, PROMPT-4)
**Files to stage:** `src/engines/RenderCache.h`, `src/engines/RenderCache.cpp`, `tests/TestThreadSafety.cpp`.
**Acceptance:** Commit message:
```
fix(rendercache): close TOCTOU race between hit-lookup and LRU update (B7)

The prior pattern dropped the read lock between hit-check and LRU
prepend, allowing concurrent eviction to corrupt m_totalBytes
accounting. Merged hit-check + LRU update under a single WriteLockGuard
at getOrRender + getOrRenderTile.

TestThreadSafety now passes 6/6 including testRenderCacheConcurrency
under 8-thread concurrent load.

Refs: §1 Blocker B7, §3 PROMPT-4 in GLYPH-PDF-AUDIT-v1.0.0.md
```

### D4: Fourth commit — SignatureManager hardening (B3 + B4 + B5, PROMPT-2)
**Files to stage:** `src/engines/SignatureManager.h`, `src/engines/SignatureManager.cpp`, `src/core/interfaces/ISignatureManager.h`, `tests/TestSignatureRealCrypto.cpp`, `ROADMAP.md` (the security section updates). Plus the relevant CHANGELOG.md hunk for the security section.
**Acceptance:** Commit message:
```
fix(signature): close 3 self-flagged PAdES correctness defects (B3+B4+B5)

B3: VRI key is now SHA-1 of raw /Contents bytes per ETSI EN 319 132
    (was hex round-trip; spec-non-conformant validators couldn't find
    the VRI entry). extractSignatureContentsHex → extractContentsRaw.

B4: validateSignatures now performs real trust-chain verification
    against the Windows system root store (CertOpenSystemStoreA) or
    a custom signing/trustStorePath. X509_VERIFY_PARAM with CRL check
    + SMIME-sign purpose. Returns "UntrustedChain" for self-signed /
    untrusted certs. The qWarning admitting "structurally weaker than
    claimed" is gone.

B5: OCSP responses verified with OCSP_basic_verify before DSS embedding.
    Unverified responses logged + signature degrades to B-T (was
    silently accepting any responder). The qWarning admitting "trusts
    responder solely on TLS" is gone.

Bonus hardening: ByteRange overlap detection (ByteRangeOverlap status);
TSA token buffer bumped 16 KB → 32 KB; i2d_X509/i2d_PrivateKey/
BIO_new_mem_buf return values validated with ERR_print_errors_fp on
failure; extractContentsRaw no longer swallows exceptions silently.

New TestSignatureRealCrypto target (skips when fixtures missing).

Refs: §1 Blockers B3+B4+B5, §3 PROMPT-2 in GLYPH-PDF-AUDIT-v1.0.0.md
```

### D5: Fifth commit — encryptWithCertificate redesign (B8, PROMPT-5)
**Files to stage:** `src/engines/PdfEditorEngine.cpp`, `tests/TestEncryption.cpp`, `tests/mocks/MockPdfEditorEngine.h`.
**Acceptance:** Commit message:
```
fix(encryption): real PDF 2.0 PubSec output from encryptWithCertificate (B8)

Prior implementation: BIO use-after-free + wrote raw CMS envelope as
.pdf (output couldn't be opened by any PDF reader).

New implementation: delegating PdfEncrypt subclass produces a real
PDF 2.0 /Filter /PubSec /SubFilter /adbe.pkcs7.s5 (AES-256) document
with per-recipient RSA-wrapped seed in /Recipients array.

TestEncryption::testCertificateEncryption now confirms output Filter
Name == "PubSec" (was raw CMS).

Refs: §1 Blocker B8, §3 PROMPT-5 in GLYPH-PDF-AUDIT-v1.0.0.md
```

### D6: SIXTH commit — fix TestSanitization failures (PROMPT-3 D3 completion)
**Investigation work:**

For `testHeaderFooterInjectionIsPrevented`:
- Open `src/engines/podofo/PoDoFoBackend.cpp` and locate `addHeaderFooter` (likely around line 780-840 per audit).
- Find the template-substitution code that resolves `{page}`, `{total}`, `{date}`, `{filename}` placeholders into the `textTemplate` string.
- After substitution, BEFORE writing the result into a content-stream `Tj` operator, wrap the final string with `pdfEscapeLiteralString(finalText)`.
- Also handle `applyBatesNumbering` similarly — the `prefix` + `suffix` strings + the constructed Bates number all need to be escape-wrapped before the `Tj` write.

For `testAnnotationInjectionIsPrevented`:
- Open `src/engines/podofo/PoDoFoBackend.cpp` and locate `embedAnnotations`.
- The failure is `PdfErrorCode::InvalidDataType` BEFORE the escape check runs — meaning PoDoFo itself rejects the annotation construction when `anno.text` contains `\\`. Likely cause: a `PdfString` construction is being called with the ALREADY-escaped form, producing invalid PDF; OR the escape is being applied at the wrong layer (e.g., applied to the `/Contents` dictionary entry which PoDoFo escapes itself, leading to double-escaping).
- Strategy: PDF `/Contents` and `/AP /N` stream are TWO DIFFERENT escape contexts. The `/Contents` dictionary entry uses PoDoFo's `PdfString` which handles escaping at the object layer (so user text goes in raw). The `/AP /N` content stream uses our `pdfEscapeLiteralString` because we write raw operators. Verify that embedAnnotations is NOT calling `pdfEscapeLiteralString` on the text destined for `/Contents` — only on the text destined for the appearance-stream `Tj` operator. If both paths apply escape, the dictionary path will produce `\\\\` which PoDoFo then writes as `\\\\\\\\` — and the test asserts `"Here is \\\\, \\( and \\)"` in the appearance stream (which is correct) but the dictionary side may corrupt.
- Possible alternative: the failure is in a totally separate code path triggered by the callout-mode appearance generation. Trace via the `fprintf(stderr, "TEST: ...")` debug output in the test (lines 308-343) to see which step fails.

For the third (unnamed) failure: read the full `TestSanitization.cpp` and identify it. Likely a Bates-numbering or watermark-escape edge case. Fix accordingly.

**Files likely modified:** `src/engines/podofo/PoDoFoBackend.cpp` (header/footer + Bates + possibly embedAnnotations).

**Verification:**
```powershell
cd build
$env:QT_QPA_PLATFORM = 'offscreen'
.\TestSanitization.exe
# Expected: 17 passed, 0 failed
```

**Acceptance:** Commit message:
```
fix(security): escape header/footer + Bates + annotation injection paths (B6 completion)

PROMPT-3 D3 missed header/footer template substitution + Bates
prefix/suffix + annotation /AP appearance-stream writes. Three
TestSanitization tests caught the gap.

- addHeaderFooter: pdfEscapeLiteralString wraps the substituted text
  before content-stream Tj write.
- applyBatesNumbering: prefix + number + suffix wrapped before write.
- embedAnnotations: appearance-stream Tj write wrapped (the /Contents
  dictionary path stays unwrapped — PoDoFo handles PdfString escaping
  at the object layer).

TestSanitization now 17/17 passing.

Refs: §1 Blocker B6 completion in GLYPH-PDF-AUDIT-v1.0.0.md
```

### D7: SEVENTH commit — autosave + crash recovery (PROMPT-1, Month 2 PRE-WORK)
**Files to stage:** `src/engines/AutosaveManager.h`, `src/engines/AutosaveManager.cpp`, `src/ui/RecoveryDialog.h`, `src/ui/RecoveryDialog.cpp`, `tests/TestAutosave.cpp`, `src/app/Bootstrapper.cpp`, `src/core/AppContext.h`, `src/GpMainWindow.h`, `src/GpMainWindow.cpp`, `src/engines/DocumentSession.h`, `src/engines/DocumentSession.cpp`, `src/shell/ModeStrip.cpp`, `src/ui/PreferencesDialog.h`, `src/ui/PreferencesDialog.cpp`.
**Pre-commit:** Run TestAutosave first and verify it passes:
```powershell
cd build
$env:QT_QPA_PLATFORM = 'offscreen'
.\TestAutosave.exe
# Expected: all tests pass
```
If it fails, debug + fix before committing.
**Acceptance:** Commit message:
```
feat(autosave): real interval autosave + crash recovery (B1+B2, PROMPT-1)

PRD §13 violation fix. Previous "AUTOSAVED · HH:MM:SS" label updated
only on manual save — actively lied about background autosave.

- AutosaveManager: QTimer-driven (configurable interval, default 5 min,
  clamped [60, 1800]); writes <currentFile>.autosave.pdf via PoDoFo
  saveDocument on a worker thread; atomic rename via MoveFileExW.
- RecoveryDialog: on MainWindow startup, scans recent files for
  orphaned .autosave.pdf newer than parent; offers recover / discard.
- DocumentSession: lastAutosave timestamp + lastAutosaveChanged signal
  + findOrphanedAutosaves helper.
- ModeStrip: label semantics fixed — shows real autosave timestamp
  (was wired to dirtyChanged). Fake hash-based sync indicator removed.
- AppContext + Bootstrapper: AutosaveManager wired as shared_ptr.
- PreferencesDialog: "Autosave interval (minutes)" QSpinBox [1,30]
  live-applies via stop/start.
- TestAutosave: 7+ test cases covering timer fires, clean-doc skip,
  orphan detection, clamp behavior, recovery flow.

Refs: §1 Blockers B1+B2, §3 PROMPT-1 in GLYPH-PDF-AUDIT-v1.0.0.md
```

### D8: EIGHTH commit — also-modified files (OCR + interface housekeeping)
**Files to stage:** `src/core/interfaces/IOcrEngine.h`, `src/engines/ocr/OcrPipeline.cpp`, `src/engines/ocr/RapidOcrEngine.cpp`, `src/engines/ocr/RapidOcrEngine.h`, `src/modes/OCRMode.cpp`.
**Investigation:** these were modified during Month 1 prompts — likely the `isMockImplementation()` sentinel that the §3 PROMPT-7 D1-D2 prescribed (runtime-gate RapidOCR engine). Verify the changes are coherent (isMockImplementation returns true for RapidOcrEngine; OCRMode disables RapidOCR + ROVER options when isMock returns true).
**Acceptance:** Commit message:
```
feat(ocr): runtime-gate RapidOCR / ROVER engine selector (B11 partial)

PROMPT-7 D1-D2 from the v1.0.0 audit. Adds isMockImplementation()
sentinel to IOcrEngine; RapidOcrEngine returns true (real PP-OCRv5
implementation lands in M5 BATCH-2). OCRMode disables RapidOCR + ROVER
combo entries with tooltip "Available in v1.1.0" when the engine
reports mock.

Refs: §1 Blocker B11, §3 PROMPT-7 D1-D2 in GLYPH-PDF-AUDIT-v1.0.0.md
```

### D9: Generate signing test fixtures + re-run TestSignatureRealCrypto
**Investigation:** Locate the fixture-generation script that TestSignatureRealCrypto references:
- Search: `grep -r "generate_fixtures" tests/ CMakeLists.txt`
- The skip message says `cmake -P tests/fixtures/signing/generate_fixtures.cmake`
- If the script doesn't exist yet, WRITE IT using OpenSSL CLI (`openssl genrsa`, `openssl req`, `openssl x509`, `openssl pkcs12 -export`) to produce:
  - `tests/fixtures/signing/test_ca_root.crt` (self-signed root CA)
  - `tests/fixtures/signing/test_signer.p12` (signer cert + key, password `test`)
  - `tests/fixtures/signing/test_signer_chain.pem` (signer + root)
  - `tests/fixtures/signing/test_document.pdf` (one-page test PDF — generate via PoDoFo or use a minimal hand-crafted PDF)
- Also produce an OCSP responder cert + a "good" OCSP response + a "bad" OCSP response (wrong signer) for the OCSP verification test.

**Execution:**
```powershell
cmake -P tests/fixtures/signing/generate_fixtures.cmake
cd build
$env:QT_QPA_PLATFORM = 'offscreen'
.\TestSignatureRealCrypto.exe
# Expected: 8/8 passing (was 2 pass, 6 skip)
```

If any test now FAILS (rather than skip), that's a real defect in PROMPT-2 — fix it before continuing.

**Commit:** include the generate_fixtures.cmake script + any .gitignore entries for the generated fixtures (don't commit the generated .p12 file itself unless it's labeled non-secret-test-only and clearly time-bound).
```
test(signature): add fixture-generation script + enable real-crypto E2E

Closes the test-coverage gap for B3+B4+B5 fixes. Script uses OpenSSL
CLI to generate a self-signed CA + signer .p12 + test PDF + good/bad
OCSP responses. Fixtures are .gitignored; CI generates them on demand.

TestSignatureRealCrypto now 8/8 passing (was 2 pass + 6 skip).
```

### D10: Clean up scratch pollution + .gitignore
**Files to delete (from repo root):**
- `scratch_pdf_editor.cpp`
- `scratch_podofo.cpp`
- `test_anno.txt`
- `test_autosave_out.txt`
- `test_autosave_success.txt`
- `test_hf.txt`
- `test_wm.txt`

**Files to add/update:**
- `.gitignore` entries:
  ```
  # Scratch / debugging artifacts
  scratch_*
  test_*.txt
  *.autosave.pdf
  
  # Test fixtures (generated on demand)
  tests/fixtures/signing/*.p12
  tests/fixtures/signing/*.crt
  tests/fixtures/signing/*.pem
  tests/fixtures/signing/*.der
  tests/fixtures/signing/test_document.pdf
  ```
- Delete via `git rm -f scratch_pdf_editor.cpp scratch_podofo.cpp test_anno.txt test_autosave_out.txt test_autosave_success.txt test_hf.txt test_wm.txt` (or just `rm` since they're untracked).

**Commit:**
```
chore: delete scratch pollution; ignore *.autosave.pdf + test_*.txt

Removes 7 leftover scratch + test-output files from repo root that
accumulated during Month 1 prompt execution. .gitignore extended to
prevent recurrence and to ignore generated signing fixtures.
```

### D11: Annotate CHANGELOG with [INTERNAL-BUILD] marker
**Files:** `CHANGELOG.md`
**Acceptance:** Update line 5 from:
```
## [1.0.0] - 2026-05-23
```
to:
```
## [1.0.0-internal] - 2026-05-23 [INTERNAL-BUILD — NOT FOR PUBLIC DISTRIBUTION]

> **⚠ Scope-lock note:** Per the Branch C SCOPE LOCK in `GLYPH-PDF-AUDIT-v1.0.0.md`,
> this `[1.0.0]` entry corresponds to the private/internal build that exists during the 6-8
> month execution sprint. **Real public v1.0.0** ships only when every claim in this changelog
> is implemented (no stubs, no preview banners, no mock OCR, no canned AI reply, no empty
> translations). The MSI at `dist\GlyphPDF-1.0.0-x64.msi` MUST NOT be published to any public
> channel (GitHub Releases, winget, chocolatey, scoop, website, social media) until the
> Branch C work completes.
```
**Commit:**
```
docs(changelog): mark [1.0.0] as INTERNAL-BUILD per Branch C SCOPE LOCK

Real public v1.0.0 ships in ~6-8 months once disclosed-stub closure
work completes (BATCH-2 through BATCH-5). Current MSI is private
internal only. This annotation prevents accidental public distribution
of an incomplete release.

Refs: SCOPE LOCK section in GLYPH-PDF-AUDIT-v1.0.0.md
```

### D12: Final verification
Run the full test suite:
```powershell
cd build
$env:QT_QPA_PLATFORM = 'offscreen'
ctest --output-on-failure -j4
```
**Expected:** All 14 test targets passing. Specifically:
- TestSanitization: 17/17 (was 14/17)
- TestAutosave: passing (was unrun)
- TestSignatureRealCrypto: 8/8 (was 2 pass + 6 skip)
- TestEncryption: 5/5 (unchanged)
- TestThreadSafety: 6/6 (unchanged)
- TestControllers: 12/12 (unchanged)
- Plus all the others (TestRedaction, TestResourceLimits, TestPerformance, TestInterfaces, TestIntegration, TestSignatureValidation, SmokeTest, UnitTests).

Then verify git state:
```bash
git log --oneline -15
# Expected: 11 new commits on top of 9a9ef37 (D1 through D11)
git status
# Expected: clean working tree
```

Report final state in <200 words: number of commits added, test pass counts, any unexpected findings. If all green: declare Month 1 closed and Month 2 ready to start.

</deliverables>

<verification>
```powershell
# 1. Verify all 11 commits exist
git log --oneline -15

# 2. Verify clean working tree
git status

# 3. Verify all tests pass
cd build
$env:QT_QPA_PLATFORM = 'offscreen'
ctest --output-on-failure -j4

# 4. Verify scratch files gone
ls scratch_* test_*.txt 2>&1
# Expected: no matches

# 5. Verify CHANGELOG has INTERNAL-BUILD marker
grep "INTERNAL-BUILD" CHANGELOG.md
# Expected: at least 1 match
```
</verification>

<constraints>
- DO NOT commit `scratch_*` or `test_*.txt` files — these are pollution, delete them.
- DO NOT commit the generated signing fixtures (.p12, .pem, .crt) — they must be regenerated per build via the cmake script.
- DO NOT push to remote in this session — only commit locally. The user will push manually when ready.
- DO NOT amend prior commits (9a9ef37 or earlier) — append clean new commits.
- DO NOT use `git add -A` or `git add .` — explicit file staging per D1-D11 prevents cross-contamination of commits.
- DO NOT skip the test re-run after D6 — the sanitization fixes must be verified before D7.
- DO NOT change the public IFooEngine interfaces beyond what Month 1 prompts already changed.
- DO NOT remove or shorten the CHANGELOG entries — only add the INTERNAL-BUILD annotation.
- DO NOT touch the SCOPE LOCK section in the audit file — that's the contract.
- If any commit's tests regress (a previously-passing target starts failing), STOP immediately, investigate, fix before continuing.
- If you discover a 4th sanitization failure or any other issue not described above, flag it in the final report — do NOT silently work around it.
</constraints>

<error_recovery>
- If `embedAnnotations` failure root cause is unclear after reading the code path: instrument with `qDebug() << "embedAnnotations step X"` calls, rebuild, re-run JUST the failing test (`./TestSanitization.exe testAnnotationInjectionIsPrevented`), trace where it bails.
- If OpenSSL fixture generation fails (e.g., openssl CLI not in PATH): fall back to a Qt-based fixture generator (use `QSslKey`, `QSslCertificate` APIs); OR skip D9 and document why so the user can generate fixtures externally.
- If a commit's pre-commit hook fails (linting / formatting): fix the underlying issue and create a NEW commit; do NOT --no-verify.
- If TestAutosave fails on the timer-fire assertion (timing-dependent test): increase `QTest::qWait` margin to 110% of the configured interval rather than the exact value.
- If the third TestSanitization failure turns out to be unrelated to escaping (e.g., a flaky test or a separate code path): document in the commit message + flag in final report — do not silently rebrand the commit.
</error_recovery>

<success_criteria>
- [ ] All 11 new commits exist with descriptive Conventional Commits messages
- [ ] `git status` shows clean working tree
- [ ] ctest: all 14 test targets pass
- [ ] TestSanitization: 17/17 (was 14)
- [ ] TestAutosave: passing
- [ ] TestSignatureRealCrypto: 8/8 (was 2 + 6 skip)
- [ ] No `scratch_*` or `test_*.txt` files in repo root
- [ ] `.gitignore` extended for scratch + autosave + signing fixtures
- [ ] `CHANGELOG.md` has `[INTERNAL-BUILD — NOT FOR PUBLIC DISTRIBUTION]` annotation
- [ ] Final <200-word report covers: commit count, test pass counts, any unexpected findings
</success_criteria>

---

**After this prompt completes successfully, Month 2 may begin.** Next prompt to run: the autosave timer was already done in PROMPT-1 (committed via D7 above); the next Month 2 work is the new prompts derived from BATCH-5 SESSION F (Edact-Ray glyph-advance normalization), BATCH-3 (OCR text-layer scrub in redaction), BATCH-3 (veraPDF subprocess for PDF/A validation). Or jump ahead to M3 BATCH-3 mode-page completions if you'd rather close the disclosed-stub surface in parallel with the remaining security work — the M2 prompts are independent of M3.
