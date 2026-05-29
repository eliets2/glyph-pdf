# M2-PROMPT-2 Source Analysis A: PoDoFoBackend + OcrEngine

## Executive Summary
`redactCanvasRecursively` (PoDoFoBackend.cpp:875-1168) parses PDF content streams recursively via `PdfContentStreamReader` and tracks text rendering mode (`Tr`) at line 977. The function **does NOT currently embed invisible text** during redaction. OcrEngine.cpp (lines 236-290) produces OCR results without any invisible rendering mode embedding. This analysis identifies exactly where D1 (invisible OCR text insertion) must hook in.

---

## redactCanvasRecursively Structure

### Entry Point
- **Function:** `redactCanvasRecursively()` at line 875 of PoDoFoBackend.cpp
- **Called from:** `applyRedactions()` at line 1215 (after inline-image/binary content checks)
- **Signature:**
  ```cpp
  void redactCanvasRecursively(PoDoFo::PdfObject& canvasObj,
                               const std::vector<PoDoFo::Rect>& pdfRects,
                               PoDoFo::PdfPage& page,
                               PoDoFo::PdfMemDocument* document,
                               std::set<int64_t>& redactedMcids)
  ```

### Content Stream Iteration
- **Mechanism:** Lines 900–901: Creates `SpanStreamDevice` from stream buffer, wraps in `PdfContentStreamReader`
- **Loop:** Line 929: `while (reader.TryReadNext(content))` reads operators and operands
- **Stack-based parsing:** Line 932: `const auto& stack = content.GetStack()` extracts operand stack
- **Keyword extraction:** Line 931: `std::string_view kw = content.GetKeyword()`
- **State tracking:** Maintains separate variables for:
  - Text position: `textX`, `textY` (lines 905–907)
  - Text block state: `inTextBlock` (line 906)
  - Text parameters: `currentFontSize`, `currentCharSpacing`, `currentWordSpacing`, `currentFontScale` (lines 910–913)
  - **Rendering mode:** `currentRenderingMode` (line 915) — **CRITICAL for D1**
  - MCID tracking: `currentMcid` (line 917)

### Form XObject Recursive Handling
- **Detection:** Line 1114: `if (kw == "Do" && stack.size() > 0 && stack[0].IsName())`
- **XObject lookup:** Lines 1124–1139: Resolves XObject name → resolves indirect references → checks Subtype="Form"
- **Recursion:** Line 1136: **`redactCanvasRecursively(*xobj, pdfRects, page, document, redactedMcids)`**
  - **Important:** MCID set is **passed by reference**, so nested XObjects add to the same tracked set
  - Form XObjects inherit the redaction rectangles and current page (no transformation matrix awareness)

### Current Text Operator Handling
Redaction logic at lines 1010–1112 processes:
- **Supported operators:** `Tj`, `TJ`, `'` (T-prime), `"` (T-double-quote)
- **Detection:** Line 1010: `bool isTextOp = (kw == "Tj" || kw == "TJ" || kw == "'" || kw == "\"")`
- **Width calculation:** Lines 1049–1075: Computes `totalAdvance` using `getEncodedStringWidth()` (line 623)
- **Redaction check:** Line 1081: `if (inTextBlock && isIntersectingSpan(textX, textX + totalAdvance, textY))`
- **Edact-Ray defense:** Lines 1087–1108: Emits numeric-only `TJ` gap (PETS 2023, Bland et al.)
  - Preserves cursor position without revealing individual glyph widths
  - Replaces redacted text with: `[ N ] TJ` where `N = -totalAdvance * 1000 / scale`
- **Non-redacted text:** Falls through to lines 1143–1148 (writes original operator + operands)

### Current Tr Tracking
- **Initialization:** Line 915: `int currentRenderingMode = 0;` (default = fully visible)
- **Update:** Lines 977–979:
  ```cpp
  else if (kw == "Tr" && stack.size() >= 1) {
      if (stack[0].IsNumberOrReal()) currentRenderingMode = static_cast<int>(stack[0].GetReal());
  }
  ```
- **Scope:** Tracked within a single `redactCanvasRecursively()` call; NOT reset across Form XObject boundaries
- **Current usage:** NONE — variable is read but never consulted during redaction or output
- **No filtering:** Redaction does not check `currentRenderingMode` before deciding what to redact
- **No embedding:** Redaction does not emit invisible text (`Tr 3`) as an alternative to redacting

---

## Existing Tr References in PoDoFoBackend.cpp

