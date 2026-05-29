# GlyphPDF v1.0.0 — Comprehensive Audit & Release Plan

**Project:** `C:\Users\User\Projects\pdf` (branch `master`, head `9a9ef37`)
**Build state:** `dist\GlyphPDF-1.0.0-x64.msi` produced; CHANGELOG dated 2026-05-23 declares SHIPPED
**Audit date:** 2026-05-26
**Stack:** C++17, Qt 6.10.2, MinGW 13.1.0, PoDoFo 0.10.3+, PDFium (BSD), qpdf (Apache-2), OpenSSL 3.x, Tesseract 5, ONNXRuntime, vcpkg `x64-mingw-dynamic`
**Inventory:** 193 source files (112 .h, 81 .cpp), 16 test files, 4 static libraries (`pdfws_core` → `pdfws_engines` → `pdfws_commands` → `pdfws_ui` → `PdfWorkstation`), 12 test executables

**Audit methodology:** 5 parallel specialized agents (architecture, UI wiring + mock-data, C++ code quality + security, PRD gap analysis, industry benchmark) wrote source-anchored reports; this document synthesizes their findings into a release-ready punch list and ships Claude Code launch prompts for execution.

**Scope correction (user-directed):** Stubbed UI surfaces (Cloud Sync ribbon entry, AI Chat panel, 23 "future engine update" ribbon tools, 4 preview-banner mode pages, fake thumbnails, empty translations, RapidOCR Mock UI, decorative panels) are **intentional roadmap placeholders** with user-visible disclosure (disabled controls, "coming in future release" dialogs, preview banners, CHANGELOG Known Issues). They define the **v1.1.0+ roadmap, not v1.0.0 blockers**.

**v1.0.0 blockers** are restricted to: (a) PRD-required functionality that is undisclosed-missing, (b) code-self-flagged correctness defects in implemented engines, and (c) security/data-integrity defects in code that DOES run today.

---

## 🔒 SCOPE LOCK — Branch C (decided 2026-05-26)

> **The team has rejected Branch A (SHIP-FAST). Real public v1.0.0 will not ship until every claim in the PRD and marketing copy is real. Estimated timeline: 6-8 months.**

### What this means

- **All §3 PROMPT-1 through PROMPT-7** (the 11 blockers + OSS release prep) **stay required**.
- **All §4 BATCH-2 through BATCH-5** (mode-page completions, 23 ribbon-tool wiring, OCR ensemble, DiffEngine LCS, translations, AI backend, Djot, comment threading, Edact-Ray defense) **promote from "v1.1.0+ roadmap" to v1.0.0 deliverables.** Treat the §4 prompts as additional v1.0.0 launch prompts.
- **§4 BATCH-1 (Cloud Sync) remains v2.0** — this is the only post-v1.0.0 item.
- **No more "scheduled for future engine update" message boxes** in the shipped v1.0.0.
- **No more preview banners** in mode pages.
- **No more decorative panels** (SignaturesPanel, PdfAValidationPanel, AIChatPanel canned reply, fake thumbnails, fake sync indicator, fake attachment extraction).
- **All translations populated** (ar/fr/de no longer empty shells).
- **Real OCR ensemble** (no more `RapidOCR_Mock`).
- **Real AI backend** wired (Anthropic/OpenAI/Gemini/local Ollama selectable).
- **DiffEngine LCS/Myers** (moves detected, not add+delete pairs).
- **Edact-Ray glyph-advance defense** in redaction.
- **veraPDF subprocess** for real PDF/A conformance validation.
- **Real-crypto E2E TestSignatureValidation** (no more Mock-only signature test).
- **Third-party security audit** before claiming "production-ready" or "Edact-Ray defended" publicly.

### Current MSI status (user-confirmed)

The existing `dist\GlyphPDF-1.0.0-x64.msi` (built 2026-05-23) **keeps its v1.0.0 label but is private/internal only — NOT publicly announced or distributed.** It functions as an internal alpha/beta during the 6-8 month Branch C execution. When real public v1.0.0 is ready, this MSI is overwritten and the public launch happens.

**Critical implication:** any github.com release, package-manager submission (winget/chocolatey/scoop), website link, social-media post, or announcement labeled "v1.0.0" must wait until Branch C completes. Do not let the current MSI leak to a public channel under the v1.0.0 label.

### Realistic 6-8 month execution schedule

This is a planning estimate; reorder or parallelize as team capacity allows. Sessions in the same row are independent and can run in parallel Claude Code sessions.

| Month | Sprint focus | Prompts to execute | Outcome |
|---|---|---|---|
| **M1 (weeks 1-4)** | Security & reliability foundation | PROMPT-6 (CMake guards) → PROMPT-3 (content-stream escaping) + PROMPT-4 (RenderCache race) parallel → PROMPT-2 (SignatureManager) → PROMPT-5 (encryptWithCertificate) | All 6 CRITICAL code-quality defects closed. License contamination locked out. Crypto pipeline trustworthy. |
| **M2 (weeks 5-8)** | Edact-Ray + PDF/A validation + real-crypto tests | PROMPT-1 (autosave + crash recovery) → BATCH-2-style new prompts: Edact-Ray glyph-advance normalization, OCR text-layer scrub, veraPDF subprocess, real-crypto E2E TestSignatureValidation | Security-tier complete. PRD §13 reliability satisfied. Third-party redaction audit can start. |
| **M3 (weeks 9-12)** | Disclosed-stub closure — mode pages | BATCH-3 sub-prompts: FormBuilderMode wired, BatchMode real execution loop, PagesMode real split form, RedactMode connected + pattern redaction backend, InspectorWidget Properties bound | 4 preview-banner mode pages become real features. InspectorWidget no longer decorative. |
| **M4 (weeks 13-16)** | Disclosed-stub closure — 23 ribbon tools | BATCH-4 sub-prompts (group by controller): View (TwoPage, EyeCare), Pages (Split/Reorder/Resize/AddHeader/AddFooter/AddPageNumbers/BatesNumber), Convert (ToHtml, ToText, Compress, PdfToPpt), Forms (Button, SigField, AutoDetect, Tabs), Security (Permissions, RemoveSecurity, Certify, Timestamp, PatternRedact, RegexRedact). Also: Strikeout/Squiggly real ToolModes; Presentation real slideshow; Share with attachment via MAPI; click-to-place form fields. | No more "scheduled for a future engine update" dialogs in the ribbon. All 86 ToolIds either work or are intentionally absent. |
| **M5 (weeks 17-20)** | OCR ensemble + Office import | BATCH-2 full: RapidOcrEngine real PP-OCRv5 (DBNet + perspective + SVTR + CTC) + LayoutDetector + LaneScheduler. BATCH-4 deferred: Office→PDF import (define HAS_LIBREOFFICE + bundle or implement direct DOCX/XLSX/PPTX parsers). | OCR ensemble actually outperforms Tesseract-alone. Office round-trip complete. |
| **M6 (weeks 21-24)** | DiffEngine LCS + Translations + AI + Djot + Comment threading | BATCH-5 sub-prompts in parallel where independent: DiffEngine Myers, translations populated (ar/fr/de via lupdate + commissioned translators), AI backend wired (Anthropic primary + OpenAI/Gemini optional + local-Ollama), comment threading depth, WS2 Djot interchange foundation if scope allows | Every feature claim real. AI chat works. Translations land. Comparison detects moves. |
| **M7-M8 (weeks 25-32)** | Hardening, third-party audit, marketing prep | External code audit (security firm or PDF Association); fix any findings; populate `LICENSE-3RD-PARTY.md` final; record marketing screenshots (real PDFs only); write launch announcement; HackerNews / r/PDF / r/opensource / r/privacy / PDF Association mailing list outreach | v1.0.0 public release. MSI signed with EV cert. Package-manager submissions go live. |

### Updated v1.1+ scope

After this 6-8 month Branch C v1.0.0 ships, the **only remaining roadmap item** is:
- **BATCH-1 — Cloud Sync** (§4): real opt-in encrypted cloud sync via QNetworkAccessManager. Requires product decision on backend model (self-hosted vs S3 vs WebDAV vs BYOSync). Recommended as v2.0 work because BYOSync is the natural OSS pattern but requires user-supplied infrastructure.

Optional v1.1+ enhancements (none release-blocking):
- macOS / Linux ports (Qt 6 is cross-platform; mostly DLL bundling work)
- Mobile companion (separate codebase, share semantic model via WS2 Djot if landed)
- MRC compression in PDF/A (WS3 — deferred from earlier; not table-stakes)
- Bates numbering — already wired in M4 but legal-vertical polish (custom presets, range selection UI)

---

## 0. Executive Verdict

GlyphPDF v1.0.0 is **structurally credible but ships with security and reliability defects in code that runs every day** — the auditing agents found 11 real release-blocking issues (not the 3 the gap-analysis agent saw initially). The disclosed-stub surface is internally consistent and honestly documented; the real risk is in the implemented engines:

- **The PRD-promised autosave timer + crash recovery does not exist** — the `● AUTOSAVED · HH:MM:SS` label actively lies to users (it updates only on manual save).
- **Three critical defects in `SignatureManager`** (~782 LOC, fully shipping today): wrong PAdES B-LT VRI key hashing (self-flagged TODO), `CMS_verify` with no trust policy and no revocation check (self-warned "structurally weaker than claimed"), OCSP embedded into DSS without `OCSP_basic_verify` (self-warned "trusts responder solely on TLS").
- **Content-stream injection vulnerability** in watermark/annotation text writing — `(text) Tj` written raw with no escaping of `(`/`)`/`\` in user-controlled strings, enabling attacker-controlled PDF operator injection.
- **`RenderCache` TOCTOU race** — read lock dropped before write lock for LRU update; concurrent eviction corrupts `m_totalBytes` accounting.
- **`encryptWithCertificate` use-after-free + fundamentally wrong design** — `inBio` freed then passed again to `i2d_CMS_bio_stream`; output written as `.pdf` is actually a CMS envelope, not a valid PDF with /Recipients encryption.
- **CMake sanitization conflict** — `qpdf_set_static_ID(pdf, 1)` after PoDoFo trailer-/ID randomization defeats the privacy mitigation.
- **Missing Poppler FATAL_ERROR guard** in CMake (MuPDF guard is present); license enforcement is asymmetric.
- **RapidOCR engine returns literal `"RapidOCR_Mock"` text** if selected — undisclosed-stub UX. UI exposes the option without runtime gating.

Everything else (Cloud Sync stub, AI Chat canned reply, 23 ribbon tools, mode-page preview banners, fake thumbnails, fake sync indicator, empty translations, decorative panels) is **already disclosed in source comments, dialogs, banners, and CHANGELOG** — those constitute the v1.1.0 roadmap and stay.

**Recommended branch: SHIP-FAST (~10 working days)** addresses the 11 blockers; v1.1.0+ work is organized into 5 batches in §4 with ready-to-paste Claude Code launch prompts.

**OSS context (project decision):** GlyphPDF is being released as fully open source under **Apache-2.0** (recommended) or **MIT**. This:
1. **Promotes Blocker B9** (Poppler/MuPDF CMake guards) from LOW hygiene to **HIGH/existential** — accidentally linking MuPDF (AGPL-3.0) or Poppler (GPL-2.0+) would forcibly relicense the whole project under those copyleft terms. The current asymmetric guard (MuPDF only, no Poppler) must be closed before tagging.
2. **Inverts the positioning** (§5 below): "Open source" goes from explicit "do not claim" (when decision was pending) to the primary asset.
3. **Replaces pricing** with donation-optional, no-commercial-tier model (GitHub Sponsors / OpenCollective if desired).
4. **Simplifies Batch 1** (Cloud Sync): BYOSync — user supplies the backend — becomes the natural OSS model. No vendor infrastructure required.
5. **Adds release-prep work** to PROMPT-7: LICENSE file, CONTRIBUTING.md, CODE_OF_CONDUCT.md, GitHub repo setup, README badges, security-policy SECURITY.md.

---

## 1. The True v1.0.0 Release-Blocker List

Synthesized from the 5 agent reports. Each blocker cites the file:line of evidence, severity, effort, and which agent surfaced it.

| ID | Title | Severity | Effort | File:Line Anchor | Source |
|---|---|---|---|---|---|
| **B1** | Implement interval autosave timer + `.autosave` file write | HIGH | M (3-5d) | `src\GpMainWindow.cpp:140-142`; `src\shell\ModeStrip.cpp:128, 196-202` | UI + Code-Quality + Gap |
| **B2** | Crash recovery scan-on-startup paired with B1 | HIGH | M (2-3d) | absence: grep `autosave\|recovery` in `src` = 0 active code | Gap + Code-Quality |
| **B3** | Fix PAdES B-LT VRI key — remove hex→bytes→hash round-trip | CRITICAL | S (4-8h) | `src\engines\SignatureManager.cpp:323-329` (self-flagged TODO) | Code-Quality + Gap |
| **B4** | `CMS_verify` add trust policy + revocation check + EKU + signing-time | CRITICAL | M (2-4d) | `src\engines\SignatureManager.cpp:752-768` (self-warned) | Code-Quality |
| **B5** | OCSP responses verified with `OCSP_basic_verify` before DSS embedding | CRITICAL | M (1-2d) | `src\engines\SignatureManager.cpp:585-588` (self-warned) | Code-Quality |
| **B6** | Watermark/annotation content-stream escaping — escape `(`, `)`, `\` | CRITICAL | S (1d) | `src\engines\podofo\PoDoFoBackend.cpp:2142, 2259` | Code-Quality |
| **B7** | `RenderCache` TOCTOU race — single lock for hit-update + LRU prepend | CRITICAL | S-M (1-2d) | `src\engines\RenderCache.cpp:117-130, 164-178` | Code-Quality |
| **B8** | `encryptWithCertificate` — fix use-after-free + redesign to produce real PDF /Recipients | CRITICAL | M (3-5d) | `src\engines\PdfEditorEngine.cpp:282-389` (specifically :345-361) | Code-Quality |
| **B9** | CMake — add Poppler FATAL_ERROR guard; tighten MuPDF check (vcpkg + pkg-config). **OSS-elevated: prevents AGPL/GPL forcible relicensing of the project.** | **HIGH** | XS (2h) | `CMakeLists.txt:43-45` | Code-Quality |
| **B10** | Remove `qpdf_set_static_ID(pdf, 1)` so trailer /ID randomization actually persists | HIGH | XS (1h) | `src\engines\podofo\PoDoFoBackend.cpp:1581-1610` (specifically `:1585`) | Code-Quality |
| **B11** | Runtime-disable RapidOCR/ROVER engine selector + CHANGELOG bullet | LOW | S (2-4h) | `src\modes\OCRMode.cpp:75-78`; `src\engines\ocr\RapidOcrEngine.cpp:112-127` | Gap + UI |

**Total v1.0.0 blocker effort: ≈12-25 person-days.** Bulk (B3-B8) is concentrated in `SignatureManager.cpp`, `PoDoFoBackend.cpp`, `RenderCache.cpp`, `PdfEditorEngine.cpp` — five files, all in the engines layer.

### Why these are blockers vs the disclosed-stubs

The user's framing is: stubs intentionally documented for future delivery are NOT blockers. Apply that test:

| Issue | Disclosed in code/UI/CHANGELOG? | User-visible failure mode if shipped? | Blocker? |
|---|---|---|---|
| Cloud Sync stub | YES — ribbon disabled, dialog, CHANGELOG | None (UI blocks the path) | NO — v1.1 |
| AI Chat canned reply | YES — text in panel, CHANGELOG | None (user reads "v1.1 will arrive") | NO — v1.1 |
| 23 ribbon tools "future update" | YES — explicit `QMessageBox` per tool | None (dialog is clear) | NO — v1.1 |
| 4 preview-banner mode pages | YES — banner at top of each | None (banner is clear) | NO — v1.1 |
| Fake thumbnails | NO direct disclosure | YES — users cannot navigate visually | **borderline** — recommend disclosure-only fix in CHANGELOG, keep code |
| Empty .ts translations | YES — CHANGELOG | Partial — Arabic gets RTL but no string translations | NO — v1.1 |
| RapidOCR Mock | PARTIAL — code stub flagged, but UI selector exposes | **YES** — selecting RapidOCR/ROVER produces literal "RapidOCR_Mock" | **YES — B11** |
| Autosave label lie | NO — label actively claims "AUTOSAVED" | **YES** — user trusts label, loses work | **YES — B1+B2** |
| DSS/VRI hex round-trip | YES — self-flagged TODO | YES — B-LT signatures fail LTV lookup in conforming validators | **YES — B3** |
| CMS_verify no trust | YES — qWarning self-admits | YES — `validateSignatures` returns "Valid" for crypto-only-valid sigs | **YES — B4** |
| OCSP unverified | YES — self-warned | YES — MITM/malicious responder can inject forged "good" status | **YES — B5** |
| Content-stream injection | NO disclosure | YES — security vulnerability in shipped code | **YES — B6** |
| RenderCache race | NO disclosure | YES — silent cache corruption under concurrent load | **YES — B7** |
| encryptWithCertificate broken | NO disclosure | YES — output is unopenable; UAF can crash | **YES — B8** |
| Poppler guard missing | NO disclosure | Latent — accidental linking = GPL contamination | **YES — B9** |
| qpdf_set_static_ID defeats randomization | NO disclosure | YES — sanitize claim broken silently | **YES — B10** |

The blockers split into **2 reliability** (B1, B2) + **6 security/correctness** (B3-B8) + **3 hygiene/disclosure** (B9, B10, B11). The disclosed-stubs all pass the "user understands what they're getting" test; the blockers all fail it.

---

## 2. Decision Tree

```
START — choose ship constraint
  │
  ├─[A] SHIP-FAST (RECOMMENDED) ── ~10-15 working days
  │   │
  │   ├─ Days 1-2  : B9 + B10 + B11 (small hygiene + RapidOCR runtime gate)
  │   ├─ Days 3-4  : B6 (content-stream escaping) + B7 (RenderCache race)
  │   ├─ Days 5-7  : B3 (VRI hex round-trip) + B4 (CMS_verify trust) + B5 (OCSP verify)
  │   ├─ Days 8-10 : B8 (encryptWithCertificate redesign)
  │   ├─ Days 11-13: B1 + B2 (autosave + crash recovery)
  │   ├─ Day 14    : Run 10 test targets + add new tests for autosave + signature path
  │   ├─ Day 15    : CHANGELOG update; tag v1.0.0; rebuild MSI; stage for distribution
  │   └─ Defer: All §4 v1.1.0+ batches (Cloud Sync, OCR ensemble, mode-page completion,
  │             Office import, translations, MRC, DiffEngine LCS, AI backend)
  │
  ├─[B] SECURITY-HARDENED ── ~5-6 weeks
  │   │  Adds explicit security work beyond A: Edact-Ray defense, real-crypto E2E tests,
  │   │  veraPDF subprocess for PDF/A validation, third-party redaction audit
  │   │
  │   ├─ Weeks 1-2 : Branch A in full
  │   ├─ Week 3    : Edact-Ray glyph-advance normalization in
  │   │              redactCanvasRecursively (ROADMAP S7 D1)
  │   ├─ Week 4    : OCR text-layer scrub in redaction rect (ROADMAP S7 D2)
  │   ├─ Week 5    : veraPDF subprocess validation (R8 risk)
  │   ├─ Week 6    : Real-crypto E2E TestSignatureValidation (replace MockSignatureManager)
  │   │              + third-party redaction audit
  │   └─ Recommended if customer base skews legal/compliance/regulated
  │
  └─[C] FEATURE-COMPLETE ── ~4-6 months ── NOT RECOMMENDED
      │  Contradicts user scope-correction (stubs are intentional roadmap, not blockers).
      │  Closes the entire disclosed-stub surface before tagging v1.0.0.
      │  Presented for completeness only.
      │
      └─ Branch A + all §4 batches sequential
