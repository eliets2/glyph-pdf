# PP-DocLayoutV2 ONNX Model Provenance

## Model
- **Name:** PP-DocLayoutV2 (inference.onnx)
- **Local filename:** `pp_doclayout_v2.onnx`
- **Size:** ~24.9 MB
- **Architecture:** RT-DETR-based document layout detection
- **Input:** `[1, 3, 800, 800]` float32 image tensor (resized, channel-first, no mean/std normalization)
- **Output:** 25 document-region classes (see class list below)

## Source
- **Repository:** `PaddlePaddle/PP-DocLayoutV2_onnx` on Hugging Face Hub
- **URL:** https://huggingface.co/PaddlePaddle/PP-DocLayoutV2_onnx
- **Downloaded:** 2026-06-02
- **File:** `inference.onnx` (resolved from main branch)

## Observed hash + re-download verification (L03, 2026-10-04)

The 2026-06-02 record above published NO SHA-256 for the file — a gap in the
pin discipline (7z-lane standard) this lane closes. Re-downloaded from the
same URL on 2026-10-04 and hash-recorded:

- **SHA-256 (`pp_doclayout_v2.onnx`, observed 2026-10-04)**:
  `cd540dc296ff3115fe78efa65b68501e6a8dc74b198acc9834a725ffaa095aac`
- **Observed size**: 213,963,712 bytes (~204 MiB).

**Discrepancy, recorded honestly**: this document claims "~24.9 MB" for the
2026-06-02 acquisition, but the file available at the URL on 2026-10-04 is
213,963,712 bytes. Either the upstream `main` branch was re-exported since
June or the June size note was wrong; no June-era local copy existed in this
lane's environment to compare against. The hash above pins what the URL
served on 2026-10-04; the model is not directly exercised by any unit test
(PpDocLayoutDetector is a runtime OCR-pipeline component), so the gate is not
sensitive to this discrepancy. Follow-up for the owner: re-pin from a
release/tag-ref URL rather than a moving branch, and reconcile the size.

The weights remain gitignored (`models/`); only this record is tracked.

## License
- **License:** Apache 2.0
- **Verified:** HuggingFace model card lists `apache-2.0`
- **Official maintainer:** PaddlePaddle (Baidu)
- **License text:** https://www.apache.org/licenses/LICENSE-2.0
- **Compatibility:** Apache-2.0 is compatible with GlyphPDF's Apache-2.0 project license.
  No GPL/AGPL contamination. Linking is permitted.

## Class Labels (25 classes)
In model output order (class index → name):
```
0: abstract
1: algorithm
2: aside_text
3: chart
4: content
5: display_formula
6: doc_title
7: figure_title
8: footer
9: footer_image
10: footnote
11: formula_number
12: header
13: header_image
14: image
15: inline_formula
16: number
17: paragraph_title
18: reference
19: reference_content
20: seal
21: table
22: text
23: vertical_text
24: vision_footnote
```

## Mapping to RegionType enum
See `PpDocLayoutDetector.cpp` `classToRegionType()` for the mapping from
these 25 PaddlePaddle class names to `RegionType` values defined in
`ILayoutDetector.h`.