| Line | Context | Code Snippet |
|------|---------|--------------|
| 915 | Initialization | `int currentRenderingMode = 0; // Tr` |
| 977–979 | Tr operator tracking | `else if (kw == "Tr" && stack.size() >= 1) { if (stack[0].IsNumberOrReal()) currentRenderingMode = static_cast<int>(stack[0].GetReal()); }` |

**Total Tr references: 2**. The rendering mode is declared but **never used** in any downstream logic.

---

## Text State Tracking Structure

The function maintains a **flat set of scalar variables** (no struct) to mirror PDF text state:

```cpp
// Line 905–917
double textX = 0.0, textY = 0.0;                    // Tm, Td, TD, T*
bool inTextBlock = false;                           // BT/ET
double leading = 0.0;                              // TL
double currentFontSize = 12.0;                      // Tf (operand 1)
double currentCharSpacing = 0.0;                    // Tc
double currentWordSpacing = 0.0;                    // Tw
double currentFontScale = 1.0;                      // Tz / 100
std::string currentFontName = "Helvetica";          // Tf (operand 0)
int currentRenderingMode = 0;                       // Tr (UNUSED)
int64_t currentMcid = -1;                           // BDC/EMC
```

**State reset points:**
- **BT operator (line 935):** Resets `textX=0.0, textY=0.0, inTextBlock=true`
- **EMC operator (line 1006):** Resets `currentMcid=-1`
- **No reset for Tr:** If a Form XObject doesn't set `Tr`, it inherits parent's mode

**Notable absences:**
- No matrix stack (only final position captured via `Tm` operands 4–5)
- No font object resolution beyond name lookup
- No coordinate transformation between parent and Form XObject content streams

---

## OcrEngine: Does It Embed Invisible Text?

### Evidence from OcrEngine.cpp
- **Lines 236–290:** `OcrEngine::processImage()` — primary OCR execution
  - Initializes Tesseract API (line 260: `d->api->SetImage(pix)`)
  - Recognizes words (line 261: `d->api->Recognize(0)`)
  - Iterates results via `ResultIterator` (lines 263–283)
  - **Extracts:** text, bounding box, confidence
  - **Returns:** `QList<OcrResult>` — **no rendering mode field**
  
- **Lines 292–327:** `OcrEngine::getRawText()` — returns plain UTF-8 text only

### OcrResult Structure (OcrTypes.h:5–9)
```cpp
struct OcrResult {
    QString text;              // Recognized word
    QRectF boundingBox;        // x, y, w, h in image pixels
    int confidence;            // 0–100
    // NOTE: no renderingMode, no visibility flag
};
```

### Conclusion
**OcrEngine currently does NOT embed invisible text.** It produces bare word detections with no rendering mode metadata. The embedding logic must be implemented in D1 within the redaction flow, **not** in OcrEngine.

---

## OcrPipeline Output Format

### OcrPipeline.cpp Overview (lines 1–209)
- **Primary class:** `OcrPipeline` — orchestrates OCR via pluggable engines
- **Run method:** Line 151: `QList<MergedOcrWord> OcrPipeline::run(const QImage &pageImage)`
- **Preprocessing:** Line 156: Applies optional image transformations (deskew, denoise, etc.)
- **Strategy routing:** Line 171: Selects PrimaryOnly / ConfidenceWeighted / RoverVote
- **Output type:** `MergedOcrWord` (word-level boxes)

### MergedOcrWord Structure (inferred from OcrPipeline.cpp:95–130)
```cpp
struct MergedOcrWord {
    QString text;              // Recognized word
    QRectF boundingBox;        // Spatial extent in original coordinates
    int confidence;            // Confidence score
    QString sourceEngine;      // "Tesseract", "RapidOCR", or "ROVER"
    // NOTE: no rendering mode
};
```

### Pipeline Strategies
- **PrimaryOnly:** Direct passthrough via `toPrimaryOnly()`
- **ConfidenceWeighted:** Use secondary engine if primary confidence < 70%
- **RoverVote:** Always run both engines, merge by IoU

### Conclusion
OcrPipeline output is **word-level boxes + text + confidence + source label**. No rendering mode is carried through. D1 must **add** rendering mode logic at the point where OCR results are embedded into the redacted PDF.

---

## Key Insertion Point for D1

**Primary hook location:** `redactCanvasRecursively()` at line 1081–1112

**Current redaction flow:**
```
1. Line 1081: Check if text intersects redaction rect
   ├─ YES (intersects): Lines 1087–1108 → emit numeric-only gap
   └─ NO (safe): Line 1143 → emit original operator
2. Line 1111: Update cursor position
```