```

**Original recommendation: SHIP-FAST (Branch A).** Rationale was:
1. CHANGELOG is honest — disclosed gaps don't deceive buyers.
2. Disclosed-stub surface is internally consistent — users understand the placeholder pattern.
3. The 11 blockers are concentrated in 5 engine files — bounded scope.
4. Branch B appropriate if launch ICP is legal/compliance/regulated.

**TEAM DECISION (2026-05-26): Branch C — Feature-Complete (~6-8 months).** The team chose "clean and powerful" over "ship fast." Real public v1.0.0 will not exist until every PRD claim and CHANGELOG promise is implemented — no stubs, no preview banners, no decorative panels, no "scheduled for a future engine update" message boxes, no Mock OCR, no canned AI reply, no empty translations. The current `v1.0.0` MSI becomes a private/internal build during the Branch C execution and is overwritten when public v1.0.0 is ready.

See the **🔒 SCOPE LOCK — Branch C** section at the top of this document for the realistic 6-8 month schedule + which prompts (§3 + promoted §4) execute in which sprint.

---

## 3. v1.0.0 Release Prompts (Claude Code launch format)

Each prompt is a self-contained initial message for a new Claude Code session. They follow the 7-H prompt-engineering structure already established in `SESSION_PROMPTS_V2.md` (role / project_context / current_state / objective / files_to_read / deliverables / verification / constraints / error_recovery / success_criteria). Paste any one prompt as the first message in a fresh Claude Code session rooted at `C:\Users\User\Projects\pdf` to execute it.

All prompts inherit the **STANDARD EXECUTION PROTOCOL** from `SESSION_PROMPTS_V2.md`:
> PHASE 1 ANALYZE → PHASE 2 PLAN → PHASE 3 IMPLEMENT + VERIFY (build after each deliverable) → PHASE 4 CONTEXT GATE at 50% → PHASE 5 FINAL VERIFICATION
>
> Environment: Qt 6.10.2 / MinGW 13.1.0 / vcpkg `x64-mingw-dynamic`. Build: `cmake --build build -- -j8`. Test: `set QT_QPA_PLATFORM=offscreen && cd build && ctest --output-on-failure -j4`.

---

### v1.0.0-PROMPT-1 — Autosave timer + crash recovery (B1 + B2)

<session_metadata>
Phase: v1.0.0 release-blocker
Priority: HIGH (PRD §13 violation)
Depends on: None
Agents: /backend (primary), /testing (verification)
Estimated context: ~30% | Risk: MEDIUM — touches MainWindow lifecycle + temp file infrastructure
Effort: 5-8 person-days
</session_metadata>

<role>
You are a senior C++17/Qt 6 engineer specializing in document recovery patterns, QTimer-driven background workflows, and atomic file I/O on Windows. You are closing the v1.0.0 release-blocking gap between PRD §13's autosave promise and the current implementation, which only updates a misleading "AUTOSAVED · HH:MM:SS" label on manual save.
</role>

<project_context>
GlyphPDF v1.0.0 at `C:\Users\User\Projects\pdf`. Stack: C++17 / Qt 6.10.2 / MinGW 13.1.0 / PoDoFo 0.10.3+. 4-library architecture (`pdfws_core` → `pdfws_engines` → `pdfws_commands` → `pdfws_ui` → `PdfWorkstation`). `Bootstrapper::createContext()` returns `AppContext` with `shared_ptr` engines. PRD §13 requires "Autosave after meaningful changes" and "Crash recovery for unsaved work." CHANGELOG admits this is currently a label-only display, not real autosave.
</project_context>

<current_state>
- [`src\GpMainWindow.cpp:140-142`](C:\Users\User\Projects\pdf\src\GpMainWindow.cpp:140) updates `_modeStrip->setAutosaveTime(QDateTime::currentDateTime())` only on `DocumentSession::dirtyChanged(false)` — that fires only when user manually saves.
- [`src\shell\ModeStrip.cpp:110-115`](C:\Users\User\Projects\pdf\src\shell\ModeStrip.cpp:110) has a 2s `QTimer` but only re-paints the label.
- [`src\ui\PdfViewerWidget.cpp:48,80`](C:\Users\User\Projects\pdf\src\ui\PdfViewerWidget.cpp:48) has a 2s `m_saveDebounceTimer` that flushes annotation sidecar JSON — NOT the PDF.
- `src\core\TempFileManager.cpp` already exists with `QStandardPaths::TempLocation/GlyphPDF` + atexit cleanup — reuse this infrastructure.
- `PoDoFoBackend::writeUpdate()` at [`src\engines\podofo\PoDoFoBackend.cpp:180-185`](C:\Users\User\Projects\pdf\src\engines\podofo\PoDoFoBackend.cpp:180) logs `qCritical` and falls through to `saveDocument` — for autosave we want a snapshot copy, not in-place update.
- No `autosave`, `recovery`, `.crash`, `.autosave` symbols exist anywhere in `src` (verified by grep). This is greenfield work.
</current_state>

<objective>
Implement a `AutosaveManager` class driven by a configurable `QTimer` (default 5-minute interval) that writes the current dirty document as `<docpath>.autosave.pdf` via PoDoFo full-save (not WriteUpdate, since we want a complete recoverable copy). On `MainWindow` startup, scan for orphaned `.autosave.pdf` files newer than their parent and prompt the user to recover. Update the ModeStrip label semantics so it shows TRUE autosave time, not last-manual-save time.
</objective>

<files_to_read>
src\GpMainWindow.h
src\GpMainWindow.cpp (lines 130-170 specifically — dirty handler + recent files load path)
src\shell\ModeStrip.h
src\shell\ModeStrip.cpp (lines 100-210 — label refresh + setAutosaveTime)
src\engines\DocumentSession.h
src\engines\DocumentSession.cpp
src\engines\PdfEditorEngine.h (saveDocument signature)
src\engines\podofo\PoDoFoBackend.h (saveDocument + writeUpdate)
src\core\TempFileManager.h (existing temp infra)
src\core\AppContext.h
src\app\Bootstrapper.cpp (where to construct AutosaveManager)
src\shell\controllers\HomeController.cpp (recent-files persistence to QSettings)
src\ui\PreferencesDialog.cpp (where to expose interval setting)
PRD.md §12 (Performance — "Autosave must not interrupt reading"), §13 (Reliability)
</files_to_read>

<deliverables>

### D1: AutosaveManager class
**Files (NEW):** `src\engines\AutosaveManager.h`, `src\engines\AutosaveManager.cpp`
**Acceptance:**
- `QObject` subclass with `Q_DISABLE_COPY_MOVE`.
- Constructor takes `AppContext*` (for engine + DocumentSession access).
- `start(int intervalSeconds = 300)` / `stop()` lifecycle.
- Owns a `QTimer` that fires every `interval` seconds.
- On tick: if `DocumentSession::isDirty() && !currentFile().isEmpty()`, dispatch save to a worker thread (use `QtConcurrent::run` capturing `weak_ptr` to engine to avoid use-after-free per the RenderCache audit pattern).
- Worker writes to `<currentFile>.autosave.pdf.tmp` via PoDoFo `saveDocument` then atomic rename to `<currentFile>.autosave.pdf` (Windows: `MoveFileExW` with `MOVEFILE_REPLACE_EXISTING`).
- Emits `autosaveStarted()` / `autosaveCompleted(QDateTime)` / `autosaveFailed(QString reason)` signals.
- On success, update DocumentSession with the true autosave timestamp via new method `DocumentSession::setLastAutosave(QDateTime)`.
- Configurable interval read from `QSettings("autosave/intervalSeconds")` with default 300; clamp [60, 1800].

### D2: DocumentSession autosave timestamp + crash-recovery API
**Files:** `src\engines\DocumentSession.h/.cpp`
**Acceptance:**
- Add `QDateTime lastAutosave() const` + `void setLastAutosave(QDateTime)` + signal `lastAutosaveChanged(QDateTime)`.
- Add static helper `static QStringList findOrphanedAutosaves(const QStringList& recentFiles)` returning `<docpath>` entries that have a newer `<docpath>.autosave.pdf` sibling on disk.

### D3: ModeStrip label semantics fix
**Files:** `src\shell\ModeStrip.h/.cpp`
**Acceptance:**
- `setAutosaveTime()` is now driven by `DocumentSession::lastAutosaveChanged` signal, NOT `dirtyChanged`.
- Label format: `● AUTOSAVED · HH:MM:SS` when real autosave occurred; `● UNSAVED` when dirty and no autosave yet; `● SAVED · HH:MM:SS` (new state) when last manual save with no subsequent dirty edit. The two states must be visually distinct.
- Remove the fake-hash `⤺ SYNCED · v.X` sync indicator entirely OR clearly relabel as `LOCAL ONLY` (no cloud sync exists in v1.0). The `unsigned int hash = ... * 31 + c.unicode()` block at [`ModeStrip.cpp:142-150`](C:\Users\User\Projects\pdf\src\shell\ModeStrip.cpp:142) must go.

### D4: Crash-recovery dialog on startup
**Files (NEW):** `src\ui\RecoveryDialog.h`, `src\ui\RecoveryDialog.cpp`. **Modify:** `src\GpMainWindow.cpp`
**Acceptance:**
- After `MainWindow` construction, before showing welcome widget, call `DocumentSession::findOrphanedAutosaves(homeController->recentFiles())`.
- If list non-empty, show `RecoveryDialog` with checkable list of `<docpath> · <autosave timestamp> · <size delta>`.
- "Recover Selected" → load `<docpath>.autosave.pdf` into the editor, show "Recovered from autosave" status, mark document dirty (so user knows to Save As).
- "Discard All" → delete every `<docpath>.autosave.pdf` from the recent list.
- "Decide Later" → no-op (orphans remain on disk until next startup).
- Test fixture: kill `PdfWorkstation.exe` process between autosave + manual save, relaunch → dialog appears.

### D5: AppContext wiring + Bootstrapper construction
**Files:** `src\core\AppContext.h`, `src\app\Bootstrapper.cpp`
**Acceptance:**
- `AppContext` adds `std::shared_ptr<AutosaveManager> autosave`.
- `Bootstrapper::createContext()` constructs it via `std::make_shared<AutosaveManager>(...)` after engines are wired.
- `MainWindow` constructor calls `_ctx->autosave->start()` after loading first document (or on any subsequent `setDocument`).
- `MainWindow` destructor calls `_ctx->autosave->stop()`.

### D6: Preferences UI for interval
**Files:** `src\ui\PreferencesDialog.h/.cpp`
**Acceptance:**
- Add to General tab: "Autosave interval (minutes)" `QSpinBox` range [1, 30], default 5.
- Persists to `QSettings("autosave/intervalSeconds")` (multiply by 60).
- Live: changing the setting calls `_ctx->autosave->stop(); _ctx->autosave->start(newInterval);`.

### D7: Tests
**Files (NEW):** `tests\TestAutosave.cpp`. **Modify:** `tests\CMakeLists.txt` (add executable + `qt_add_executable` + `target_link_libraries pdfws_engines Qt6::Test`).
**Acceptance:**
- Test that timer fires at the configured interval (use `QSignalSpy` + `QTest::qWait`).
- Test that no save occurs when document is clean.
- Test that `<currentFile>.autosave.pdf` exists after a dirty + timer-tick cycle.
- Test that `findOrphanedAutosaves()` detects an orphan with `mtime` newer than parent.
- Test that on `start(0)` (or interval < 60) the manager clamps to 60.
- Add to ctest target list. Headless via `QT_QPA_PLATFORM=offscreen`.

### D8: CHANGELOG + README + ROADMAP updates
**Files:** `CHANGELOG.md`, `README.md`, `ROADMAP.md`, `SESSION_BRIEF_NEXT.md`
**Acceptance:**
- CHANGELOG: REMOVE the "Autosave" entry from Known Issues. ADD under Reliability: "Real interval autosave + crash recovery dialog (`AutosaveManager`)."
- README: under Features → add "Real interval autosave with crash-recovery prompt."
- ROADMAP §"Phase 7 — Quality + Polish → Session 16": mark "crash recovery + autosave timer" as ✅ DONE.
- SESSION_BRIEF_NEXT: update "What is NOT yet started" — remove autosave; add a "Recently Closed" section listing this work.

</deliverables>

<verification>
```bash
cmake --build build -- -j8
set QT_QPA_PLATFORM=offscreen && cd build && ctest --output-on-failure -j4
# Expected: 11/11 pass (10 existing + new TestAutosave)

# Manual fixture: open a doc, edit, wait 6 min, kill PdfWorkstation.exe via Task Manager,
# relaunch from Desktop shortcut → RecoveryDialog appears with the document listed.

# Verify no fake-hash sync indicator remains
grep -n "hash = hash \* 31" src\shell\ModeStrip.cpp
# Expected: 0 matches

# Verify AutosaveManager is in Bootstrapper
grep -n "AutosaveManager" src\app\Bootstrapper.cpp
# Expected: at least 1 match

