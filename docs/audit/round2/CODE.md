# GlyphPDF — Round-2 Code Archaeology Audit
**Date:** 2026-06-09  
**Branch:** `audit-remediation`  
**HEAD at time of audit:** `b0b12fb` (33/33 ctest, clean -Werror)  
**Auditor:** code-archaeologist (read-only; no source edits)  
**Source of truth:** `docs/audit/AUDIT-2026-06-02.md` + on-disk code

---

## WP Completeness Table

| WP | Title | Status | Evidence |
|----|-------|--------|----------|
| **WP-0** | Truth-up docs/memory | **PARTIAL** | CHANGELOG de-promoted to `[Unreleased]` (line 5, CHANGELOG.md). `src/app/Bootstrapper.cpp` wires Ollama-only. However: `CLAUDE.md` §0 still cites `main` head `e1c5394` and "34 ctest targets / 32 pass" — not updated to reflect `audit-remediation` HEAD `b0b12fb` or 33/33. CHANGELOG §0 note references G-11/G-12 "stale sprint labels" — no commit labeled WP-0 in the tracker. |
| **WP-1** | Remove cloud upload | **DONE** | Commits: `2725e30` (C-01), `e1ea760` (C-05), `fcf644c` (CollaborationManager), `66950a0` (cloud AI), `e61150c` (README/CHANGELOG). `AnthropicProvider.*`, `OpenAiProvider.*`, `GeminiProvider.*` absent from `src/`. Grep confirms no `CollaborationManager` source files remain. AIChatPanel shows Ollama-only. 32/32 ctest verified. |
| **WP-2** | Crypto/security fixes | **DONE** | 14 commits (see tracker §7). `TestEncryption.cpp:testCertificateEncryption` does real CMS-unwrap + AES-CBC decrypt + FlateDecode round-trip, gating A-01. `TestRedaction.cpp:testRedactedTextUnextractable` decodes all PoDoFo streams (not raw bytes), gating G-02. `TestRedaction.cpp:testRedactionRemovesImageBytes` asserts 1×1 neutralisation of secret image XObject, gating A-02. All new tests are wired in `CMakeLists.txt` with `add_test`. |
| **WP-3** | Architecture integrity | **DONE** | Commits: `71769b0` (D-01), `7e5c542` (D-02), `3ac41fc` (D-04/D-07), `1fac594` (D-05), `ccce84b` (D-10). `PdfEncryptPubSec` now in its own TU (`src/engines/podofo/PdfEncryptPubSec.cpp`). `PdfPageOps` free-functions added (`src/engines/podofo/PdfPageOps.h/.cpp`). `EditAnnotationCommand.h`/`RedactCommand.h` moved to `src/ui/`. OCR sources removed from `pdfws_ui`, compiled once in `pdfws_engines`. `ProvenanceGuard::checkEditVia` called in `HomeController::onSave()`. Deferred: D-03 / D-06 / D-08 / D-09 (per tracker note). |
| **WP-4** | Fix UI mock data | **PARTIAL** | C-02: `PdfStructureMapper::mapPdfToSemantic` is now a real implementation (page-paragraph extraction via `PdfContentStreamReader`). `applySemanticToPdf` intentionally returns `false` (born-PDF write-back correctly deferred to OCR path, documented in comment). C-03 (FormBuilder delete/move/resize engine-side implementation): `m_deleteFieldBtn->setEnabled(false)` still present at `FormBuilderMode.cpp:143,230,421`; "Coming in v1.1" tooltip removed but button still disabled with no engine wiring — **NOT DONE**. Other C MOCK/PARTIAL surfaces and stub panels: not audited for this round (WP-4 incomplete). |
| **WP-5** | All backends selectable in UI | **NOT DONE** | No commit in tracker. `CMakeLists.txt` still has conditional backend guards (`HAS_PDFIUM`, `HAS_QPDF`, etc.) with no UI selector wired — `BackendRouter` exists but no PreferencesDialog selector for engine choice. |
| **WP-6** | Performance/concurrency | **DONE** | Commits: `164f923` (F-01), `0119fef` (F-03/F-10), `d3aaa1d` (F-04), `79e243b` (F-02), `ab9ad18` (F-05), `0384aa2` (SP-2 type fix). 32/32 ctest. |
| **WP-7** | Tests + release gate | **PARTIAL** | G-01: `TestResourceLimits.cpp` fully rewritten — real assertions (no QSKIP), wired in CMakeLists. G-02: `testRedactedTextUnextractable` uses PoDoFo `CopyTo()` decoding — **DONE**. G-03: `testEncryptionPreventsPasswordlessAccess` + `testCertificateEncryption` round-trip — **DONE**. G-05 (B_LTA tautology): original `P || !P` referenced at `TestSignatureRealCrypto.cpp:203` area — the `testBLTA_DocTimestampPresent` test now asserts `QVERIFY2(!ok, ...)` + `QCOMPARE(mgr.lastSignOutcome(), PartialLtvMissing)` — the tautology has been **replaced** with real assertions. G-06 (OCSP `QEXPECT_FAIL`): `testRevokedCertReportsRevoked` at line ~431 still uses QSKIP on missing `revoked_cert.p12` fixture — the `QEXPECT_FAIL` mentioned in the audit has been replaced by a hard `QCOMPARE(..., "Revoked")`, BUT the test QSKIP-gates on a fixture that does not exist on disk (`tests/fixtures/` directory absent from working tree) so it never executes under ctest — **effectively still NOT GATING**. G-07 (`DocumentFuzzer` wired to no test): `DocumentFuzzer.cpp` is compiled into `docmodel` static lib but no test binary includes or calls it — `tests/` has no `TestDjotFuzz.cpp` or equivalent. CMakeLists.txt grep finds zero reference to `DocumentFuzzer` in the test section — **NOT DONE**. G-04 (sanitization vector coverage), G-08 (hostile-PDF robustness beyond one qpdf case), G-09 (ASan/UBSan CI), i18n: all **NOT DONE** (see i18n finding below). G-10 (`-Werror`): `CMakeLists.txt:28` has `-Werror=return-type -Werror=unused-result -Werror=shadow` — **DONE**. |
| **WP-8** | Supply chain | **NOT DONE** | No commits. B-01: `tests/fixtures/` directory is absent from the current working tree (the key/P12 files were deleted from the working copy), but **the git history purge (filter-repo) has not been executed** — commit `a6ea6aa` still contains `ca.key`, `signer.key`, `test_signer.p12` in history (the keys live in git objects, not in the working tree). Remaining: B-04 (CRED_PERSIST_ENTERPRISE), B-05 (CI sha256), B-06 (pdfium PROVENANCE.md) — all unaddressed. |
| **WP-9** | Final verification / capstone H | **NOT DONE** | Per tracker and SP-6 prompt: emergence capstone H was cap-blocked and never executed. No verification commit. |

