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