# Verify CHANGELOG no longer lists autosave under Known Issues
grep -n "Autosave:" CHANGELOG.md
# Expected: 0 matches under "Known Issues" section
```
</verification>

<constraints>
- DO NOT use `WriteUpdate` for autosave snapshots — full `saveDocument` produces a complete recoverable file. WriteUpdate is for signed-document incremental save only.
- DO NOT block the GUI thread for autosave I/O — must be `QtConcurrent::run` or QThread.
- DO NOT capture raw `this` pointers in the worker lambda — use `QPointer` or `weak_ptr<AutosaveManager>`.
- DO NOT autosave password-protected documents you don't have the password for (skip silently with `qWarning`).
- DO NOT delete the autosave file after a successful manual Save — leave it for one more cycle in case the Save itself failed mid-write. Cleanup happens at next startup after detecting `mtime(.autosave) < mtime(parent)`.
- DO NOT remove the existing 2-second annotation-sidecar debounce — that is unrelated and serves a different purpose.
- SCOPE: Only autosave + crash recovery + label-semantics + sync-indicator removal. No other engine changes.
</constraints>

<error_recovery>
- If `MoveFileExW` fails (file locked by AV): retry once after 250ms, then log warning and leave the `.tmp` file (next cycle overwrites it).
- If PoDoFo `saveDocument` throws on the autosave path: catch, log `qWarning("Autosave failed: %s", e.what())`, emit `autosaveFailed`, do NOT show user dialog (autosave should be invisible).
- If `findOrphanedAutosaves` returns >50 items: cap the recovery dialog at 50 and add a "Show All" button (defensive against runaway autosave).
- If user opens a `.autosave.pdf` directly (drag-drop): treat it as a normal PDF — do not infinite-loop the autosave logic.
</error_recovery>

<success_criteria>
- [ ] Build: 0 errors
- [ ] Tests: 11/11 pass (10 existing + new TestAutosave)
- [ ] `src\engines\AutosaveManager.h/.cpp` exist; constructed by Bootstrapper
- [ ] `AppContext` includes `std::shared_ptr<AutosaveManager> autosave`
- [ ] Manual fixture: kill-during-edit produces a `.autosave.pdf`; relaunch shows RecoveryDialog
- [ ] ModeStrip label shows real autosave timestamp; no fake-hash sync indicator
- [ ] PreferencesDialog has "Autosave interval (minutes)" setting; live-applies
- [ ] CHANGELOG updated: removed from Known Issues, added under Reliability
</success_criteria>

---

### v1.0.0-PROMPT-2 — SignatureManager security hardening (B3 + B4 + B5)

<session_metadata>
Phase: v1.0.0 release-blocker
Priority: CRITICAL (3 self-flagged crypto defects in shipped code)
Depends on: None
Agents: /security (primary), /backend (OpenSSL API)
Estimated context: ~40% | Risk: HIGH — cryptographic correctness; touches every signature path
Effort: 4-7 person-days
</session_metadata>

<role>
You are a senior cryptographic engineer specializing in PAdES digital signatures (ETSI EN 319 142-1), X.509 PKI, OCSP and CRL processing, CMS via OpenSSL 3.x, and PDF signature byte-range semantics (ISO 32000-2 §12.8). You are fixing three release-blocking defects in `SignatureManager` that the code itself flags via `TODO` comments and `qWarning` self-admissions.
</role>

<project_context>
GlyphPDF v1.0.0 ships PAdES B-LT and B-LTA signing as a marquee feature (per CHANGELOG and README). The implementation is real (~782 LOC in `src\engines\SignatureManager.cpp`), produces DSS dictionaries with VRI entries, fetches OCSP responses, embeds DocTimeStamp for B-LTA, and supports certificate-based encryption. Tests use `MockSignatureManager` and exercise the public API surface but do not validate against the real OpenSSL/PoDoFo crypto pipeline (acknowledged gap in CHANGELOG).
</project_context>

<current_state>
**Three release-blocking defects flagged by the code itself:**

1. **VRI key wrong** ([`src\engines\SignatureManager.cpp:323-329`](C:\Users\User\Projects\pdf\src\engines\SignatureManager.cpp:323)):
   ```cpp
   // TODO(audit-2026-05-23): remove hex round-trip in VRI key construction.
   QByteArray rawContents = QByteArray::fromHex(sigContentsHex);
   QByteArray vriKey = QCryptographicHash::hash(rawContents, QCryptographicHash::Sha1).toHex().toUpper();
   ```
   ETSI EN 319 132 requires the VRI key to be **uppercase hex of SHA-1 of the raw `/Contents` byte stream as written to the file**, NOT of hex-decoded inner bytes. Validators looking up by spec-conformant key will not find this VRI entry — silently breaking B-LT long-term validation.

2. **CMS_verify with no trust policy** ([`src\engines\SignatureManager.cpp:752-768`](C:\Users\User\Projects\pdf\src\engines\SignatureManager.cpp:752)):
   ```cpp
   X509_STORE_set_default_paths(store);
   CMS_verify(cms, NULL, store, NULL, NULL, CMS_DETACHED | CMS_BINARY);
   // qWarning() << "PAdES long-term validity is structurally weaker than claimed.";
   ```
   No `X509_VERIFY_PARAM`, no signing-time check, no EKU enforcement, no revocation check. `validateSignatures` returns `trustStatus = "Valid"` or `"ValidWithDSS"` based purely on CMS cryptographic integrity — not actual trust chain validation against a real root store.

3. **OCSP embedded into DSS unverified** ([`src\engines\SignatureManager.cpp:585-588`](C:\Users\User\Projects\pdf\src\engines\SignatureManager.cpp:585)):
   ```cpp
   // qWarning() << "OCSP response embedded into DSS without OCSP_basic_verify.
   //              Trusts responder solely on TLS.";
   dssOcsps.append(ocspResponse);
   ```
   MITM or malicious OCSP responder can inject forged "good" status that becomes part of a "B-LT" signature.

**Additional findings (from audit, fix opportunistically while in this code):**
- ByteRange validation accepts overlapping ranges at [`SignatureManager.cpp:697-716`](C:\Users\User\Projects\pdf\src\engines\SignatureManager.cpp:697) — `off1 + len1 > off2` is not rejected. Shadow attack defense gap.
- TSA token buffer at [`SignatureManager.cpp:432-449`](C:\Users\User\Projects\pdf\src\engines\SignatureManager.cpp:432) reserved at 16 KB; multi-cert TSA tokens can exceed. Bump to 32 KB or compute dynamically.
- `extractSignatureContentsHex` swallows all exceptions and returns empty at [`SignatureManager.cpp:465-487`](C:\Users\User\Projects\pdf\src\engines\SignatureManager.cpp:465), causing `buildDssDictionary` to skip VRI entirely — user thinks they have B-LT, gets B-T silently.
- `i2d_X509` / `i2d_PrivateKey` size-only first calls don't check `len > 0` at lines 163-166 + 525-528.
- `BIO_new_mem_buf` failure unhandled at multiple lines (149-150, 318, 743, 751).
</current_state>

<objective>
Fix the three CRITICAL defects (VRI key hashing, CMS_verify trust, OCSP verification) so the shipped B-LT and B-LTA signatures actually pass conforming-validator checks. Opportunistically close the ByteRange overlap, TSA buffer, and exception-swallow gaps. Add a real-crypto integration test that produces a B-LT signature and validates it with OpenSSL CLI.
</objective>

<files_to_read>
src\engines\SignatureManager.h
src\engines\SignatureManager.cpp (the whole file — 782 LOC, but particularly lines 100-200 (loadP12), 300-410 (DSS), 450-530 (extractContents + sign), 560-620 (OCSP), 680-770 (validate))
src\core\interfaces\ISignatureManager.h (the public surface — do not break it)
tests\TestSignatureValidation.cpp
tests\mocks\MockSignatureManager.h
ROADMAP.md (Phase 3 + R3 risk + License Matrix for crypto deps)
</files_to_read>

<deliverables>

### D1: Fix VRI key hashing (B3)
**Files:** `src\engines\SignatureManager.cpp`
**Acceptance:**
- Refactor `extractSignatureContentsHex` → split into `extractSignatureContentsRaw(QByteArray& rawBytes, QByteArray& hexAsWritten)` returning both forms.
- `buildDssDictionary` signature changes to take `const QByteArray& sigContentsRaw` instead of `const QString& sigContentsHex`.
- VRI key construction becomes:
  ```cpp
  QByteArray vriKey = QCryptographicHash::hash(sigContentsRaw, QCryptographicHash::Sha1).toHex().toUpper();
  ```
- Remove the `TODO(audit-2026-05-23)` comment.
- Add unit test: produce a known-bytes signature, compute VRI key via OpenSSL CLI (`openssl dgst -sha1`), compare to GlyphPDF output. Must match.

### D2: Add real trust policy to CMS_verify (B4)
**Files:** `src\engines\SignatureManager.cpp`
**Acceptance:**
- Replace `X509_STORE_set_default_paths(store)` with a configurable trust source:
  - QSettings key `signing/trustStorePath` (defaults to system Windows cert store via `wincrypt` API on Windows).
  - For tests: allow injection of a custom `X509_STORE*` via new test-only method `setTrustStoreForTest(X509_STORE*)`.
- Add `X509_VERIFY_PARAM`:
  ```cpp
  X509_VERIFY_PARAM* params = X509_VERIFY_PARAM_new();
  X509_VERIFY_PARAM_set_flags(params, X509_V_FLAG_CRL_CHECK | X509_V_FLAG_CRL_CHECK_ALL);
  X509_VERIFY_PARAM_set_purpose(params, X509_PURPOSE_SMIME_SIGN);  // closest to PAdES
  X509_STORE_set1_param(store, params);
  ```
- Add signing-time verification: parse `signingTime` attribute from CMS, check within cert validity window.
- Add EKU check: cert must have `id-kp-emailProtection` OR `id-kp-codeSigning` OR `id-kp-clientAuth` (PAdES does not mandate a specific EKU but must not be `id-kp-OCSPSigning` etc.).
- Verify chain to trusted root; if fails, set `info.trustStatus = "UntrustedChain"` (new status string).
- Remove the `qWarning("structurally weaker than claimed")` self-admission.
- Update `validateSignatures` return: new field `SignatureInfo::trustStoreUsed` (string identifying which root store).

### D3: Verify OCSP responses before DSS embedding (B5)
**Files:** `src\engines\SignatureManager.cpp`
**Acceptance:**
- Before appending `ocspResponse` to `dssOcsps`, call:
  ```cpp
  OCSP_RESPONSE* resp = d2i_OCSP_RESPONSE(...);
  if (!resp || OCSP_response_status(resp) != OCSP_RESPONSESTATUS_SUCCESSFUL) { skip; }
  OCSP_BASICRESP* basic = OCSP_response_get1_basic(resp);
  if (!basic) { skip; }
  if (OCSP_basic_verify(basic, certChain, store, 0) != 1) { skip + log; }
  // Also check OCSP signer cert chain to trusted root
  ```
- Failed verification: do NOT embed; log `qWarning`; signature degrades to B-T (no DSS for that signer's OCSP).
- Remove the self-admission `qWarning`.
- Add unit test with a known-bad OCSP response (wrong signer) — verify it is rejected.

### D4: ByteRange overlap rejection (defensive)
**Files:** `src\engines\SignatureManager.cpp:697-716`
**Acceptance:**
- Add `if (off1 + len1 > off2) { info.trustStatus = "ByteRangeOverlap"; continue; }` before the EOF-coverage check.
- Add unit test with crafted overlapping ranges.

### D5: TSA token buffer + exception-swallow cleanup
**Files:** `src\engines\SignatureManager.cpp`
**Acceptance:**
- Bump TSA placeholder buffer from `contents.assign(16384, '\0')` to `contents.assign(32768, '\0')` at line 434 (with comment "32 KB accommodates multi-cert TSA chains; bump if cert chain exceeds").
- `extractSignatureContentsRaw` (renamed in D1): replace `catch (...)` blocks with `catch (const PoDoFo::PdfError&) + catch (const std::exception&)` and propagate via `ErrorInfo` instead of returning empty silently.

### D6: Size-validation hardening on i2d_X509 / i2d_PrivateKey / BIO_new_mem_buf
**Files:** `src\engines\SignatureManager.cpp` (lines 163-166, 525-528, 172-179, 149-150, 318, 743, 751)
**Acceptance:**
- Every `int len = i2d_X(cert, nullptr);` followed by `if (len <= 0) return false;` (or appropriate error path).
- Every `BIO* bio = BIO_new_mem_buf(...)` followed by `if (!bio) return false;` BEFORE first use.

### D7: Real-crypto E2E test
**Files (NEW):** `tests\TestSignatureRealCrypto.cpp`. **Modify:** `tests\CMakeLists.txt`
**Acceptance:**
- New test executable linked against `pdfws_engines` (real `SignatureManager`, NOT mock).
- Fixture: ships a test `.p12` + cert chain + a one-page test PDF in `tests\fixtures\signing\`.
- Test 1: sign with `PAdESLevel::B_T` → validate → `trustStatus == "Valid"` or `"ValidWithDSS"`.
- Test 2: sign with `PAdESLevel::B_LT` → validate → DSS present, VRI key matches OpenSSL CLI computation.
- Test 3: sign with `PAdESLevel::B_LTA` → validate → DocTimeStamp present, archive timestamp valid.
- Test 4: verify against a cert chain with no trusted root → `trustStatus == "UntrustedChain"`.
- Test 5: verify against an OCSP response with wrong signer → OCSP not embedded.
- Mark as `QSKIP` if test fixtures missing (so CI doesn't fail on devs without the `.p12`).

### D8: ROADMAP + CHANGELOG updates
**Files:** `ROADMAP.md`, `CHANGELOG.md`
**Acceptance:**
- ROADMAP §"Phase 3 — Security" → mark VRI hex round-trip / CMS trust policy / OCSP verify as ✅ closed.
- CHANGELOG: add under Security: "Hardened PAdES B-LT/B-LTA: real trust-policy verification, OCSP response verification, byte-range overlap rejection, VRI key spec-conformance fix."
- CHANGELOG: REMOVE the "TestSignatureValidation uses MockSignatureManager" entry from Known Issues.

</deliverables>

<verification>
```bash
cmake --build build -- -j8
set QT_QPA_PLATFORM=offscreen && cd build && ctest --output-on-failure -j4
# Expected: all tests pass including new TestSignatureRealCrypto (or QSKIP if fixtures absent)

# Verify VRI key matches OpenSSL CLI
openssl dgst -sha1 -binary <signature_contents_raw> | xxd -p -u
# Compare against GlyphPDF's vriKey output — must match byte-for-byte

# Verify TODO comment is gone
grep -rn "TODO(audit-2026-05-23)" src/
# Expected: 0 matches

# Verify self-admissions are gone
grep -rn "structurally weaker\|Trusts responder solely on TLS" src/
# Expected: 0 matches

# Validate a signed PDF in Adobe Reader DC — must show green check + LTV box
```
</verification>

<constraints>
- DO NOT break the public `ISignatureManager` interface — extend with new fields, don't remove.
- DO NOT use MD5 or SHA-1 for signature digest (VRI key SHA-1 is spec-required and stays).
- DO NOT skip OCSP entirely if verification fails — degrade to B-T with logged warning, not silent acceptance.
- DO NOT cache OCSP responses across sessions without checking `nextUpdate` field.
- DO NOT use system trust store on Windows via OpenSSL's `set_default_paths` — that uses OpenSSL's compiled-in paths, not Windows wincrypt. Use the `wincrypt` API explicitly.
- DO NOT modify the public `SignatureManager` API used by `SignDocumentHelper` and `SignatureDialog` — they're stable.
- SCOPE: SignatureManager only. Do not touch FormManager or other engines in this session.
</constraints>

<error_recovery>
- If OpenSSL functions return unexpected error codes: `ERR_print_errors_fp(stderr)` and log the OpenSSL error string in the ErrorInfo `technicalDetails`.
- If the test `.p12` is missing: emit a one-time `qInfo` and `QSKIP` — do not fail.
- If MOC build issues arise from new ISignatureManager fields: regenerate via `cmake --build build --target pdfws_core_autogen`.
- If `OCSP_basic_verify` returns 0 but cert chain looks correct: check whether the OCSP responder cert needs `id-kp-OCSPSigning` EKU — common cause.
</error_recovery>

<success_criteria>
- [ ] Build: 0 errors
- [ ] All existing tests pass + new TestSignatureRealCrypto passes (or QSKIPs cleanly)
- [ ] VRI key SHA-1 matches OpenSSL CLI byte-for-byte
- [ ] `validateSignatures` returns `UntrustedChain` for cert with no trusted root
- [ ] OCSP unverified responses are NOT embedded in DSS
- [ ] ByteRange overlap test: crafted overlapping ranges produce `ByteRangeOverlap` status
- [ ] No `TODO(audit-2026-05-23)` or `structurally weaker` strings remain in source
- [ ] Adobe Reader DC shows green check + full LTV box on a B-LTA-signed test PDF
</success_criteria>

---

### v1.0.0-PROMPT-3 — Content-stream injection fix (B6)

<session_metadata>
Phase: v1.0.0 release-blocker
Priority: CRITICAL (security vulnerability in shipped code)
Depends on: None
Agents: /security, /backend
Estimated context: ~15% | Risk: LOW — bounded fix in 2 functions
Effort: 1 person-day
</session_metadata>

<role>
You are a senior C++ engineer specializing in PDF content-stream syntax (ISO 32000-2 §7.3.4), PostScript string literal escaping, and injection-class vulnerability mitigation. You are closing a content-stream injection vulnerability where user-controlled strings are written raw into PDF text operators.
</role>

<project_context>
`PoDoFoBackend.cpp` writes user-controlled watermark text and annotation text directly into PDF content streams using PostScript-style `(text) Tj` operators. PDF literal strings inside `( ... )` must escape three characters: `(` `)` `\` (per ISO 32000-2 §7.3.4.2 Table 3). The current code performs zero escaping, allowing attacker-controlled strings to inject arbitrary content-stream operators.
</project_context>

<current_state>
**Two confirmed injection sites:**

1. Watermark text at [`src\engines\podofo\PoDoFoBackend.cpp:2259`](C:\Users\User\Projects\pdf\src\engines\podofo\PoDoFoBackend.cpp:2259):
   ```cpp
   wm << "(" << text << ") Tj\n";  // text is unescaped user input
   ```

2. Annotation text at [`src\engines\podofo\PoDoFoBackend.cpp:2142`](C:\Users\User\Projects\pdf\src\engines\podofo\PoDoFoBackend.cpp:2142):
   ```cpp
   "(" + anno.text.toStdString() + ") Tj"  // anno.text is unescaped
   ```

**Proof of vulnerability:** A watermark text of `Hello) Tj 1 0 0 1 100 200 cm /XObject Do (` terminates the literal early, injects a CTM transform and an `XObject Do` invocation referencing whatever XObject names the attacker can guess from the document, then opens a new literal to consume the trailing `) Tj`. Result: attacker drew arbitrary geometry / referenced unintended XObjects / corrupted the document.

**Same risk pattern exists in:** any code path that writes user-controlled strings to a content stream. Audit by grep for `<< "(" <<` and `+ ") Tj"` patterns in `src\engines\podofo\`.
</current_state>

<objective>
Implement a `pdfEscapeLiteralString(const QString&)` (or `std::string`) helper that correctly escapes `(`, `)`, and `\` per ISO 32000-2 §7.3.4.2, route all content-stream string writes through it, and add unit tests that prove injection is no longer possible.
</objective>

<files_to_read>
src\engines\podofo\PoDoFoBackend.h
src\engines\podofo\PoDoFoBackend.cpp (specifically the watermark and annotation embedding sections — lines 2100-2300, plus grep all `(" <<` and `") Tj"` patterns)
src\engines\PdfEditorEngine.cpp (any other content-stream writes)
tests\TestSanitization.cpp (will extend)
</files_to_read>

<deliverables>

### D1: pdfEscapeLiteralString helper
**Files (NEW):** `src\engines\podofo\PdfStringEscape.h`, `src\engines\podofo\PdfStringEscape.cpp` (or inline as static in PoDoFoBackend.cpp)
**Acceptance:**
- Signature: `std::string pdfEscapeLiteralString(const std::string& input);` and overload for `QString`.
- Escapes `\` → `\\`, `(` → `\(`, `)` → `\)`.
- Optionally escapes non-printable bytes as 3-digit octal `\nnn` per spec.
- Constexpr-friendly where possible; thread-safe (no static state).
- Unit-tested against the spec examples in ISO 32000-2 §7.3.4.2 Table 3.

### D2: Route all watermark + annotation writes through escaper
**Files:** `src\engines\podofo\PoDoFoBackend.cpp` lines 2142 + 2259 (and any others found by grep)
**Acceptance:**
- Line 2259: `wm << "(" << pdfEscapeLiteralString(text) << ") Tj\n";`
- Line 2142: `"(" + pdfEscapeLiteralString(anno.text.toStdString()) + ") Tj"`
- grep `<< "(" <<` and `+ ") Tj"` in `src/engines/podofo/` returns 0 unescaped matches after this.

### D3: Audit + fix any other content-stream string writes
**Files:** all of `src\engines\podofo\`
**Acceptance:**
- Grep for `Tj`, `TJ`, `'`, `"` operators with user-controlled strings.
- Header-footer injection at [`PoDoFoBackend.cpp:780-840`](C:\Users\User\Projects\pdf\src\engines\podofo\PoDoFoBackend.cpp:780) (`addHeaderFooter`): template substitution of `{page}`, `{filename}` etc. — `{filename}` from the user-loaded file path is user-controllable. Escape it.
- Bates numbering at [`PoDoFoBackend.cpp:780-840`](C:\Users\User\Projects\pdf\src\engines\podofo\PoDoFoBackend.cpp:780): `prefix`/`suffix` strings are user input. Escape.
- `editTextInline` at [`PoDoFoBackend.cpp:362-488`](C:\Users\User\Projects\pdf\src\engines\podofo\PoDoFoBackend.cpp:362): the new text passed to PdfPainter — verify PdfPainter does the escaping (read PoDoFo source/docs); if not, escape before passing.

