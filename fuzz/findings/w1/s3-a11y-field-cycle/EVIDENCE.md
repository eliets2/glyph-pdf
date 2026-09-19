# EVIDENCE — S3: cyclic /Fields hierarchy kills the process (unbounded recursion)

- **Type:** CRASH / DoS (uncaught std::bad_alloc and hard SEGV, non-deterministically)
- **Surface:** `collectFieldGaps` — src/engines/AccessibilityChecker.cpp:137-178,
  reached from `scanAccessibility` (AcroForm /Fields walk). Unlike the image
  walk (`collectImageGaps`, depth cap 8 + visited-set intent), the field walk
  has NO depth cap and NO cycle detection: it recurses on /Kids and
  accumulates `fullName = parentName + "." + ownName` at every level.
- **Harness:** `fuzz/harnesses/harness_a11y.cpp`; crafted fixture
  `fuzz/corpus/a11y-crashers/fields_cycle_no_ft.pdf` (AcroForm /Fields [5 0 R];
  obj 5 /Kids [6 0 R]; obj 6 /Kids [5 0 R] — pure container cycle) and
  `fields_self_cycle.pdf` (obj 5 /Kids [5 0 R]). Also
  `fields_cycle_with_tu.pdf` (cycle with /FT+/TU — the else-branch still
  recurses) and `fields_deep_2000.pdf` (acyclic 2000-deep chain).
- **Observed:**
  - Unmutated cycle fixtures → process terminates with uncaught
    `std::bad_alloc` (heap exhausted by the quadratically-growing name
    strings of the infinite recursion) — captured as
    `FINDING S3_SCAN_THREW_STD: std::bad_alloc` at the driver boundary.
  - Mutant #14 of the cycle fixture (5834-byte `segfault-seed.bin`, preserved
    here) killed the driver mid-campaign with a hard SEGV (rc=139/127, no
    output) — reproduced 1-of-3 runs, the other 2 runs report the bad_alloc
    path: which limit (stack vs heap) loses the race varies per run.
  - In the app this is a remote-ish DoS: opening a crafted PDF's accessibility
    panel terminates GlyphPDF.
- **Repro:**
  ```
  PATH="$PWD/build-fz:$PATH" ./build-fz/fuzz/fuzz_a11y.exe one \
      fuzz/corpus/a11y-crashers/fields_cycle_no_ft.pdf
  # → FINDING S3_SCAN_THREW_STD: std::bad_alloc (rc=42) or hard SEGV
  PATH="$PWD/build-fz:$PATH" ./build-fz/fuzz/fuzz_a11y.exe one \
      fuzz/findings/w1/s3-a11y-field-cycle/segfault-seed.bin
  ```
- **Suggested fix direction (hand-off — not applied):** give collectFieldGaps
  the same discipline as collectImageGaps: an explicit depth cap plus a
  visited-reference set (std::set<PdfReference>) checked before recursing
  into /Kids.
- **Scope note:** applyAccessibilityFix is NOT affected (it never walks /Fields);
  the S4 renderer and the fix surfaces stayed clean under the same campaign.
- **Environment:** g++ 16.1.0 ucrt64, Qt 6, vendored PoDoFo 1.1.0, build-fz
  Debug, feat/sweep-w1-fuzz.
