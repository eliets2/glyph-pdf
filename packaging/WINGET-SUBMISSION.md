# winget manifests — GlyphPDF

These are the [Windows Package Manager](https://learn.microsoft.com/windows/package-manager/)
manifests for `Glyph.GlyphPDF`, version 1.0.0.

## Files

| File | Purpose |
|------|---------|
| `Glyph.GlyphPDF.yaml` | Version manifest (points to the default locale) |
| `Glyph.GlyphPDF.installer.yaml` | Installer manifest (MSI URL, SHA-256, ProductCode) |
| `Glyph.GlyphPDF.locale.en-US.yaml` | English metadata (description, license, tags) |

## Publishing to winget-pkgs

1. Build the release MSI and upload it as a GitHub Release asset at
   `https://github.com/eliets2/glyph-pdf/releases/download/v1.0.0/GlyphPDF-1.0.0-x64.msi`.
2. Put the MSI SHA-256 (from `dist/GlyphPDF-1.0.0-x64.msi.sha256`) into
   `Glyph.GlyphPDF.installer.yaml` → `InstallerSha256`.
3. Validate locally:
   ```powershell
   winget validate --manifest packaging\winget
   # optional sandbox install test:
   winget install --manifest packaging\winget
   ```
4. Submit a PR to [microsoft/winget-pkgs](https://github.com/microsoft/winget-pkgs)
   placing these files under
   `manifests/g/Glyph/GlyphPDF/1.0.0/`, or use
   [`wingetcreate`](https://github.com/microsoft/winget-create):
   ```powershell
   wingetcreate submit --token <gh-token> packaging\winget
   ```

## Per-release checklist

- Bump `PackageVersion` in all three files.
- Generate a **new** MSI `ProductCode` (keep `UpgradeCode` constant) in
  `packaging/GlyphPDF.wxs`, and mirror it into the installer manifest.
- Refresh `InstallerUrl`, `InstallerSha256`, and `ReleaseDate`.