**D1 insertion options:**

### Option A: Modify line 1087–1108 (replace redaction with invisible text)
**Pros:** Reuses all existing text advance/positioning logic
**Cons:** Requires OCR results at this point (architectural mismatch)

### Option B: Post-redaction invisible-text layer injection (recommended for MVP)
**Pros:** Decouples OCR pipeline from redaction logic
**Location:** After line 1215 in `applyRedactions()`, before painter/annotation cleanup (line 1250)

---

## Risk Factors for D1 Implementation

### 1. BT/ET Stack Scope
**Risk:** Rendering mode is sticky across the entire text block
- Setting `Tr 3` at line N affects all text through the next `ET`
- If redacted text is in the middle of a text block, invisible mode contaminates subsequent text

**Mitigation:** Always restore `Tr 0` after embedding invisible text

### 2. Nested Form XObjects Without Tr Reset
**Risk:** Form XObjects do not reset `Tr` to 0 on entry
- Parent stream sets `Tr 3`, child Form XObject inherits it

**Mitigation:** Reset `currentRenderingMode = 0` before recursing into Form XObjects

### 3. Font/Size/Position Mismatch
**Risk:** OCR word bounding boxes (image pixels) ≠ PDF text-space coordinates
**Mitigation:** Account for DPI, CTM, and font metrics in coordinate transformation

### 4. Glyph Encoding
**Risk:** OCR text is UTF-8; PDF strings must match font encoding (Simple/CID/Identity-H)
**Mitigation:** Use `GlyphAdvanceCalculator` to validate encoding; fall back to numeric gap on failure

### 5. Form XObject Coordinate Systems
**Risk:** Form XObjects have their own BBox; isIntersectingSpan() does not account for CTM
**Mitigation:** Document as limitation for MVP; D2/D3 can add CTM tracking

### 6. Annotations and Hyperlinks
**Risk:** Embedding text with links preserves link destinations
**Mitigation:** Emit invisible text without link annotations

### 7. Mixed Rendering Modes
**Risk:** Existing text may use `Tr 1` (stroke), `Tr 2` (fill+stroke), `Tr 4` (clip)
**Mitigation:** Save/restore rendering mode around invisible text emission

---

## Summary of Findings

### What exists:
1. ✅ Text rendering mode (`Tr`) is tracked (line 915, 977–979)
2. ✅ Text operator redaction logic is complete (lines 1010–1112)
3. ✅ Form XObject recursion is implemented (line 1136)
4. ✅ MCID tracking for structure cleanup is in place (lines 982–1008)
5. ✅ Numeric-only gap defense (Edact-Ray) is deployed (lines 1087–1108)

### What is missing:
1. ❌ OCR integration: No OCR results available in redactCanvasRecursively()
2. ❌ Invisible text emission: No code to output `Tr 3` + OCR text
3. ❌ Text encoding: No mapping of UTF-8 OCR text → PDF string encoding
4. ❌ Coordinate transformation: No image-space to text-space conversion
5. ❌ Rendering mode state restoration: No guarantee `Tr` is reset after invisible text

### Recommended D1 approach:
1. **Decouple:** Implement invisible-text injection as post-redaction pass (after line 1215)
2. **Feed OCR:** Query OcrPipeline for each redacted region
3. **Embed:** Inject `BT ... 3 Tr ... (text) Tj ... 0 Tr ... ET` blocks
4. **Encode:** Use GlyphAdvanceCalculator to validate OCR text encoding
5. **Test:** Verify Tr state does not leak; test with CID fonts (CJK)

---

## File Paths Summary

| File | Lines | Purpose |
|------|-------|---------|
| PoDoFoBackend.cpp | 875–1168 | `redactCanvasRecursively()` main engine |
| PoDoFoBackend.cpp | 623–636 | `getEncodedStringWidth()` helper |
| PoDoFoBackend.cpp | 1215 | Call site from `applyRedactions()` |
| PoDoFoBackend.h | 46 | `applyRedactions()` signature |
| OcrEngine.cpp | 236–290 | `processImage()` Tesseract integration |
| OcrPipeline.cpp | 151–208 | `run()` strategy routing |
| OcrTypes.h | 5–9 | `OcrResult` struct |
| GlyphAdvanceCalculator.cpp | 16–64 | `sumAdvances()` encoding-aware width |
| GlyphAdvanceCalculator.h | 1–35 | TextState struct and interface |
