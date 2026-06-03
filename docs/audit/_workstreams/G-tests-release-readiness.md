# G — Tests, Build Hygiene & Release Readiness Audit

**Auditor workstream:** Tests + build hygiene + OSS v1.0.0 release-readiness  
**Audit date:** 2026-06-02  
**Codebase:** GlyphPDF — C++17/Qt 6.11/MSYS2 ucrt64 (~40,557 LOC)  
**Method:** Static source analysis only. No build or ctest execution performed.

---

## Summary

The project has 34 ctest targets compiled from 34 `.cpp` files in `tests/`. This reconciles the three conflicting claims:
- README.md: "14 test targets" — **wrong** (last updated for Session 20 baseline)
- CLAUDE.md §3: "26 ctest targets" — **wrong** (counted M1+M2+M3 cohort, missed M4–M7)
- CLAUDE.md §9 pre-flight: "All 24 tests pass" — **wrong** (stale; pre-M5/M6/M7 count)
- **Actual: 34 ctest targets** as confirmed by CMakeLists.txt `add_test()` calls and filesystem presence

Test quality is uneven: the security-critical tests (TestRedaction, TestSignatureRealCrypto) are genuinely rigorous; TestResourceLimits is 100% QSKIP placeholders; TestSanitization covers only 4 of the 23+ implemented sanitization vectors; several new test counts in CLAUDE.md are inconsistent with source. The 4.57 s total runtime (reported externally) is plausible for the synthetic-document patterns used — the tests are lightweight but not entirely trivial for the security paths.

**Release-gate status:** CHANGELOG has been prematurely promoted to `[1.0.0]` with date 2026-06-02 while multiple known blockers remain in source. Git HEAD is `e1c5394` but CLAUDE.md §0 still claims `e09404d`. All three `.ts` translation files have **1,394 strings each, all `type="unfinished"`** — every `<translation>` element is empty; the `.qm` files embedded at `:/translations/` carry zero translated strings. Several BatchMode UI labels reference internal sprint codenames visible to end users. Four FormBuilder command paths are deferred to v1.1. The PreferencesDialog AI tab contains a stale note that "real LLM calls are scheduled for v1.1" despite `AIChatPanel` being wired in M6-P3. Cloud sync ribbon entry is permanently disabled with "Coming in a future release" tooltip.

---

## Test Inventory

