# GlyphPDF — Round-2 Verification & Audit (2026-06-09)

**Branch:** `audit-remediation` HEAD `53e1a40` · build clean under `-Werror` · `ctest` reports 37/37
**Method:** 6 independent auditors (security re-verify, UX, UI, code-completeness, repo, vault) after the WP-0…WP-8a remediation was stabilized + committed.

> **Bottom line:** the remediation is **~80% real and verified**, but the §7 tracker **overstates completion**. Three things marked "DONE" are not actually closed, and the matured real-crypto test is silently skipping. None of this is fake work — it's *incomplete* work labelled done.

## Verification verdict (security)
13 fixes re-checked against committed source: **10 HOLD · 3 WEAK · 0 BROKEN.**
HOLD: A-01 cert-encryption (real CMS→AES round-trip), A-02 image excision, E-01/E-03/E-04/E-06/E-09, A-04, password-AES.

## The gaps that still bite (reported, NOT auto-fixed)

| # | Severity | Finding | Evidence | Why it matters |
|---|---|---|---|---|
| 1 | **HIGH (security)** | **CHAIN-1 "silent un-signing" is NOT closed** | `D-05` ProvenanceGuard is a **no-op gate** (hardcoded `DirectStructural` it always allows); `E-08` incremental `writeUpdate` has **no production caller** — all saves go through full `saveDocument` → invalidates a signed doc's `/ByteRange`. `HomeController.cpp:117` TODO confirms enforcement deferred. | The flagship "sign → can't be silently altered" guarantee is still breakable. |
| 2 | **CRITICAL (UX/claims)** | **~52 ribbon buttons have no handler** → click = `qWarning` + silence | UX-01; across View/Edit/Convert/Forms/Protect | Same class as the fake-SYNCED indicator, at 50× the scale. A user clicks "Validate"/"toEPUB"/"Measure"/… and nothing happens, no feedback. |
| 3 | **HIGH (test integrity)** | **`TestSignatureRealCrypto` QSKIPs** — `tests/fixtures/signing/` not on disk → `REQUIRE_FIXTURES()` skips every method | CODE.md | The "33/33 pass" includes this **skipping**, not validating. The real-crypto gate for E-01/E-06/E-08 never executes in CI. (Hardcoded abs path `TestSignatureRealCrypto.cpp:148` is a second CI bug.) |
| 4 | ~~**HIGH (release blocker)**~~ **CLOSED R2-8** | ~~**B-01 private keys still in git history** (`ca.key`,`signer.key`,`test_signer.p12`, commit `a6ea6aa`)~~ | REPO.md | `git filter-repo --invert-paths` run 2026-06-10; all three files absent from all refs. Fresh test-only fixtures restored (R2-7 new entropy, test CA). `TestSignatureRealCrypto` passes 100%. Collaborators must re-clone. |
| 5 | **HIGH (security-adjacent)** | More **silent-success-on-failure**: `permissionsDocument` / `removeSecurity` discard `saveDocument` return | UX-03/04 (`SecurityController.cpp:480-514`) | Same E-04 pattern, unswept. |
| 6 | **HIGH (UX orphans)** | Cloud-orphaned ribbon items still enabled (Forms›Distribute, Protect›Compliance) | UX-02 | Dead since WP-1 cloud removal; should be removed/disclosed. |
| 7 | **MEDIUM (UI)** | Theme breakage — hardcoded dark hex in `CommentsWidget`, `CompressDialog`, `SignaturesWidget`; no QSS for QTabWidget/QGroupBox/QSpinBox | UI T-01/02/03, VS-02/03/04 | Broken visuals in Light + High-Contrast themes. |
| 8 | ~~**MEDIUM (security)**~~ **CLOSED R2-3** | ~~Redaction still misses images in Form-XObject resources, annotation `/AP` streams, `/SMask`s; forged-CRL soft-fails; A-03 PDFium render alloc unclamped~~ | sec2 NF-1/NF-2/NF-4 — all three closed in commit `39ffe46` | Fixed: Form-XObject local resources, AP stream walk, SMask neutralisation, CRL hard-fail, render clamp. |

## Confirmed NOT-DONE (vs tracker)
- **SP-5b** (B-01 key purge) · **SP-6 / capstone H** (never run) · **G-07 TestDjotFuzz** (not written — `djotToDocument` is real, so it *can* be) · **i18n** 0/1394.

