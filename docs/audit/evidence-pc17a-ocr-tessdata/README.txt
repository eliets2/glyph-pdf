PROGRAM-CONSOLIDATION-2026-09-25 §1.7a — first-run OCR on a pristine machine

CI failure (run 36538793928, CX-14 artifact excerpt in
ci-artifact-run36538793928-flow5-excerpt.txt): flow5 died with
"OCR failed: Tesseract language data for 'eng' is unavailable." after
burning its whole narrated 250s budget. Root cause: OcrEngine::initialize
seeds its strict AppLocalData copy ONLY from applicationDirPath()/tessdata
(the location the MSI ships — packaging/deploy.ps1 throws when missing) and
never reads the MSYS2 prefix's share/tessdata where CI's
mingw-w64-ucrt-x86_64-tesseract-data-eng installs eng.traineddata; the
policy download gate is off in tests by design. A developer machine that had
ever run the app carries the seed in AppLocalData and passed — a pristine
runner cannot pass. Not load, not flake: deterministic on a pristine state.

Files:
- ci-artifact-run36538793928-flow5-excerpt.txt  the captured CI failure text
- fail-before-flow5-no-seed.txt   local reproduction of the pristine state
  (AppLocalData seed AND build-side staging removed): byte-identical failure
  signature — 'Tesseract language data for 'eng' is unavailable', narrated
  250s budget exhausted, 2P/1F, 255536ms (CI: 296s).
- pass-after-flow5-staged-seed.txt  staged copy present, AppLocalData seed
  still absent: flow5 recognizes 'OCRME 42' at the FIRST poll (3s,
  textLen=8) and — the production-path proof — AppLocalData/tessdata/
  eng.traineddata exists again after the run, seeded BY THE ENGINE from the
  staged copy.
- pass-after-x3-additional-totals.txt  two further flow5 runs, 3P/0F each
  (3/3 with the pass-after above).
- pass-after-TestOcrPreprocessPrefs.txt  the fast pin
  firstRunOcrSeedIsStagedBesideTheBuild (6P/0F).
- nc-reverted-staging-removed.txt  staging removed → the pin fails in 10ms
  with the reason (missing staging can never again silently cost a 250s
  budget on a pristine machine).

The fix: CMakeLists stages eng.traineddata from the MSYS2 prefix
(MSYSTEM_PREFIX → MSYS2_ROOT → C:/msys64 default; GLYPHPDF_TESSDATA_DIR
override) beside the build output, mirroring the models/ staging above it —
zero downloads, zero policy changes; tests exercise the same zero-network
first-run path a production install ships.
