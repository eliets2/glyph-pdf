# M2-PROMPT-3 Source Analysis A: PdfAValidationPanel + Engine

## Executive Summary

The current `PdfAValidationPanel` is a **hardcoded mock UI** with static violation data and no button handlers connected. The engine's `exportPdfA()` method exists and works with PoDoFo's PDF/A conversion (via PoDoFoBackend), but the panel has no integration with validation logic. **No VeraPdf references exist** in the codebase yet.

---

## PdfAValidationPanel Structure

### File: `src/modes/PdfAValidationPanel.cpp` (lines 33–88)

**Class Hierarchy:**
- `PdfAValidationPanel : public QFrame` with `Q_OBJECT` macro
- Inherits from Qt's `QFrame`; acts as a right-sidebar widget panel
- Instantiated once per app lifetime in `GpMainWindow::switchMode(id="pdfa")`

**Constructor (lines 33–88) — Hardcoded Content:**

The panel builds a static UI with:

1. **Header Bar** (lines 40–44): Label "PDF/A · VALIDATION" with fixed height 32px

2. **Summary Card** (lines 51–65): 
   - Title: "PDF/A-2B VALIDATION"
   - Hardcoded badge: "WARNINGS"
   - Hardcoded metadata: `"47 RULES · 2 FAILURES · 3 WARNINGS\nCONFORMANCE: PDF/A-2b (ISO 19005-2)"`
   - **All numbers are fake** — no actual validation is performed

3. **Issues List** (lines 67–75): 
   - Section heading "ISSUES · 5" (hardcoded count)
   - Five `issueRow()` calls with static violation data:
     - Rule 6.2.3.3-1 (error): "Embedded fonts required · Page 3"
     - Rule 6.2.8-1 (error): "Non-embedded font: Arial · Page 7"
     - Rule 7.1-1 (warning): "Optional content (layers) detected"
     - Rule 6.3.3-1 (warning): "Annotation appearance stream missing"
     - Rule 6.2.10-1 (warning): "Device-dependent color space in image"

4. **Three Action Buttons** (lines 77–83):
   ```cpp
   auto* fix = new QPushButton(tr("Fix Automatically (3)"));      // line 77
   auto* conv = new QPushButton(tr("Convert to PDF/A-2B"));       // line 78
   auto* exp = new QPushButton(tr("Export Report"));              // line 80
   ```
   - **NO `connect()` calls** — buttons are created but inactive
   - All three buttons are added to the layout (`col->addWidget(...)`)
   - No signals are wired to any slots

### PdfAValidationPanel.h (lines 1–13)

**Public API:**
```cpp
class PdfAValidationPanel : public QFrame {
    Q_OBJECT
public:
    explicit PdfAValidationPanel(QWidget* parent = nullptr);
};
```

- **Only the constructor** is public — no other methods or signals
- **No member variables** — panel is purely UI; holds no state
- **No way to:**
  - Set the current document path
  - Update violation results
  - React to button clicks
  - Know which PDF is being validated

---

## IPdfEditorEngine.h — PdfA Types

### File: `src/core/interfaces/IPdfEditorEngine.h`

**PdfA-Related Method:**
```cpp
virtual bool exportPdfA(const QString &outputPath, int conformanceLevel) = 0;  // line 100
```

- **Parameter:** `conformanceLevel` (int) — 0=1B, 2=2B, 3=3B (inferred from PoDoFoBackend switch)
- **Return:** bool success/failure
- **Purpose:** Convert current loaded document to PDF/A and save

**PdfA-Related Types:**
- **No enum for conformance levels** in the interface
- **No struct for validation results** (e.g., no `PdfAValidationReport`)
- **No validation methods** (e.g., `validatePdfA()`, `getPdfAViolations()`, etc.)

---

## PdfEditorEngine.cpp — exportPdfA Implementation

### File: `src/engines/PdfEditorEngine.cpp` (lines 254–266)

```cpp
bool PdfEditorEngine::exportPdfA(const QString &outputPath, int conformanceLevel)
{
    QMutexLocker locker(&d->mutex);
    d->clearErr();
    if (!d->backend) return d->noBackend("exportPdfA");
    bool ok = d->backend->exportPdfA(outputPath, conformanceLevel);
    if (!ok)
        d->setErr(ErrorInfo::Error,
                  QObject::tr("PDF/A export failed. The document may contain features incompatible with the selected conformance level."),
                  QStringLiteral("exportPdfA level=%1, path=%2").arg(conformanceLevel).arg(outputPath),
                  ErrorInfo::Retry);
    return ok;
}
```

**Behavior:**
- Delegates to `d->backend->exportPdfA(...)` (PoDoFoBackend)
- Thread-safe via `QMutexLocker`
- Sets error via `ErrorInfo` on failure
- **No validation performed** — only conversion

### PoDoFoBackend::exportPdfA (lines 1360–1419)

**Implementation Strategy:**
- Maps `conformanceLevel` int to PoDoFo's `PdfALevel` enum:
  - 1 (default) → `L1B` (PDF/A-1b, v1.4)
  - 2 → `L2B` (PDF/A-2b, v1.7)
  - 3 → `L3B` (PDF/A-3b, v2.0)