## What IS verified-good
- All 8 Pattern-5 mock surfaces GONE; SYNCED indicator gone; cloud removal clean (zero orphaned headers).
- A-01/A-02 proven by real tests; WP-2 crypto solid; WP-3 architecture; WP-6 concurrency; `-Werror` active.
- Governance files (LICENSE/SECURITY/CONTRIBUTING/CODE_OF_CONDUCT) all real.
- Vault drift fixed (3-way HEAD disagreement resolved; CLAUDE.md now canonical).

## Auto-fixed this round (safe)
- Untracked `.agents/` + `.obsidian/`; deleted scratch `tests/test_sig_dict.cpp`; gitignore `dist/`+`.obsidian/` (commit `59f960b`). `.gitignore` graphify/scratch (commit `b0b12fb`).

## Recommended follow-up sessions (focused)
1. **CHAIN-1 close (security, /security):** make ProvenanceGuard a real gate + route signed-doc saves through `writeUpdate`; add a test that a signed doc stays valid after edit-save. **Highest priority.**
2. **Ribbon dead-button sweep (/frontend+/ux):** for all ~52 handler-less tools — implement, remove, or honestly disable+tooltip. Remove the cloud-orphans.
3. **Test integrity (/testing):** regenerate fresh test fixtures so `TestSignatureRealCrypto` actually RUNS; fix the hardcoded path; write the G-07 djot-fuzz harness.
4. **SP-5b (/security+/devops, solo):** `git filter-repo` key purge + fresh fixtures — gate before any push/public.
5. **Theme fixes (/frontend):** route the hardcoded-color widgets through GpTheme; add QSS for the 3 uncovered widget classes.
6. **SP-6 capstone (/security):** run emergence-engine over remediated code; produce `H-emergent-risk.md`; final release-gate.

Per-domain detail: `round2/{SEC-reverify,UX,UI,CODE,REPO,VAULT}.md`.

---

## R2 Remediation Progress

