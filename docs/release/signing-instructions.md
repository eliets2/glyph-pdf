# MSI Code-Signing Instructions — GlyphPDF v1.0.0

## Prerequisites

- Windows SDK installed (provides `signtool.exe`, typically at
  `C:\Program Files (x86)\Windows Kits\10\bin\<version>\x64\signtool.exe`)
- An EV (Extended Validation) code-signing certificate, or a standard OV certificate.
  EV is strongly recommended: eliminates SmartScreen "unknown publisher" warnings immediately.
- The certificate must be available to Windows CertStore (`/a` flag auto-selects the best
  available cert), or you can supply `/f cert.p12 /p password`.

## Step 1 — Verify the MSI exists

```
dir dist\GlyphPDF-1.0.0-x64.msi
```

Expected: file present, size ~50-200 MB depending on bundled Qt/MSYS2 runtime.

## Step 2 — Sign the MSI (EV cert via Windows CertStore)

```cmd
signtool sign ^
  /a ^
  /tr http://timestamp.digicert.com ^
  /td sha256 ^
  /fd sha256 ^
  /d "GlyphPDF — Open-Source PDF Workstation" ^
  /du "https://glyphpdf.com" ^
  dist\GlyphPDF-1.0.0-x64.msi
```

Flags:
- `/a`  — auto-select best certificate from Windows CertStore
- `/tr` — RFC 3161 timestamp server (DigiCert; use Sectigo's if your cert is from Sectigo:
           `http://timestamp.sectigo.com`)
- `/td sha256` — timestamp digest algorithm (SHA-256)
- `/fd sha256` — file digest algorithm (SHA-256)
- `/d`  — product description shown in UAC dialog
- `/du` — product URL shown in UAC dialog

## Step 3 — Verify the signature

```cmd
signtool verify /pa /v dist\GlyphPDF-1.0.0-x64.msi
```

Expected output includes:
```
Successfully verified: dist\GlyphPDF-1.0.0-x64.msi
```

## Step 4 — Compute SHA-256 for release notes

```powershell
Get-FileHash dist\GlyphPDF-1.0.0-x64.msi -Algorithm SHA256
```

Record this hash — it goes into:
- `docs/release/release-notes-v1.0.0.md` (SHA-256 Verification section)
- The GitHub Release body
- `packaging/winget/Glyph.GlyphPDF.installer.yaml` (InstallerSha256 field)
- `docs/release/chocolatey/glyphpdf.nuspec` (checksum)
- `docs/release/scoop/glyphpdf.json` (hash field)

## Alternative — PFX file (CI/CD or non-EV)

```cmd
signtool sign ^
  /f path\to\certificate.pfx ^
  /p YOUR_PASSWORD ^
  /tr http://timestamp.digicert.com ^
  /td sha256 ^
  /fd sha256 ^
  dist\GlyphPDF-1.0.0-x64.msi
```

## Notes

- Do NOT sign the MSI before all build artifacts are finalized (windeployqt must have run).
  Signing is the last step before upload.
- The `.wixpdb` file (`dist\GlyphPDF-1.0.0-x64.wixpdb`) does not need to be signed or
  distributed; it is a build debugging artifact.
- If using GitHub Actions with a certificate stored as a repository secret, adapt the
  `release.yml` workflow to decode the base64 PFX secret and invoke signtool in the
  release job before the upload step.
