# M8-PROMPT-1 Walkthrough — Marketing Prep for GlyphPDF v1.0.0

**Executed:** 2026-06-02
**Head at start:** c454a76 (M7-P3 MRC complete)
**Head at end:** 30ec27c (D4 press kit)
**Agent:** Claude Sonnet 4.6

---

## What was done

### D1 — Hero screenshot guide (DONE, human action required for actual captures)
**Commit:** c83a245 `marketing: hero screenshots for v1.0.0 launch (M8-P1 D1)`

Created `marketing/screenshots/screenshot-guide.md` with step-by-step
instructions for 8 hero screenshots at 1920 x 1080:
1. Welcome screen (Dark, 4K version also)
2. Annotation + ribbon Edit tab (Dark, 4K)
3. Digital signing dialog (Dark)
4. Redaction with Edact-Ray defense (Dark)
5. OCR confidence overlay (Dark, 4K)
6. Forms mode (Light)
7. High Contrast accessibility + F6 (HC)
8. MRC compression dialog (Dark)

All shots specify REAL PDFs only (no Lorem Ipsum). Pre-flight checklist includes
exact PowerShell for window resize to 1920 x 1080. Cannot be automated by an
agent — requires the running GUI.

Also created `marketing/screenshots/README.md` placeholder with expected filenames.

**Human action required:** Run the app, follow screenshot-guide.md, capture 8 PNGs
(+3 4K versions), place in marketing/screenshots/.

---

### D2 — Demo video production script (DONE, human action required for recording)
**Commit:** b4278ff `marketing: 60-90s demo video (M8-P1 D2)`

Created `marketing/demo-script.md` — full shot-by-shot script for a 75s demo:
- 8 scenes with exact actions, voiceover lines, timing targets
- Runtime table: 4+6+12+11+14+11+10+7 = 75s total
- Post-production notes: MP4 H.264 CRF 18 60fps; 30fps social cut; YouTube
  thumbnail recommendation (Scene 5 green checkmark)
- All scenes grounded in real implemented features

**Human action required:** Record screen with OBS/Camtasia, narrate voiceover,
edit per the script, export MP4 to marketing/demo-v1.0.0.mp4.

---

### D3 — Website copy (FULLY DONE, automatable)
**Commit:** 14b5f48 `marketing: website + landing page copy (M8-P1 D3)`

Created `marketing/website-copy.md` covering:
- 3 hero headline variants (A/B/C), sub-headline, one-liner
- 3 feature pillars: Security You Can Verify / OCR That Reads Like a Human /
  A Real Desktop App, Not a Wrapper
- Each pillar has a body paragraph + proof-point list (all from README/CHANGELOG)
- Contribution invite section + comparison table vs. generic free editors
- SEO meta tags (title, description, keywords) + Open Graph tags
- Short-form variants for HN, Reddit, ProductHunt

All claims backed by README.md + CHANGELOG.md at HEAD c454a76. No invented features.

**Human action required:** Fill in [YOUR-ORG]/[URL] placeholders before publishing.
Review once more at actual launch date to ensure claims match current HEAD.

---

### D4 — Press kit (DONE, screenshot integration deferred)
**Commit:** 30ec27c `marketing: press kit (M8-P1 D4)`

Created:
- `marketing/press-kit/product-summary.md` — 1-page product summary with:
  capabilities overview, technical architecture, test coverage summary,
  license matrix, press kit contents list, zip assembly instructions,
  4 suggested press angles
- `marketing/press-kit/logos/` — 5 brand assets copied from resources/branding/:
  app_icon.png, wordmark.png, splash.png, installer_logo.png, tray_icon.png
- `marketing/press-kit/screenshots/` — placeholder directory

**Human action required:**
1. After capturing hero screenshots (D1), copy to marketing/press-kit/screenshots/
2. Assemble press-kit.zip using PowerShell one-liner in product-summary.md
3. Fill in [ORG], [URL], [maintainer@email] placeholders in product-summary.md

---

## Deferred items (not started, human-gated)

| Item | Why deferred | Owner |
|------|-------------|-------|
| Hero screenshots (8 PNGs) | Cannot run GUI agent-side | Human |
| Demo video MP4 | Cannot record screen agent-side | Human |
| press-kit.zip | Depends on screenshots | Human |
| [ORG]/[URL] placeholder fill | Not yet decided / not in source | Human |
| Founder photo (press kit) | Optional, not in scope by default | Human |
| Translation of website copy (fr/de/ar) | Out of scope for v1.0.0 launch day | Future |

---

## Honest status assessment

- D1 and D2 are INSTRUCTIONS only, not final artifacts. This is the correct scope:
  agents cannot run a Qt GUI or capture a screen recording.
- D3 is fully complete and publication-ready modulo placeholder fill.
- D4 is as complete as it can be without screenshots: brand assets in logos/,
  product summary written, zip assembly documented.
- No source files were touched. No src/, tests/, or CMakeLists.txt changes.

---

## Files created this session

```
marketing/
  screenshots/
    README.md
    screenshot-guide.md
  demo-script.md
  website-copy.md
  press-kit/
    product-summary.md
    logos/
      app_icon.png
      installer_logo.png
      splash.png
      tray_icon.png
      wordmark.png
    screenshots/            (placeholder, empty)
docs/walkthroughs/
  m8-prompt-1-walkthrough.md
```

---

## Next prompt

**M8-P2** — OSS governance: LICENSE finalization (Apache-2.0 vs MIT decision),
CONTRIBUTING.md, SECURITY.md, GitHub repo setup, CI workflows (GitHub Actions:
build + test on push, ctest on PR).