| Finding | Status | Session | Evidence |
|---------|--------|---------|----------|
| **CHAIN-1** ProvenanceGuard no-op + writeUpdate no callers | **CLOSED** | R2-1 | `ProvenanceGuard.cpp:25-31` throws on DjotThenSave+isSigned; `HomeController/EditController` route signed saves through `writeUpdate()`; `hasPdfSignatures()` added to `IPdfEditorEngine`; `TestChain1.cpp` (6 tests). |
| **UX-01** ~52 ribbon tools have no handler (silent qWarning) | **CLOSED** | R2-4 | 6 tools wired to real engine; 42 added to `RibbonModel::plannedTools()` (disabled+tooltip); `TestRibbonIntegrity.cpp` guards. |
| **UX-02** Cloud-orphans enabled (Forms›Distribute, Protect›Compliance) | **CLOSED** | R2-4 | `sendForm/collect/submit/auditLog/dlp/policy` removed from `RibbonModel.cpp`; `ToolId::Cloud` deleted. |
| **UX-03** permissionsDocument discards EncryptDocumentHelper return | **CLOSED** | R2-2 | `SecurityController.cpp:479-513` — `result` atomic captures return; failure shows `QMessageBox::critical`; worker wrapped in try/catch to prevent `std::terminate`. |
| **UX-04** removeSecurity discards saveDocument return | **CLOSED** | R2-2 | `SecurityController.cpp:524-539` — return checked; on failure shows `QMessageBox::critical`, `viewer->reload()` is NOT called. |
| **UX-14** onSave failure status-bar only (no modal) | **CLOSED** | R2-2 | `HomeController.cpp:155-162` — failure now shows `QMessageBox::critical` matching onSaveAs path; status bar retained. |
| **D3** EditController::onReplaceAllRequested discards save return | **CLOSED** | R2-2 | `EditController.cpp:282-299` — writeUpdate/saveDocument return checked; failure shows `QMessageBox::critical` and returns early. |
| **UX-08/C-03** FormBuilder delete/move/resize UI-only | **CLOSED** | R2-6 | `FormManager::removeFieldByName`+`updateFieldRect` via PoDoFo AcroForm; `AddFormFieldCommand::undo()` wired; `DeleteFormFieldCommand`/`MoveFormFieldCommand`/`ResizeFormFieldCommand` call engine; delete button enables on selection; `TestFormPersistence.cpp` (TP-1..TP-5) — all green. |
| **NF-1** Forged-CRL soft-fails as UntrustedChain | **CLOSED** | R2-3 | `src/engines/SignatureManager.cpp:1500-1507` — `X509_V_ERR_CRL_SIGNATURE_FAILURE` and `X509_V_ERR_UNABLE_TO_DECRYPT_CRL_SIGNATURE` set `isForgedCrl=true`; mapped to `isValid=false`/`Invalid` at line 1530-1533; benign CRL codes remain soft UntrustedChain. |
| **NF-2** Redaction misses Form-XObject resources + AP streams + SMasks | **CLOSED** | R2-3 | `src/engines/podofo/PoDoFoBackend.cpp` — `redactCanvasRecursively` takes `activeResources` param (line 1081); Do-name, font, Properties lookups use `canvasResources` (lines 1258, 1284, 1407); Form XObject recursion passes Form's own `/Resources` (lines 1460-1464); SMask neutralised before base image (lines 1443-1451); annotation `/AP`→`/N` Form XObjects walked before removal (lines 1603-1636). Test: `TestRedaction.cpp::testRedactionNeutralizesImageInFormLocalResources`. |
| **NF-4** PdfiumBackend render alloc unclamped (OOM/overflow) | **CLOSED** | R2-3 | `src/engines/pdfium/PdfiumBackend.cpp:102-110` (`renderPage`) and `lines 140-148` (`renderTile`) — clamp to 20000px per dimension and 120MP area; return empty `QImage()` on violation. Test: `TestRobustness.cpp::testPdfiumRenderOversizedPageRejected`. |
| **T-01** CommentsWidget hardcoded dark hex | **CLOSED** | R2-5 | `src/ui/CommentsWidget.cpp` — inline styleSheets removed; `gp::Theme::accent()/okGreen()` used; QPalette roles for text. |
| **T-02** CompressDialog hardcoded dark hex | **CLOSED** | R2-5 | `src/modes/CompressDialog.cpp` — bg0/bg3/line/lineStrong/fg0/fg2/accent tokens throughout. |
| **T-03** SignaturesWidget hardcoded dark hex | **CLOSED** | R2-5 | `src/ui/SignaturesWidget.cpp` — fg2()/bg3()/bg2()/okGreen()/danger() tokens; drops `color:white`. |
| **T-10** AnnotationLayer Qt::blue HC selection | **CLOSED** | R2-5 | `src/ui/AnnotationLayer.cpp:207` — `QPalette::Highlight` (HC-visible). |
| **VS-02** No QSS for QTabWidget | **CLOSED** | R2-5 | `resources/theme_{dark,light,highcontrast}.qss` — QTabWidget::pane + QTabBar::tab rules added. |
| **VS-03** No QSS for QGroupBox | **CLOSED** | R2-5 | Same 3 QSS files — QGroupBox + QGroupBox::title rules added. |
| **VS-04** No QSS for QSpinBox | **CLOSED** | R2-5 | Same 3 QSS files — QSpinBox + button rules added. |
| **CODE** TestSignatureRealCrypto QSKIPs + hardcoded path | **CLOSED** | R2-7 | Fresh test-only CA+signer fixtures in `tests/fixtures/signing/`; `.gitattributes` marks PDFs binary (CRLF fix); hardcoded `C:/Users/.../signed_blt.pdf` replaced with `m_tmpDir.filePath()`; POST_BUILD deploys podofo 1.1.0 DLL. Suite runs 16/16, **0 QSKIP**. |
| **G-07** TestDjotFuzz not written | **CLOSED** | R2-7 | `tests/TestDjotFuzz.cpp` — DocumentFuzzer round-trip harness (12 seeds + 4 edge cases). Encodes via `documentToDjot`, decodes via `djotToDocument`; asserts no crash/null + leaf text presence. Structural equivalence tests `QEXPECT_FAIL`-guarded until M5 AST-walking lands. 19/19 tests pass in ctest. |
| **B-01** Private keys in git history | **CLOSED** | R2-8 | `git filter-repo --invert-paths` run; `ca.key`/`signer.key`/`test_signer.p12` absent from all refs. Fresh test-only fixtures restored. Collaborators must re-clone. |
| **i18n honesty** Multilingual claim with 0 translated strings | **CLOSED** | R2-9 | `README.md`, `CHANGELOG.md`, `docs/release/release-notes-v1.0.0.md` updated: English-only for v1.0; Arabic/French/German translations planned for future release. |
| **NF-2 CTM bug** Form XObject redaction CTM not inherited | **CLOSED** | R2-9 | `redactCanvasRecursively` now passes `parentCtm` to Form XObject recursion; image bbox computed in page space. `testRedactionNeutralizesImageInFormLocalResources` now PASS. Full suite 37/37. |
| **H-emergent-risk** Emergence capstone | **CLOSED** | R2-9 | `docs/audit/round2/H-emergent-risk.md` written: ER-1 OCSP replay, ER-2 redaction+incremental-save, ER-3 FEK+multi-recipient, ER-4 SaveAs guard gap, ER-5 CMS two-pass downgrade oracle. ER-1+ER-5 compose into practical attack; ER-2 is architectural. |

