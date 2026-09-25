# EVIDENCE — S2: batch-preset naming renders outside the output dir + reserved device names

- **Type:** ACCEPTS-INVALID ×2 (logic/path-containment; not a crash)
- **Surfaces:** `BatchPresetSchema::resolveNaming` + `BatchPresetCodec::parse`
  (src/core/BatchPreset.cpp:77-124, 634-643); consumer
  `BatchMode::confirmOverwrite`-path BatchMode.cpp:961 joins the rendered name
  with `QDir(outDir).filePath(name)`.
- **Harness:** `fuzz/harnesses/harness_batchpreset.cpp` — campaign
  (2176 execs = 69 seeds × 32 mutants, 9 finding execs) + the input-independent
  `probes` mode (fixed hostile matrix, 67 violations).
- **Class 1 — template literals escape the output dir (P2 / P1_RENDER_PATH_ESCAPE).**
  `sanitizeNameComponent` guards only token VALUES; the template's literal
  characters are copied verbatim, so a preset whose `output.naming` is
  `..\..\{basename}.pdf`, `../../{basename}.pdf`, `C:\{basename}.pdf` or
  `{basename}/{n}.pdf` PARSES CLEAN (only the ".pdf"-suffix and known-token
  rules run) and renders a name with path separators / drive letters / `..`.
  The header contract (BatchPreset.h:94 "Resolve the naming template to a
  FILE NAME (no directory components)") is violated by the function itself.
  Impact: batch outputs are written (via the overwrite-confirmed commit at
  BatchMode.cpp:961) outside the chosen output dir — the resolved full path IS
  shown in the overwrite dialog, so this is a defense-in-depth break, not a
  silent one. Seeds preserved here: seed-naming_dots.json, seed-naming_drive.json,
  seed-naming_sep.json, seed-naming_dots_fwd.json.
- **Class 2 — reserved DOS device names survive token sanitization (P1_RESERVED_DEVICE_NAME).**
  `{basename}` = CON / NUL.pdf / AUX / COM1 / LPT1 renders `CON.pdf`,
  `NUL.pdf.pdf`, … — all still DOS-device names under Win32. A batch run would
  "write" the output into the device (data loss reported as success).
- **Class 3 — unbounded rendered length.** `naming_huge.json` (60 000-char
  template) and `naming_date_long.json` ({date}×4000) parse clean and render
  >240-char names (beyond Win32 component limits; runtime write fails late).
- **Repro (deterministic, in-process):**
  ```
  PATH="$PWD/build-fz:$PATH" ./build-fz/fuzz/fuzz_batchpreset.exe one \
      fuzz/findings/w1/s2-batchpreset-naming-escapes/seed-naming_dots.json
  # → VERDICT FINDING P2_PRESET_NAMING_PATH_ESCAPE template=<..\..\{basename}.pdf> ...
  PATH="$PWD/build-fz:$PATH" ./build-fz/fuzz/fuzz_batchpreset.exe probes
  # → 67 violations, rc=42
  ```
- **Suggested fix direction (hand-off — not applied):** after resolving,
  reject when `QDir::isRelativePath(result)` is false OR the result contains
  `/ \ :` OR any component is `..` OR the first component is a reserved device
  name; i.e. run the same containment check the harness asserts over the FINAL
  rendered name (template literals included), not only over token values.
- **Environment:** g++ 16.1.0 ucrt64, Qt 6, build-fz Debug, feat/sweep-w1-fuzz.
  No crashes, no hangs, no codec round-trip or slug-grammar violations in the
  campaign (P3/P4 clean across 2176 execs).