### D4: Injection tests
**Files (extend):** `tests\TestSanitization.cpp`
**Acceptance:**
- Test: watermark with text `Foo) Tj 1 0 0 1 100 200 cm (` — verify the resulting page content stream does NOT contain an unbalanced literal or injected operators.
- Test: annotation with text containing literal `\`, `(`, `)` — verify roundtrip preserves the characters when extracted.
- Test: header-footer template with filename containing `(` — verify safe.
- Test: pdfEscapeLiteralString invariants — applying twice equals applying once (idempotent under double-escape from caller side).

### D5: CHANGELOG note
**Files:** `CHANGELOG.md`
**Acceptance:**
- Add under Security: "Fixed content-stream literal-string injection vulnerability in watermark, annotation, header/footer, and Bates numbering code paths (`pdfEscapeLiteralString`)."

</deliverables>

<verification>
```bash
cmake --build build -- -j8
set QT_QPA_PLATFORM=offscreen && cd build && ctest --output-on-failure -j4 -R TestSanitization

# Manual: create a watermark with text "Hello) Tj end (foo" — open resulting PDF in Adobe;
# verify watermark renders as literal text, no extra geometry visible.

# Verify no unescaped patterns remain
grep -nE '<< *"\(" *<<|\+ *"\\\) Tj"' src/engines/podofo/
# Expected: 0 matches (or only matches inside pdfEscapeLiteralString itself)
```
</verification>

<constraints>
- DO NOT use hex strings (`<...>`) as an "easier" workaround — they have their own escaping rules and break tooling expectations.
- DO NOT change the visual appearance of any existing watermark/annotation output (escaping changes only the on-disk byte stream).
- DO NOT escape `>` or `<` — those are legal in literal strings.
- DO NOT touch UI-layer string sanitization — that's separate from content-stream syntax.
- SCOPE: Content-stream string-write escaping only. No other PoDoFoBackend changes.
</constraints>

<success_criteria>
- [ ] Build: 0 errors
- [ ] Tests: all pass + new injection tests
- [ ] grep `<< "(" <<` in `src/engines/podofo/` returns only escaped sites
- [ ] Manual injection PoC PDF renders harmlessly
- [ ] CHANGELOG updated
</success_criteria>

---

### v1.0.0-PROMPT-4 — RenderCache TOCTOU race + concurrency hardening (B7)

<session_metadata>
Phase: v1.0.0 release-blocker
Priority: CRITICAL (silent cache corruption under load)
Depends on: None
Agents: /backend (Qt concurrency), /performance
Estimated context: ~20% | Risk: MEDIUM — touches hot path; needs careful lock discipline
Effort: 1-2 person-days
</session_metadata>

<role>
You are a senior Qt 6 engineer specializing in `QReadWriteLock`, lock-free patterns, and high-throughput cache design for desktop rendering pipelines. You are fixing a TOCTOU race between hit-lookup and LRU-update in the 3-tier render cache that ships in v1.0.0.
</role>

<project_context>
`RenderCache` is a 3-tier LRU cache (page metadata, rendered pages, text layers) with a 256 MB default cap, used by `PdfViewerWidget` for page rendering and by viewport prefetch (`QtConcurrent::run` spawning a background thread that reads ahead ±3 pages). Multiple `PdfiumBackend` instances and the prefetch thread access the cache concurrently.
</project_context>

<current_state>
**Race at [`src\engines\RenderCache.cpp:117-130`](C:\Users\User\Projects\pdf\src\engines\RenderCache.cpp:117) and similarly at `:164-178`:**
```cpp
m_lock.lockForRead();
if (m_renderedPages.contains(key)) {
    m_hits.fetchAndAddRelaxed(1);
    QImage img = m_renderedPages.value(key).image;
    m_lock.unlock();                    // GAP

    m_lock.lockForWrite();              // another thread can evict here
    m_lruList.removeOne(key);
    m_lruList.prepend(key);
    m_lock.unlock();
    return img;
}
```
Between `unlock()` and `lockForWrite()`, another thread can run `evictIfNeeded` and remove `key` from `m_renderedPages`. The image copy `img` is OK (snapshotted), but the LRU then `prepend`s a key with no entry — corrupts `m_totalBytes` accounting and LRU consistency. Over time, eviction either over-evicts (false-positive memory pressure) or under-evicts (real OOM).

**Additional concurrency issues found:**
- Prefetch lambda captures `this` raw at [`RenderCache.cpp:251-280`](C:\Users\User\Projects\pdf\src\engines\RenderCache.cpp:251); if cache is destroyed while task is running, UAF.
- `evictIfNeeded` comment says "assumes write lock held" but no debug assertion enforces it.
- `m_pageSizes` read under read lock, mutated under separate write lock — two concurrent `pageSize(p)` queries for same `p` both call PDFium twice (wasted work, not a correctness bug).
- `OcrPipeline::run` is not internally synchronized — relies on engine-level mutex but `d->preprocessor` is touched directly. Currently stateless; document the invariant.
</current_state>

<objective>
Eliminate the TOCTOU race by merging hit-lookup and LRU-update under a single write lock (or implementing a lock-free LRU). Capture cache state safely in prefetch lambdas using `weak_ptr` or `QPointer`. Add a debug assertion that enforces the "write lock held" precondition on `evictIfNeeded`. Add a stress test that triggers concurrent eviction + lookup.
</objective>

<files_to_read>
src\engines\RenderCache.h
src\engines\RenderCache.cpp (all 350+ lines)
src\engines\ocr\OcrPipeline.h
src\engines\ocr\OcrPipeline.cpp (preprocessor invariant)
src\ui\PdfViewerWidget.cpp (RenderCache consumer)
tests\TestThreadSafety.cpp (will extend)
</files_to_read>

<deliverables>

### D1: Fix TOCTOU at getOrRender + getOrRenderTile
**Files:** `src\engines\RenderCache.cpp` lines 117-130 + 164-178
**Acceptance:**
- Replace read-lock-then-write-lock pattern with single write-lock for the lookup + LRU-update sequence (read-lock for metadata-only paths can stay).
- Alternative: split LRU into a separate `std::atomic<uint64_t>` access counter per entry; eviction picks the lowest. Document trade-off.
- Verify `m_totalBytes` invariant after every insertion/eviction via debug assertion.

### D2: Prefetch lambda safety
**Files:** `src\engines\RenderCache.cpp:251-280` + `RenderCache.h`
**Acceptance:**
- Cache instances must be created via `std::make_shared<RenderCache>` (Bootstrapper already does this for engines).
- Prefetch lambda captures `std::weak_ptr<RenderCache>` (not `this`); first action is `auto self = weak.lock(); if (!self) return;`.
- Cancellation token check happens both at top of each loop iteration AND between the lookup and write inside one iteration.

### D3: evictIfNeeded assertion
**Files:** `src\engines\RenderCache.cpp:314-323`
**Acceptance:**
- Add thread-local sentinel pattern: a `thread_local bool t_writeLocked = false` set/cleared by lock/unlock RAII wrappers; `evictIfNeeded` asserts `Q_ASSERT(t_writeLocked);`.
- Document the invariant in the function header comment.

### D4: pageSize de-duplication (minor)
**Files:** `src\engines\RenderCache.cpp:56-79`
**Acceptance:**
- Use a per-key `std::shared_future<QSizeF>` so concurrent `pageSize(p)` queries share one PDFium call.
- Alternative: accept double-query as a benign perf cost and just comment that.

### D5: OcrPipeline preprocessor thread-safety note
**Files:** `src\engines\ocr\OcrPipeline.cpp`
**Acceptance:**
- Add header comment documenting the invariant: "OcrPreprocessor is stateless; OcrPipeline::run may be called concurrently. If preprocessor ever becomes stateful, add explicit synchronization."

### D6: Concurrency stress test
**Files (extend):** `tests\TestThreadSafety.cpp`
**Acceptance:**
- Spawn N=8 threads doing `getOrRender(randomKey)` for 1000 iterations each against a cache with cap forced low (4 MB) to force frequent eviction.
- After all threads complete, verify:
  - `m_totalBytes == sum(value.image.sizeInBytes() for v in m_renderedPages.values())` — invariant holds.
  - `m_lruList.size() == m_renderedPages.size()` — no orphan keys in LRU.
  - No assertion failures during the run (catches the t_writeLocked sentinel violations).
- Run with `--repeat 5` flag to catch flakiness.

</deliverables>

<verification>
```bash
cmake --build build -- -j8
set QT_QPA_PLATFORM=offscreen && cd build && ctest --output-on-failure -j4 -R TestThreadSafety --repeat-until-fail 5

# Verify lock pattern fixed
grep -nA5 "m_lock.lockForRead()" src/engines/RenderCache.cpp
# Expected: no unlock-then-lockForWrite pattern remains
```
</verification>

<constraints>
- DO NOT remove the `QAtomicInt m_hits/m_misses` counters — they're useful diagnostics.
- DO NOT increase the cache size cap to mask the race — fix the underlying logic.
- DO NOT change the public RenderCache API (getOrRender/getOrRenderTile/clear/setMaxCacheSize).
- DO NOT introduce a global mutex for PDFium — that's a separate concern (also flagged in audit; defer to v1.1).
- SCOPE: RenderCache concurrency only.
</constraints>

<success_criteria>
- [ ] Build: 0 errors
- [ ] Tests: TestThreadSafety stress test passes 5/5 repetitions
- [ ] `m_totalBytes` invariant holds after stress test
- [ ] No unlock-then-lockForWrite pattern remains in RenderCache.cpp
- [ ] Prefetch lambda uses weak_ptr
</success_criteria>

---

### v1.0.0-PROMPT-5 — encryptWithCertificate redesign (B8)

<session_metadata>
Phase: v1.0.0 release-blocker
Priority: CRITICAL (UAF crash + functionally wrong output)
Depends on: None
Agents: /security, /backend
Estimated context: ~25% | Risk: HIGH — fundamental rework of cert-based encryption path
Effort: 3-5 person-days
</session_metadata>

<role>
You are a senior C++ + OpenSSL engineer specializing in PDF 2.0 §7.6.4 public-key security handlers (`/Filter /PubSec`, `/SubFilter /adbe.pkcs7.s5`) and CMS envelope construction with recipient-specific RSA-wrapped AES keys. You are reimplementing GlyphPDF's certificate-based encryption path, which currently has a use-after-free bug AND produces invalid PDF output.
</role>

<project_context>
GlyphPDF v1.0.0 advertises certificate-based encryption (CHANGELOG: "Certificate-based encryption (multi-recipient)"). The implementation at `PdfEditorEngine::encryptWithCertificate` exists and compiles, but is fundamentally broken in two ways: (1) `BIO* inBio` use-after-free, and (2) writes raw CMS envelope bytes to `.pdf` instead of constructing a proper PDF /Recipients structure per PDF 2.0 spec.
</project_context>

<current_state>
**Use-after-free at [`src\engines\PdfEditorEngine.cpp:345-361`](C:\Users\User\Projects\pdf\src\engines\PdfEditorEngine.cpp:345):**
```cpp
BIO *inBio = BIO_new_mem_buf(pdfData.data(), pdfData.size());
CMS_ContentInfo *cms = CMS_encrypt(recipients, inBio, ...);
BIO_free(inBio);                              // freed at line 349
...
BIO *outBio = BIO_new(BIO_s_mem());
if (!i2d_CMS_bio_stream(outBio, cms, inBio, CMS_BINARY)) { ... }   // line 361 — USE AFTER FREE
```

**Wrong output format:**
The code writes the CMS envelope bytes as the entire `.pdf` file. Opening the resulting file in any PDF viewer fails because it is not a PDF — it is a CMS BER-encoded blob.

**Correct design per PDF 2.0 §7.6.4:**
- PDF uses an `/Encrypt` dictionary with `/Filter /PubSec` and `/SubFilter /adbe.pkcs7.s5` (AES-256).
- Each recipient gets an entry in `/Recipients` array — a PKCS#7/CMS-enveloped 24-byte seed wrapped under that recipient's RSA public key.
- The actual file encryption key is derived from the 20-byte session key + 4-byte permissions per spec §7.6.4.3.
- PoDoFo has limited public-key encryption support; may need to call OpenSSL directly to build the /Recipients entries then hand the encryption-key seed to PoDoFo.
</current_state>

<objective>
Reimplement `encryptWithCertificate` to:
1. Fix the use-after-free (lifecycle correction).
2. Produce a real PDF 2.0 public-key-secured document with `/Filter /PubSec` and a proper `/Recipients` array, NOT a raw CMS envelope.
3. Result must open in Adobe Reader, prompt for any matching recipient's private key, and decrypt successfully.
</objective>

<files_to_read>
src\engines\PdfEditorEngine.h
src\engines\PdfEditorEngine.cpp (lines 282-400 — full encryptWithCertificate)
src\core\interfaces\IPdfEditorEngine.h (encryptWithCertificate signature)
src\engines\podofo\PoDoFoBackend.h
src\engines\podofo\PoDoFoBackend.cpp (encryption-related code, especially `encryptDocument` at line ~1366-1391 for the AES-256 pattern)
ROADMAP.md (§Phase 3 cert encryption)
tests\TestEncryption.cpp
</files_to_read>

<deliverables>

### D1: Fix UAF
**Files:** `src\engines\PdfEditorEngine.cpp:345-361`
**Acceptance:**
- Either keep `inBio` alive across both `CMS_encrypt` and `i2d_CMS_bio_stream` calls, or pass `nullptr` for the content arg to `i2d_CMS_bio_stream` (CMS_STREAM was already set).
- Wrap all `BIO*` allocations in `std::unique_ptr<BIO, BioDeleter>` RAII.

### D2: Decide on public-key encryption strategy
Read PoDoFo's public-key encryption support:
- If PoDoFo 0.10.x supports `PdfEncryptPubSec` or similar: use it. Set algorithm to PubSecAES256, populate recipients list, let PoDoFo handle /Recipients array construction.
- If not (likely for current PoDoFo version): construct the CMS-enveloped seeds manually via OpenSSL, then patch the PoDoFo `/Encrypt` dictionary to use `/Filter /PubSec` with our manually-built `/Recipients` array. This is more work but is the correct path.

Document the chosen strategy in a code comment.

### D3: Implement encryptWithCertificate correctly
**Files:** `src\engines\PdfEditorEngine.cpp` + possibly `src\engines\podofo\PoDoFoBackend.cpp`
**Acceptance:**
- Accept list of recipient X509 certs + permission flags (existing API surface).
- Generate a random 20-byte seed + append 4-byte permission flags = 24-byte session-key material.
- For each recipient: CMS_encrypt the 24-byte seed with that recipient's public key.
- Construct `/Encrypt << /Filter /PubSec /SubFilter /adbe.pkcs7.s5 /V 4 /Length 256 /CF ... /Recipients [...] >>` and apply to the PoDoFo document.
- Save the document via PoDoFo. Resulting `.pdf` is a real PDF.

### D4: Round-trip test
**Files:** `tests\TestEncryption.cpp`
**Acceptance:**
- Generate a test RSA key pair + self-signed cert.
- Encrypt a test PDF for that recipient.
- Open the encrypted PDF, decrypt using the private key (via PoDoFo or OpenSSL directly), verify content matches original.
- Test multi-recipient: encrypt for 2 certs, decrypt with either private key — both must succeed.
- Manual: try opening the encrypted PDF in Adobe Reader with the matching `.pfx` imported — Reader should prompt for cert selection then decrypt.

### D5: Documentation
**Files:** `CHANGELOG.md`, `README.md`
**Acceptance:**
- CHANGELOG: clarify "Certificate-based encryption" was broken in earlier dev builds and is now spec-conformant PDF 2.0 PubSec.
- README: document the supported algorithm (AES-256 via /adbe.pkcs7.s5).

</deliverables>

<verification>
```bash
cmake --build build -- -j8
set QT_QPA_PLATFORM=offscreen && cd build && ctest --output-on-failure -j4 -R TestEncryption

# Manual verification with Adobe Reader DC
```
</verification>

<constraints>
- DO NOT use AES-128 — v1.0 claim is AES-256.
- DO NOT skip the random session key — never reuse keys across encryptions.
- DO NOT write the CMS envelope as the file body again — output must be a valid PDF.
- DO NOT break `encryptDocument` (the password-based path) — that one is fine.
- SCOPE: Certificate-based encryption only. Password-based stays.
</constraints>

<success_criteria>
- [ ] Build: 0 errors
- [ ] No UAF on the encryptWithCertificate path (verify under ASan if available)
- [ ] Encrypted PDF opens in Adobe Reader DC and prompts for cert
- [ ] Multi-recipient: both private keys can decrypt
- [ ] Round-trip test passes
</success_criteria>

---

### v1.0.0-PROMPT-6 — CMake license guard + sanitization fix (B9 + B10)

<session_metadata>
Phase: v1.0.0 release-blocker
Priority: HIGH (license risk + privacy claim defeat)
Depends on: None
Agents: /devops (CMake), /security (sanitization correctness)
Estimated context: ~10% | Risk: LOW — small, bounded
Effort: 2-4 hours
</session_metadata>

<role>
You are a senior CMake + license-compliance engineer working on a privacy-positioned PDF tool. You are closing two small but real defects: the Poppler FATAL_ERROR guard is missing (MuPDF guard exists, asymmetric protection) and `qpdf_set_static_ID(pdf, 1)` defeats trailer-/ID randomization done one block earlier in sanitization.
</role>

<project_context>
ROADMAP §"Forbidden Dependencies" explicitly names both MuPDF (AGPL-3.0) and Poppler (GPL-2.0+) as license-incompatible — must NEVER link in-process. `CMakeLists.txt:43-45` guards MuPDF with FATAL_ERROR. No symmetric Poppler check exists. Separately, `PoDoFoBackend::sanitizeDocument` randomizes the trailer /ID (line 1559) then runs qpdf post-processing that calls `qpdf_set_static_ID(pdf, 1)` (line 1585), setting /ID to a deterministic value — defeating the randomization.
</project_context>

<current_state>
[`CMakeLists.txt:43-45`](C:\Users\User\Projects\pdf\CMakeLists.txt:43):
```cmake
if(TARGET mupdf OR TARGET libmupdf)
    message(FATAL_ERROR "MuPDF/libmupdf detected. AGPL-3.0 license is incompatible.")
endif()
```
No symmetric block for Poppler. Also doesn't catch vcpkg-named targets (`mupdf-pkg`) or `pkg-config mupdf`.

[`src\engines\podofo\PoDoFoBackend.cpp:1559`](C:\Users\User\Projects\pdf\src\engines\podofo\PoDoFoBackend.cpp:1559) randomizes trailer /ID[1]; then at [`:1585`](C:\Users\User\Projects\pdf\src\engines\podofo\PoDoFoBackend.cpp:1585) `qpdf_set_static_ID(pdf, 1)` resets it deterministically. The qpdf static-ID wins because it runs later.
</current_state>

<objective>
Add symmetric Poppler FATAL_ERROR guard + tighten MuPDF guard to catch vcpkg and pkg-config sources. Remove the `qpdf_set_static_ID(pdf, 1)` call OR re-randomize after qpdf save so trailer /ID sanitization actually persists.
</objective>

<files_to_read>
CMakeLists.txt (full file)
src\engines\podofo\PoDoFoBackend.cpp lines 1540-1620 (sanitize + qpdf post-process)
LICENSE-3RD-PARTY.md
ROADMAP.md (§"Forbidden Dependencies")
</files_to_read>

<deliverables>

### D1: Symmetric Poppler guard
**Files:** `CMakeLists.txt:43-45` (extend)
**Acceptance:**
```cmake
# MuPDF + Poppler FATAL_ERROR guards (license: AGPL-3.0 + GPL-2.0+ incompatible)
if(TARGET mupdf OR TARGET libmupdf OR TARGET mupdf-pkg)
    message(FATAL_ERROR "MuPDF detected (AGPL-3.0). Use PDFium for rendering.")
endif()
if(TARGET poppler OR TARGET poppler-cpp OR TARGET libpoppler)
    message(FATAL_ERROR "Poppler detected (GPL-2.0+). Use PDFium for rendering.")
endif()

find_package(MuPDF QUIET)
if(MuPDF_FOUND)
    message(FATAL_ERROR "MuPDF found via find_package (AGPL-3.0). Remove.")
endif()
find_package(Poppler QUIET)
if(Poppler_FOUND)
    message(FATAL_ERROR "Poppler found via find_package (GPL-2.0+). Remove.")
endif()

include(FindPkgConfig)
pkg_check_modules(MUPDF QUIET mupdf)
if(MUPDF_FOUND)
    message(FATAL_ERROR "MuPDF found via pkg-config (AGPL-3.0). Remove.")
endif()
pkg_check_modules(POPPLER QUIET poppler)
if(POPPLER_FOUND)
    message(FATAL_ERROR "Poppler found via pkg-config (GPL-2.0+). Remove.")
endif()
```

### D2: Fix sanitization /ID conflict
**Files:** `src\engines\podofo\PoDoFoBackend.cpp:1581-1610`
**Acceptance:**
- OPTION A (simpler): Remove the `qpdf_set_static_ID(pdf, 1)` call. qpdf will generate a random /ID by default during write — that's compatible with the trailer-/ID randomization done above.
- OPTION B (more explicit): Keep static-ID call but follow it with a second randomization pass after qpdf writes (open the resulting file, randomize trailer /ID[1], save again). More expensive; not recommended.
- Verify by computing the trailer /ID of a freshly-sanitized PDF twice (two separate sanitize runs on the same input): the two outputs must have DIFFERENT /ID[1] values.

### D3: Tests
**Files:** `tests\TestSanitization.cpp` (extend)
**Acceptance:**
- Test: sanitize the same input PDF twice; verify trailer /ID[1] differs between the two outputs (currently fails per audit; passes after D2).
- Test: build attempt with a CMake module that defines `target_compile_definitions(... -DHAS_MUPDF=1)` should FATAL_ERROR — manual verification only since CMake re-run is involved.

### D4: LICENSE-3RD-PARTY.md update
**Files:** `LICENSE-3RD-PARTY.md`
**Acceptance:**
- Remove the "*(future)*" tag from PDFium and qpdf rows — they are linked today.
- Add a row for ONNX Runtime (MIT, used by RapidOcrEngine when HAS_RAPIDOCR).
- Verify version columns match what vcpkg installs.

</deliverables>

<verification>
```bash
cmake --build build -- -j8
set QT_QPA_PLATFORM=offscreen && cd build && ctest --output-on-failure -j4 -R TestSanitization

# Verify Poppler guard is present
grep -n "Poppler detected" CMakeLists.txt
# Expected: at least 1 match

# Verify qpdf_set_static_ID is gone (option A) or addressed (option B)
grep -n "qpdf_set_static_ID" src/engines/podofo/PoDoFoBackend.cpp
# Expected: 0 matches (option A) or surrounded by comment explaining option B

# Manual sanitization /ID test
./PdfWorkstation --sanitize input.pdf output1.pdf
./PdfWorkstation --sanitize input.pdf output2.pdf
diff <(pdfinfo output1.pdf | grep "ID:") <(pdfinfo output2.pdf | grep "ID:")
# Expected: differ
```
</verification>

<constraints>
- DO NOT remove the existing MuPDF guard — extend it.
- DO NOT use qpdf with `--static-id` flag in any form (defeats sanitization).
- DO NOT add new dependencies to fix this — pure config + small code change.
- SCOPE: CMake guards + sanitization /ID conflict only.
</constraints>

<success_criteria>
- [ ] Build: 0 errors
- [ ] Tests pass
- [ ] Poppler guard added to CMakeLists.txt (4 detection methods total)
- [ ] qpdf_set_static_ID removed or compensated
- [ ] LICENSE-3RD-PARTY.md updated
</success_criteria>

---

### v1.0.0-PROMPT-7 — RapidOCR runtime disable + CHANGELOG + v1.0.0 tag (B11 + release)

<session_metadata>
Phase: v1.0.0 release-blocker (final)
Priority: HIGH (close the only undisclosed-stub gap + tag release)
Depends on: PROMPT-1 through PROMPT-6 complete
Agents: /frontend (UI gate), /pm (release coordination)
Estimated context: ~15% | Risk: LOW
Effort: 4-8 hours + release-day operations
</session_metadata>

<role>
You are a senior release engineer closing the final v1.0.0 gap (RapidOCR Mock UI exposure), updating user-facing documentation to match the new shipped reality, and tagging + building the final v1.0.0 release.
</role>

<project_context>
This is the final prompt in the v1.0.0 release sprint. All six prior prompts (autosave, signature hardening, content-stream escaping, RenderCache race, certificate encryption, CMake guards) have shipped to master. The remaining task: runtime-gate the RapidOCR engine selector in OCRMode so users cannot accidentally select a stub engine that returns "RapidOCR_Mock" text. Then update CHANGELOG / README / ROADMAP to match shipped reality and tag v1.0.0.
</project_context>

<current_state>
[`src\engines\ocr\RapidOcrEngine.cpp:112-127`](C:\Users\User\Projects\pdf\src\engines\ocr\RapidOcrEngine.cpp:112) is an explicit STUB that returns `OcrResult{ text: "RapidOCR_Mock", boundingBox: QRectF(0,0,100,20), confidence: 85 }`. [`src\modes\OCRMode.cpp:65-68`](C:\Users\User\Projects\pdf\src\modes\OCRMode.cpp:65) populates an engine combo with "Tesseract Only / RapidOCR PP-OCRv5 / ROVER Ensemble" entries, only gated by `HAS_RAPIDOCR` compile flag. The compile flag IS defined in the shipped build → the combo offers options that produce literal "RapidOCR_Mock" output.
</current_state>

<objective>
Add a runtime `isMockImplementation()` sentinel to `RapidOcrEngine` + `IOcrEngine` interface; in OCRMode at runtime, query the engine and disable / hide the RapidOCR + ROVER selector options if the implementation is a mock. Update CHANGELOG to disclose the v1.0.0 gap explicitly. Tag and release v1.0.0.
</objective>

<files_to_read>
src\engines\ocr\RapidOcrEngine.h
src\engines\ocr\RapidOcrEngine.cpp (the STUB block at lines 112-127)
src\core\interfaces\IOcrEngine.h
src\modes\OCRMode.h
src\modes\OCRMode.cpp (engine combo at lines 65-78)
CHANGELOG.md
README.md
ROADMAP.md
SESSION_BRIEF_NEXT.md
packaging\build-msi.bat (release build script)
</files_to_read>

<deliverables>

### D1: Add isMockImplementation() to IOcrEngine
**Files:** `src\core\interfaces\IOcrEngine.h`
**Acceptance:**
- Add `virtual bool isMockImplementation() const { return false; }` (default false, opt-in for stubs).
- Implement in `RapidOcrEngine` to return `true` (until PP-OCRv5 lands).
- Implement in `OcrEngine` (Tesseract) to return `false`.

### D2: Runtime-gate the OCRMode engine selector
**Files:** `src\modes\OCRMode.cpp:65-78`
**Acceptance:**
- After populating combo items, iterate engines via `_ctx->ocrPrimary->isMockImplementation()` etc. and `setItemEnabled(idx, false)` + add tooltip "Available in v1.1.0" for mock implementations.
- Default selection: first non-mock engine (Tesseract).
- "ROVER Ensemble" requires BOTH engines to be non-mock; disable if either is mock.

### D3: CHANGELOG update
**Files:** `CHANGELOG.md`
**Acceptance:**
- Add a new section under Known Issues:
  > **OCR engine selector — v1.0.0 limitation**
  > Only the Tesseract engine is functional in v1.0.0. The RapidOCR PP-OCRv5 and ROVER Ensemble options are disabled at runtime; their backend implementation is scheduled for v1.1.0. (`src\engines\ocr\RapidOcrEngine.cpp:112` — actual PP-OCRv5 DBNet+SVTR pipeline pending.)
- Add the full set of v1.0.0 work landed in this sprint to the [1.0.0] section:
  - Reliability: real interval autosave + crash recovery (`AutosaveManager`)
  - Security: PAdES B-LT/B-LTA hardening (trust policy, OCSP verification, VRI key fix, byte-range overlap rejection)
  - Security: content-stream injection fix (`pdfEscapeLiteralString`)
  - Reliability: RenderCache TOCTOU race fix + concurrency hardening
  - Security: certificate-based encryption fixed (real PDF 2.0 PubSec output, no more CMS envelope mishap)
  - License: symmetric Poppler FATAL_ERROR guard + tightened MuPDF guard
  - Privacy: trailer /ID randomization now actually persists (removed conflicting qpdf static-ID)

### D4: README + ROADMAP + SESSION_BRIEF_NEXT updates
**Files:** `README.md`, `ROADMAP.md`, `SESSION_BRIEF_NEXT.md`
**Acceptance:**
- README: Features section mentions real autosave + crash recovery. Security section mentions trust-policy-aware signature validation.
- ROADMAP: mark Sessions 7 D1/D2/D3 (redaction hardening), Session 16 D2 (crash recovery), and the new security work as ✅ DONE for v1.0.0.
- SESSION_BRIEF_NEXT: rewrite to reflect post-v1.0.0 state. "What was just shipped" section listing the v1.0.0 sprint deliveries. "What is NOT yet started" updated to remove all items now closed.

### D5: OSS release prep — LICENSE + governance files
**Files (NEW):** `LICENSE`, `CONTRIBUTING.md`, `CODE_OF_CONDUCT.md`, `SECURITY.md`, `.github\ISSUE_TEMPLATE\bug_report.md`, `.github\ISSUE_TEMPLATE\feature_request.md`, `.github\PULL_REQUEST_TEMPLATE.md`, `.github\FUNDING.yml`
**Acceptance:**
- `LICENSE` — full Apache-2.0 text (or MIT if user chose that). Copy from `https://www.apache.org/licenses/LICENSE-2.0.txt` (canonical).
- Add `// SPDX-License-Identifier: Apache-2.0` header to every `.h` / `.cpp` in `src\`. Script it via:
  ```bash
  for f in $(find src -name "*.h" -o -name "*.cpp"); do
    grep -q "SPDX-License-Identifier" "$f" || sed -i '1i// SPDX-License-Identifier: Apache-2.0\n' "$f"
  done
  ```
- `CONTRIBUTING.md` — how to set up the build env, code style (cite existing implicit conventions: `gp::` namespace, `shared_ptr` engines, `IFooEngine` interfaces), testing requirement (must add a test for new engine method), commit message style (Conventional Commits recommended), DCO sign-off (`git commit -s`).
- `CODE_OF_CONDUCT.md` — Contributor Covenant 2.1 (industry standard, copy from `https://www.contributor-covenant.org/version/2/1/code_of_conduct/`).
- `SECURITY.md` — responsible disclosure policy: report security issues privately via GitHub Security Advisory or email; 90-day disclosure window; in-scope (signing, redaction, encryption, sanitization, file parsing) vs out-of-scope (DoS via crafted PDF unless leads to RCE).
- `.github\ISSUE_TEMPLATE\*` — bug report (build info, repro steps, expected/actual) + feature request (use case, PRD alignment).
- `.github\PULL_REQUEST_TEMPLATE.md` — checklist (build green, tests added, CHANGELOG updated, signed-off).
- `.github\FUNDING.yml` — `github: <user>` and/or `open_collective: glyphpdf`.

### D6: README — OSS badges + positioning
**Files:** `README.md`
**Acceptance:**
- Add badges at top: `[Apache-2.0]` `[Build Status]` `[Release version]` `[Sponsor]` (via shields.io).
- Reposition the README opening to match §5 positioning: "Open-source, privacy-first PDF workstation. Native Windows, Qt 6, C++17. PAdES B-LTA signing. Verifiable redaction. No cloud. No AI. No telemetry."
- Add sections: "Why GlyphPDF" (3-pillar features), "Install" (winget command + GitHub Releases link + build-from-source pointer), "Contributing" (link CONTRIBUTING.md), "License", "Sponsor".
- Add screenshot of the app (capture at 1920x1080 with dark theme + a real PDF loaded — not the fake-thumbnails sidebar; load a real document for the screenshot).

### D7: LICENSE-3RD-PARTY.md — public OSS license matrix
**Files:** `LICENSE-3RD-PARTY.md`
**Acceptance:**
- Already exists per code-quality audit; ensure every shipped dependency has its license verified and credit text included.
- Add the bundled font licenses (if shipping fonts in `:/fonts`).
- Add ONNX Runtime (MIT) row.
- Verify versions match what vcpkg installs.

### D8: GitHub repo setup
**Files:** none (GitHub UI / `gh` CLI operations)
**Acceptance:**
- Repo public (verify settings).
- Default branch: `main`.
- Branch protection on `main`: require PR review (or solo-maintainer self-merge with status checks).
- GitHub Actions CI workflow (`.github\workflows\ci.yml`): build on Windows MinGW, run ctest, comment status on PRs.
- GitHub Actions release workflow (`.github\workflows\release.yml`): triggers on `v*` tag, builds MSI, creates GitHub Release with assets + SHA-256.
- Labels: `bug`, `enhancement`, `good first issue`, `help wanted`, `documentation`, `security`, `v1.1.0-roadmap`.
- Pre-populate issues from the v1.1.0 Batch-3/Batch-4 work in §4 (use the batch prompts as issue bodies; tag `v1.1.0-roadmap`).

### D9: Test gate + manual smoke
**Files:** none
**Acceptance:**
- Run `set QT_QPA_PLATFORM=offscreen && cd build && ctest --output-on-failure -j4` — all targets pass.
- Manual smoke:
  1. Open a 50-page PDF, edit, wait 6 minutes, verify autosave fired (`.autosave.pdf` exists + ModeStrip shows new timestamp).
  2. Sign a PDF with a test cert at PAdESLevel::B_LTA, open in Adobe Reader DC, verify green check + LTV box.
  3. Add a watermark with text containing `(` `)` `\` — open in Adobe, verify text renders as-written.
  4. Open OCRMode — verify only Tesseract is selectable.
  5. Verify `About` dialog shows the Apache-2.0 license + LICENSE-3RD-PARTY content.

### D10: Tag + build + release
**Files:** none (git + packaging operations)
**Acceptance:**
- `git tag -a v1.0.0 -m "Release v1.0.0 - open source under Apache-2.0"` (after all changes committed).
- `git push origin v1.0.0`.
- Run `cd packaging && build-msi.bat`.
- Verify `dist\GlyphPDF-1.0.0-x64.msi` exists with new build date.
- Sign the MSI with code-signing cert (if available) before distribution.
- GitHub Actions release workflow auto-creates the GitHub Release; OR manually via `gh release create v1.0.0 dist\GlyphPDF-1.0.0-x64.msi --notes-file CHANGELOG-v1.0.0-excerpt.md`.
- Submit to `winget` (`winget submit`), `chocolatey` (`choco push`), `scoop` bucket (PR to scoop-extras or maintain own bucket).
- Announce: HackerNews "Show HN: GlyphPDF — open-source privacy-first PDF workstation with PAdES B-LTA". r/PDF subreddit. r/opensource. EU privacy-focused subreddits (r/privacy, r/degoogle, r/privacytoolsio). PDF Association mailing list.

</deliverables>

<verification>
```bash
cmake --build build -- -j8
set QT_QPA_PLATFORM=offscreen && cd build && ctest --output-on-failure -j4
# All tests pass

# Verify combo disable
# Manual: launch app, switch to OCR mode, verify RapidOCR option grayed out

# Verify CHANGELOG accuracy
grep -c "v1.1" CHANGELOG.md
# Expected: at least 1 (the new RapidOCR Known Issue note)

# Tag verification
git tag --list v1.0.0
# Expected: v1.0.0

# MSI verification
ls dist/GlyphPDF-1.0.0-x64.msi
```
</verification>

<constraints>
- DO NOT remove the RapidOcrEngine source files — the implementation will land in v1.1.0.
- DO NOT hide the engine combo entirely — show the disabled options with tooltips so users see the roadmap.
- DO NOT skip a single test target — all 11+ must pass.
- DO NOT tag v1.0.0 if any test fails or if any manual-smoke step fails.
- DO NOT push to remote without explicit user confirmation if uncommitted local-only changes exist.
- SCOPE: Final release-tag prompt; coordinate, don't add new features.
</constraints>

<success_criteria>
- [ ] Build: 0 errors
- [ ] Tests: all targets pass
- [ ] OCRMode runtime-disables RapidOCR / ROVER options
- [ ] CHANGELOG has explicit v1.0.0 + Known Issues entries
- [ ] README + ROADMAP + SESSION_BRIEF_NEXT updated
- [ ] Manual smoke 4-step passes
- [ ] git tag v1.0.0 exists
- [ ] MSI built fresh
</success_criteria>

---

## 4. v1.0.0 Extended Work — Branch C scope (was "v1.1.0+ roadmap")

**Branch C decision (2026-05-26)** promotes these from "v1.1.0+ deferred" to **required v1.0.0 work**. The 5 batches below remain organized as compact launch prompts; the senior engineer reading them with the cited `file:line` anchors has enough to execute. Full 7-H expansion can be done per-session if the batch proves too large for one Claude Code instance.

Only **BATCH-1 (Cloud Sync)** stays deferred — it has been moved to §4.5 below as the sole remaining v2.0 roadmap item.

All prompts inherit the STANDARD EXECUTION PROTOCOL from §3.

---

### v1.0.0-BATCH-3 — Mode-page completions (FormBuilder / Batch / Pages / Redact / Inspector)

**ROADMAP reference:** Session 8 (forms), Session 11 (page ops)
**Effort:** L (one sprint per panel; parallelizable across 4 sessions)
**Prerequisites:** v1.0.0 shipped

**Compact launch prompt:**

```
You are completing 5 placeholder mode pages in GlyphPDF (C:\Users\User\Projects\pdf) that currently
ship as "preview banners" with every interactive child disabled. The engine implementations
underneath are already real and tested. Your job is the UI wiring + click handlers.

Read first:
  - src\modes\FormBuilderMode.cpp (preview banner + 9 tool toggles, all disabled)
  - src\modes\BatchMode.cpp (preview banner + pipeline cards + fake QtConcurrent loop)
  - src\modes\PagesMode.cpp (preview banner + hardcoded 12 fake page names + QLabel placeholder
    for "Form layout")
  - src\modes\RedactMode.cpp (preview banner + 3 pill toggles + blank PdfViewerWidget canvas)
  - src\ui\InspectorWidget.cpp lines 331-553 (Properties tab is decorative; 6 color swatches +
    6 align buttons + X/Y/W/H + contents QTextEdit all unbound)

Deliverables:
  D1: FormBuilderMode — replace preview banner with drag-and-drop field placement palette;
      remove `setEnabled(false)` loop; wire 9 field-type toggles to AddFormFieldCommand;
      hook the canvas to the user's open document (currently a new blank PdfViewerWidget).
  D2: BatchMode — replace preview banner with file-list dropper + operation selector wired to
      real ConversionManager::convertTo + IPdfEditorEngine::optimizeDocument loops.
      Replace the QThread::msleep(200) fake loop with real engine dispatch.
  D3: PagesMode — replace the QLabel "Form layout: SPLIT AT radio, ..." placeholder with real
      QFormLayout (SPLIT-AT radio buttons, N-PAGES QSpinBox, NAMING pattern QLineEdit,
      DESTINATION QFileDialog, Preview canvas, Split QPushButton wired to
      IPdfEditorEngine::extractPageAsBytes loop). Replace hardcoded 12 page names with real
      page count + names from current document.
  D4: RedactMode — replace preview banner; hook the canvas to the user's open document; wire the
      3 pill toggles to RedactCommand; implement Pattern redaction (email/phone/ID regex) behind
      the "Mark by Pattern ▾" pill (extend RedactCommand to accept QRegularExpression).
  D5: InspectorWidget Properties tab — wire color swatches to annotation.color setter; align
      buttons to AnnotationLayer geometry mutators; X/Y/W/H grid bound to annotation.rect with
      QDoubleSpinBox; opacity/blend/border bound; contents QTextEdit two-way bound to
      annotation.text; reply thread wired to AnnotationItem.replies list.
  D6: Tests — TestModeWiring (smoke test that each mode can be activated, displays correctly,
      and at least one widget responds to click).
  D7: CHANGELOG: remove the 4 preview-banner items from any v1.0.0 disclosure;
      add new entries under UI.

Constraints:
  - DO NOT touch the engine layer; UI wiring only.
  - DO NOT change the public IFormManager / IPdfEditorEngine API.
  - Run the test suite after each mode page completion; keep all targets green.
  - Context-gate at 50%; this is large enough that you may need 2 sessions.
```

---

### v1.0.0-BATCH-2 — OCR ensemble pipeline (RapidOCR real + LayoutDetector + LaneScheduler)

**ROADMAP reference:** WS1 / Session 9 expanded
**Effort:** XL (multi-sprint per ROADMAP Phase 5)
**Prerequisites:** v1.0.0 shipped

**Compact launch prompt:**

```
You are implementing the OCR ensemble pipeline that GlyphPDF's CHANGELOG and ROADMAP have promised
since v1.0.0 but ships as a stub. Reference ROADMAP.md Phase 5 / WS1 for the full architecture
(layout detector ensemble → GPU lane warm worker + CPU lane → word-level ROVER merge).

Read first:
  - src\engines\ocr\RapidOcrEngine.cpp lines 112-127 (the STUB to replace)
  - src\engines\ocr\OcrPipeline.cpp (existing ROVER merge logic — keep)
  - src\engines\ocr\OcrPreprocessor.cpp (existing Leptonica preprocessing — keep)
  - src\engines\ocr\RapidOcrEngine.cpp lines 53-91 (3 ONNX Sessions already constructed — use them)
  - ROADMAP.md Phase 5 section in full
  - Existing ONNX models in C:\Users\User\Projects\pdf\models\ (det/cls/rec for PP-OCRv5)

Deliverables:
  D1: RapidOcrEngine real PP-OCRv5 — DBNet text-detection box extraction from det session
      output → perspective transform of each box → cls session for orientation → SVTR
      recognition + CTCLoss decoding from rec session. Replace the STUB block at line 112-127.
      Add isMockImplementation() to return false now.
  D2: ILayoutDetector interface + PP-DocLayoutV2Detector + SuryaDetector (or stub Surya if license
      audit reveals GPL-3.0; see ROADMAP R6 risk). Implements layout region classification with
      reading-order index.
  D3: LaneScheduler — GPU lane with persistent warm worker (NEVER spawn-per-page per ROADMAP
      anti-pattern); CPU lane with QtConcurrent::mapped bounded by idealThreadCount; cross-page
      pipeline layout(P+1) ‖ ocr(P) ‖ fusion(P-1).
  D4: OcrDjotMapper (optional v1.1; required if WS2 Djot batch is also scheduled) — maps fused
      OCR output to SemanticDocument per ROADMAP WS2.
  D5: OCRMode UI updates — wire engine selector (now Tesseract / RapidOCR / ROVER all real),
      preprocessing options, per-word confidence coloring overlay (green ≥90 / yellow 70-89 /
      red <70), per-region redo, review-before-save workflow.
  D6: Remove the runtime gate added in v1.0.0-PROMPT-7 D2 (RapidOCR isMockImplementation now
      returns false; selector becomes fully usable).
  D7: Tests — TestOcrEnsemble (real recognition accuracy on a known test image; ROVER merge
      reduces error rate vs primary alone).
  D8: CHANGELOG: move RapidOCR + ROVER from Known Issues to Features.

Constraints:
  - DO NOT use PaddlePaddle runtime — ONNXRuntime only per ROADMAP.
  - DO NOT spawn ONNX process per page — persistent warm worker mandatory.
  - DO NOT use character-level voting — word-level ROVER only.
  - DO NOT require both engines — Tesseract alone must work.
  - CONTEXT GATE: large scope; expect 2-3 sessions.
```

---

### v1.0.0-BATCH-4 — 23 ribbon tools → real engine wiring

**ROADMAP reference:** Sessions 10, 11, 12 (cleanup + completion)
**Effort:** L-XL
**Prerequisites:** v1.0.0 shipped

**Compact launch prompt:**

```
You are wiring the 23 ribbon tools in GlyphPDF that currently show "scheduled for a future
engine update" message boxes. Each tool's QMessageBox stub is documented at the file:line
listed in C:\Users\User\Desktop\GLYPH-PDF-AUDIT-v1.0.0.md Appendix B (UI Wiring section 1).

Read first:
  - C:\Users\User\Desktop\GLYPH-PDF-AUDIT-v1.0.0.md sections 1.1-1.6 (every stubbed tool with
    file:line)
  - src\shell\controllers\ all 7 controllers
  - src\core\ToolId.h (87 entries)

Stubbed tools to wire (group by controller):
  ViewController: TwoPage, EyeCare (sepia/warm filter)
  PagesController: Split, Reorder (real engine call exists; reroute ribbon to it), Resize,
                   AddHeader, AddFooter, AddPageNumbers, BatesNumber
  ConvertController: ToHtml, ToText, Compress (ribbon; reroute to existing screen-nav path),
                     PdfToPpt (NEW — add to ConversionManager)
  FormsController: Button, SigField, AutoDetect (form field auto-detection), Tabs (tab order)
  SecurityController: Cloud (re-enable when Cloud Sync batch lands; for now keep disabled),
                      Permissions, RemoveSecurity, Certify, Timestamp,
                      PatternRedact + RegexRedact (couples to BATCH-3 D4)

Deliverables:
  D1-Dn: For each stubbed tool, write the engine call. Many engine methods already exist; some
         need new methods on IPdfEditorEngine (Compress alternative path, sepia filter,
         Permissions modify dialog, RemoveSecurity flow, Certify cert-time signing,
         Timestamp document-level timestamp without signing).
  D+: Also fix the SHOULD-FIX issues per Audit Appendix B section 13:
     - Strikeout / Squiggly silently aliased to Underline (EditController.cpp:70-71): create
       real ToolMode::Strikeout + ToolMode::Squiggly and wire AnnotationLayer to render them.
     - Presentation aliased to Fullscreen: implement real slideshow mode.
     - Share opens mailto: without attachment: use Windows MAPI (or copy file path + show user
       instructions).
     - FormsController hardcoded field placement at QRectF(100,100,...): add click-to-place flow.
  D++: Office→PDF: define HAS_LIBREOFFICE in CMake + bundle soffice OR write direct DOCX/XLSX/PPTX
       parsers using OpenXLSX + DuckX + custom PPTX (effort comparable to MRC; defer to a separate
       sub-batch if too big).

Constraints:
  - DO NOT regress any existing functional tool.
  - DO NOT add new ToolId entries; reuse what's in ToolId.h.
  - DO NOT use any subprocess for tools currently in-process (preserve privacy positioning).
  - Test after each tool wired; commit per-tool atomic.
```

---

### v1.0.0-BATCH-5 — Localization + DiffEngine LCS + WS2 Djot + AI backend + Edact-Ray + comment threading

**ROADMAP reference:** Sessions 3.5 (WS2), 10 (DiffEngine), 15 (Localization)
**Effort:** XL across multiple releases
**Prerequisites:** v1.0.0 shipped

**Compact launch prompt (split across multiple sessions):**

```
You are working on cross-cutting improvements that touch UI, engine, and i18n layers. This batch
intentionally covers multiple smaller deliverables that share no critical-path dependency.

Split into separate sessions per deliverable:

SESSION A — Localization:
  - Run `lupdate src/ -ts translations/glyphpdf_*.ts` to populate ar/fr/de .ts files with all
    tr() strings from the codebase.
  - Commission translators (vendor task; out of scope for the Claude session — leave as
    placeholder bullets in translations/README.md).
  - Per-widget RTL audit for Arabic build (verify text alignment, scroll direction, dialog
    button order — Qt's auto-RTL handles most, custom widgets may need explicit setLayoutDirection).
  - Re-enable ar/fr/de in PreferencesDialog language combo once translations land.

SESSION B — DiffEngine LCS upgrade:
  - Replace QSet<QString>::operator- at src\engines\DiffEngine.cpp:48-69 with Myers LCS diff
    (use a public-domain implementation or write per the Myers 1986 paper).
  - DiffResult gets new MoveOperation type ({removed-block, added-block, similarity-score}).
  - CompareWidget renders moves as a third color (not just add+delete).
  - Test on legal-document pair with reordered clauses.

SESSION C — WS2 Djot interchange (ROADMAP Phase 1.5):
  - Implement docmodel C++ library (SemanticDocument tree, ProvenanceTag, no Qt dep).
  - Implement pdfws_djot library (vendor Lua 5.4 reference parser; IDjotCodec encode/decode;
    IDjotMapper PDF↔SemanticDocument).
  - ProvenanceGuard rejects born-PDF + signed edit-via-Djot path.
  - Round-trip property test (1000 random SemanticDocuments).
  - This is prerequisite for some Batch 2 + Batch 4 work — schedule before those if their
    Djot dependencies are needed.

SESSION D — AI backend wiring:
  - AIChatPanel: replace canned-reply placeholder with real LLM backend.
  - Default provider: Anthropic Claude (CredentialManager.cpp:60 already supports storage).
  - Add provider selection: Anthropic / OpenAI / Gemini / Local (Ollama HTTP).
  - Stream responses; show citations to page numbers; pass current-page text as context.
  - PreferencesDialog.cpp:140-142 — un-disable OpenAI / Gemini provider entries.
  - PreferencesDialog.cpp:256-273 — "Test key" performs real API roundtrip (not format-only).
  - SCOPE CHOICE: cloud-only initially, or include local-Ollama path for privacy positioning?
    Recommend: cloud + local both; default to "ask user" on first chat.

SESSION E — Comment threading depth:
  - AnnotationItem.replies list (already exists; not surfaced).
  - CommentsWidget threaded view + filtering by status/author/date.
  - ISO 32000 /IRT linking (reply-to-annotation parent pointer).

SESSION F — Edact-Ray glyph-advance normalization (ROADMAP S7 D1):
  - Implement sum-of-advances normalization in redactCanvasRecursively.
  - Replace gap with space glyph whose width = sum of removed advances.
  - Test with crafted-width sequence (defeats character-recovery attack).

SESSION G — InspectorWidget Properties tab wiring:
  (already covered in BATCH-3 D5; can be moved here if BATCH-3 too large)

Constraints per session:
  - Each session must run tests + commit atomically.
  - Stay within scope of the one labeled session per Claude Code instance.
```

---

## 4.5 — v2.0 Roadmap (post-v1.0.0)

### v2.0-BATCH-1 — Real Cloud Sync backend

**ROADMAP reference:** PRD §9.14
**Effort:** XL (multi-sprint, requires backend architecture decision)
**Prerequisites:** Branch C v1.0.0 shipped and stabilized; Cloud Sync product/backend decision made

**Compact launch prompt:**

```
This batch requires a product decision before engineering: WHICH cloud backend? Options:
  A) Self-hosted Weaver endpoint (per AppContext.h DefaultCloudSyncEndpoint constant).
  B) S3 + custom auth (good privacy posture; needs server work).
  C) WebDAV (broad compatibility, weaker auth story).
  D) Customer-supplied backend (BYOSync — most privacy-aligned, smallest TAM).

