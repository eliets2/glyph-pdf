# PP-OCRv5 Model Provenance

Genuine PP-OCRv5 **mobile** ONNX weights acquired for GlyphPDF (Apache-2.0 product).
All assets sourced from the **official PaddlePaddle organization on Hugging Face**
(pre-converted ONNX exports of the canonical PaddleOCR models), license **Apache-2.0**.

Acquired: 2026-06-01. Asset acquisition only — no source code, CMake, or git was touched.

## Files placed in this directory

| File | Role | Bytes | SHA256 |
|---|---|---|---|
| `PP-OCRv5_mobile_det_infer.onnx` | Text detection (DBNet, PP-OCRv5 mobile) | 4,826,518 | `a431985659dc921974177a95adcfbb90fd9e51989a5e04d70d0b75f597b6e61d` |
| `PP-OCRv5_mobile_rec_infer.onnx` | Text recognition (CTC, PP-OCRv5 mobile) | 16,534,782 | `da72dc72ca4dc220df0dfde68c1dedc31c58d3e76a25871122e5056227d50092` |
| `ppocrv5_rec_dict.txt` | Recognition character dictionary (18,383 chars) | 74,012 | `d1979e9f794c464c0d2e0b70a7fe14dd978e9dc644c0e71f14158cdf8342af1b` |
| `PP-LCNet_x1_0_textline_ori_infer.onnx` | Text-line orientation classifier (0deg/180deg), PP-OCRv5 | 6,777,816 | `38aa97cd4be591e0ad304e659f07ba30d946f27a63315433f6659c69c8778345` |

## Per-file provenance

### PP-OCRv5_mobile_det_infer.onnx
- **Source URL:** https://huggingface.co/PaddlePaddle/PP-OCRv5_mobile_det_onnx/resolve/main/inference.onnx
- **Repo:** `PaddlePaddle/PP-OCRv5_mobile_det_onnx` (official PaddlePaddle org)
- **License:** Apache-2.0 (declared in repo README front-matter)
- **Upstream:** https://github.com/PaddlePaddle/PaddleOCR (PP-OCRv5 mobile detection), exported to ONNX by PaddlePaddle (paddle2onnx)
- **Byte size:** 4,826,518
- **SHA256:** a431985659dc921974177a95adcfbb90fd9e51989a5e04d70d0b75f597b6e61d
- **Validation:** ONNX ir_version=6, ai.onnx **opset 11** (<= 20, OK for bundled ORT 1.17.3). Loads under onnxruntime CPUExecutionProvider. Input `x: [N,3,H,W]` float; output `[N,1,H,W]` float (probability map). Header is genuine ONNX protobuf (not an HTML page or git-LFS pointer stub). **PASS.**

### PP-OCRv5_mobile_rec_infer.onnx
- **Source URL:** https://huggingface.co/PaddlePaddle/PP-OCRv5_mobile_rec_onnx/resolve/main/inference.onnx
- **Repo:** `PaddlePaddle/PP-OCRv5_mobile_rec_onnx` (official PaddlePaddle org)
- **License:** Apache-2.0 (declared in repo README front-matter)
- **Upstream:** https://github.com/PaddlePaddle/PaddleOCR (PP-OCRv5 mobile recognition, `model_name: PP-OCRv5_mobile_rec` per inference.yml), exported to ONNX by PaddlePaddle
- **Byte size:** 16,534,782
- **SHA256:** da72dc72ca4dc220df0dfde68c1dedc31c58d3e76a25871122e5056227d50092
- **Validation:** ONNX ir_version=3, ai.onnx **opset 7** (<= 20, OK for bundled ORT 1.17.3). Loads under onnxruntime CPUExecutionProvider. Input `x: [N,3,48,W]` float; output `[N,T,18385]` float (CTC logits). The **18385** class dim = 18,383 dictionary chars + CTC blank + 1, confirming `ppocrv5_rec_dict.txt` matches this model's vocabulary. Header is genuine ONNX protobuf. **PASS.**

