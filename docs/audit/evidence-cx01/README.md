# CX-01 evidence — Tag Document destroys inline images

Fix: the content-stream rewrite emitted the inline-image dictionary wrapped
in `<< >>` (`BI\n<< /W 2 … >>\nID\n`), but PDF 32000 §8.9.7 requires BARE
key/value pairs between BI and ID — a renderer reads the tokens directly, so
the wrapped dict parsed as unknown keys and every inline image on the page
was silently destroyed while the text-only candidate invariant still passed.

Files:
- `fail-before.txt` — the new test `inlineImageSurvivesTagging` (added in the
  same commit) fails on the unfixed writer: the bare-dict assertion
  `!dictPart.contains("<<")` fires; the other 17 tests still pass.
- `pass-after.txt` / `pass-after-full.txt` — after the fix: full suite
  `Totals: 18 passed, 0 failed, 1 skipped` (the skip is the pre-existing,
  recorded veraPDF-CLI-dependence skip). The test pins: bare pairs only,
  both inline images' payload bytes byte-identical (AHx hex + raw binary),
  the image XObject `Do` intact, and the whole page pixel-identical under
  PDFium (150 dpi) before vs after.
- `invariant-negative-control.txt` — the candidate invariant extension
  (per-stream records of every XObject `Do` name and every inline image's
  payload bytes, compared before vs after) catches a writer that drops a
  single inline-image payload byte: `r.ok` FALSE with
  "painting-preservation invariant failed on page 1". The control was a
  temporary source mutation, reverted before the fix was committed.

Notes:
- Whitespace immediately before the `EI` terminator is terminator syntax
  (§8.9.7 "should be preceded by a whitespace byte"); PoDoFo's inline reader
  folds it into the data buffer and the writer re-emits it canonically as a
  single `\n`, so the invariant compares payload bytes with trailing
  whitespace stripped. The emitted `ID` is followed by exactly one
  whitespace byte, then the exact payload bytes, then whitespace, then `EI`.
