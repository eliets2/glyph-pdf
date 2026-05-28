# GlyphPDF — Next Claude Code Session Brief

**Updated:** 2026-05-28 | **Branch:** main | **Head:** `a6ea6aa` + uncommitted work
**Project root:** `C:\Users\User\Projects\pdf`
**Branch C SCOPE LOCK:** real public v1.0.0 ships in ~8-9 months from M2 start (per `GLYPH-PDF-AUDIT-v1.0.0.md` on Desktop). Current `dist\GlyphPDF-1.0.0-x64.msi` is **private/internal only** — do NOT publish to any public channel until M8 launch.

---

## What has been built (committed + tested)

| Commit | Sessions | What it contains |
|--------|----------|-----------------|
| `a5d9ec9` | 1 | `AppContext` shared_ptr DI + `Bootstrapper` wiring |
| `307b09b` | 2–5 | `ToolId`/`ToolRegistry`, `IPdfBackend`/`IPdfRenderer`/`IPdfSearcher` abstractions, `PoDoFoBackend`, `PdfiumBackend`, `RenderCache` (LRU, 256 MB, tiled), `QpdfBackend`, save pipeline, repair-on-open |
| `bc00648` | 6 | `ISignatureManager` + `PAdESLevel` enum, PAdES B-LT/B-LTA, DSS/VRI, cert encryption |
| `d0cb76a` | 7–11 | Page ops (crop/resize/reorder/header-footer/Bates), forms (radio/combobox/listbox/date/numeric/FDF), OCR pipeline (Tesseract+RapidOCR+ROVER), diff/conversion engine, batch/compare modes |
| `8ddb588` | – | Untracked `Testing/Temporary/LastTest.log` (gitignore fix) |
| `ad9c1ab` | – | DLL ABI fix + qoffscreen deploy (now obsolete after MSYS2 migration — see below) |
| `1f86e3d` | – | SESSION_BRIEF doc update |
| `9a9ef37` | – | v1.0.0 pre-release checkpoint with full audit + bundled MSI |
| `a6ea6aa` | M1 (Month 1) | **Month 1 SCOPE LOCK work** — autosave + crash recovery (AutosaveManager + RecoveryDialog), SignatureManager hardening B3+B4+B5 (real trust policy, OCSP verify, VRI key fix), content-stream injection fix (PdfStringEscape), RenderCache TOCTOU race fix, encryptWithCertificate real /PubSec output, CMake Poppler+MuPDF guards, qpdf_set_static_ID removal, RapidOCR runtime gate, signing fixtures + TestSignatureRealCrypto, TestAutosave |

**12-14 test targets:** UnitTests, TestThreadSafety, TestControllers, TestRedaction, TestEncryption, TestResourceLimits, TestSanitization, TestSignatureValidation, TestInterfaces, SmokeTest, TestIntegration, TestPerformance, TestAutosave (new), TestSignatureRealCrypto (new) — pre-MSYS2 status was 12 passing + 2 failing with 0xc0000139 (UnitTests + TestControllers). **MSYS2 migration (next pending) fixes the 2 failures.**

---

## What is PENDING (next steps in dependency order)

### Phase A: MSYS2 native build migration — COMPLETE

**Status:** Executed. MSYS2 ucrt64 toolchain installed (GCC 16.1.0, Qt 6.11.0 + qt6-pdf, all deps via pacman). CMakeLists.txt, build scripts, README, CHANGELOG all updated. **Gate: ctest 14/14 passing.**

**Why ucrt64 (not mingw64):** `qt6-pdf` is not packaged for MSYS2 mingw64. All packages including qt6-pdf exist in ucrt64. UCRT is system-native on Windows 10+.