## R3 Remediation Progress (Emergent Risks)

| Finding | Status | Session | Evidence |
|---------|--------|---------|----------|
| **ER-4** ProvenanceGuard scope gap: SaveAs bypasses gate | **CLOSED** | R3-1 | `HomeController.cpp:176-191` — isSigned guard + cancel-default `QMessageBox::warning` before `saveDocumentAs`; `GpTheme.h` `warning()` amber color; `SignaturesWidget.cpp:69-80` three-way trust status (green/amber/red); `TestChain1.cpp:134-169` coverage. |
| **ER-5** CMS two-pass downgrade oracle UntrustedChain display | **CLOSED** | R3-1 | `SignaturesWidget.cpp:69-80` — `UntrustedChain` renders amber (`gp::Theme::warning()`); compound attack path blocked by ER-1 certID match. |
| **NF-6 / ER-1** OCSP certID mismatch in extractOcspFromDss | **CLOSED** | R3-2 | `SignatureManager.cpp:597-755` — full certID serial matching loop; `validateSignatures` downgrades `NoCertMatch` → `UntrustedChain` + `isValid=false`; `ISignatureManager.h` gains `ocspStatus`/`ocspNoteNF6`; `TestSignatureRealCrypto.cpp` adversarial DER test + positive hasDss path. |
| **ER-2** Redaction + incremental save leaks excised bytes | **CLOSED** | R3-3 | `SecurityController.cpp:394-407` — UI-layer `QMessageBox::critical` hard block; `PdfEditorEngine.cpp:1124-1133` — engine-layer guard returns `false`+`ErrorInfo` (defense-in-depth); `TestRedaction.cpp:614-656` — `testRedactionOnSignedDocIsBlocked()`. |
| **ER-3** FEK derivation + multi-recipient re-encryption | **CLOSED** | R3-4 | `IPdfEditorEngine.h` + `PoDoFoBackend.cpp` `recipientCount()` added; `SecurityController.cpp` — guard in `encryptDocument()` + `permissionsDocument()` when `recipientCount() > 1`; `TestRobustness.cpp` — `testRecipientCountReturnedCorrectly()`. |

## R4 Remediation Progress (v1.0 Launch Gate — 2026-06-10)

Multi-agent audit (native-adversary + guarantee-verification-engine + code-archaeologist + tester) on HEAD of audit-remediation. 9 new findings; all blockers resolved in this session.