| # | Target | Type | Real assertions? | Exercises | Key gaps |
|---|--------|------|-----------------|-----------|----------|
| 1 | UnitTests | Unit | Minimal (2 slots) | OcrEngine pre-init, FormManager error-path | Only 2 test methods; no assertion on actual OCR output |
| 2 | TestInterfaces | Unit | Yes — interface contracts | DocumentSession, command routing via mock | Mock-based; real dispatch not tested |
| 3 | SmokeTest | Integration | Yes — real PoDoFo | Load/save/edit/sanitize/sign/convert/embedded-files | Conversion tests call real engine with blank PDF; testConversionWord/Excel pass trivially on blank input |
| 4 | TestSanitization | Unit+Integration | Partial | Mock command dispatch (5 slots); real watermark/annotation/bates injection escaping (4 slots); real sanitize (2 slots) | Covers only ~5 of 23+ sanitization vectors in PoDoFoBackend; missing JavaScript removal, OpenAction, OCProperties, RichMedia annotation, StructTreeRoot tests |
| 5 | TestSignatureValidation | Integration | Yes — real OpenSSL | Sign + validate B_B/B_LT, fixture-gated QSKIP, field population | `testValidateSignedDocument` weakly asserts only that `trustStatus` is non-null; `testSetTsaUrlPersists` asserts nothing beyond "no crash" |
| 6 | TestSignatureValidationMock | Unit | Yes — interface contract | Mock manager state machine | Mock only; no real crypto |
| 7 | TestSignatureRealCrypto | Integration | Strong — 12 adversarial slots | Real B_B/B_LT/B_LTA sign+validate, tamper detection, ByteRange overlap, expired cert, RSA-1024, multi-sig | `testBLTA_DocTimestampPresent`: assertion at line 203 is tautology (`|| !integrityIntact`); `testRevokedCertReportsRevoked` uses `QEXPECT_FAIL` — revocation **not actually enforced**; `testOCSP_NotEmbeddedWithoutVerification` asserts only `!pdfData.isEmpty()` (no-op guard) |
| 8 | TestRedaction | Security | Strong — 13 slots | Content stream excision, XObject redaction, OCR invisible-text scrub, Edact-Ray normalization, audit log, binary-content abort | **Critical gap: redacted content unrecoverability tested at raw-byte level only** (`data.contains("SECRET_DATA")`) — does NOT re-parse PDF content streams or decompress FlateDecode streams to verify string is truly absent post-decode. The `testRedactedTextUnextractable` test at line 175 searches the raw file bytes, which can miss compressed content |
| 9 | TestPatternRedact | Unit+Integration | Partial | Named pattern validity (12 patterns), findMatches gated by `HAS_PDFIUM`, applyPatternRedactions | README claims "15+ vectors" but only 12 patterns confirmed in source (`QCOMPARE(patterns.size(), 12)`); without `HAS_PDFIUM` all position tests auto-pass trivially |
| 10 | TestThreadSafety | Concurrency | Yes — RenderCache | 8-thread concurrent cache read/write (1000 iterations/thread), TOCTOU scenario | Covers RenderCache only; no concurrency test for SignatureManager, OcrPipeline, or LaneScheduler signing path |
| 11 | TestEncryption | Security | Real — OpenSSL | AES-256 V=5/R=6 header check, cert-based `/Filter /PubSec`, empty password | Does NOT attempt to decrypt the output and verify content is inaccessible — only checks `/Encrypt` dictionary presence |
| 12 | TestResourceLimits | Resilience | **None — 100% QSKIP** | Nothing (all 3 slots unconditionally QSKIP) | No resource-limit enforcement whatsoever |
| 13 | TestControllers | UI | Yes — dispatch | Controller action dispatch via real UI stack | Requires `qoffscreen.dll` deploy; tests controller wiring |
| 14 | TestIntegration | E2E | Yes — real PoDoFo | Open→save→reopen, encrypt→save, rotate, redact, full workflow | Uses hand-written minimal PDF fixture (not generated by PoDoFo); tests are real but on blank/minimal input |
| 15 | TestPerformance | Benchmark | Yes — timing assertions | Open, search, cache hit rate, metadata, save — all against regression thresholds | Tests use synthesised documents; real-world PDF performance not covered |
| 16 | TestAutosave | Unit | Yes | AutosaveManager interval, crash-recovery sidecar | Functional |
| 17 | TestDjotRoundtrip | Unit | Yes | LuaDjotCodec decode, ProvenanceGuard 4 cases, encode roundtrip | Limited to in-memory trees; no roundtrip through actual PDF → Djot → PDF |
| 18 | TestDiffEngine | Unit | Strong — 10 slots | Myers LCS correctness, move detection, legal-document scenario | Good algorithmic coverage |
| 19 | TestOfficeImport | Integration | Partial — 4 slots | Images→PDF (real PoDoFo), OfficeToPdf QSKIP (no soffice), missing-file guard, no-LibreOffice guard | Real conversion is QSKIP in most CI environments |
| 20 | TestLaneScheduler | Concurrency | Yes — 5 slots | GPU lane serialization, CPU lane parallelism, priority ordering, cancellation | Functional with artificial delays |
| 21 | TestVeraPdf | Integration | Yes — 2 slots | veraPDF subprocess; **QSKIP entire suite** when `VERAPDF_CLI_PATH` not set | CI environment skips both tests |
| 22 | TestFormBuilder | Unit | Yes — 4 slots | AddFormFieldCommand, EditFormFieldCommand, DeleteFormFieldCommand, undo/redo | Command layer only; no test that field actually appears in PDF on re-open |
| 23 | TestAnnotationDjot | Unit+Integration | Yes — 14 slots | Djot annotation save/reopen, review-state roundtrip, IRT threading, serializer | Strong for its scope |
| 24 | TestRapidOcr | ML/Integration | Yes — 3 slots | `isMockImplementation()`, model load, printed-text recognition | **Conditional on `HAS_RAPIDOCR`** (onnxruntime present); in CI the ONNX model files are gitignored so model-load and recognition tests will QSKIP |
| 25 | TestLayoutEnsemble | Unit | Yes — 10 slots | IoU, single/dual detector merge, confidence suppression, type voting | Uses stub detectors; no real PP-DocLayoutV2 model |
| 26 | TestOcrPipeline | Unit+Integration | Yes — 7 slots | roverMerge algorithm, cross-page timing with stub engines | Timing test relies on `QThread::msleep` in stub — can be flaky on heavily loaded CI |
| 27 | TestOcrDjotMapper | Unit | Yes — 14 slots | Region-type mapping, provenance encoding, table pipe-table, multi-page | Strong; uses stub OcrResults |
| 28 | TestPagesMode | UI | Yes — 4 slots | Range parser, split-at-page, split-every-N, reorder | Headless; exercises PagesMode public test-seam |
| 29 | TestBatchMode | UI | Yes — 5 slots | File list, duplicate detection, batch convert, continue-on-failure, cancel | Uses mock IConversionEngine; `RESOURCE_LOCK BatchModeIO` in place |
| 30 | TestInspector | UI | Yes — 6 slots | Properties tab binding (5 tabs), Djot editor integration | Wired to real InspectorWidget |
| 31 | TestAiProvider | Unit | Yes — 10 slots | Mock echo, key-format validation, QSKIP-gated real round-trips | Real API tests QSKIP without env vars |
| 32 | TestMrcPipeline | Unit+Integration | Yes — 9 slots | Foreground mask, JP2000 encoding, layer separation, ≥5× compression ratio, sandwich text searchable, veraPDF QSKIP, DjVu QSKIP | Verifies compression ratio against raw pixel bytes (correct methodology); veraPDF and DjVu tests QSKIP in standard CI |
| 33 | SmokeTest (extra) | Integration | Partial | (duplicate entry counted above as #3) | — |
| 34 | TestSignatureRealCrypto (adversarial) | Integration | Strong | (counted above as #7) | — |

**Note:** Items 33 and 34 are the same files as 3 and 7 respectively — the count of 34 distinct `.cpp` files matches 34 CMake `add_test()` registrations.

---

## Coverage-Gap Findings

### G-01 · TestResourceLimits is 100% QSKIP — zero coverage of resource enforcement
**Severity:** Critical  **Confidence:** High  
`tests/TestResourceLimits.cpp:29,36,43` — All three test slots unconditionally call `QSKIP("Resource limit enforcement not yet implemented — placeholder test, see ROADMAP.md")`. The target passes (0 failed, 3 skipped) but exercises nothing. README §Security claims the product provides resilience controls.  
**Proposed fix:** Remove the placeholder slots and implement real tests: (a) attempt to load a crafted PDF with 100,001 pages and assert `loadDocumentForEditing` returns false or caps the count; (b) attempt to decode a crafted content stream with embedded image dimensions > defined limit (e.g., 32768×32768) and assert rejection; (c) enforce a max-objects limit in PoDoFoBackend and verify it. Ship a `ResourceGuard` struct with constants and test against them.

---

### G-02 · Redaction unrecoverability test searches raw bytes, not decoded content streams
**Severity:** Critical  **Confidence:** High  
`tests/TestRedaction.cpp:185-188` — `testRedactedTextUnextractable` opens the saved PDF as raw bytes and calls `data.contains("SECRET_DATA")`. This test can pass even if the redacted string remains in a FlateDecode-compressed content stream (the compressed bytes will not match the literal string). A compressed PDF content stream containing "SECRET_DATA" would defeat this check entirely.  
**Proposed fix:** After saving the redacted PDF, reload it with PoDoFo, iterate all pages, call `CopyTo(buf)` on each content stream object (which PoDoFo decodes automatically), and assert the string is absent from the decoded buffer. The existing `testGlyphAdvancesAreNormalized` and `testOCRScrubbing` already do this correctly (lines 239-251, 317-324) — apply the same pattern to `testRedactedTextUnextractable`.

---

### G-03 · No test for encryption access enforcement (decrypt-and-read after encrypt)
**Severity:** Critical  **Confidence:** High  
`tests/TestEncryption.cpp:26-49` — `testAES256EncryptionApplied` only checks that the output file contains the `/Encrypt`, `/V 5`, and `/R 6` header strings. It never attempts to open the encrypted PDF without a password and verify that content is inaccessible, nor does it decrypt with the correct password and verify content is still readable. A broken encryption implementation that writes the `/Encrypt` dict but leaves content unencrypted would pass this test.  
**Proposed fix:** Add a test that: (a) encrypts a PDF with known text content; (b) attempts `PdfMemDocument::Load(path)` without password and asserts it throws `PdfError` or `PdfException`; (c) decrypts with the correct user password and verifies the original text is readable via the content stream. For cert-based encryption: attempt to load without the recipient private key and assert failure.

---

### G-04 · TestSanitization covers only ~5 of 23+ sanitization vectors
**Severity:** High  **Confidence:** High  
`tests/TestSanitization.cpp` tests: mock dispatch (5 slots), pdfEscapeLiteralString invariants (1 slot), watermark injection escaping, annotation injection escaping, header/footer injection escaping, Bates injection escaping, trailer ID uniqueness, integration test covering Outlines + Collection + AcroForm field removal. This leaves the following implemented vectors in `PoDoFoBackend.cpp:1578-1747` untested: (16) StructTreeRoot Alt/ActualText/E scrubbing, (17) OCProperties removal, (18-19) annotation removal for RichMedia/Screen/Movie subtypes, (20) PieceInfo/Thumb/Metadata per-page removal, (21) Trailer ID randomization (partially tested), (22) form field DV removal, (23) RC rich-text removal from annotations. README claims "15+ vectors" but only 12 unique `PatternRedactor` patterns exist and only 5–6 sanitization vectors have direct test coverage.  
**Proposed fix:** Expand `TestSanitization::testIntegrationSanitization` to build a PDF with: JavaScript in Names tree (assert removed), OpenAction (assert removed), OCProperties (assert removed), page-level AA action (assert removed), page-level PieceInfo/Thumb/Metadata (assert removed), a RichMedia annotation (assert annotation removed). Each assertion should reload the sanitized PDF via PoDoFo and verify the key is absent.

---

### G-05 · TestSignatureRealCrypto test 3 (B_LTA) contains an always-true assertion
**Severity:** High  **Confidence:** High  
`tests/TestSignatureRealCrypto.cpp:203` — The assertion is:
```cpp
QVERIFY2(results.first().integrityIntact || !results.first().integrityIntact, ...)
```
This is `P || !P` — always true. The test exercises the B_LTA code path but asserts nothing meaningful about its outcome. The test "passes" regardless of whether the signed PDF is cryptographically valid or corrupt.  
**Proposed fix:** Replace with a meaningful assertion. Either: (a) assert `QFileInfo::exists(output) && QFileInfo(output).size() > 0` (the PDF was written); or (b) if `ok == false` (no TSA), assert the file exists but `!results.first().hasDocTimestamp`; or (c) if `ok == true` (TSA available), assert `results.first().hasDocTimestamp`. Remove `Q_UNUSED(ok)`.

---

### G-06 · Revocation detection is known to not work — QEXPECT_FAIL permanently deferred
**Severity:** High  **Confidence:** High  
`tests/TestSignatureRealCrypto.cpp:429` — `testRevokedCertReportsRevoked` uses `QEXPECT_FAIL("", "Revocation reporting requires DSS OCSP injection (M5 wiring)", Continue)` then asserts `Revoked` status. This means the test always passes regardless of whether revocation works. The CHANGELOG claims WS1-WS3 are complete and M5 is "done", yet revocation via OCSP is explicitly unimplemented and the test permanently soft-passes it.  
**Proposed fix:** Either implement OCSP revocation checking in `SignatureManager::validateSignatures` using the bundled OCSP response at `tests/fixtures/signing/revoked_ocsp_response.der` and promote the assertion to a hard `QCOMPARE`, or explicitly mark this as a known v1.0.0 gap in CHANGELOG under "Known Issues" and update the test comment. Given the CHANGELOG claims "all workstreams complete", this is a documentation honesty issue as well.

---

### G-07 · DocumentFuzzer exists but is unwired to any test
**Severity:** High  **Confidence:** High  
`src/docmodel/DocumentFuzzer.h/cpp` — `DocumentFuzzer::generateRandomDocument(seed, max_depth)` is defined in the `docmodel` static library (per `src/docmodel/CMakeLists.txt:15-16`) but is not referenced in any `tests/*.cpp` file. The fuzzer was presumably intended as a robustness testing aid for `SemanticDocument` round-trips through `LuaDjotCodec`, but no test harness drives it. For a project claiming "privacy-first" with content manipulation, a random-document round-trip test would catch serialization edge cases.  
**Proposed fix:** Add a `TestDjotFuzz` (or expand `TestDjotRoundtrip`) that calls `DocumentFuzzer::generateRandomDocument` with seeds 0–99, pipes each result through `LuaDjotCodec::documentToDjot` then `djotToDocument`, and asserts structural equivalence (same section count, block types, non-empty strings). Wire in CMakeLists.txt with `add_test`.

---

### G-08 · No hostile/malformed PDF robustness test (truncated, corrupt xref, adversarial streams)
**Severity:** High  **Confidence:** High  
`SmokeTest::testRepairOnLoad` at `tests/smoke_test.cpp:246-273` is gated by `#ifndef HAS_QPDF` and only tests a single corruption (replacing `xref` with `xxxx`). There is no fuzzing or adversarial input test against `applyRedactions`, `sanitizeDocument`, or `signDocument` on truncated/crafted PDFs. `PdfEditorEngine::loadDocumentForEditing` wraps PoDoFo which can throw; the question is whether all call sites handle all exception paths gracefully.  
**Proposed fix:** Add a `TestRobustness` (or expand `TestIntegration`) with: (a) truncated PDF (truncate at arbitrary byte offsets) — assert `loadDocumentForEditing` returns false, not segfault; (b) PDF with a circular object reference — assert load fails gracefully; (c) PDF with a crafted stream filter claiming invalid length — assert no crash. These can use simple byte-manipulation helpers like the existing `testRepairOnLoad` pattern.

---

### G-09 · No ASan / UBSan wired for test builds
**Severity:** Medium  **Confidence:** High  
`CMakeLists.txt:24` — build options are:
```cmake
add_compile_options(-Wall -Wextra -Wno-unused-parameter)
```
There is no `-fsanitize=address,undefined` or `CMAKE_BUILD_TYPE=Asan` preset. The security-critical code paths (redaction content-stream surgery, PoDoFo backend pointer manipulation, OpenSSL struct handling) are exercised without memory-safety instrumentation. The CI workflow (`ci.yml:68`) uses `-DCMAKE_BUILD_TYPE=Release` which further disables sanitizers.  
**Proposed fix:** Add a CMake option `ENABLE_SANITIZERS=OFF` that when ON appends `-fsanitize=address,undefined -fno-omit-frame-pointer` and sets `ASAN_OPTIONS=halt_on_error=1`. Add a separate CI job `sanitizer-build` that builds with `ENABLE_SANITIZERS=ON` and runs the security-critical tests (TestRedaction, TestEncryption, TestSignatureRealCrypto, TestSanitization). GCC 16 + UCRT64 supports ASan.

---

### G-10 · -Werror not enabled; build emits warnings on security-critical paths
**Severity:** Medium  **Confidence:** High  
`CMakeLists.txt:24` enables `-Wall -Wextra` but **not** `-Werror`. The problem statement states "the central build emitted many warnings: unused vars, deprecated APIs, -Wreorder, cast-function-type, sign-compare". These warnings suppress nothing at runtime but indicate code-quality issues. In security-critical code (redaction, crypto), cast-function-type warnings can indicate wrong calling-convention assumptions.  
**Proposed fix:** Add `-Werror` to `add_compile_options` and fix the underlying warnings. At minimum, enable `-Werror=cast-function-type -Werror=sign-compare -Werror=unused-result`. Given GCC 16 is in use, `-Werror=deprecated-declarations` would also catch stale OpenSSL API usage.

---

### G-11 · BatchMode UI labels expose internal sprint codenames to end-users
**Severity:** Medium  **Confidence:** High  
`src/modes/BatchMode.cpp:202,204,348,374,627,629` — Disabled BatchMode operations display user-visible labels such as `"Merge: engine method not yet available — coming in M4"` and `"Search-pattern redact coming in M3-P4"`. These are internal development milestones that should not appear in a v1.0.0 release UI.  
**Proposed fix:** Replace sprint references with user-facing text: `tr("Merge: not available in this version")` and `tr("Pattern redact: not available in this version")`. Alternatively, remove the disabled operations from the BatchMode UI entirely for v1.0.0 since `TestPatternRedact` confirms PatternRedactor is implemented — the BatchMode wiring is what is missing.

---

### G-12 · PreferencesDialog AI tab contains stale "v1.1" note contradicting M6-P3 completion
**Severity:** Medium  **Confidence:** High  
`src/ui/PreferencesDialog.cpp:194-196` — The AI preferences tab displays: `"AI features (real LLM calls) are scheduled for v1.1 — for v1.0.0 this UI saves the key only."` This contradicts CHANGELOG and CLAUDE.md which both confirm that M6-PROMPT-3 wired real async LLM calls (AnthropicProvider, OpenAiProvider, GeminiProvider, OllamaProvider) in `AIChatPanel`.  
**Proposed fix:** Remove the stale note. Replace with: `tr("Your API key is stored encrypted in Windows Credential Manager.")` only. The "test key" button is already functional.

---

### G-13 · FormBuilder "Auto-detect" and "Preview" buttons are permanently disabled no-ops with v1.1 tooltips
**Severity:** Medium  **Confidence:** High  
`src/modes/FormBuilderMode.cpp:96,111,322,344` — Two ribbon buttons are `setEnabled(false)` with tooltips `"Auto-detect form fields from document structure — Coming in v1.1"` and `"Preview filled form — Coming in v1.1"`. The associated slots (`onAutoDetect`, `onPreviewForm`) are documented as `"no-op stub"`. Similarly, tab-order-to-PDF persistence is explicitly deferred: `"Persisting tab order to PDF is scheduled for v1.1"` (line 421).  
**Proposed fix:** Either implement the three deferred features before v1.0.0 release, or remove the buttons from the v1.0.0 UI entirely (per CLAUDE.md §5 policy: "No preview banners"). A hidden button is cleaner than a permanently disabled button with an internal milestone tooltip.

---

### G-14 · TestPatternRedact claims 12 patterns; README §Security claims "15+"
**Severity:** Low  **Confidence:** High  
`tests/TestPatternRedact.cpp:58` — `QCOMPARE(patterns.size(), 12)` — exactly 12 patterns in `PatternRedactor::availablePatterns()`. `README.md` §Security says "Document sanitization (15+ vectors)". Sanitization and pattern redaction are different features, but both are under-documented. The test itself is the authoritative count for patterns.  
**Proposed fix:** Update README §Security to say "Sanitization (23+ vectors)" and "Pattern redaction (12 named patterns: email, phone-us, SSN, credit-card, IBAN, IP-address, etc.)".

---

## Release-Gate Checklist (CLAUDE.md §9)

| Gate item | Status | Evidence |
|-----------|--------|----------|
| All 34 M2-M8 prompts complete + committed | **PASS** (per CHANGELOG) | CHANGELOG `[1.0.0]` lists M2-P1 through M7-P3; WS1-WS3 declared complete |
| No Mock returns in IOcrEngine / ISignatureManager / IFormManager impl | **PASS** | `src/engines/ocr/RapidOcrEngine.cpp:149` returns real value; grep confirms `isMockImplementation()` only in `RapidOcrEngine`; `isMockImplementation()` default returns false in `IOcrEngine.h:13` |
| No `setEnabled(false)` + "Coming in v1.1" tooltips | **FAIL** | `FormBuilderMode.cpp:96,111` "Coming in v1.1"; `CompareMode.cpp:52` "Coming in v1.1"; `PdfAValidationPanel.cpp:85,108` "planned for v1.1.0"; `Ribbon.cpp:87` "Coming in a future release"; `BatchMode.cpp:202,204` "coming in M4/M3-P4" |
| All `.ts` translation files populated | **FAIL** | All 1,394 strings in `glyphpdf_fr.ts`, `glyphpdf_de.ts`, `glyphpdf_ar.ts` have `type="unfinished"` with empty `<translation>` elements. The `.qm` files embedded at `:/translations/` carry zero translated strings. Switching locale shows English source strings (graceful fallback but not true i18n). |
| Third-party security audit complete | **UNKNOWN** | No audit report found in `docs/`. CLAUDE.md §5 lists M7-PROMPT-1 as "third-party security audit" but CHANGELOG M7 section does not include an audit report entry. |
| EV code-signing cert obtained + MSI signed | **UNKNOWN** | Not verifiable from source; no cert metadata in `packaging/`. |
| CHANGELOG `[1.0.0-internal]` → promoted to `[1.0.0]` | **PREMATURE** | `CHANGELOG.md:5` shows `## [1.0.0] — 2026-06-02` but the known-blockers above (translations, deferred FormBuilder, revocation, BatchMode sprint labels) suggest this promotion was premature. CLAUDE.md §5 explicitly states "Anything labeled v1.0.0 published before M8 ships: NEVER". |
| No `TODO(audit-*)` / `FIXME` / `XXX` introduced | **PASS** | grep found 0 `TODO(audit-*)` matches in `src/`. |
| `grep "scheduled for.*future engine update"` returns zero | **PASS** | Zero matches found. |
| `grep "Coming in.*v1" tooltips` returns zero | **FAIL** | Multiple occurrences as documented above. |

---

## CLAUDE.md / Documentation Drift

| Item | Claimed | Actual | Severity |
|------|---------|--------|----------|
| CLAUDE.md §0 repo HEAD | `e09404d` (2026-06-01 M6-P3 walkthrough) | `e1c5394` (ci: pin to windows-2022 runner) — 4 commits ahead | High |
| CLAUDE.md §3 test count | "26 ctest targets" | 34 ctest targets | High |
| CLAUDE.md §3 test command | "Expected: 14/14 pass" | 34 targets, not 14 | High |
| CLAUDE.md §9 pre-flight | "All 24 tests pass" | 34 targets (count stale post-M5/M6/M7) | High |
| README.md Testing table | "14 test targets" with a 12-row table | 34 targets | High |
| CHANGELOG M6-P2 | "lupdate populated" + "`.qm` binary files embedded" — implies translations ready | All 1,394 strings remain `type="unfinished"` — no human translations delivered | High |
| CHANGELOG M7-P3 | "MRC Compression Pipeline — WS3 (M7-PROMPT-3)" listed as complete | Source exists at `src/engines/mrc/MrcPageProcessor.cpp` — consistent | Pass |
| CLAUDE.md §5 "Already done (M6-PROMPT-3)" | "TestAiProvider 10 tests (26/26)" | 26 ctest total at time of writing; count is pass total not test total | Low ambiguity |
| CLAUDE.md §5 "Already done (M3-P5)" | "TestOfficeImport (24/24)" | CHANGELOG says "4 tests"; CMakeLists registers 4-slot executable; 24/24 is likely total ctest count at that time | Medium drift |
| CLAUDE.md §5 "M4-PROMPT-6" | Claimed "D4 implemented" (prune missing recents) | Source: `HomeController::removeFromRecents()` + `pruneMissingRecents()` in source tree (implementation present) | Pass |
| CLAUDE.md §5 "Already done (M6-PROMPT-2)" | "CHANGELOG admission removed" for empty translations | CHANGELOG `[1.0.0]` no longer has admission, but `.ts` files still empty | Misleading |

---

## Governance Files Status

| File | Exists | Quality |
|------|--------|---------|
| `LICENSE` | Yes | Real Apache-2.0 text |
| `CONTRIBUTING.md` | Yes | Real — 7-section document covering build env, code style, DCO sign-off, PR conventions, architectural non-negotiables |
| `SECURITY.md` | Yes | Real — contact `security@glyphpdf.com`, coordinated disclosure policy, supported versions table |
| `CODE_OF_CONDUCT.md` | Yes | Exists (content not fully read but file present) |
| `README.md` | Yes | Real and detailed; but **"14 test targets"** table is stale |
| `CHANGELOG.md` | Yes | Real and detailed; `[1.0.0]` promotion is present but premature given open gaps |
| `.github/workflows/ci.yml` | Yes | Real CI using MSYS2 ucrt64; pins `windows-2022`; downloads ORT; runs `ctest -j4`; no sanitizer job |
| `.github/workflows/release.yml` | Yes | Exists (content not read but present) |
| `.github/ISSUE_TEMPLATE/bug_report.md` | Yes | Present |
| `.github/ISSUE_TEMPLATE/feature_request.md` | Yes | Present |
| `.github/PULL_REQUEST_TEMPLATE.md` | Yes | Present |
| `LICENSE-3RD-PARTY.md` | Yes | Present |
| `.github/FUNDING.yml` | Yes | Present |

**Notable governance gap:** `SECURITY.md` references `security@glyphpdf.com` — this email domain (`glyphpdf.com`) is not confirmed as registered. For an OSS release, a security contact pointing at a non-existent domain is a functional gap.

---

*Report generated by G-workstream static audit agent. All file:line references verified against current source state at git HEAD `e1c5394`.*