**Build environment:**
- Compiler: `C:\msys64\ucrt64\bin\g++.exe` (GCC 16.1.0)
- CMake: `C:\msys64\ucrt64\bin\cmake.exe` (4.3.3)
- Qt6: `C:\msys64\ucrt64\` (6.11.0)
- All deps: `C:\msys64\ucrt64\` via pacman

### Phase B: Months 2-8 — Branch C v1.0.0 execution

**Status:** 34 prompts written at `C:\Users\User\Desktop\GLYPH-PDF-MONTHS-2-8-PROMPTS.md`.

Sprint cadence per audit SCOPE LOCK:

| Month | Sprint focus | Prompts | Weeks |
|---|---|---|---|
| **M2** | Security tier completion + LaneScheduler infrastructure (WS1 dep) | 5 prompts | 5-6 |
| **M3** | Mode-page completions (FormBuilder, Batch, Pages, Redact, Inspector) | 5 prompts | 4 |
| **M4** | 23 ribbon tools wired + WS2 Djot foundation (Phase 1.5) | 7 prompts | 5-6 |
| **M5** | OCR ensemble (WS1 layout+OCR) + Office import + WS2 OCR→Djot mapping | 4 prompts | 5 |
| **M6** | DiffEngine LCS + i18n + AI + WS2 Djot annotation rich text + comments + edge cleanup | 7 prompts | 5 |
| **M7** | Hardening (third-party audit + bug bash) + WS3 MRC compression pipeline | 3 prompts | 6 |
| **M8** | Launch (marketing, governance, MSI sign, package-manager submit, announce) | 3 prompts | 4 |
| **Total** | | **34 prompts** | **34-36 weeks** |

**Three workstreams integrated:**
- **WS1** OCR ensemble — M2 LaneScheduler infrastructure → M5 layout detector ensemble + cross-page pipelining
- **WS2** Djot interchange — M4 foundation (Phase 1.5: docmodel + pdfws_djot libs + ProvenanceGuard + round-trip test) → M5 OCR→Djot mapping → M6 annotation rich text
- **WS3** MRC compression — M7 (JBIG2 foreground + JPEG2000 background + WS1 word-box sandwich text → PDF/A-2b)

---

## How to start the next session

1. **First — execute MSYS2 migration** (Phase A). Open a fresh Claude Code session rooted at `C:\Users\User\Projects\pdf`. Paste `C:\Users\User\Desktop\GLYPH-PDF-MSYS2-MIGRATION.md`. Wait 6-10 hours (mostly pacman downloads + clean rebuild).
2. **Verify 14/14 ctest passing** after migration.
3. **Then start M2**, beginning with the smallest infrastructure prompt: M2-PROMPT-6 (CMake guards is already done in M1; start M2 with the security tier work — see `GLYPH-PDF-MONTHS-2-8-PROMPTS.md` for the dependency-aware order).
4. **Dependency-aware execution order** (NOT prompt number order):
   - M2: M2-PROMPT-1 (Edact-Ray) → M2-PROMPT-2 (OCR scrub) || M2-PROMPT-3 (veraPDF) || M2-PROMPT-4 (real-crypto coverage) || M2-PROMPT-5 (LaneScheduler infra) — last 4 parallel.
   - M3: all 5 prompts can run in parallel (different files).
   - M4: M4-PROMPT-1 through M4-PROMPT-6 parallel by controller; M4-PROMPT-7 (Djot foundation) runs in parallel — it's a new library, no UI coupling.
   - M5: M5-PROMPT-1 (RapidOCR real) → M5-PROMPT-2 (LayoutEnsemble — needs M2-PROMPT-5 LaneScheduler) → M5-PROMPT-4 (OCR→Djot mapping — needs M4-PROMPT-7 Djot foundation). M5-PROMPT-3 (Office import) independent.
   - M6: most prompts independent; M6-PROMPT-4 (Djot annotation rich text) needs M4-PROMPT-7 + M3-PROMPT-5.
   - M7: M7-PROMPT-3 (MRC) needs M5 outputs + M4-PROMPT-7 Djot foundation. M7-PROMPT-1 (audit) + M7-PROMPT-2 (bug bash) independent.
   - M8: sequential — D5-D7-D9-D10 gates each other (full smoke + tag + sign + announce).
5. **One Claude Code session per prompt.** Don't reuse sessions. Each prompt is heavy enough to want its own clean context window.
6. **If a prompt hits the 50% context gate**, it writes STATE.md and stops. Start a new session, paste the SAME prompt + add: "Continue from STATE.md in the repo root."
7. **After every prompt completes**, run `ctest --output-on-failure -j4` to confirm no regression. Commit atomically.

---

## Build + test (MSYS2 ucrt64 native)

```powershell
# Setup PATH for MSYS2 ucrt64 native
$env:PATH = 'C:\msys64\ucrt64\bin;C:\Users\User\Projects\pdf\build;' + $env:PATH
$env:QT_QPA_PLATFORM = 'offscreen'
$env:QT_PLUGIN_PATH = 'C:\msys64\ucrt64\share\qt6\plugins'

# Build
cd C:\Users\User\Projects\pdf
cmake -B build -G "Ninja"
cmake --build build --parallel 8

# Test
cd build
ctest --output-on-failure -j4
```

Or use MSYS2 UCRT64 shell directly: `C:\msys64\ucrt64.exe` → `cd /c/Users/User/Projects/pdf` → same commands.

---

## Architecture (post-Branch C SCOPE LOCK)

```
pdfws_core (interfaces, ToolId, AppContext, commands base)
    docmodel (SemanticDocument tree, ProvenanceTag)              [M4]
    pdfws_djot (Lua-vendored Djot codec + mapper)                [M4]
    pdfws_engines (PoDoFo, PDFium, qpdf, OCR ensemble, MRC)
        scheduling/LaneScheduler (GPU + CPU + cross-page)         [M2]
        engines/mrc/MrcPageProcessor (JBIG2 + JP2K + sandwich)    [M7]
        pdfws_commands (undo commands)
            pdfws_ui (MainWindow, controllers, modes, ribbon, dialogs)
                PdfWorkstation (main.cpp + Bootstrapper)