Once decided:
  - Replace CollaborationManager stubs (pushToCloud, pullFromCloud) with real QNetworkAccessManager
    flow (POST + multipart, GET + ETag) per chosen backend.
  - Re-enable cloud ribbon entry (src\shell\Ribbon.cpp:82-86).
  - Implement Account sign-in + token management (extend CredentialManager.cpp).
  - Replace SecurityController.cpp:293-302 stub message with real flow.
  - Implement secure-share-links (PRD §9.11), document expiration / access revocation.
  - Drive ModeStrip sync indicator (currently removed in v1.0.0-PROMPT-1 D3 OR rebrand to LOCAL
    ONLY) from real cloud round-trip.
  - Version history (PRD §9.14).
  - Match the v1.0.0 positioning: "opt-in, end-to-end encrypted" — never breaks the privacy story.

This batch is NOT recommended for v1.1.0; suggest v2.0 timing to preserve focus on closing
v1.0.0 → v1.1 surface gaps first.
```

---

## 5. Positioning Recommendation (OSS edition)

Synthesized from the industry benchmark agent's report (Appendix E), adjusted for the **fully open-source under Apache-2.0** (recommended) or **MIT** release decision.

### Recommended positioning
> **GlyphPDF is the open-source, privacy-first PDF workstation. Native, fast, with bank-grade signing and verifiable redaction. Truly local — audit the code yourself. Free forever. Donate if it serves you.**

### Why this wins (OSS edition)
1. **It is true on every claim.** "Privacy-first" goes from a marketing claim to a verifiable one — anyone can read the code and confirm no telemetry, no phone-home, no cloud-required path.
2. **OSS is itself a differentiator.** Adobe, Foxit, Nitro, PDFelement, Bluebeam, PDF-XChange are all closed-source. The credible OSS competitors in the PDF tooling space are: **Sumatra** (reader-only), **Okular** (reader + annotation, KDE-heavy), **MuPDF** (AGPL — most projects can't link it), **Skim** (Mac-only academic), **qpdf** (CLI library, not editor). None of them ship a full-feature editor with PAdES B-LTA + secure redaction + OCR. GlyphPDF would be the first OSS PDF tool to match commercial editors on the security tier.
3. **The license itself is a feature for regulated buyers.** EU GDPR-conscious organizations, legal firms, government, healthcare, financial institutions — many have policies that prefer or require open-source for sensitive document handling. "You can audit the redaction code yourself" is a far stronger claim than "trust us, we redact correctly."
4. **It frames every gap as a roadmap invitation, not a missing feature.** Stubs in the UI become "first-PR opportunities." The CHANGELOG's honest disclosure becomes contributor-friendly.
5. **It is graceful as the project matures.** Optional opt-in cloud sync (v2.0) → community can run their own server. Local AI (v2.0) → community can swap models. Mobile companion (v2.0) → community can fork for their platform.

### Monetization model
**Pure OSS + optional donations (user-confirmed):**
- LICENSE: **Apache-2.0** (recommended for explicit patent grant — important for a project that touches crypto and PDF parsing where patent landmines exist) OR MIT (simpler, equally permissive for adoption).
- No commercial tier, no enterprise upsell, no dual-license, no paid support contracts.
- Donations: **GitHub Sponsors** (lowest friction; built into the repo page) and/or **OpenCollective** (better fiscal-host transparency). Donation button surfaces in `About → Support`, README, and website footer — never inline in the app's working surface.
- No CLAs required from contributors (Apache-2.0 has implicit patent grant via section 3 — no separate CLA needed unless the maintainer plans a dual-license future).
- Telemetry: opt-in only, anonymized, with clear local-disable toggle in Preferences. Default OFF.

### Distribution
- **Source:** GitHub repository (recommend `github.com/<user>/glyphpdf`), public from v1.0.0 tag day.
- **Binary releases:** GitHub Releases with signed MSI for Windows; SHA-256 + GPG signature published alongside.
- **Reader binary:** ship a **free GlyphPDF Reader** as a separate (smaller, OCR-stripped) build for viral distribution — same codebase, CMake option `BUILD_READER_ONLY=ON`.
- **Package managers:** submit to `winget` (Windows Package Manager), `chocolatey`, and `scoop` for one-line installs on Windows. Future: Homebrew for macOS port, Flatpak/AppImage for Linux port.
- **Documentation:** GitHub Pages or read-the-docs hosted; user manual + developer docs + ROADMAP all accessible without the repo clone.

### Marketing-message ladder for v1.0 launch
- **One-liner:** "Open-source PDFs. Done right. Locally."
- **Sub-line:** "Native Windows PDF editor with PAdES B-LTA signing and verifiable redaction. Free, Apache-2.0, no cloud, no AI, no subscription, no telemetry."
- **Three-pillar features:** "Sign / Redact / Edit — all without uploading a single page. All in source you can audit."
- **Proof points:** Apache-2.0 license. PAdES B-LTA verified in Adobe Reader. Edact-Ray-defended redaction (Branch B target). Audited code on GitHub.
- **Contribution invitation:** "Want a feature that's not in v1.0? The roadmap is in CHANGELOG; the issues are open; PRs welcome."

### What you CAN now claim (couldn't before)
- **"Open source under Apache-2.0"** — the most credible privacy proof there is.
- **"Audit the redaction code yourself"** — a claim no commercial competitor can make.
- **"No vendor lock-in"** — explicit per the license.
- **"Community-extensible"** — once contributions land, this becomes self-reinforcing.

### What still NOT to claim
- "The Adobe alternative" — too generic; positioning above is sharper.
- "All-in-one PDF tool" — implies feature parity with Adobe; you'll lose this comparison.
- "AI-powered PDF editor" — you don't have AI in v1.0; cloud AI is contra-positioning.
- "The fastest PDF editor" — Sumatra owns this perception among OSS readers.
- "Enterprise-ready" until SSO + admin + DLP land (v2.0+).
- "Production-tested" until the v1.0.0 SHIP-FAST blocker fixes ship and at least one external code audit lands.

---

## Appendices

The following sections are the full agent reports embedded verbatim. They contain detailed file:line evidence that backs every claim in §0-5.

- **Appendix A:** Architecture & Features (code-archaeologist; 757 lines)
- **Appendix B:** UI Wiring & Mock-Data (general-purpose UI agent; 426 lines)
- **Appendix C:** C++ Code Quality & Security (general-purpose code-quality agent; 811 lines)
- **Appendix D:** v1.0 Gap Analysis & Decision Tree (planning-prd-agent; 587 lines)
- **Appendix E:** Industry Benchmark & Positioning (research-specialist; 473 lines)

---

## Appendix A — Architecture & Features (synthesized findings)

12-section structural map covering: layered architecture with ASCII dependency graph; per-directory inventory of 193 source files across `app/`, `core/`, `engines/`, `commands/`, `shell/`, `modes/`, `ui/`, `util/`; 12-interface class catalogue with implementation counts (`IPdfPage` has zero implementations — orphan); engine-by-engine reality check:

- **PoDoFoBackend** — real at ~1700 lines, deepest single file in codebase. `loadDocument`/`saveDocument`/`metadata`/`editTextInline` all real. `redactCanvasRecursively` (~1134-1291) tokenizes content stream + normalizes glyph advances per Edact-Ray defense. `sanitizeDocument` has 22 vectors (matches "15+" claim). `writeUpdate` falls through to full save with `qCritical` (honest disclosure).
- **PdfiumBackend** — real, refcounted `FPDF_InitLibrary`. `renderPage`/`renderTile`/`searchText`/`extractText` real. Mutex-protected.
- **QpdfBackend** — real, conditional on `HAS_QPDF`. `linearize`/`repair`/`isLinearized`/`inspectJson` all real with `#else` fallback.
- **RenderCache** — real 3-tier LRU (256 MB cap, configurable), tiled rendering for >50MP pages, memory pressure auto-eviction. **TOCTOU race** at lines 117-130 + 164-178 (Blocker B7).
- **SignatureManager** — real ~782 LOC, full PAdES B-B/B-T/B-LT/B-LTA. **Three self-flagged defects** at lines 323-327, 585-588, 752-768 (Blockers B3/B4/B5). OpenSSL `OCSP_request_new`, CMS_encrypt for cert encryption, DSS/VRI/DocTimeStamp construction all present.
- **FormManager** — real 600 LOC, AcroForm CRUD, RadiosInUnison (PDF bit 25), MultiSelect (bit 21), FDF + CSV import/export, flatten.
- **CollaborationManager** — `pushToCloud`/`pullFromCloud` are intentional stubs returning `true` (disclosed in `Ribbon.cpp:82` ribbon-disable + `SecurityController.cpp:293-302` user dialog).
- **RapidOcrEngine** — `processImage` body is STUB at line 112-127 returning hardcoded `OcrResult{ text: "RapidOCR_Mock", boundingBox: QRectF(0,0,100,20), confidence: 85 }`. 3 ONNX sessions correctly constructed and warm-reused (compliant with anti-spawn-per-page rule).
- **OcrEngine + OcrPipeline + OcrPreprocessor** — real Tesseract path with Leptonica preprocessing, word-level ROVER merge with IoU > 0.5 alignment.
- **UpdateChecker** — real (Session 18), JSON manifest, SHA-256 verification, HTTPS-only enforcement.
- **TempFileManager** — real singleton with atexit cleanup.
- **CredentialManager** — real Windows `wincred` wrapper.

