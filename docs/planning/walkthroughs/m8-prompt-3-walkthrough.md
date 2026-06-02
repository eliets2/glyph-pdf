# M8-PROMPT-3 Walkthrough — Tag v1.0.0 + Release Prep

**Date:** 2026-06-02
**Status:** COMPLETE (all deliverables except human-gated steps D2/D4)
**HEAD before:** db08c38 (M8-P2 complete — governance + SPDX headers, 32/32 ctest)

---

## Summary of Deliverables

| ID | Deliverable | Status | Notes |
|----|-------------|--------|-------|
| D1 | Final verification gate | PASSED | 32/32 x 3 repetitions, 0 failures |
| D2 | Sign MSI | PREP ONLY | Instructions at docs/release/signing-instructions.md |
| D3 | CHANGELOG promotion | DONE | Committed |
| D4 | Tag + push prep | PREP ONLY | Commands at docs/release/release-commands.md |
| D5 | GitHub Release draft | DONE | docs/release/release-notes-v1.0.0.md |
| D6 | Package manager manifests | DONE | winget/chocolatey/scoop drafts |
| D7 | Announcements + walkthrough | DONE | This file |

---

## D1 — Exact ctest Output

```
Test project C:/Users/User/Projects/pdf/build
(full output in session — 32/32 passed across all 3 repetitions)
100% tests passed, 0 tests failed out of 32
Total Test time (real) = 5.71 sec
```

Command run:
```bash
export PATH="/c/msys64/ucrt64/bin:$PATH"
cd /c/Users/User/Projects/pdf/build
cmake --build . --parallel 8
QT_QPA_PLATFORM=offscreen ctest --output-on-failure -j4 --repeat-until-fail 3
```

Build result: `ninja: no work to do.` (already up to date from M8-P2)
Test result: 32/32 passed, 3 repetitions, 0 failures.

---

## D3 — CHANGELOG Changes

1. `[Unreleased] — v1.0.0 Branch C SCOPE LOCK execution (M2-M8)` promoted to `[1.0.0] — 2026-06-02`.
2. Verbose internal scope-lock preamble (6 lines) replaced with single-line public release note.
3. `[1.0.0-internal] - 2026-05-23 [INTERNAL-BUILD — NOT FOR PUBLIC DISTRIBUTION]` annotation removed (label shortened to `[1.0.0-internal] - 2026-05-23`).
4. Scope-lock warning block (5 lines beginning with `> **⚠ Scope-lock note:**`) removed.
5. `### Known Issues` section replaced with `### Known Limitations (v1.0.0)`:
   - REMOVED (closed): MRC compression (closed M7-P3), RapidOCR stub (closed M5-P1) — struck-through items deleted.
   - REMOVED: Pattern redaction "not yet implemented" note (it IS implemented).
   - KEPT as honest v1.0.0 limitations: remote signing, cloud collaboration, untranslated locales, prune-recent-files, Djot comment composer toolbar, installer logo dialog, brand PNG size.

---

## D2 — MSI Signing Status

MSI exists at: `dist/GlyphPDF-1.0.0-x64.msi`
`signtool` must be supplied by the user (Windows SDK).

Signing instructions: `docs/release/signing-instructions.md`

The user must:
1. Obtain an EV or OV code-signing certificate.
2. Run the exact signtool command from the instructions.
3. Run `signtool verify /pa /v dist\GlyphPDF-1.0.0-x64.msi` to confirm.
4. Compute SHA-256 via `Get-FileHash` and update all REPLACE_SHA256 placeholders in manifests.

---

## D4 — Exact Release Gate Commands (for user to run)

```bash
# From repo root, after MSI is signed and SHA-256 placeholders are updated:

git tag -a v1.0.0 -m "Release v1.0.0 — GlyphPDF open-source PDF workstation (Apache-2.0)"
git tag --list v1.0.0   # verify
git push origin v1.0.0  # triggers release.yml in GitHub Actions
```

GitHub repo public + branch protection: see `docs/release/release-commands.md` Phase 3.

---

## D6 — Package Manifest Notes

All three manifests use placeholder `REPLACE_SHA256` and `YOUR_USERNAME`.

**Before submitting:**
1. Replace `YOUR_USERNAME` with the actual GitHub username/org.
2. Replace `REPLACE_SHA256` (and `REPLACE_DOWNLOAD_URL` in nuspec) with the real values.
3. For winget: `ProductCode` is set to the UpgradeCode GUID `fb1fbe00-e1af-449c-8a58-2d2e308b9127`
   since WiX v4 auto-generates ProductCode per build. The UpgradeCode is the stable identifier.
   Winget uses UpgradeCode for `winget upgrade` matching.

**Submission targets:**
- winget: PR to `microsoft/winget-pkgs` under `manifests/g/GlyphPDF/GlyphPDF/1.0.0/`
- chocolatey: `choco pack` then `choco push` to community repository
- scoop: PR to `ScoopInstaller/Extras` bucket, or maintain own bucket at e.g. `YOUR_USERNAME/scoop-bucket`

---

## D7 — Files Created

```
docs/release/
  signing-instructions.md       — D2: signtool commands + SHA-256 workflow
  release-commands.md           — D4: exact git tag + push commands + checklist
  release-notes-v1.0.0.md      — D5: GitHub Release body (full)
  winget/
    GlyphPDF.yaml               — winget version manifest
    GlyphPDF.installer.yaml     — winget installer manifest
    GlyphPDF.locale.en-US.yaml  — winget locale manifest (en-US)
  chocolatey/
    glyphpdf.nuspec             — chocolatey package spec
  scoop/
    glyphpdf.json               — scoop manifest with autoupdate
  announcements/
    hn-post.md                  — HackerNews "Show HN" post
    reddit-posts.md             — r/PDF, r/opensource, r/privacy, r/degoogle, r/privacytoolsio
    twitter-thread.md           — 8-tweet thread + Mastodon toot
    blog-post-why-we-built-glyphpdf.md  — full blog post

docs/planning/walkthroughs/
  m8-prompt-3-walkthrough.md    — this file
```

---

## Confirmed: No Tag Created, No Push Performed

```
git tag --list v1.0.0
(empty — no tag exists)
```

All tagging and pushing is left to the user as an explicit human gate.

---

## Session Patterns Observed

- Build was already up to date (M8-P2 completed cleanly). The cmake --build step cost 0 seconds.
- All 32 tests pass deterministically across 3 repetitions (total wall time 5.71s per run).
- WiX v4 does not expose a static ProductCode attribute; the UpgradeCode `fb1fbe00-e1af-449c-8a58-2d2e308b9127` is the stable MSI identifier for winget/package-manager matching.
- The CHANGELOG had two "Known Issues" that were already struck-through (MRC compression closed M7-P3, RapidOCR stub closed M5-P1) — removed from the public release notes as resolved.
