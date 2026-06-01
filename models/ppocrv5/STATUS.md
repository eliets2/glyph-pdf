# PP-OCRv5 Model Status

## RESOLVED (2026-06-01) — real PP-OCRv5 mobile acquired

The blocker recorded below for M5-PROMPT-1 is **closed**. The directory now holds
**genuine PP-OCRv5 mobile ONNX weights** (Apache-2.0, official PaddlePaddle org on
Hugging Face). Full per-file provenance, source URLs, byte sizes and SHA-256 hashes
are recorded in [`PROVENANCE.md`](PROVENANCE.md).

| File | Role | Version |
|---|---|---|
| `PP-OCRv5_mobile_det_infer.onnx` | Text detection (DBNet) | PP-OCRv5 mobile, opset 11 |
| `PP-OCRv5_mobile_rec_infer.onnx` | Text recognition (CTC/SVTR) | PP-OCRv5 mobile, opset 7, output dim 18385 |
| `ppocrv5_rec_dict.txt` | Recognition dictionary (18,383 chars) | matches rec output (18383 + CTC blank + 1) |
| `PP-LCNet_x1_0_textline_ori_infer.onnx` | Text-line orientation (0°/180°) | PP-OCRv5-era, opset 7, output dim 2 |

Decision taken: **option 1 — real PP-OCRv5 weights** (recommended path from the
original decision matrix below). The prior FAKE 10,000,000-byte placeholders
(`ch_PP-OCRv4_*`) were deleted; see PROVENANCE.md "What this replaced".

**Filename note for wiring:** the genuine keys file is `ppocrv5_rec_dict.txt`
(the M5-P1 prompt and some docs say `ppocr_keys.txt` — that name was never present;
use the real filename). RapidOcrEngine + PpOcrDecoder load `ppocrv5_rec_dict.txt`.

**Runtime compatibility:** all four models use ONNX opset 7 or 11, within range of the
bundled ONNX Runtime 1.17.3 (supports up to ~opset 20). They load under the CPU
execution provider. Validated — see PROVENANCE.md.

---

## Original blocker (historical — now closed)

The three ONNX files in this directory were once PP-OCRv4 / placeholder weights, not
PP-OCRv5. M5-PROMPT-1 (RapidOcrEngine real PP-OCRv5) required actual PP-OCRv5 weights
to deliver the accuracy improvement the prompt claims.

### Decision for M5-PROMPT-1 (resolved: chose option 1)
1. **Download real PP-OCRv5 weights** (recommended) — fulfils the accuracy claim ✅ **DONE**
2. Amend M5-PROMPT-1 to PP-OCRv4 — faster to start, weaker positioning (not taken)

## Models directory

`models/` is gitignored (binary files, too large for the repo). This STATUS.md and
PROVENANCE.md are tracked to document the required model state + acquisition record.