| Finding | Status | Session | Evidence |
|---------|--------|---------|----------|
| **N-1** Image XObject optimize path: `int64_t w/h` cast to `int` in `QImage()` without dimension cap | **CLOSED** | R4 | `PoDoFoBackend.cpp:3297-3298` — `constexpr int64_t kMaxOptimizeDim = 10000; if (w > kMaxOptimizeDim \|\| h > kMaxOptimizeDim) continue;` inserted before QImage construction, matching replaceImage() cap. |
| **N-2** ISA (Incremental Saving Attack): unsigned trailing revisions reported as "Valid" | **CLOSED** | R4 | `SignatureManager.cpp:1287` — `bool hasUnsignedTrailing` pre-declared; lines 1353-1381 scan tail bytes for `startxref` without DSS/ETSI markers; lines 1770-1776 downgrade `Valid`/`ValidWithDSS` → `ValidWithUnsignedChanges` + `isValid=false`. `SignaturesWidget.cpp:75-76` — `ValidWithUnsignedChanges` renders amber alongside `UntrustedChain`. |
| **G-B1** Release notes claimed "Four providers" (Anthropic/OpenAI/Gemini/Ollama); only OllamaProvider exists | **CLOSED** | R4 | `docs/release/release-notes-v1.0.0.md:61-64` — AI section rewritten to "Local AI via Ollama"; cloud providers noted as planned. |
| **G-B2** Release notes + ribbon claimed "9 field types" incl. Signature/Button; both map to FieldType::Text | **CLOSED** | R4 | `docs/release/release-notes-v1.0.0.md:75-76` — corrected to "7 field types"; Signature/Button noted as planned. |
| **G-B3** `update/checkOnStartup` defaulted to `true`; QTimer fired 3 s after launch with no consent | **CLOSED** | R4 | `GpMainWindow.cpp:574` — default changed to `false`; update check now opt-in. |
| **T-B1** `TestResourceLimits`: `QVERIFY(true)` tautology + `Q_UNUSED(loaded)` in two tests | **CLOSED** | R4 | `tests/TestResourceLimits.cpp:101-107` — `Q_UNUSED(loaded)` removed; replaced with `if (loaded) QVERIFY(engine.pageCount() >= 0)` / `else QVERIFY(!loaded)`. Same pattern applied to `testMaxImageDimensionEnforcement`. |
| **A-B2** `undo`/`redo` in `plannedTools()` despite QUndoStack wired in all controllers | **CLOSED** | R4 | `core/ToolId.h` — `Undo`, `Redo` added to enum; `ToolId.cpp` — string mappings added; `HomeController.cpp` — `handledTools()` + `activate()` cases call `_ctx->undoStack->undo()/redo()`; `RibbonModel.cpp` — removed from `plannedTools`. |
| **A-W1** `watermark` in `plannedTools()` despite `WatermarkDialog` fully implemented + handler at `GpMainWindow.cpp:446` | **CLOSED** | R4 | `core/ToolId.h` — `Watermark` added; `ToolId.cpp` — string mapping; `HomeController::activate()` — `ToolId::Watermark` calls `_mainWindow->onScreenSelected("watermark")`; `RibbonModel.cpp` — removed from `plannedTools`. |
| **A-W2** `compare` in `plannedTools()` despite `CompareMode` fully implemented | **CLOSED** | R4 | `core/ToolId.h` — `Compare` added; `ToolId.cpp` — mapping incl. `compareDocs` alias; `HomeController::activate()` — `ToolId::Compare` calls `_mainWindow->onScreenSelected("compare")`; `RibbonModel.cpp` — removed `compare` + `compareDocs` from `plannedTools`. |
| **Bonus** `PoDoFo 0.10.4` typo in Acknowledgements (project uses 1.1.0) | **CLOSED** | R4 | `docs/release/release-notes-v1.0.0.md:159` — corrected to `PoDoFo 1.1.0`. |

## R5 Finish Pass (v1.0 launch close-out — 2026-06-10)

Default OCR → ROVER ensemble, professional MSI packaging, and the residual R4 hardening items.

| Finding | Status | Session | Evidence |
|---------|--------|---------|----------|
| **ROVER default** OCR shipped single-engine Tesseract by default | **CLOSED** | R5 | `EditController.cpp:332-360` — default `ocr/engine` = `auto`; resolves to `ensemble` (ROVER) when PP-OCRv5 models present, else `tesseract`. Explicit selections keep loud-fail (audit §7 Pattern 5). `PreferencesDialog.cpp:205` — "Automatic (ROVER ensemble when available)" added as default option; disable-loop indices shifted. `OcrEngine.cpp` — bundled tessdata seeded from install dir before any network fetch. |
| **PKG-1** Installer shipped no ONNX models → ROVER dead on installed app | **CLOSED** | R5 | `packaging/deploy.ps1` — new canonical deploy: objdump DLL closure (104 DLLs), bundles `models/ppocrv5` (3 ONNX + dict), `models/pp_doclayout`, `tessdata`, licenses; hard-fails if any model missing. MSI built 287 MB, SHA-256 in release notes. |
| **PKG-2** No professional installer UX | **CLOSED** | R5 | `packaging/GlyphPDF.wxs` — `WixUI_InstallDir` (license-agreement + install-dir dialogs), `License.rtf` (Apache-2.0), ARP metadata. `packaging/build-msi.ps1` — build→deploy→WiX→SHA-256 pipeline; `.bat` shims forward to PS1. |
| **N-3** Unbounded recursion in redaction walkers | **CLOSED** | R5 | `PoDoFoBackend.cpp` — `redactCanvasRecursively` gains `depth` param + `kMaxCanvasDepth=32` guard; `cleanStructElement` gains `depth` + `kMaxStructDepth=64`. Cyclic Form XObject / `/K` references now skip subtree instead of stack-exhausting. |
| **N-4** Type3/tiling-pattern redaction gap | **DOCUMENTED** | R5 | `SECURITY.md` — T-RED-2 known-limitation row added with mitigation (rasterize-and-re-OCR). Engineering fix deferred to v1.1. |
| **T-H1** 9 untested sanitization vectors + annotation action gap | **CLOSED** | R5 | `PoDoFoBackend.cpp` sanitize — strips dangerous annotation `/A` actions (Launch/URI/SubmitForm/etc; keeps internal GoTo) + annotation `/AA`. `TestSanitization.cpp::testIntegrationSanitizationExtendedVectors` — covers OpenAction, catalog AA, PieceInfo (catalog+page), EmbeddedFiles, Launch/URI annotation actions. |
| **T-H2** `testRevokedCertReportsRevoked` accepted UntrustedChain, never exercised Revoked | **CLOSED** | R5 | `generate_revoked_ocsp.py` rewritten (pyca/cryptography) to emit a real signed OCSP response with the revoked cert's actual serial; fixture regenerated (496 B). Test tightened to require exactly `Revoked`. |
| **T-H3** `testOCSP_NotEmbedded` asserted only `!pdfData.isEmpty()` | **CLOSED** | R5 | Test rewritten: asserts signing succeeds, DSS `/OCSPs` is absent/empty when no OCSP obtained, and verdict is never `Revoked` without an embedded response. |
| **isMock honesty** `RapidOcrEngine::isMockImplementation()` hard-returned false | **CLOSED** | R5 | Now returns `true` under `#else` (built without onnxruntime — genuinely a stub), `false` when `HAS_RAPIDOCR`. |
| **OCRMode** "OCR Verify" screen reachable but unwired (dead combos/run button) | **CLOSED** | R5 | `ScreenNav.cpp` — unfinished review screen hidden for v1.0; OCR runs from Edit ribbon (real ROVER path). Release notes OCR section corrected to match shipped behaviour. |