```

Dual-Model boundary: PoDoFo/PDFium/qpdf own the Structural model (source of truth); docmodel+pdfws_djot own the Semantic model (editing/interchange); ProvenanceGuard refuses Djot save-back for born-PDF signed documents.

---

## Architectural non-negotiables (enforce every session)

- **MuPDF (AGPL-3.0): NEVER link in-process** — CMakeLists.txt:43-45 FATAL_ERROR guard
- **Poppler (GPL-2.0+): NEVER link in-process** — CMakeLists.txt symmetric guard (added M1 PROMPT-6)
- **DjVu: NEVER as output format** — import-only, gated HAS_DJVU=OFF default
- **veraPDF (AGPL-3.0): SUBPROCESS ONLY** — never link in-process
- **qpdf in the signing path: NEVER** — flattens xref, invalidates ByteRange
- **Black rectangle redaction: NEVER** — always excise from content stream + Edact-Ray glyph-advance normalization
- **Spawn-per-page ONNX process: NEVER** — use LaneScheduler persistent warm worker
- **Character-level OCR majority vote: NEVER** — word-level ROVER only
- **Djot grammar reimplementation: NEVER** — vendor the Lua reference parser (MIT)
- **JBIG2 pattern-matching mode: NEVER** — 2013 Xerox digit-substitution incident; lossless or symbol-distinct only
- **Storing raw Djot where spec requires XHTML: NEVER** — transcode on save to /RC XHTML; original Djot in /PieceInfo only
- **Edit-via-Djot-save-back on signed born-PDF: NEVER** — ProvenanceGuard refuses
- **All incremental saves must use PoDoFo `WriteUpdate`** when signatures exist
- **SHA-256 only for signature hashing** — no MD5 or SHA-1 (except VRI key spec-required SHA-1)

---

## Reference documents on Desktop

- `GLYPH-PDF-AUDIT-v1.0.0.md` — comprehensive audit + 11 release-blockers + decision tree + positioning + appendices A-E
- `GLYPH-PDF-MONTH1-REMEDIATION.md` — Month 1 closure prompt (already executed; reference for pattern)
- `GLYPH-PDF-MSYS2-MIGRATION.md` — MSYS2 native build migration (Phase A — next to run)
- `GLYPH-PDF-MONTHS-2-8-PROMPTS.md` — all 34 prompts for Months 2-8 Branch C v1.0.0 execution

Plus repo docs:
- `PRD.md` — product requirements (source of truth for features)
- `ROADMAP.md` — engineering roadmap (Sessions 1-20 + WS1 + WS2 + WS3 + Phase 1.5)
- `CHANGELOG.md` — version history + Branch C unreleased work
- `LICENSE-3RD-PARTY.md` — dependency license matrix
- `README.md` — public-facing build instructions + architecture overview

---

## Recently closed (M1 work — committed in a6ea6aa)

These items are CLOSED. The next session agent does not need to re-investigate.

1. **PAdES B-LT VRI key spec non-conformance (B3)** — `extractContentsRaw` returns raw `/Contents` bytes; VRI key = SHA-1(raw bytes); validators per ETSI EN 319 132 can now find DSS/VRI entries.
2. **CMS_verify with no trust policy (B4)** — `validateSignatures` now uses Windows system root via `CertOpenSystemStoreA` (or custom `signing/trustStorePath`); X509_VERIFY_PARAM with CRL check + SMIME-sign purpose; returns `UntrustedChain` for self-signed / untrusted certs.
3. **OCSP responses embedded unverified (B5)** — `OCSP_basic_verify` runs before DSS embedding; rejected responses degrade signature to B-T.
4. **Content-stream injection in watermark/annotation/header-footer/Bates (B6)** — `pdfEscapeLiteralString` wraps all user-controlled strings before content-stream `Tj` writes.
5. **RenderCache TOCTOU race (B7)** — single `WriteLockGuard` wraps both hit-check and LRU update at lines 148-158 + 189-199.
6. **`encryptWithCertificate` UAF + wrong output (B8)** — new PdfEncrypt delegating subclass produces real `/Filter /PubSec`; TestEncryption confirms.
7. **CMake Poppler FATAL_ERROR guard missing (B9)** — symmetric guard added; 4 detection methods (target + find_package + pkg-config) for both MuPDF + Poppler.
8. **qpdf_set_static_ID defeating trailer /ID sanitization (B10)** — removed; qpdf now generates random /ID on save, preserving sanitization.
9. **RapidOCR Mock UI exposure (B11)** — `isMockImplementation()` sentinel added; OCRMode runtime-disables RapidOCR + ROVER selector with tooltip "Available in v1.1.0".
10. **Autosave label lied + no crash recovery (B1+B2)** — `AutosaveManager` with QTimer (default 5-min interval, configurable 1-30 min via Preferences); `RecoveryDialog` scans for orphaned `.autosave.pdf` on startup.
11. **TestSignatureValidation Mock-only** — `TestSignatureRealCrypto` added with 8 cases; signing fixtures generated via `tests/fixtures/signing/generate.bat`.

These were ALL release-blockers per the audit. Closing them was the M1 milestone.

---

*End of brief. Read this + `GLYPH-PDF-AUDIT-v1.0.0.md` + the relevant prompt file before starting any next session.*