---

## Regressions and New Debt Introduced by the Remediation

### 1. `TestSignatureRealCrypto` still QSKIP-gates the entire suite under ctest

**File:** `tests/TestSignatureRealCrypto.cpp:40-65`  
The `REQUIRE_FIXTURES()` macro fires because `tests/fixtures/signing/` does not exist on disk. Every test in the suite (`testBT_SignAndValidate`, `testBLT_VRIKeyMatchesOpenSSLSha1`, `testBLTA_DocTimestampPresent`, `testCertifyWritesDocMDP`, `testRevokedCertReportsRevoked`, etc.) reports 0.03 s "pass" in ctest by means of QSKIP — none of them execute. The WP-2 comment block at lines 49-58 explicitly acknowledges this as a pre-existing defect, but it was not fixed in WP-7. The real-crypto tests for E-01, E-06, E-08 are therefore untested in CI. This is the single highest-risk regression carry-forward.

### 2. `testBLT_VRIKeyMatchesOpenSSLSha1` hardcodes an absolute build path

**File:** `tests/TestSignatureRealCrypto.cpp:148`  
```cpp
QString output = "C:/Users/User/Projects/pdf/build/signed_blt.pdf";
```
This path is hardcoded to the developer's machine. On any other build machine ctest either writes to a non-existent path or collides with a previous run. The other tests use `m_tmpDir.filePath(...)`. This was introduced during WP-2 and not caught because the suite QSKIPs before reaching it. Finding: new debt from the remediation.