### ppocrv5_rec_dict.txt
- **Source URL:** extracted from https://huggingface.co/PaddlePaddle/PP-OCRv5_mobile_rec_onnx/resolve/main/inference.yml
  (the official rec model ONNX repo ships the dictionary embedded under `PostProcess.CTCLabelDecode.character_dict`; no separate dict file is published there)
- **License:** Apache-2.0 (same repo as the rec model)
- **Byte size:** 74,012
- **SHA256:** d1979e9f794c464c0d2e0b70a7fe14dd978e9dc644c0e71f14158cdf8342af1b
- **Format:** one character per line, UTF-8, LF line endings, no BOM. **18,383 lines/characters.**
  First entries: U+3000 (ideographic space), then CJK; last entries: emoji clock glyphs (canonical PP-OCRv5 multilingual dict ordering).
- **Validation:** parsed from the official YAML with PyYAML; line count (18,383) is consistent with the rec model output class count (18,385 = dict + CTC blank + 1). **PASS.** Note: for PaddleOCR CTC decode the blank index is prepended by the decoder; emit this file as the rec keys list exactly as-is (do NOT add a blank line manually).

### PP-LCNet_x1_0_textline_ori_infer.onnx
- **Source URL:** https://huggingface.co/PaddlePaddle/PP-LCNet_x1_0_textline_ori_onnx/resolve/main/inference.onnx
- **Repo:** `PaddlePaddle/PP-LCNet_x1_0_textline_ori_onnx` (official PaddlePaddle org)
- **License:** Apache-2.0 (declared in repo README front-matter)
- **Upstream:** https://github.com/PaddlePaddle/PaddleOCR — PP-LCNet_x1_0 text-line orientation classifier (the PP-OCRv5-era replacement for the v2 angle/`cls` model)
- **Byte size:** 6,777,816
- **SHA256:** 38aa97cd4be591e0ad304e659f07ba30d946f27a63315433f6659c69c8778345
- **Validation:** ONNX ir_version=3, ai.onnx **opset 7** (<= 20, OK for bundled ORT 1.17.3). Loads under onnxruntime CPUExecutionProvider. Input `x: [N,3,80,160]` float; output `[N,2]` float (2 classes: 0deg / 180deg). Header is genuine ONNX protobuf. **PASS.** This replaces the previous `ch_ppocr_mobile_v2.0_cls` angle classifier with the v5-generation text-line orientation model the task preferred.

## Validation environment
- Validator: `onnxruntime` 1.26.0, `onnx` 1.21.0, Python 3.14.5 (Windows).
- **Runtime-compatibility note:** the GlyphPDF app bundles **ORT 1.17.3**, which supports ONNX opset up to ~20. All four ONNX models use opset 7 or 11, so they are within range and expected to load on the bundled 1.17.3 runtime. (Validation here was performed on a newer ORT because that is what is installed in this environment; opset levels were checked explicitly to guarantee 1.17.x compatibility.)
- Each ONNX was checked to NOT be exactly 10,000,000 bytes, NOT a few-KB stub, NOT an HTML error page, and NOT a git-LFS pointer file; each actually instantiates an `InferenceSession` and exposes valid input/output tensors.

## What this replaced
The previous three files in this directory were FAKE placeholders, each exactly 10,000,000 bytes
(`ch_PP-OCRv4_det_infer.onnx`, `ch_PP-OCRv4_rec_infer.onnx`, `ch_ppocr_mobile_v2.0_cls_infer.onnx`).
They were deleted after the real PP-OCRv5 files above validated. The tracked `STATUS.md` was left in place.

---

WIRING TODO for PROMPT 4 (M5-P1): (1) update RapidOcrEngine.cpp lines ~58-60 to load these v5 filenames; (2) add rec-dictionary loading + CTC decode using ppocrv5_rec_dict.txt (no dict handling exists yet in src/engines/ocr/); (3) add model deployment — NO CMake rule copies models/ to the runtime AppLocalDataLocation/models/ppocrv5 path, so either add an install/copy step (check packaging/ MSI too) or call initialize(dataPath) pointing at the repo models dir; (4) real RapidOCR output then feeds the existing ROVER merge in OcrPipeline.cpp (RoverVote).
