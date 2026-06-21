# GlyphPDF v1.3.1 — Maintenance Release

**Release date:** 2026-06-16
**Type:** Patch release (backward-compatible with v1.3.0)

---

## Summary

v1.3.1 polishes the v1.3.0 feature set: it removes leftover placeholder content
from the OCR Verify screen, wires the in-app update experience end-to-end, and
gives the MSI installer a branded, professional wizard.

---

## What's New

### Fixes
- **OCR Verify screen** no longer shows hardcoded demo content (the
  "Performance Overview / $2,418M" sample). It now presents a clean empty state
  until you run OCR on a real document, and clears properly afterward.

### In-app updates
- The on-launch **update check** is wired to a live manifest and is on by default
  (a one-time notice explains it; toggle under Preferences ▸ Updates).
- A polished **update dialog** replaces the inline banner buttons: it shows the
  new vs. installed version, a release-notes link, a download progress bar, and a
  primary button that walks Download → (SHA-256 + signature verified) → Install.
- **Preferences ▸ Updates** "Check Now" now respects the selected channel.
- The app version now derives from the build (fixing a stale hardcoded value).

### Installer
- A **branded MSI wizard**: a custom Welcome/Finish background and banner, plus a
  **"Launch GlyphPDF"** option on the Finish screen.

All v1.3.0 features and the v1.2.x stability/security fixes are included.

---

## Notes & limitations

- The MSI is **not yet code-signed**, so the in-app *Install & Restart* step is
  blocked by the Authenticode check; the notify → download → SHA-256 verify steps
  work, and the downloaded MSI can be run manually.
- Annotation **Erase** remains an explicit "not yet implemented" placeholder.

---

## Installation

### Installer

1. Download `GlyphPDF-1.3.1-x64.msi`
2. Verify SHA-256:
   ```powershell
   (Get-FileHash "GlyphPDF-1.3.1-x64.msi" -Algorithm SHA256).Hash
   # Expected: 8D96CE144A504C450E858C858B6AA8E0C54D710D3D4C6FDA6FB0D22824DF962A
   ```
3. Run the installer — upgrades in-place from v1.0.x / v1.2.x / v1.3.0.

### Portable

Download `GlyphPDF-1.3.1-x64-portable.zip`, verify its SHA-256, and extract.

---

## Files

| File | SHA-256 |
|------|---------|
| `GlyphPDF-1.3.1-x64.msi` | `8D96CE144A504C450E858C858B6AA8E0C54D710D3D4C6FDA6FB0D22824DF962A` |
| `GlyphPDF-1.3.1-x64-portable.zip` | `29D15BD5E120ED1459987AD6F7F1D7183E910AC58D59B6E593131E658502D2A5` |