### 3. `D-03` still present: `pdfws_engines` links `Qt6::Widgets` as PUBLIC

**File:** `CMakeLists.txt:195`  
```cmake
target_link_libraries(pdfws_engines
    PUBLIC  pdfws_core
    PRIVATE podofo::podofo OpenSSL::SSL OpenSSL::Crypto Qt6::Network Qt6::Widgets ...
```
`Qt6::Widgets` is `PRIVATE` in the CMake line (correct), but the workstream D report identifies this as a layering concern — the D-03 entry was explicitly deferred. No regression introduced but the debt remains open.

### 4. `D-08` and `D-09` deferred: `BackendRouter` raw owning pointers + `PdfEditorEngine` downcast

**Files:** `src/engines/BackendRouter.cpp`, `src/engines/PdfEditorEngine.cpp:~95`  
Both deferred from WP-3. The downcast `dynamic_cast<PoDoFoBackend*>` in `PdfEditorEngine` is still live. Not a new regression but an acknowledged carry-forward.

### 5. `HomeController::onSave()` ProvenanceGuard — DjotThenSave path not yet enforced

**File:** `src/shell/controllers/HomeController.cpp:117`  
```cpp
// will be gated further once the Djot editor is wired (TODO(WP-4)).
```
The `DirectStructural` path gates correctly (D-05 DONE). The `DjotThenSave` enforcement point requires WP-4 completion. Not a regression of WP-3, but CHAIN-1 ("Silent un-signing") is only half-broken; the DjotThenSave save path remains ungated.

### 6. No orphaned headers or CMake refs after WP-1 cloud deletion

Grep for `CollaborationManager`, `AnthropicProvider`, `OpenAiProvider`, `GeminiProvider` across `src/` returns zero matches. No dangling `#include` or `target_sources` entries. WP-1 deletion was clean.

### 7. i18n: translations still 0/1394

**Files:** `translations/glyphpdf_{fr,ar,de}.ts`  
All three files consist entirely of `<translation type="unfinished"></translation>` entries. The `.qm` artifacts compiled from these contain zero translations. `CMakeLists.txt:1198-1213` wires `qt_add_translations` correctly (Qt 6.11 `RESOURCE_PREFIX` supported), so the build infrastructure is ready, but the strings themselves are untouched. This was a documented release blocker in §3 and remains completely open.

---

## `tests/test_sig_dict.cpp` Verdict

**Classification: DEBUG SCRATCH — DELETE**

The file is a standalone `int main()` diagnostic tool that loads a PDF from `argv[1]`, iterates signature fields, and prints their dictionary keys to stdout. It is:

- Not a Qt Test (`QObject` subclass), so it cannot be wired into ctest without a structural rewrite.
- Not referenced anywhere in `CMakeLists.txt` (confirmed by grep — no `add_executable` or `target_sources` entry).
- Not tracked by git (noted as untracked in the task brief).
- Logically superseded: the diagnostic it provides (inspecting `/V` dictionary keys in a signed PDF) is the exact information already exercised by `TestSignatureRealCrypto::testBLT_VRIKeyMatchesOpenSSLSha1` (raw PDF byte inspection) and `TestSignatureValidation`.

