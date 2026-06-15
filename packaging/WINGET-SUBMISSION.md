# winget manifests — GlyphPDF

These are the [Windows Package Manager](https://learn.microsoft.com/windows/package-manager/)
manifests for `Glyph.GlyphPDF`, version 1.2.0.

## Files

| File | Purpose |
|------|---------|
| `Glyph.GlyphPDF.yaml` | Version manifest |
| `Glyph.GlyphPDF.installer.yaml` | Installer manifest (MSI URL, SHA-256, ProductCode) |
| `Glyph.GlyphPDF.locale.en-US.yaml` | English metadata (description, license, tags) |

## Publishing to winget-pkgs

1. Verify the MSI is uploaded to the GitHub Release at:
   `https://github.com/eliets2/glyph-pdf/releases/download/v1.2.0/GlyphPDF-1.2.0-x64.msi`
2. Validate locally:
   ```powershell
   winget validate --manifest packaging\winget
   # optional sandbox install test:
   winget install --manifest packaging\winget
   ```
3. Submit a PR to [microsoft/winget-pkgs](https://github.com/microsoft/winget-pkgs)
   placing these files under `manifests/g/Glyph/GlyphPDF/1.2.0/`, or use
   [`wingetcreate`](https://github.com/microsoft/winget-create):
   ```powershell
   wingetcreate submit --token <gh-token> packaging\winget
   ```

## Per-release checklist

- [ ] Bump `PackageVersion` in all three files.
- [ ] Generate a **new** MSI `ProductCode` (keep `UpgradeCode` constant) in
      `packaging/GlyphPDF.wxs`, and mirror it into the installer manifest.
- [ ] Refresh `InstallerUrl`, `InstallerSha256`, and `ReleaseDate`.
- [ ] Update `ReleaseNotesUrl` and `ReleaseNotes` in the locale manifest.

## Version history

| Version | ProductCode | Date |
|---------|-------------|------|
| 1.0.0 | `{5BA17AB1-F3AC-4A62-B02E-27FC9B77F691}` | 2026-06-10 |
| 1.0.1 | `{83AD3854-CF43-4F27-B34D-CBB42B5624AB}` | 2026-06-14 |
| 1.2.0 | `{C1F384AE-8A11-4770-B1BB-86A80F53F713}` | 2026-06-15 |
| 1.2.1 | `{70670D6B-02A3-4DB3-A7E0-6874D3376352}` | 2026-06-15 |
| 1.3.0 | `{79DAC62D-3F70-45CD-AE85-05DCE56901D7}` | 2026-06-15 |
