# M3-PROMPT-4 Source Analysis — RedactMode

## RedactMode.cpp preview shell

- **Preview banner**: lines 23–29
  - Text: `"Preview — not wired in v1.0.0\n\nUse the ribbon toolbar for production-ready operations.\nThis mode page is scheduled for v1.1."`
  - Widget: `QLabel` with objectName `"modePreviewBanner"`
  - Styling: yellow/orange warning background (`rgba(255, 200, 100, 0.92)`)

- **"Mark by Pattern" pill**: line 48
  - Widget type: `QToolButton`
  - Text: `tr("Mark by Pattern ▾")` (dropdown indicator)
  - Property: `variant="ghost"` with `setCheckable(true)`
  - Status: exists in UI layout but **all interactive child widgets are disabled** (lines 72–81)

- **Existing real code**: 
  - Lines 1–17: includes (Qt and GlyphPDF headers)
  - Lines 18–82: constructor only — **no event handlers, no backend calls**
  - All interaction disabled by loop at lines 72–81: `w->setEnabled(false)` for buttons, line edits, combos, checkboxes, radio buttons, toolbuttons
  - **Verdict**: pure UI shell with disabled controls; no wired behavior

---

## SecurityController.cpp stubs

- **PatternRedact stub**: lines 84–87
  - Triggered by: `case ToolId::PatternRedact:`
  - Exact QMessageBox: `QMessageBox::information(_mainWindow, tr("Document Security"), tr("Advanced permission controls and security tools are scheduled for the next engine update."))`
  - **No separate message for PatternRedact**; grouped with RegexRedact and others

- **RegexRedact stub**: lines 85–87 (same as PatternRedact)
  - Same QMessageBox as above
  
- **Other redaction stubs** in same case block:
  - `ToolId::Permissions` (line 80)
  - `ToolId::RemoveSecurity` (line 81)
  - `ToolId::Certify` (line 82)
  - `ToolId::Timestamp` (line 83)
  - All share the **single stub message** at lines 86–87

---

## PatternRedactor.h

- **Exists**: NO
- File not found in any engine directory (`src/engines/` searched)
- **Implication**: PatternRedactor class must be designed from scratch for D2 deliverable

---

## PdfEditorEngine.h redaction methods

- **applyRedactions**:
  ```cpp
  bool applyRedactions(int pageIndex, const QList<QRectF> &rects) override;  // line 63
  ```
  - Signature: takes page index + list of rectangles (existing API from D1)
  - Implementation: `PoDoFoBackend::applyRedactions` at lines 1192–1361 (fully functional for D1)

- **applyPatternRedactions**: 
  - **Absent** from both `PdfEditorEngine.h` (lines 1–81) and `IPdfEditorEngine.h` (interface)
  - Must be added to interface + engine for D2

- **findText / searchText**:
  - **Absent** from both `PdfEditorEngine.h` and `IPdfEditorEngine.h`
  - Not implemented; must be designed for D2 (required for pattern matching across pages)

---

## PDFium text extraction with per-character bounding boxes

- **FPDFText_GetCharBox**: EXISTS in codebase
  - Located: `src/engines/pdfium/PdfiumBackend.cpp` lines 188, 223
  - Signature: `FPDFText_GetCharBox(textPage, charIndex, &c_left, &c_right, &c_bottom, &c_top)`
  - Returns: bounding box in PDF coordinates for individual character at `charIndex`
  - **Status**: already integrated in PdfiumBackend for text extraction

- **FPDFText_LoadPage**: also present (line 164, 223)
  - Creates `FPDF_TEXTPAGE` for per-character analysis

- **How PatternRedactor will get per-character coordinates**:
  1. Use `FPDFText_LoadPage(page)` to get text page handle
  2. Iterate characters with `FPDFText_GetCharBox()` for each match
  3. Collect bounding boxes into `QList<QRectF>` for `applyRedactions()`
  4. Invoke existing D1 redaction engine with collected rectangles

---

## Integration approach for D1–D4

### D1: applyRedactions (existing, functional)
- Takes rectangles, removes content stream text operators via `redactCanvasRecursively()` (PoDoFoBackend.cpp lines 875–1190)
- Applies visual black boxes, sanitizes structure tree, generates audit log
- Called by SecurityController::applyRedactions (lines 332–412)

### D2: PatternRedactor class design (stub → real)
**Required API surface:**
```cpp
class PatternRedactor {
  public:
    // Find all occurrences of pattern on page
    QList<QRectF> findMatches(int pageIndex, const QString &pattern, bool useRegex=false);
    
    // Apply redactions for found matches
    bool redactPattern(int pageIndex, const QString &pattern, bool useRegex=false);
};
```

**Implementation path:**
- Use `FPDFText_LoadPage()` + `FPDFText_GetCharBox()` from PDFium backend
- Regex: use Qt's `QRegularExpression`
- Collect bounding boxes → pass to existing `applyRedactions(pageIndex, rects)`

### D3: RedactMode.cpp wiring
- Enable "Mark by Pattern" pill → call `PatternRedactor::findMatches()`
- Highlight matches on canvas overlay
- Apply button → call `PatternRedactor::redactPattern()`

### D4: SecurityController routing
- Replace stub at lines 84–87 with real `PatternRedactor` call
- Show match count in status bar (reuse D1 progress dialog pattern at lines 359–412)
- Validate non-empty matches before proceeding (similar to line 346–348 check for redaction regions)

---

## Summary: Readiness for implementation

| Component | Status | Blocker? |
|-----------|--------|----------|
| D1 applyRedactions | ✅ Fully functional | No |
| D1 applyRedactions (PoDoFoBackend) | ✅ Defensive text scrubbing + audit log (lines 1192–1361) | No |
| RedactMode.h/.cpp skeleton | ✅ UI exists, disabled | No |
| PatternRedactor.h | ❌ Absent | Yes (design from scratch) |
| PdfEditorEngine.applyPatternRedactions | ❌ Absent | Yes (add interface + impl) |
| IPdfEditorEngine.findText/searchText | ❌ Absent | Yes (add interface) |
| SecurityController PatternRedact branch | ✅ Stub exists (lines 84–87) | No (ready to replace) |
| PdfiumBackend FPDFText_GetCharBox | ✅ Integrated (lines 164, 188, 223) | No |

**Critical path for D2–D4:**
1. Design `PatternRedactor` class using PDFium's `FPDFText_GetCharBox()` for bounding boxes
2. Add `applyPatternRedactions()` to `IPdfEditorEngine` + `PdfEditorEngine`
3. Wire SecurityController case at lines 84–87 to new PatternRedactor
4. Enable RedactMode.cpp controls and connect to PatternRedactor backend