**Net v1.0 status:** all R4 + R5 blockers closed; N-4 documented as a known limitation. Remaining deferred to v1.1: N-4 engineering fix, full OCR-Verify review screen, OCSP issuer-hash certID upgrade (M5).

### R5 addendum — distribution + repo hygiene (2026-06-10, same session)

| Item | Status | Evidence |
|------|--------|----------|
| Repo hygiene: 741 unnecessary tracked files removed | **DONE** | Untracked: `vcpkg_installed/` (698 dead files — project on MSYS2), `vcpkg.json`, `.context/` (21 AI session notes), `memory/` (10 Obsidian notes), `SESSION_*.md`, internal handoff docs, `graphify-out/`, `Testing/`, `trc_out/tsv_out.txt`, `dist/*.msi` (287 MB binary). All added to `.gitignore`. Tracked: 1758 → 1017 files. |
| winget distribution | **READY** | `packaging/winget/` — 3-manifest set for `Glyph.GlyphPDF` 1.0.0; `winget validate` PASSES. MSI `ProductCode` pinned (`5BA17AB1-…`) in both `.wxs` and installer manifest. Submission steps in `packaging/WINGET-SUBMISSION.md`. Blocked only on: GitHub Release asset upload (manifest InstallerUrl), then PR to microsoft/winget-pkgs. |
| License visibility (MIT + Apache) | **DONE** | `LICENSE-3RD-PARTY.md` — added missing MIT entries (Lua 5.4, Djot reference parser), fixed stale PoDoFo 0.10.4→1.1.0. About dialog (`MenuBar.cpp`) rewritten: links Apache-2.0, names MIT/Apache/BSD/LGPL bundle, points to installed `LICENSE-3RD-PARTY.md`; removed contradictory "All rights reserved" + stale "Qt 6.10.2". Both license files ship in the MSI install dir. |
| Final artifact | **BUILT** | `dist/GlyphPDF-1.0.0-x64.msi` (273.8 MB), SHA-256 `160297D5…BBFC29B1` — consistent across `.sha256` file, release notes, and winget manifest. 37/37 ctest green on the shipped binary. |
| Local path cleanup | **DONE** | Deleted: `vcpkg_installed/` (106 MB), `deploy/` (401 MB, regenerable), `Testing/`, scratch txt outputs, `vcpkg.json`, stale `packaging/stage` DLL clutter + wrong v4 models. |

**Outside-of-code launch steps (M8, in order):** (1) commit + tag `v1.0.0`; (2) Authenticode-sign the MSI (hash changes → regenerate `.sha256`, release notes, winget manifest); (3) create GitHub Release, upload MSI+sha256; (4) promote CHANGELOG `[Unreleased]` → `[1.0.0]`; (5) `wingetcreate submit` / PR to winget-pkgs; (6) optional: purge old 53 MB MSI blob from git history via `git filter-repo` (requires collaborator re-clone, same as R2-8).
