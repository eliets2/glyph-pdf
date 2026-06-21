# GlyphPDF v1.2.1 — Startup Crash Fix

**Release date:** 2026-06-15
**Type:** Critical patch (all v1.0.x and v1.2.0 users affected — upgrade required)

---

## Summary

v1.2.1 fixes a startup crash that affects **every install** of v1.0.0, v1.0.1, and v1.2.0
on machines where the MSYS2 ucrt64 `podofo` package is installed alongside the vendored
podofo 1.1.0 used to build GlyphPDF.

---

## Bug Fixed

### BUG-1 — CRITICAL: GlyphPDF crashes on startup with "Entry Point Not Found"

**Symptom:** `GlyphPDF.exe - Entry Point Not Found` dialog on every launch:
> The procedure entry point `_ZN6PoDoFo10PdfContentC1Ev` could not be located
> in the dynamic link library

**Root cause:** `packaging/deploy.ps1` uses an objdump dependency-closure loop to
bundle MinGW DLLs. The loop found `libpodofo.dll` imported by `GlyphPDF.exe` and
copied it from `C:\msys64\ucrt64\bin` (podofo **0.10.4**). However, `GlyphPDF.exe`
was linked against the vendored podofo **1.1.0** from `third_party/podofo/install/`.
The two versions are ABI-incompatible: `PoDoFo::PdfContent::PdfContent()` is not
exported by 0.10.4, causing the entry-point crash on load.

**Fix:** `deploy.ps1` now explicitly stages `third_party/podofo/install/bin/libpodofo.dll`
(1.1.0) before the objdump closure runs. The closure's already-present guard then skips
the ucrt64 copy. If the vendored directory is absent (builds that use the MSYS2 0.10.4
path), the closure correctly picks up the matching DLL as before.

---

## Included From v1.2.0

All five security fixes from v1.2.0 are included — see
[release-notes-v1.2.0.md](release-notes-v1.2.0.md) for full detail:

- **SECFIX-1**: OCSP test-fixture bypass removed from production builds
- **SECFIX-2**: AES-256-GCM downgrade oracle fixed
- **SECFIX-3**: TOCTOU in MSI update verifier fixed
- **SECFIX-4**: Unbounded recursion + over-broad allowlist in SignatureManager fixed
- **SECFIX-5**: OllamaProvider data-exfiltration vector restricted

---

## Installation

### Installer

1. Download `GlyphPDF-1.2.1-x64.msi`
2. Verify SHA-256:
   ```powershell
   (Get-FileHash "GlyphPDF-1.2.1-x64.msi" -Algorithm SHA256).Hash
   # Expected: A4B16DD2080FE2F6EA3E1CD81966865322EEFAB78BE31169F1319161DD7644DC
   ```
3. Run the installer — upgrades in-place from v1.0.x and v1.2.0.

---

## Files

| File | SHA-256 |
|------|---------|
| `GlyphPDF-1.2.1-x64.msi` | `A4B16DD2080FE2F6EA3E1CD81966865322EEFAB78BE31169F1319161DD7644DC` |
