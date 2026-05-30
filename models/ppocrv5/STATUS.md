# PP-OCRv5 Model Status

## Current files

The three ONNX files in this directory are **PP-OCRv4 weights**, not PP-OCRv5:

| File | Actual version |
|---|---|
| `ch_PP-OCRv4_det_infer.onnx` | PP-OCRv4 detection |
| `ch_PP-OCRv4_rec_infer.onnx` | PP-OCRv4 recognition |
| `ch_ppocr_mobile_v2.0_cls_infer.onnx` | PP-OCRv2 classifier |

## Blocker for M5-PROMPT-1

M5-PROMPT-1 (RapidOcrEngine real PP-OCRv5) requires actual PP-OCRv5 weights to deliver the accuracy improvement the prompt claims. Using PP-OCRv4 weights is acceptable as a short-term workaround but weakens the "2026 OCR accuracy" positioning.

## Download instructions

PP-OCRv5 ONNX weights are available from the PaddleOCR releases:
- Detection: `PP-OCRv5_mobile_det_infer.onnx` or `PP-OCRv5_server_det_infer.onnx`
- Recognition: `PP-OCRv5_mobile_rec_infer.onnx` or `PP-OCRv5_server_rec_infer.onnx`
- Classifier: `ch_ppocr_mobile_v2.0_cls_infer.onnx` (unchanged from v2, still current)

Source: https://github.com/PaddlePaddle/PaddleOCR (check Releases for latest ONNX exports)

## Decision for M5-PROMPT-1

Before executing M5-PROMPT-1, choose one:
1. **Download real PP-OCRv5 weights** (recommended) — fulfils the accuracy claim
2. **Amend M5-PROMPT-1 to PP-OCRv4** — faster to start, weaker positioning

Document the decision in CHANGELOG when M5-PROMPT-1 executes.

## Models directory

`models/` is gitignored (binary files, too large for the repo). This STATUS.md is tracked to document the required model state.
