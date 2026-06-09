# GlyphPDF — Round-2 Verification & Audit (2026-06-09)

**Branch:** `audit-remediation` HEAD `59f960b` · build clean under `-Werror` · `ctest` reports 33/33
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
| 4 | **HIGH (release blocker)** | **B-01 private keys still in git history** (`ca.key`,`signer.key`,`test_signer.p12`, commit `a6ea6aa`) | REPO.md | Must `git filter-repo --invert-paths` before any public push. SP-5b never ran. |
| 5 | **HIGH (security-adjacent)** | More **silent-success-on-failure**: `permissionsDocument` / `removeSecurity` discard `saveDocument` return | UX-03/04 (`SecurityController.cpp:480-514`) | Same E-04 pattern, unswept. |
| 6 | **HIGH (UX orphans)** | Cloud-orphaned ribbon items still enabled (Forms›Distribute, Protect›Compliance) | UX-02 | Dead since WP-1 cloud removal; should be removed/disclosed. |
| 7 | **MEDIUM (UI)** | Theme breakage — hardcoded dark hex in `CommentsWidget`, `CompressDialog`, `SignaturesWidget`; no QSS for QTabWidget/QGroupBox/QSpinBox | UI T-01/02/03, VS-02/03/04 | Broken visuals in Light + High-Contrast themes. |
| 8 | **MEDIUM (security)** | Redaction still misses images in Form-XObject resources, annotation `/AP` streams, `/SMask`s; forged-CRL soft-fails; A-03 PDFium render alloc unclamped | sec2 NF-1/NF-2/NF-4 | Residual redaction-leak + DoS surface. |

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
| **UX-03/04** permissionsDocument/removeSecurity discard save return | OPEN | — | R2-2 pending |
| **UX-08/C-03** FormBuilder delete/move/resize UI-only | **CLOSED** | R2-6 | `FormManager::removeFieldByName`+`updateFieldRect` via PoDoFo AcroForm; `AddFormFieldCommand::undo()` wired; `DeleteFormFieldCommand`/`MoveFormFieldCommand`/`ResizeFormFieldCommand` call engine; delete button enables on selection; `TestFormPersistence.cpp` (TP-1..TP-5) — all green. |
| **NF-1** Forged-CRL soft-fails as UntrustedChain | OPEN | — | R2-3 pending |
| **NF-2** Redaction misses Form-XObject resources + AP streams + SMasks | OPEN | — | R2-3 pending |
| **NF-4** PdfiumBackend render alloc unclamped (OOM/overflow) | OPEN | — | R2-3 pending |
| **T-01** CommentsWidget hardcoded dark hex | **CLOSED** | R2-5 | `src/ui/CommentsWidget.cpp` — inline styleSheets removed; `gp::Theme::accent()/okGreen()` used; QPalette roles for text. |
| **T-02** CompressDialog hardcoded dark hex | **CLOSED** | R2-5 | `src/modes/CompressDialog.cpp` — bg0/bg3/line/lineStrong/fg0/fg2/accent tokens throughout. |
| **T-03** SignaturesWidget hardcoded dark hex | **CLOSED** | R2-5 | `src/ui/SignaturesWidget.cpp` — fg2()/bg3()/bg2()/okGreen()/danger() tokens; drops `color:white`. |
| **T-10** AnnotationLayer Qt::blue HC selection | **CLOSED** | R2-5 | `src/ui/AnnotationLayer.cpp:207` — `QPalette::Highlight` (HC-visible). |
| **VS-02** No QSS for QTabWidget | **CLOSED** | R2-5 | `resources/theme_{dark,light,highcontrast}.qss` — QTabWidget::pane + QTabBar::tab rules added. |
| **VS-03** No QSS for QGroupBox | **CLOSED** | R2-5 | Same 3 QSS files — QGroupBox + QGroupBox::title rules added. |
| **VS-04** No QSS for QSpinBox | **CLOSED** | R2-5 | Same 3 QSS files — QSpinBox + button rules added. |
| **CODE** TestSignatureRealCrypto QSKIPs + hardcoded path | OPEN | — | R2-7 pending |
| **G-07** TestDjotFuzz not written | OPEN | — | R2-7 pending |
| **B-01** Private keys in git history | OPEN | — | R2-8 pending (solo, last) |
