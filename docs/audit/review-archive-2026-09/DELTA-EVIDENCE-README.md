# Code review evidence — 6 September 2026

Reviewed committed feat/parity-glm snapshot cf5ddc789f95059257455a267f7a7bf58600ed36.
Uncommitted work and any later HEAD shown in the manifest are outside this review.
No source checkout, installed application, or another session's build folder was modified.
No private PDFs, screenshots, or live UI capture are included. Fixtures are synthetic.

## Reproduce

Place the scripts and C++ probe sources in an isolated workspace's work/ directory.
Generate work/parity-cf5ddc7.zip with git archive from the exact reviewed commit.
review_delta.py uses dependency trees from work/parity-06b542d/, the previous isolated
snapshot. Supply the same three existing dependency directories there or adapt those paths:
third_party/podofo/install, third_party/pdfium, onnxruntime-win-x64-1.17.3.
Dependencies and compiler binaries are not included in this archive.

Use C:/Python314/python.exe from the workspace parent:
  work/review_delta.py build
  work/review_delta.py test
  work/compile_delta_probe.py
  work/delta_compile_fallback.py
  work/compile_canvas_probe.py

The scripts use C:/msys64/ucrt64. Tests use QT_QPA_PLATFORM=offscreen and require the
test-only Windows settings and loopback-server permissions used by TestOllamaProvider.
No remote AI service is called. The canvas probe paints its own synthetic widget into
an in-memory image and checks a pixel; it does not capture the desktop or installed app.

## Results

- Fresh selected build succeeded with Debug and -g0 (disk conservation); full compiler
  output is in delta-build.log. 23/23 selected CTest suites passed in 50.00 seconds.
- 100 CTest suites registered; the other 77 were not run.
- The Qt-only OcrPreprocessor compilation fails with two missing carryResolution errors.
  The wrapper prints the compiler's nonzero exit and diagnostics in its log.
- The sanitize failure probe preserves the redacted/original files but the partial result
  has an empty sanitize destination. Retrying with presenter arguments fails; retrying
  with the original request's path succeeds (D01).
- The canvas probe shows a white pixel where the word overlay should be drawn in a
  letterboxed image (D04).
- Earlier undo-history, collapsed-column, and false structural-change probes still
  reproduce against cf5ddc7. Their log is parity_probe-cf5ddc7.log.
- Same-stream PUBLIC_KEEP_TEXT survived in BOTH handwritten and PoDoFo-generated
  redaction fixtures. The ledger's E-1 corruption case was not independently reproduced;
  this does not establish that every reported fixture is safe.
- D02 (worker lifetime) and D05 (sanitized destination replacement) are source-traced
  findings. No deliberate use-after-free crash or failing disk write was induced.

Consult CODE-REVIEW-2026-09-06.md for code locations, scope, and acceptance instructions.