- Sets metadata: PDF version, PDF/A level, producer, creator
- Adds Output Intent dict to catalog (required for PDF/A)
- Calls `d->document->Save(...)` to write the file

**No validation step** — just structural conversion.

---

## VeraPdf References in Codebase

### grep Results:

Files mentioning VeraPdf (9 found):
1. `ROADMAP.md` — planning document
2. `CHANGELOG.md` — version history
3. `SESSION_BRIEF_NEXT.md` — next-session notes
4. `CLAUDE.md` — project brief / memory
5. `MONTHS-2-8-PROMPTS.md` — long-term planning
6. `MSYS2-MIGRATION.md` — environment setup guide
7. `M1-REMEDIATION.md` — M1 phase notes
8. `AUDIT-v1.0.0.md` — audit document
9. `README.md` — main readme

**Result:** No VeraPdf subprocess, wrapper, or validator class exists in source code. References are only in documentation/planning.

---

## Integration Points & Gaps

### How PdfAValidationPanel is Shown

**File:** `src/GpMainWindow.cpp` (lines 460–462)
```cpp
} else if (id == "pdfa") {
    if (!_pdfaPanel) _pdfaPanel = new PdfAValidationPanel(this);
    replaceRight(_pdfaPanel);
}
```

- Panel is created on-demand when mode "pdfa" is selected
- Stored in `GpMainWindow::_pdfaPanel` (QPointer-like member in .h line 86)
- **No way to pass document path or trigger validation on show**

### Document Path & Conformance — NOT Currently Available in Panel

**Missing from PdfAValidationPanel:**
- No `currentDocPath` member
- No `currentConformance` member or enum
- Panel cannot access `GpMainWindow`'s loaded document
- **Panel is truly stateless**

**Available at app level:**
- `GpMainWindow` has `IPdfEditorEngine* _engine`
- `_engine->currentFile()` returns the open PDF path
- Conformance level would need to be passed in or stored elsewhere

### Where VeraPdfValidator Should Be Called

**Logical placement:**
1. **Option A (Reactive):** Connect button to VeraPdf subprocess
   - When user clicks "Fix Automatically" or "Convert" button
   - Call VeraPDF CLI to validate the current document
   - Parse subprocess output into violation list
   - Update panel UI

2. **Option B (Proactive):** Auto-validate on panel show
   - When `switchMode("pdfa")` is called
   - Get current file path from engine
   - Launch VeraPDF subprocess
   - Populate panel with real results

3. **Recommended:** Add signals/slots to PdfAValidationPanel
   - Public method: `setDocumentPath(const QString&)`
   - Public method: `setConformanceLevel(int)`
   - Public signal: `validationRequested()`
   - Button connect() calls in GpMainWindow or controller

---

## Current Implementation Gaps

| Component | Current State | Required |
|-----------|---|---|
| **Panel state** | None | `currentDocPath`, `currentConformance` (int or enum) |
| **Buttons** | Created, no handlers | `connect()` to slots for Fix/Convert/Export |
| **Validation** | Hardcoded mock data | VeraPDF subprocess integration |
| **Engine support** | `exportPdfA()` only | Add `validatePdfA()` returning structured results |
| **UI updates** | Static on construct | Dynamic based on subprocess results |
| **Error handling** | Basic in engine | Subprocess stderr parsing & error display |

---

## Type Summary for M2-PROMPT-3

**exportPdfA Signature (IPdfEditorEngine):**
```cpp
virtual bool exportPdfA(const QString &outputPath, int conformanceLevel) = 0;
```

**conformanceLevel values** (from PoDoFoBackend):
- `1` (default) = PDF/A-1b
- `2` = PDF/A-2b
- `3` = PDF/A-3b

**No existing enum** — use plain `int` or define:
```cpp
enum PdfAConformance { A1B = 1, A2B = 2, A3B = 3 };
```

**No validation return type** — would need:
```cpp
struct PdfAViolation {
    QString ruleId;
    QString description;
    int pageNumber;
    bool isError;  // vs warning
};

struct PdfAValidationResult {
    int totalRules;
    int errorCount;
    int warningCount;
    QList<PdfAViolation> violations;
    QString conformanceLevel;
};
```

---

## Notes for Next Session (M2-PROMPT-4)

1. **Extract VeraPdf binary path** from system or embedded resources
2. **Design PdfAValidationPanel API:**
   - Accept `currentDocPath` and `currentConformance` at show/update time
   - Wire button clicks to engine/validator calls
3. **Subprocess integration:**
   - Launch `verapdf --format json <inputPdf>` (or appropriate flags)
   - Parse JSON results
   - Populate panel dynamically
4. **Engine enhancement:**
   - Add optional `validatePdfA(...)` method returning violations
   - Or integrate validation into `exportPdfA()` workflow
5. **Error states:**
   - Handle subprocess crash
   - Handle missing VeraPdf binary
   - Display user-friendly messages