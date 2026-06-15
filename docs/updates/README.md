# In-app updates

GlyphPDF's `UpdateChecker` (`src/core/UpdateChecker.cpp`) fetches a small JSON
manifest over HTTPS, compares it to the running app's version, and — if a newer
version exists — shows a **dismissible banner** under the ribbon with
**Download / Install & Restart / Dismiss**. It never downloads or installs
without a click, verifies the MSI's **SHA-256** before offering install, and
verifies the MSI's **Authenticode signature** before launching it.

The on-launch check is **on by default** and can be turned off in
**Preferences ▸ Updates** (`update/checkOnStartup`). A one-time notice explains
this on first run.

## Manifest schema (`latest.json`)

Served at `https://eliets2.github.io/glyph-pdf/updates/latest.json`
(`beta.json` for the beta channel).

```json
{
  "version": "1.3.0",
  "releaseDate": "2026-06-15",
  "downloadUrl": "https://github.com/eliets2/glyph-pdf/releases/download/v1.3.0/GlyphPDF-1.3.0-x64.msi",
  "sha256": "B7859F33...",
  "releaseNotes": "https://github.com/eliets2/glyph-pdf/releases/tag/v1.3.0",
  "minVersion": "1.0.0"
}
```

## One-time setup (GitHub Pages)

1. Repo **Settings ▸ Pages ▸ Build and deployment**: *Deploy from a branch*,
   branch = the branch you publish from, folder = **`/docs`**.
   With `/docs` as the site root, `docs/updates/latest.json` is served at
   `https://eliets2.github.io/glyph-pdf/updates/latest.json` (the default the app ships with).
2. Releases are tagged on `audit-remediation`. Pages serves from one branch, so
   either merge release commits into the Pages branch or point Pages at
   `audit-remediation` so the published `docs/updates/latest.json` is current.

## Each release

```powershell
# after packaging\build-msi.ps1 has produced dist\GlyphPDF-<v>-x64.msi.sha256
powershell -ExecutionPolicy Bypass -File packaging\gen-update-manifest.ps1 -Version 1.4.0
git add docs/updates/latest.json
git commit -m "release: publish v1.4.0 update manifest"
# push to the GitHub Pages branch
```

Users on older versions then see the banner within ~3 s of launching the app.

## Caveat — code signing required for auto-install

`applyUpdate()` calls `verifyAuthenticode()` and **refuses to launch an unsigned MSI**
(a deliberate security control). Until the release MSI is Authenticode code-signed
(see [`../../packaging/signing-instructions.md`](../../packaging/signing-instructions.md)),
users will get the *notify → download → SHA-256 verify* steps, but **Install & Restart**
will report a signature-verification failure. Sign the MSI to enable end-to-end auto-update.