7 ribbon controllers with full `ToolId` mapping (HomeController 9 tools all real; ViewController 11 tools 9 real + 2 stubs; EditController 23 tools all real; PagesController 12 tools 6 real + 6 stubs; ConvertController 9 tools 5 real + 3 stubs + Linearize/PdfA real; FormsController 12 tools 8 real + 4 stubs; SecurityController 17 tools 11 real + 6 stubs + Cloud explicitly disabled).

11 mode-panel inventory (4 preview banners disclosed: BatchMode/PagesMode/FormBuilderMode/RedactMode; 3 fully-decorative: AIChatPanel/SignaturesPanel/PdfAValidationPanel; 4 real: ToolMode/OCRMode-with-caveats/CompareMode/CompressDialog/WatermarkDialog).

CMake build system: `HAS_PDFIUM`/`HAS_QPDF`/`HAS_TESSERACT`/`HAS_RAPIDOCR`/`HAS_OPENXLSX`/`HAS_DUCKX` flags. MuPDF FATAL_ERROR guard at lines 43-45 (Poppler symmetric guard missing — Blocker B9).

12 test executables: UnitTests, TestInterfaces, SmokeTest, TestSanitization, TestSignatureValidation (mock-only — gap acknowledged), TestRedaction, TestThreadSafety (brittle assertion `f1 || f2`), TestEncryption, TestResourceLimits (all-QSKIP), TestControllers, TestIntegration, TestPerformance. CMake guards each behind `if(EXISTS ...)`.

