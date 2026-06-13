# Release Gate Commands — GlyphPDF v1.0.0

> **HUMAN GATE** — None of these commands have been run. Execute them in order after:
> 1. D1 test gate passes (32/32 x 3 repetitions — CONFIRMED 2026-06-02)
> 2. MSI is signed (see `docs/release/signing-instructions.md`)
> 3. SHA-256 placeholders in package manifests are replaced with the real hash
> 4. You have reviewed and approved the CHANGELOG promotion

---

## Phase 1 — Tag the release

```bash
# Run from the repo root (not inside build/)
git tag -a v1.0.0 -m "Release v1.0.0 — GlyphPDF open-source PDF workstation (Apache-2.0)"

# Verify the tag was created
git tag --list v1.0.0
git show v1.0.0 --stat
```

## Phase 2 — Push tag to GitHub

```bash
# This triggers the release.yml GitHub Actions workflow automatically
git push origin v1.0.0
```

The `release.yml` CI workflow (`.github/workflows/release.yml`) will:
1. Build the release MSI via `packaging/build-msi.bat`
2. Attach `GlyphPDF-1.0.0-x64.msi` and its `SHA256SUMS.txt` to the GitHub Release
3. Publish the GitHub Release as a draft (you promote to published manually)

## Phase 3 — GitHub repository setup (run once before push)

```bash
# Requires: gh auth login (GitHub CLI authenticated)
bash docs/github-setup.sh
```

Manual steps in GitHub Settings (cannot be automated via gh CLI):
- **Visibility:** Settings → Danger Zone → Change repository visibility → Make Public
- **Branch protection:** Settings → Branches → Add rule:
  - Branch name pattern: `main`
  - Check: "Require a pull request before merging"
  - Check: "Require status checks to pass before merging"
  - Required status check: `ci` (from `.github/workflows/ci.yml`)
  - Check: "Include administrators" (recommended)

## Phase 4 — Publish GitHub Release

After the release.yml workflow completes:
1. Go to: https://github.com/YOUR_USERNAME/glyphpdf/releases/tag/v1.0.0
2. Click "Edit draft release"
3. Paste the content of `docs/release/release-notes-v1.0.0.md` into the release body
4. Attach the signed MSI (if the workflow used an unsigned build, re-upload the signed one)
5. Attach `SHA256SUMS.txt`
6. Click "Publish release"

## Phase 5 — Verify release integrity

```powershell
# Download and verify SHA-256 matches
$expected = (Get-Content SHA256SUMS.txt | Select-String "GlyphPDF-1.0.0-x64.msi").Line.Split()[0]
$actual   = (Get-FileHash GlyphPDF-1.0.0-x64.msi -Algorithm SHA256).Hash.ToLower()
if ($expected -eq $actual) { "MATCH — release is good" } else { "MISMATCH — do not distribute" }
```

## Phase 6 — Package manager submissions (after release is live)

```bash
# winget (PR to microsoft/winget-pkgs)
# Canonical manifests live in packaging/winget/ (Glyph.GlyphPDF.*.yaml).
# Update InstallerSha256 in packaging/winget/Glyph.GlyphPDF.installer.yaml first.
gh pr create --repo microsoft/winget-pkgs \
  --title "New package: Glyph.GlyphPDF version 1.0.0" \
  --body "Submitting GlyphPDF 1.0.0 (Apache-2.0 open-source PDF workstation)"

# chocolatey
# See docs/release/chocolatey/glyphpdf.nuspec — fill SHA-256 first
choco pack docs/release/chocolatey/glyphpdf.nuspec
choco push glyphpdf.1.0.0.nupkg --source https://push.chocolatey.org --api-key YOUR_KEY

# scoop (PR to ScoopInstaller/Extras, or push to own bucket)
# See docs/release/scoop/glyphpdf.json — fill hash + URL first
```

---

**CONFIRMATION (pre-run checklist):**
- [ ] `ctest --repeat-until-fail 3` passes 32/32 (DONE 2026-06-02)
- [ ] CHANGELOG promoted to [1.0.0] with release date 2026-06-02 (DONE)
- [ ] MSI signed with EV cert (`signtool verify /pa` passes)
- [ ] SHA-256 of signed MSI recorded
- [ ] Package manifest SHA-256 placeholders replaced
- [ ] `git status` is clean (no uncommitted changes)
- [ ] `git tag --list v1.0.0` shows no existing tag (avoid double-tag)
