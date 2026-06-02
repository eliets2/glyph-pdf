# M5-PROMPT-1 — RapidOcrEngine real PP-OCRv5 inference

**Date:** 2026-06-02 · **Sprint:** 4 of 18 (M5 / WS1 OCR ensemble) · **Status:** core complete, real recognition proven (28/28 ctest). One sub-item deferred.

## Goal
Replace the `RapidOcrEngine` Mock stub with a real ONNX-Runtime PP-OCRv5 inference pipeline, so RapidOCR produces genuine text that feeds the existing ROVER ensemble.

## Deliverables

| D | What | Status | Commit |
|---|------|--------|--------|
| D1 | Resolve model question (genuine PP-OCRv5 weights) | ✅ | `094ece7` |
| D2–D4 | `PpOcrDecoder`: DBNet detect, perspective warp, cls, SVTR + CTC decode | ✅ | `cf37db1` |
| D5 | Wire decoder into `processImage`; remove Mock; `isMock()`=false; model deploy | ✅ | `19c4178` |
| D6 | `TestRapidOcr` — real recognition proof | ✅ (ROVER-vs-Tesseract test deferred) | `19c4178` |
| D7 | OCRMode UI gate auto-enables + CHANGELOG closure | ✅ | (this commit) |
| D8 | This walkthrough | ✅ | (this commit) |
| D9 | Vault refresh | suggested text below |

## How it was executed (full transparency)
- An autonomous agent committed **D1** and wrote `PpOcrDecoder.{h,cpp}` (D2–D4) + a fixture, then **died on a usage limit before building/committing** — leaving 23 KB of uncommitted, never-compiled decoder.
- Continued **inline**: wired the decoder into CMake, fixed one real compile error (the `IoNames` struct held `Ort::AllocatedStringPtr`, whose deleter is non-default-constructible → stored names as `std::string` instead), tidied a `-Wmisleading-indentation` warning, and committed D2–D4 (`cf37db1`).
- D5: `processImage` now calls `PpOcrDecoder::run` (Mock removed); updated to v5 filenames + `ppocrv5_rec_dict.txt`; added model-path resolution (`AppLocalData` → `applicationDirPath()`) and a CMake `file(COPY)` that deploys `models/ppocrv5` beside the build output so the app + tests find them.
- **D6 is the moment of truth:** `TestRapidOcr` renders known strings ("Hello World", "2026"), loads the genuine PP-OCRv5 mobile models, and asserts the decoded text. `ctest -R TestRapidOcr` → **Passed**; full suite **28/28**. The never-run decoder works. (Note: QtTest/qDebug stdout is swallowed by a Windows console-handle quirk under the offscreen platform + onnxruntime warning spam — the canonical ctest pass with hard content assertions is the proof.)

## Architecture notes
- `PpOcrDecoder` borrows the warm `Ort::Session*`s owned by `RapidOcrEngine` (no spawn-per-page). Its geometry + CTC helpers are ONNX-free `static` functions. No OpenCV, no PaddlePaddle runtime — ORT 1.17.3 only (model opsets 7/11).
- Dict indexing: CTC blank = 0, dict line *i* → class *i+1*; rec output dim 18385 = 18,383 dict chars + blank + 1.

## Deferred / follow-up
- **ROVER comparative-accuracy test** (Tesseract+RapidOCR ≥ either alone): RapidOCR now feeds the existing `OcrPipeline` ROVER merge, but a head-to-head accuracy test on a labelled set is not yet added.
- The onnxruntime "Removing unused initializer" warnings are benign Paddle→ONNX export artifacts; could be silenced by raising the ORT log level.
- Installed-app model deployment (to `AppLocalData`) is an installer/M8 concern; the build-dir copy covers dev + tests.

## Suggested vault update (`C:\Users\User\.claude\memory\projects\glyphpdf\`)
- `01-current-state.md`: Head → (this commit); **M5-PROMPT-1 core complete**, RapidOCR real (isMock=false), tests **28/28**; next = PROMPT 5 (M5-P2 LayoutEnsemble). Add the lesson: *`Ort::AllocatedStringPtr` can't be default-constructed — copy to `std::string`.*
- `05-prompts-index.md`: mark M5-PROMPT-1 substantially done (ROVER accuracy test outstanding).