Orphan inventory: `IPdfPage` interface has no implementations. `RedactMode.cpp` CMake/filesystem mismatch flagged.

Provenance: 8 recent commits trace the v1.0.0 ship through Sessions 1-11 (commits a5d9ec9 → ad9c1ab → 9a9ef37).

---

## Appendix B — UI Wiring & Mock-Data (synthesized findings)

**86 ToolId entries audited.** ~46 (53%) route to real engine calls. The rest break down:

**23 ribbon tools (27%) fall through to "scheduled for a future engine update" `QMessageBox` calls:**

| Controller | Stubbed tools | File:line |
|---|---|---|
| ViewController | `TwoPage`, `EyeCare` | [ViewController.cpp:58-71](C:\Users\User\Projects\pdf\src\shell\controllers\ViewController.cpp:58) |
| PagesController | `Split`, `Reorder` (engine works via thumbnail drag, ribbon button stubs), `Resize`, `AddHeader`, `AddFooter`, `AddPageNumbers`, `BatesNumber` | [PagesController.cpp:65-81](C:\Users\User\Projects\pdf\src\shell\controllers\PagesController.cpp:65) |
| ConvertController | `ToHtml`, `ToText`, `Compress` (ribbon path stubs; screen-nav path real — discoverability conflict) | [ConvertController.cpp:55-60](C:\Users\User\Projects\pdf\src\shell\controllers\ConvertController.cpp:55) |
| FormsController | `Button`, `SigField`, `AutoDetect`, `Tabs` | [FormsController.cpp:61-67](C:\Users\User\Projects\pdf\src\shell\controllers\FormsController.cpp:61) |
| SecurityController | `Cloud` (disclosed with explicit "Do NOT pretend it works" comment), `Permissions`, `RemoveSecurity`, `Certify`, `Timestamp`, `PatternRedact`, `RegexRedact` | [SecurityController.cpp:80-87, 293-330](C:\Users\User\Projects\pdf\src\shell\controllers\SecurityController.cpp:80) |

**2 tools silently aliased (SHOULD-FIX):** `Strikeout`/`Squiggly` both map to `ToolMode::Underline` at [EditController.cpp:70-71](C:\Users\User\Projects\pdf\src\shell\controllers\EditController.cpp:70) — ribbon button lies. `Presentation` aliased to `Fullscreen` (no real slideshow).