**Recommendation:** Delete `tests/test_sig_dict.cpp`. It is a developer probe written to investigate the `E-08`/`E-01` signing bugs during WP-2, has no path to becoming a gated test without significant restructuring, and its presence as an untracked file will confuse future contributors. If the dictionary-dump behavior is useful for manual debugging, it belongs in a `tools/` subdirectory, not `tests/`, and must be noted in `.gitignore` or committed with a clear `# NOT A CTEST TARGET` header.

---

## Deferred D-Mediums Status

| ID | Finding | Status after remediation |
|----|---------|--------------------------|
| D-03 | `pdfws_engines` links `Qt6::Widgets` PUBLIC | **OPEN** — deferred per WP-3 tracker note; still present in CMakeLists.txt |
| D-06 | `tr` local variable shadows `QObject::tr()` in `CompareMode` | **OPEN** — deferred; workstream D-architecture-quality.md:155 confirms this; no fix commit |
| D-08 | `BackendRouter` returns raw owning pointers | **OPEN** — deferred; `BackendRouter.cpp` not modified in WP-3 commits |
| D-09 | `PdfEditorEngine` casts `IPdfDocument*` back to `PoDoFoBackend*` | **OPEN** — deferred; downcast acknowledged in workstream D at line 232 |

All four D-mediums are confirmed open. They carry no security or correctness impact (auditor's own classification) but contribute to CHAIN-1 risk surface and should be addressed in WP-4/WP-7.

---

## Overall Post-Remediation Health Verdict

**Grade: Substantially safer, not yet shippable.**

The remediation executed cleanly on what it targeted. The four highest-severity chains (crypto, architecture, performance, cloud removal) have real code changes backed by real characterization tests. The -Werror build gate is active. 33/33 ctest passes consistently.

**What is genuinely done:**
- CHAIN-2 ("Confidential-but-plaintext") broken: A-01 fixed with CMS round-trip proof; E-04/E-08 gated.
- CHAIN-3 ("Redacted-but-leaking") broken at the text layer: G-02 test now decodes streams; A-02 image excision tested.
- CHAIN-4 ("Supply-chain → key theft") effectively neutralised: cloud AI removed; B-03 hardened.
- CHAIN-1 ("Silent un-signing") half-broken: D-05 wired for DirectStructural; E-08 incremental-write guard in place. The DjotThenSave half awaits WP-4.
- Performance UAF/deadlock fixes (F-01–F-05) committed.
- ODR violation (D-01) and UI layering violation (D-02) resolved.

**What remains dangerous or incomplete:**
1. **B-01 NOT done**: private RSA keys (`ca.key`, `signer.key`, `test_signer.p12`) remain in git history at commit `a6ea6aa`. The working tree files were deleted but the history was never purged. Any `git clone` can recover them with `git show a6ea6aa:tests/fixtures/signing/ca.key`. This is a hard block before any public repo push.
2. **TestSignatureRealCrypto entirely QSKIPs** in ctest: the real-crypto tests for E-01, E-06, E-08 (the most dangerous pre-fix behaviors) never execute in CI. The suite validates only that QSKIP fires successfully.
3. **G-07 (DocumentFuzzer) not wired**: `DocumentFuzzer::generateRandomDocument` is compiled but has zero test callers. The djot-roundtrip fuzz path (feed random `SemanticDocument` → `documentTodjot` → `djotToDocument` → compare) was the stated purpose and is entirely absent.
4. **i18n 0/1394**: all three locale files are `type="unfinished"`. The app ships English-only while the UI implies multilingual support.
5. **WP-4 (FormBuilder engine-side), WP-5 (backend selectors), WP-8 (full supply chain), WP-9 (capstone H verification)**: all pending.
6. **`test_sig_dict.cpp`** untracked scratch file should be deleted.
7. **Hardcoded absolute path** in `TestSignatureRealCrypto.cpp:148` is a CI-portability bug introduced by the remediation.

**Release gate from AUDIT §3 is not clear.** Four of the original release blockers (i18n, FormBuilder, OCSP revocation gating in CI, history purge) remain open. Do not tag v1.0.0 on this state.