**4 mode pages ship as "preview banners" with every interactive child `setEnabled(false)`:**
- `BatchMode` ([BatchMode.cpp:28-167](C:\Users\User\Projects\pdf\src\modes\BatchMode.cpp:28)) — banner + hardcoded pipeline cards + a fake `QtConcurrent` loop that just sleeps `5 × 200ms` per file (queue is empty anyway, Run button disabled).
- `PagesMode` ([PagesMode.cpp:25-84](C:\Users\User\Projects\pdf\src\modes\PagesMode.cpp:25)) — banner + 12 hardcoded fake page names + `QLabel` placeholder for "Form layout".
- `FormBuilderMode` ([FormBuilderMode.cpp:23-62](C:\Users\User\Projects\pdf\src\modes\FormBuilderMode.cpp:23)) — banner + 9 tool toggles + blank `PdfViewerWidget` canvas (not connected to user's document).
- `RedactMode` ([RedactMode.cpp:23-81](C:\Users\User\Projects\pdf\src\modes\RedactMode.cpp:23)) — banner + 3 pill toggles + blank canvas. Real redaction still works from ribbon's `MarkRedact → ApplyRedact` path.

**3 panels fully decorative:**
- `AIChatPanel` ([AIChatPanel.cpp:55-79](C:\Users\User\Projects\pdf\src\modes\AIChatPanel.cpp:55)) — canned reply `"AI: (v1.1) AI responses will appear here once real LLM calls are wired."` on every Send. Three tabs (`CHAT`/`SUMMARY`/`SEARCH`) not wired. Suggestion buttons have no `connect()`.
- `SignaturesPanel` ([SignaturesPanel.cpp:43-89](C:\Users\User\Projects\pdf\src\modes\SignaturesPanel.cpp:43)) — hardcoded `"Elie Matta / GlobalSign CA / EXPIRES 2027-03-14 / FINGERPRINT A8:F2:31:8E:…"` plus fake `"✓ VALID"` badge. Place Signature button has no `connect()`. Real signing is via ribbon's `Sign` tool.
- `PdfAValidationPanel` ([PdfAValidationPanel.cpp:56-83](C:\Users\User\Projects\pdf\src\modes\PdfAValidationPanel.cpp:56)) — hardcoded `"47 RULES · 2 FAILURES · 3 WARNINGS"` + 5 fake rule violations + 3 dead action buttons.

**Other mock surfaces:**
- `ThumbnailSidebar` ([ThumbnailSidebar.cpp:229-269](C:\Users\User\Projects\pdf\src\ui\ThumbnailSidebar.cpp:229)) — paper-shaped widgets with dummy text-block widgets ("`// Fake content blocks on paper`" comment). Class never calls `renderPage()`. Users cannot navigate visually.
- `OCRMode` ([OCRMode.cpp:219-300](C:\Users\User\Projects\pdf\src\modes\OCRMode.cpp:219)) — entire fake "Q4 Performance" financial report HTML mock-up with `$2,418M`, `14.2%`, hardcoded "WORD #13 / CONFIDENCE 64%" regardless of real document.
- `CompareMode` ([CompareMode.cpp:29](C:\Users\User\Projects\pdf\src\modes\CompareMode.cpp:29)) — hardcoded `"Q4-Report-v1.pdf ↔ Q4-Report-v2.pdf"` header; Prev/Next/Close buttons `setEnabled(false)` with tooltip "Coming in v1.1". DiffEngine itself runs.
- `Sidebar` Files tab ([Sidebar.cpp:100-121](C:\Users\User\Projects\pdf\src\shell\Sidebar.cpp:100)) — double-click writes a placeholder text file (`"[Metadata Payload: ...]"`) instead of the real attachment bytes. Active deception — Blocker B-borderline.
- `InspectorWidget` Properties tab ([InspectorWidget.cpp:331-553](C:\Users\User\Projects\pdf\src\ui\InspectorWidget.cpp:331)) — almost entirely decorative: 6 color swatches, 6 align buttons, opacity/blend/border rows, X/Y/W/H grid, contents `QTextEdit` all unbound.

**Autosave + sync reality:**
- No autosave timer. Only `QTimer` near save logic is the 500ms annotation-sidecar debouncer (not PDF).
- `[ModeStrip.cpp:128]` shows `● AUTOSAVED · HH:MM:SS` but updates only on `dirtyChanged(false)` (i.e. on manual save).
- `[ModeStrip.cpp:142-150]` `⤺ SYNCED · v.X` uses fake hash: `unsigned int hash = 0; for (QChar c : path) hash = hash * 31 + c.unicode(); int ver = (hash % 50) + 1;`.

**i18n:** `translations/glyphpdf_{ar,fr,de}.ts` files are 6-line empty XML shells. Loader path is real and RTL flip works for Arabic, but zero strings translated.

**FindBar regex:** Honored only for Comments/Bookmarks scopes, not Document text scope ([EditController.cpp:117-118](C:\Users\User\Projects\pdf\src\shell\controllers\EditController.cpp:117)). Redact-All is literal-only (no regex/pattern flag forwarded).

**StatusBar audit (13 cells):** 11 real, 2 hardcoded (`OCR · EN`, `UTF-8`).

**Disclosed-vs-undisclosed split:** Most placeholder UI is HONESTLY disclosed via banners, "future release" dialogs, disabled controls, CHANGELOG entries, and even explicit code comments like `"v1.0.0: Cloud Sync backend is a stub. Do NOT pretend it works"` ([SecurityController.cpp:293-296](C:\Users\User\Projects\pdf\src\shell\controllers\SecurityController.cpp:293)). The few **undisclosed** surfaces (autosave label lie, fake sync indicator, fake thumbnails, fake attachment extraction, decorative SignaturesPanel/PdfAValidationPanel, RapidOCR Mock UI exposure) are the ones surfaced as Blockers B1/B11 + the borderline Files-tab attachment.

---

## Appendix C — C++ Code Quality & Security (synthesized findings)

**Severity totals: 6 CRITICAL · 14 HIGH · 18 MEDIUM · 11 LOW = 49 findings**

### Top release-blockers (all surfaced as B3-B10 in §1)

1. **Autosave label lies** ([`ModeStrip.cpp:71-204`](C:\Users\User\Projects\pdf\src\shell\ModeStrip.cpp:71)) — no `QTimer` drives `saveDocument`. The 2-second debounce timer at [`PdfViewerWidget.cpp:48`](C:\Users\User\Projects\pdf\src\ui\PdfViewerWidget.cpp:48) flushes annotation JSON only.
2. **PAdES B-LT VRI key wrong** — `[SignatureManager.cpp:323-327]` self-flagged `TODO(audit-2026-05-23)`. Code: `QByteArray rawContents = QByteArray::fromHex(sigContentsHex); QByteArray vriKey = QCryptographicHash::hash(rawContents, SHA1).toHex().toUpper();` — ETSI EN 319 132 requires SHA-1 over **raw `/Contents` byte stream as written**, not hex-decoded inner bytes.
3. **CMS_verify with no trust policy** — `[SignatureManager.cpp:752-768]`. `X509_STORE_set_default_paths` + `CMS_verify(..., CMS_DETACHED | CMS_BINARY)` with no `X509_VERIFY_PARAM`, no signing-time check, no EKU enforcement, no revocation check. Code's own `qWarning` admits *"PAdES long-term validity is structurally weaker than claimed."*
4. **OCSP embedded unverified** — `[SignatureManager.cpp:585-588]`. Self-warned: *"OCSP response embedded into DSS without `OCSP_basic_verify`. Trusts responder solely on TLS."* MITM/malicious responder can inject forged "good" status.
5. **Content-stream injection** — `[PoDoFoBackend.cpp:2259]` (`wm << "(" << text << ") Tj\n"`) and `[:2142]` (`"(" + anno.text.toStdString() + ") Tj"`). User-controlled strings written raw with no escaping of `(`/`)`/`\`. PoC: `Hello) Tj 1 0 0 1 100 200 cm /XObject Do (` injects attacker-controlled operators.

### Additional CRITICAL findings

6. **RenderCache TOCTOU race** — `[RenderCache.cpp:117-130 + 164-178]`. Read lock dropped before write lock for LRU update; concurrent eviction corrupts `m_totalBytes`.
7. **encryptWithCertificate UAF + wrong design** — `[PdfEditorEngine.cpp:345-361]`. `BIO* inBio` freed at line 349, then passed again to `i2d_CMS_bio_stream` at line 361. Output written as `.pdf` is a raw CMS envelope (not a valid PDF with /Recipients per PDF 2.0 §7.6.4).
8. **RapidOcrEngine hardcoded stub** — `[RapidOcrEngine.cpp:112-127]`. Returns `OcrResult{text: "RapidOCR_Mock", confidence: 85}` for every page when selected.

### Additional HIGH findings

- Raw `new` + `dynamic_cast` leak path at `[BackendRouter.cpp:18-21]` + `[PdfEditorEngine.cpp:79-94]` (latent; dynamic_cast always succeeds today).
- Dead-code router methods `rendererFor`/`writerFor` leak on every call (but never called — dead).
- `i2d_X509`/`i2d_PrivateKey` second-call writes with no `len > 0` validation at multiple sites (163-166, 525-528, 172-179).
- `BIO_new_mem_buf` failure unhandled at 149-150, 318, 743, 751.
- No `Q_DISABLE_COPY_MOVE` on QObject subclasses (DocumentSession, CollaborationManager, ConversionManager, CredentialManager, UpdateChecker) — move-slicing risk.
- `QNetworkAccessManager` stack-local in `httpPost` helpers — child reply destroyed before deferred deletion; queued signals from late SSL errors crash.
- Modal `QEventLoop::exec()` for TSA/OCSP/tessdata HTTP — reentry time bomb; signing blocks GUI for up to 30s × 2.
- `OcrEngine::initialize` lazy-called from `processImage` on main thread → silent failure forever after first attempt.
- PDFium global init refcount mutex-protected but PDFium API is single-threaded; two `PdfiumBackend` instances can race inside PDFium's font cache.
- `TestThreadSafety` assertion `f1 || f2` proves nothing about data races.
- B-LTA TSA token buffer 16KB may truncate multi-cert chains — silent B-LTA breakage.
- ByteRange validation accepts overlapping ranges at `[SignatureManager.cpp:697-716]` — shadow-attack defense gap.
- Redaction's image-bbox intersection uses text-cursor position instead of CTM-derived bbox (both directions wrong) at `[PoDoFoBackend.cpp:1078-1085]`.
- Redaction abort does not roll back partial structure-tree mutations.
- Redaction audit-log SHA-256 of "after" state is computed against in-memory snapshot, not file-as-saved — log is misleading.
- Sanitize document does NOT clear AcroForm `/XFA` entry — full XML payload retained.
- Sanitize trailer `/ID[0]` (original ID) NOT scrubbed (only `[1]` randomized).
- Many `PoDoFoBackend` methods swallow ALL exceptions silently via `catch (...)` — diagnostic context lost.
- `extractSignatureContentsHex` swallows exceptions returning empty → user thinks B-LT, gets B-T silently.
- Missing `-Wpedantic`, `-fstack-protector-strong`, `-D_FORTIFY_SOURCE=2`, sanitizer build target.

### Additional MEDIUM (selected)

- `STACK_OF(X509)` API misuse in `loadP12` cleanup.
- `OcrEngine::processImage` `delete[] word` after `QString::fromUtf8` — leak on bad_alloc throw.
- `QtConcurrent::run` lambda captures `this` raw — UAF if cache destroyed mid-task.
- Glyph-advance normalization assumes single SPACE replacement; may fail on CJK/non-unit-font-scale documents.
- `deleteObjectAt` uses 10×10pt redaction rect at click point — coarse, fires full redaction-log machinery.
- `rewriteImageMatrix` does string-search on binary content stream — first-match wrong abstraction.
- `encryptDocument` AESV3R6 has no minimum password length check.

### Positives confirmed

- **Redaction content-stream surgery IS real.** `redactCanvasRecursively` tokenizes BT/ET text blocks, identifies operators in redaction rects, normalizes glyph advances per Edact-Ray. Aborts (returns false) rather than falling back to visual-overlay if surgery fails.
- **MuPDF FATAL_ERROR guard present** at `[CMakeLists.txt:43-45]`.
- **Sanitize has 22 vectors** (matches "15+" claim): trailer /Info, catalog /Metadata + /PieceInfo + /MarkInfo + /OutputIntents, EmbeddedFiles, JavaScript, OpenAction, /AA, StructTreeRoot Alt/ActualText/E, /OCProperties, /Outlines, /Collection, page /AA + /A + /PieceInfo + /Thumb + /Metadata, annot /Contents + /RC, RichMedia/Screen/Movie, AcroForm /V + /DV, trailer /ID[1] randomize. (BUT `qpdf_set_static_ID(pdf, 1)` later defeats trailer /ID — Blocker B10.)
- **SHA-256 only in signing path** (no MD5/SHA-1 except VRI key which is spec-required SHA-1).
- **ONNX session reuse** in RapidOcrEngine (3 `Ort::Session` warm-kept) — compliant with anti-spawn-per-page rule.
- **Word-level ROVER** in OcrPipeline (compliant with anti-char-level-vote rule).
- **qpdf-in-signing-path correctly guarded** at `[PdfEditorEngine.cpp:166-185]` — skipped when signatures present.

---

## Appendix D — v1.0 Gap Analysis & Decision Tree (synthesized findings)

**Verdict:** CHANGELOG-declared v1.0.0 is credible with focused remediation. Headline framing: GlyphPDF is a real v1.0.0 with one true ship-blocker (autosave timer) and one code-self-flagged correctness defect (DSS/VRI hex round-trip) when viewed purely through the PRD lens. The code-quality audit (Appendix C) expanded this to 11 blockers by surfacing security defects in implemented code.

### PRD §9.1-9.17 reality matrix (~85 rows synthesized)

| PRD Section | Status |
|---|---|
| §9.1 Viewing | DONE (PdfiumBackend + RenderCache; smooth, themed) |
| §9.2 Text/object editing | DONE (editTextInline + image ops all wired) |
| §9.3 Annotation | DONE (highlight/underline/notes/stamps/shapes all real; comment threading PARTIAL) |
| §9.4 OCR | PARTIAL (Tesseract real; RapidOCR Mock; ensemble pipeline absent — DOCUMENTED-FUTURE per CHANGELOG) |
| §9.5 Conversion | PARTIAL (PDF→Word/Excel/HTML/CSV real; PowerPoint MISSING; Office→PDF MISSING (HAS_LIBREOFFICE never defined); images→PDF MISSING) |
| §9.6 Forms | DONE (all field types + FDF/CSV import/export + flatten) |
| §9.7 E-signatures | DONE with B3 self-flagged defect (PAdES B-LTA real; remote send-for-signing DOCUMENTED-FUTURE) |
| §9.8 Redaction | DONE (real content-stream excision + glyph normalization); pattern redaction DOCUMENTED-FUTURE |
| §9.9 Page management | DONE (all ops + headers/footers/Bates wired) |
| §9.10 Comparison | PARTIAL (DiffEngine uses set-difference not LCS — disclosed in CHANGELOG) |
| §9.11 Security | DONE (AES-256, cert encryption B8-blocked, watermarks, sanitization 22 vectors) |
| §9.12 Batch | PARTIAL (BatchMode shipped as preview banner — DOCUMENTED-FUTURE) |
| §9.13 Compression/optimization | DONE (estimate + apply); MRC DOCUMENTED-FUTURE |
| §9.14 Cloud/sync | DOCUMENTED-FUTURE (fully disclosed via ribbon disable + dialog + CHANGELOG) |
| §9.15 Accessibility | DONE (59+ accessible name/desc; F6/F1; tab order; HC theme) |
| §9.16 Search/navigation | DONE (FindBar + BookmarkPanel + status bar nav); thumbnails are fake placeholders (Appendix B) |
| §9.17 File I/O | DONE for PDF; PARTIAL for Office round-trip (export only, no import) |

**Non-functional gap matrix (PRD §12/13/14/15):**
- **§12 Performance** — DONE except: "Autosave must not interrupt reading" → MISSING (no background timer). Blocker B1.
- **§13 Reliability** — DONE except: "Autosave after meaningful changes" + "Crash recovery for unsaved work" → MISSING. Blockers B1+B2.
- **§14 Security/Privacy** — DONE except: DSS/VRI hex round-trip code-smell self-flagged. Blocker B3.
- **§15 Accessibility/Localization** — Framework DONE; ar/fr/de .ts content empty (DOCUMENTED-FUTURE).

### ROADMAP Sessions 1-20 reconciliation

SESSION_BRIEF (2026-05-21) said Sessions 7-11 code present but depth unverified, and Sessions 12-20 unstarted. Reality (verified by source reads):

| Session | Brief said | Reality |
|---|---|---|
| 1-6 | Done | MATCHES (Bootstrapper, ToolRegistry, IPdfBackend+PoDoFoBackend, PDFium+RenderCache, qpdf, PAdES B-LT/B-LTA — all real) |
| 3.5 | Unstarted (Djot/WS2) | MATCHES (grep `docmodel|Djot|SemanticDocument` = 0 files) |
| 7 | Pending (redaction hardening) | MOSTLY DONE — D2/D3 done, D4 audit log done, D5 22 vectors confirmed, D6 partial; D1 Edact-Ray glyph normalization implemented per source read |
| 8 | Forms + WS2 | DONE minus WS2 (Djot annotation model absent) |
| 9 | OCR ensemble + WS1 | PARTIAL (Tesseract real; RapidOCR STUB; LayoutDetector/LaneScheduler/Surya absent) |
| 10 | Conversion + WS2 | PARTIAL (most exports real; PPT/Office-import missing; DiffEngine set-diff; WS2 absent) |
| 11 | Page ops | MATCHES |
| 12 | Unstarted (find/replace/bookmark) | DONE — work landed (BRIEF outdated) |
| 13 | Unstarted (watermark + MRC + WS3) | PARTIAL (watermark + standard compression real; MRC absent) |
| 14 | Unstarted (accessibility) | DONE |
| 15 | Unstarted (localization) | PARTIAL (framework done, content empty) |
| 16 | Unstarted (error handling + recovery) | PARTIAL — ErrorInfo/ErrorDialog real; D2 crash recovery + autosave MISSING (Blocker B1+B2) |
| 17 | Unstarted (WiX MSI) | DONE — `dist\GlyphPDF-1.0.0-x64.msi` exists |
| 18 | Unstarted (auto-update) | DONE — UpdateChecker.cpp real (282 LOC) |
| 19 | Unstarted (print polish) | DONE minus veraPDF subprocess gate |
| 20 | Unstarted (integration tests + v1.0 freeze) | DONE — TestIntegration + TestPerformance exist; CHANGELOG dated 2026-05-23 |

**Net unlanded ROADMAP scope:** Session 3.5 / WS2 Djot, Session 9 / WS1 OCR ensemble, Session 13 / WS3 MRC compression, Session 16 D2 crash recovery autosave timer, Session 19 veraPDF subprocess.

### Recommended scope cuts for v1.0.0 credibility

Keep these explicitly as v1.1.0+ in release notes:
- Full enterprise mode §11 (SSO/DLP/admin dashboards)
- Cloud sync §9.14 (already disclosed)
- RTL Arabic per-widget audit
- Pattern redaction (already disclosed)
- Send-for-signing workflow (already disclosed)
- OCR ensemble (already disclosed; add UI gate per Blocker B11)
- MRC compression (already disclosed)
- Mobile + Web companion §17
- Real-time co-authoring (already out of scope per PRD §7.2)

### Final blocker reconciliation (gap-analysis view vs code-quality view)

The gap-analysis agent saw 3 PRD-grounded blockers (B1 autosave, B2 crash recovery, B3 DSS/VRI hex). The code-quality agent expanded this to 11 by surfacing security defects in implemented C++ that the PRD doesn't explicitly mandate but that compromise the security claims GlyphPDF DOES make. The full §1 blocker list is the union. SHIP-FAST branch (§2) addresses all 11 in 10-15 working days.

---

## Appendix E — Industry Benchmark & Positioning (synthesized findings)

**Executive insight:** GlyphPDF cannot win a feature-count war against Adobe or Foxit at v1.0, and should not try. It can win on a positioning none of the top 5 competitors can credibly take from it: privacy-first, native, no cloud, no AI sending docs to a server, no subscription, bank-grade local signing, verifiable redaction.

### Competitive landscape (2024-2026) — key vendors

| Vendor | Price | Signing | Redaction | OCR | AI | Notable |
|---|---|---|---|---|---|---|
| **Adobe Acrobat Pro** | $19.99/mo subscription only | B-T, B-LT non-certified; **B-LTA gap in certified docs** (June 2025 Adobe community thread) | Vulnerable to Bland 2023 glyph-position attack | Sensei | AI Assistant extra-cost | Reference impl, but with documented gaps |
| **Foxit PDF Editor** | $11.99/mo or perpetual ~$159 | PAdES incl. LT | Standard | Built-in | AI Assistant + MCP host (Gmail/Salesforce/Jira) | **Perpetual line lost active feature dev effective Aug 2025** (security-only N/N-1) |
| **Nitro PDF Pro** | $9.99-15/mo or perpetual ~$180-250 | Yes incl. cert | Yes | Yes | None | Win-primary, weak Mac, deliberately no AI |
| **PDF-XChange Editor** | Perpetual + per-version | Yes | Yes | Yes | None | Best-in-class annotation (G2 9.2), Win-only |
| **PDFelement (Wondershare)** | Subscription + perpetual | Yes incl. cert | Yes | 98% form accuracy | Heavy AI: 92% content analysis, 30s/200-page summarization | User-friendly, telemetry concerns |
| **Smallpdf** | $15/mo Pro, free=2 conv/day | Limited | Limited | Yes | Summarize, chat | Cloud-essence, not for sensitive docs |
| **Bluebeam Revu** | $240-400/yr; **perpetual discontinued for new customers**; AI tier coming Q1 2026 | Yes | Yes | Yes | "Max" tier with AI 2026 | AEC vertical lock-in |
| **ABBYY FineReader PDF 16** | $117-165/user/yr | Yes | Yes | 99%+ claim, 200+ languages | Limited | OCR moat |
| **Sumatra PDF** | Free OSS (GPLv3) | View only | None | None | None | Fastest reader, 80% less RAM |
| **Okular** | Free OSS (KDE) | Verify only | Annotate only | None | None | Cross-platform, unusual signature-verify capability |
| **MuPDF / mupdf-gl** | AGPL or commercial | View only | None | None | None | AGPL incompatible with commercial closed-source (precisely why GlyphPDF avoids it) |

### v1.0 table stakes — non-negotiable bar

Zero tolerance: fast viewing (<3s for 100p), full-text search incl. OCR, core annotation set, page operations, basic form fill, password protection + AES-256, autosave + crash recovery, hi-DPI correctness, basic keyboard nav.

Low tolerance: basic signing (image + cert), basic export (image/PDF-A).

User-tolerable v1.0 gaps: AI (Nitro ships without it successfully), cloud sync (PRD §7.2 OOSS), mobile/web companion (PRD §17 roadmap), send-for-signing workflow, MRC compression, Bates numbering (unless legal-vertical launch ICP).

**Brutal failure modes** (deal-breakers regardless of "v1.0"): render fidelity bugs (font substitution, character drop), OCR producing gibberish silently, redaction that doesn't actually redact.

### OCR engine landscape (2025-2026)

| Engine | Architecture | Deployment | Notable 2026 datapoint |
|---|---|---|---|
| Tesseract 5 | LSTM-CNN hybrid | Local CPU | Still gold standard for ship-with-app local OCR |
| PaddleOCR PP-OCRv5 (via RapidOCR/ONNXRuntime) | 5M-param specialized | Local CPU/GPU | "Overtakes Tesseract as most-starred OCR project on GitHub" 2025 |
| Surya | VLM, layout-aware | Local GPU or slow CPU | Strong on layout; heavier weight class |
| Mistral OCR 3 (Dec 2025) | VLM | Cloud only | $2/1000 pages — 97% cheaper than Textract, double-digit lead on handwriting/tables |
| Textract / GoogleDocAI / Azure FR | Cloud only | AWS/GCP/Azure | Expensive at scale |
| ABBYY engine | Hybrid proprietary | Local or cloud | 99%+ commercial gold standard |

**GlyphPDF's stack (Tesseract 5 + RapidOCR PP-OCRv5 via ONNXRuntime + ROVER word merge) IS credible for 2026 as a privacy-first local product.** Mistral/Textract/DocAI are cloud-only and incompatible with privacy positioning. Tesseract + PaddleOCR have complementary failure modes; ROVER merge is published, peer-reviewed, and improves on either alone. RapidOCR's CPU-only ONNXRuntime packaging is correct (no PaddlePaddle 500MB+ Python dep). Surya is a nice v2 addition but not v1.0 requirement.

### PAdES B-LTA — competitive landscape

Real local-desktop B-LTA is uncommon. Adobe Acrobat has a documented certified-document B-LTA gap. Foxit/Nitro/PDF-XChange stop at B-T or B-LT with awkward manual flows. Cloud-signing services (DocuSign/Adobe Sign/Yousign) require server trust. **A correctly-implemented local B-LTA flow with DSS/VRI is a genuine differentiator** — IF the implementation is correct (must pass Adobe Reader green-check + EU LOTL validator). Most valuable to EU enterprises (eIDAS), government contractors, legal firms, financial institutions with archival obligations.

### Redaction — Edact-Ray attack (Bland 2023, PETS)

Real published attack against 11 major PDF redaction tools including Adobe Acrobat. Glyph-advance side channel allows character recovery from "redacted" PDFs. Both Adobe and Microsoft notified; **neither has patched as of most recent public reporting**. A vendor that explicitly defends (sum-of-advances normalization) makes a security claim no major competitor makes. Caveat: implementation must be correct + must not over-claim (other redaction failure modes exist: metadata, hidden layers, OCR mismatches, embedded files). Recommend third-party redaction audit before claiming publicly.

### MRC compression (PDF/A tri-layer)

Adobe gold standard. Foxit + GdPicture/Nutrient SDK + ABBYY + Internet Archive pipeline are the credible players. Important caveat: JBIG2 pattern-matching mode has famous (2013 Xerox) digit-substitution bug; German BSI banned pattern-matching JBIG2. Use lossless or symbol-distinct JBIG2 mode only. **For v1.0: not a table-stake; "expected for serious professional positioning but acceptable to defer past v1.0."** Ship basic compression (downsample + font subset + unused-object removal = easy 60%); defer full MRC to v1.1/v1.2.

### AI features

Adobe AI Assistant, Foxit AI + MCP integration, Smallpdf, PDFelement, Wondershare all ship cloud AI. Nitro deliberately skips. **Shipping without AI is not a problem if positioned correctly** as privacy-first. Most-wanted AI use cases: summarization, Q&A, smart form filling, redaction suggestion. Generally not wanted: AI image generation in PDF, AI document rewriting (fidelity nerve).

**Recommendation for GlyphPDF:** ship v1.0 with no AI. Make the absence a stated design choice. Roadmap local-only AI for v2.0 driven by user demand, not competitive pressure. Either complete AIChatPanel to local-model integration before v1.0 OR remove from v1.0 UI entirely (current canned-reply is worse than nothing).

### Qt 6 desktop polish bar (2026)

Modern-feel table-stakes: hi-DPI correctness (SVG icons, no raster scale blur), native window chrome (Mica/Acrylic on Win11), dark mode follows OS, smooth 60+fps scroll, real accessibility (Narrator/JAWS/NVDA), keyboard nav everywhere, signed installer (no "Unknown publisher" SmartScreen), auto-update with enterprise override.

2015-feel red flags: default Qt widget style, blurry icons on 4K, white-on-grey dark mode, modal-everywhere, unsigned installer.

### Distribution & monetization model (OSS edition — user-confirmed)

**License: Apache-2.0** (recommended) or **MIT**. Compatible with all current deps (PoDoFo LGPL/MPL dual, PDFium BSD-3, qpdf Apache-2, OpenSSL Apache-2, Qt LGPL-3, Tesseract Apache-2, Leptonica BSD-2, ONNXRuntime MIT, OpenXLSX BSD-3, DuckX MIT). MuPDF AGPL + Poppler GPL remain forbidden under both Apache-2.0 and MIT — Blocker B9 closes this guard.

**Apache-2.0 vs MIT decision:** Apache-2.0 recommended because it includes explicit patent-grant (§3), which matters for a project touching crypto (signatures) and PDF parsing where patent landmines exist. MIT is simpler but doesn't grant patents explicitly — a corporate contributor could later assert a patent against the project. For a security-positioned tool, Apache-2.0's defensive patent posture is worth the slightly longer LICENSE file.

**Monetization: pure OSS + optional donations.** No commercial tier, no enterprise upsell, no dual-license, no paid support contracts in v1.0.
- **GitHub Sponsors** (recommended — zero-friction; integrated into the repo page; 0% platform fee).
- **OpenCollective** (alternative — better fiscal transparency, public ledger).
- Donation surfaces: `About → Support` dialog, README "Sponsor" badge, website footer. **Never** as a popup, never inline in the app's working surface (would violate the privacy-first trust story).
- No CLAs required (Apache-2.0 §5 grants implicit patent license from contributors).
- Telemetry: opt-in only, anonymized, default OFF, clear toggle in Preferences.

### Distribution channels

- **Source:** GitHub repo (suggested `github.com/<user>/glyphpdf`), public from v1.0.0 tag.
- **Binary releases:** GitHub Releases. Signed MSI for Windows. SHA-256 + GPG signature published alongside.
- **Reader binary:** separate `GlyphPDF Reader` build (same codebase, `BUILD_READER_ONLY=ON` CMake option). Smaller footprint, OCR/signing stripped, view + annotate + verify-signatures only. Distributes virally.
- **Package managers:** `winget`, `chocolatey`, `scoop` for one-line Windows installs. Future: Homebrew (macOS port), Flatpak/AppImage (Linux port).
- **Docs:** GitHub Pages or read-the-docs hosted; user manual + dev docs + ROADMAP accessible without repo clone.

### Why OSS makes the positioning stronger

Closed-source PDF editors must ask buyers to **trust** their privacy claims. An OSS PDF editor proves them. For the privacy-conscious buyer segment (legal, medical, financial, government contractors, EU/GDPR-conscious enterprises, IT departments uncomfortable with Adobe Document Cloud), "audit the code yourself" is the most credible claim there is. None of the top 5 commercial competitors can match this — and the existing OSS competitors don't have the full-feature editor scope.

### Positioning recommendation (final — OSS edition)

> **GlyphPDF is the open-source, privacy-first PDF workstation. Native, fast, with bank-grade signing and verifiable redaction. Truly local — audit the code yourself. Free forever, Apache-2.0. Donate if it serves you.**
>
> For people who handle documents they can't afford to leak: contracts, legal filings, medical records, financial reports, R&D notes. For IT admins who can't approve cloud-bound editors. For anyone tired of "trust us" privacy claims when they could just read the source.

**Why this wins:** OSS is itself the differentiator. Adobe / Foxit / Nitro / PDFelement / Bluebeam are all closed-source. The existing OSS competitors (Sumatra, Okular, MuPDF, Skim) don't ship full-feature editors with PAdES B-LTA + secure redaction + OCR. GlyphPDF would be the first OSS PDF tool to match commercial editors on the security tier. The license itself is a buyer-trust feature for regulated industries.

### Marketing-message ladder for v1.0 launch (OSS edition)

- **One-liner:** "Open-source PDFs. Done right. Locally."
- **Sub-line:** "Native Windows PDF editor with PAdES B-LTA signing and verifiable redaction. Free, Apache-2.0, no cloud, no AI, no subscription, no telemetry."
- **Three-pillar features:** "Sign / Redact / Edit — all without uploading a single page. All in source you can audit."
- **Proof points:** Apache-2.0 on GitHub. PAdES B-LTA verified in Adobe Reader. Edact-Ray-defended redaction (Branch B target). Code audit + security policy public.
- **Contribution invitation:** "Want a feature that's not in v1.0? Roadmap in CHANGELOG; issues open; PRs welcome."

### What you CAN now claim (couldn't before)

- **"Open source under Apache-2.0"** — the most credible privacy proof.
- **"Audit the redaction code yourself"** — claim no commercial competitor can make.
- **"No vendor lock-in"** — explicit per the license.
- **"Community-extensible"** — self-reinforcing once contributions land.
- **"Bug bounty / responsible disclosure"** — credible because security researchers can see the code.

### What still NOT to claim

- "The Adobe alternative" — too generic.
- "All-in-one PDF tool" — implies feature parity.
- "AI-powered" — you don't have AI in v1.0; contra-positioning.
- "The fastest" — Sumatra/MuPDF own this perception.
- "Enterprise-ready" — until SSO + admin + DLP land (v2.0+).
- "Production-tested" — until SHIP-FAST blocker fixes ship and at least one external audit lands.

### Cited sources (17)

aiproductivity.ai 2026 PDF roundup, Foxit August 2025 perpetual policy KB, Adobe Community June 2025 B-LTA thread, Bland 2023 PETS paper + Edact-Ray arXiv 2206.02285, Argelius Labs + Redactable.com 2025 redaction analyses, abit.ee PaddleOCR/Tesseract 2025 coverage, byteiota Mistral OCR 3 launch, MarkTechPost Top 6 OCR 2025, RapidOCR GitHub project, BetaNews Foxit AI 2025-12-22, Bluebeam pricing page, Smallpdf pricing, ABBYY pricing, Nutrient PAdES guide, GdPicture MRC overview, Qt 6 Accessibility + High DPI docs.

---

*End of mega-file. Total v1.0.0 release work: ~12-25 person-days across 7 launch prompts. v1.1.0+ work organized into 5 batches.*
