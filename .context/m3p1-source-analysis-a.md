# M3-PROMPT-1 Source Analysis A — FormBuilderMode + FormManager

## FormBuilderMode.h (Lines 1-12)
**Class Definition:**
```cpp
class FormBuilderMode : public QWidget {
    Q_OBJECT
public:
    explicit FormBuilderMode(QWidget* parent = nullptr);
};
```
- **Minimal interface**: Only constructor, no public methods, slots, or signals defined yet
- **Signals/Slots needed**: Will be added in v1.1 for preview banner → toolbar button interactions
- **No Q_PROPERTY**: State tracking (selected tool type, etc.) not yet exposed

---

## FormBuilderMode.cpp Preview Banner (Lines 17-65)

### Preview Banner & Disable Loop
- **Banner created**: Lines 21-27
  - Text: "Preview — not wired in v1.0.0"
  - Style: rgba(255, 200, 100, 0.92) orange background, #5c3000 dark text
  - Inserted at position 0 in layout (topmost)
  
- **Disable-all loop**: Lines 51-60
  - **Lines 51-52**: Fetch all child widgets via `findChildren<QWidget*>()`
  - **Lines 53-60**: Iterate and disable (skip previewBanner):
    - QPushButton, QLineEdit, QComboBox, QCheckBox, QRadioButton, QToolButton
  - **Effect**: All 9 toolbar buttons + "Auto-detect Fields", "Tab Order", "Preview Form", "Exit Form" are disabled

### 9 Toggle Buttons (Lines 37-46)
**Field type buttons** (line 37: `const QStringList tools`):
1. TEXT FIELD (line 38)
2. CHECKBOX
3. RADIO
4. DROPDOWN
5. LIST BOX
6. DATE
7. NUMERIC
8. SIGNATURE
9. BUTTON

**Button setup** (lines 39-46):
- Lines 39-42: Loop over tools list, create QToolButton per tool
- Line 41: `b->setCheckable(true)` (toggle-able)
- Line 41: `b->setAutoExclusive(true)` (mutually exclusive group)
- Line 42-44: First button checked by default (TEXT FIELD)
- **Line 45**: All buttons added to toolbar layout `tr`

### Additional Toolbar Buttons (Lines 47-50)
- Line 47: "Auto-detect Fields" button (ghost variant)
- Line 48: "Tab Order" button (pill variant, checkable)
- Line 49: "Preview Form" button (ghost variant)
- Line 50: "Exit Form" button (ghost variant)

### PdfViewerWidget Reference (Line 52)
- **Line 52**: `auto* canvas = new PdfViewerWidget;`
- **Created as child**: `new PdfViewerWidget` (no parent passed, implicitly added to layout)
- **Added to layout**: Line 53: `col->addWidget(canvas, 1)` (stretches to fill)
- **No connection**: No signals/slots wired to FormBuilderMode

---

## ModeController.h/cpp Integration Points

### Mode Switching Flow (ModeController.cpp, Lines 30-48)
**setScreen() method**:
- Line 31: Early return if already on screen
- Lines 33-38: Panel-only screens (signature, ai, pdfa, compress, watermark) keep _viewer
- Lines 40-48: Central-swap screens (ocr, redact, compare, pages, batch, **form**)
  - **Line 40**: Check if mode widget exists in `_byId` hash
  - **Lines 42-52**: Lazy initialization
    - Line 51: FormBuilderMode instantiated: `new FormBuilderMode(this)`
    - Line 53: Added to stacked widget: `addWidget(target)`
  - **Lines 55-59**: Activate via `setCurrentWidget(target)`
  - Line 60: Emit `screenChanged("form")`

### Context Available to FormBuilderMode
**At construction (ModeController.cpp, line 51)**:
- `parent` parameter: ModeController (a QStackedWidget)
- **Not passed**: 
  - Document reference (PdfViewerWidget, FormManager)
  - Canvas reference (created locally in FormBuilderMode constructor)
  - Current file path
  - Undo/redo stack

**Accessible post-construction** (from parent ModeController):
- Via `dynamic_cast<ModeController*>(parent)->viewer()` → PdfViewerWidget
- But FormBuilderMode constructor has no parent parameter type info

**Current limitation**: FormBuilderMode is **isolated** — no connection to active document or FormManager instance.

---

## FormManager.h Public API (Lines 11-41)

### Core Field Operations
```cpp
// Extraction & validation
bool extractFormFields(const QString &pdfFilePath) override;
bool hasXfaForms(const QString &pdfFilePath) override;

// Add field by type
bool addTextField(const QString &pdfFilePath, int pageIndex, const QRectF &rect,
                   const QString &fieldName, const QString &outputPath) override;
bool addDateField(const QString &pdfFilePath, int pageIndex, const QRectF &rect,
                   const QString &fieldName, const QString &outputPath) override;
bool addNumericField(const QString &pdfFilePath, int pageIndex, const QRectF &rect,
                     const QString &fieldName, const QString &outputPath) override;
bool addCheckBox(const QString &pdfFilePath, int pageIndex, const QRectF &rect,
                  const QString &fieldName, const QString &outputPath) override;
bool addRadioButton(const QString &pdfFilePath, int pageIndex, const QRectF &rect,
                     const QString &fieldName, const QString &outputPath) override;
bool addDropdown(const QString &pdfFilePath, int pageIndex, const QRectF &rect,
                  const QString &fieldName, const QStringList &options, const QString &outputPath) override;
bool addListBox(const QString &pdfFilePath, int pageIndex, const QRectF &rect,
                 const QString &fieldName, const QStringList &options, bool multiSelect, const QString &outputPath) override;
```

### Fill / Import / Export / Flatten
```cpp
bool fillForm(const QString &pdfFilePath, const QVariantMap &fieldData, const QString &outputPath) override;
bool exportFormData(const QString &pdfFilePath, const QString &outputPath, const QString &format) override;
bool importFormData(const QString &pdfFilePath, const QString &dataFilePath, const QString &outputPath) override;
bool flattenForm(const QString &pdfFilePath, const QString &outputPath) override;
```

### Signature
- All methods are **override**, implementing IFormManager interface
- Return type: all `bool` (success/fail)
- All methods take **absolute file paths** (pdfFilePath, outputPath)
- **No state management**: Each call reads fresh from disk, writes new output

---

## FormManager.cpp Implementation Sections

### AddTextField (Lines 133-158)
- **Lines 135-137**: Load PDF via PoDoFo::PdfMemDocument
- **Lines 138-139**: Validate page index
- **Lines 140-141**: Create PoDoFo::Rect (with Y-flip: `page.GetMediaBox().Height - rect.y() - rect.height()`)
- **Line 143**: `page.CreateField<PoDoFo::PdfTextBox>(fieldName.toStdString(), pdfRect)`
- **Line 144**: Initialize text to empty
- **Line 146**: `doc.Save(outputPath.toUtf8().constData())`
- **Pattern repeats** for addCheckBox, addRadioButton, addDropdown, addListBox, addDateField, addNumericField

### Field Type Handling (addDateField, addNumericField)
- **Lines 197-219** (addDateField):
  - Creates PdfTextBox (like text field)
  - Adds AA (Additional Actions) dictionary
  - Lines 207-208: Format action with JavaScript: `AFDate_FormatEx("yyyy-mm-dd")`
  - Lines 211-212: Keystroke action with JavaScript: `AFDate_KeystrokeEx("yyyy-mm-dd")`
  - Line 214: Attaches AA dict to field
  
- **Lines 253-275** (addNumericField):
  - Similar structure with `AFNumber_Format()` and `AFNumber_Keystroke()`
  - Flags: 2 decimal places, 0 currency, etc.

### Radio Buttons & Flags (Lines 297-334)
- **Lines 315-325** (addRadioButton):
  - Creates `page.CreateField<PoDoFo::PdfRadioButton>()`
  - Reads existing Ff (field flags) from dictionary
  - Sets flag bit 25 (RadiosInUnison): `flags |= (1 << 25)`
  - Writes back: `field.GetDictionary().AddKey(PoDoFo::PdfName("Ff"), ...)`

### Dropdown / ListBox (Lines 336-386)
- **Lines 350-363** (addDropdown):
  - Creates `page.CreateField<PoDoFo::PdfComboBox>()`
  - Inserts options via `field.InsertItem(PoDoFo::PdfString(opt.toStdString()))`
  - Sets selected index to 0
  - Sets flag bit 18 (Edit): `flags |= (1 << 18)`
  
- **Lines 388-410** (addListBox):
  - Creates `page.CreateField<PoDoFo::PdfListBox>()`
  - If multiSelect, sets flag bit 21: `flags |= (1 << 21)`

### FillForm Integration (Lines 65-131)
- **Line 69**: Load document
- **Lines 70-74**: Get AcroForm, iterate fields
- **Lines 75-131**: Type-switch on field.GetType():
  - PdfFieldType::TextBox → `textField->SetText(PdfString(val.toString()))`
  - PdfFieldType::CheckBox → `checkBox->SetChecked(val.toBool())`
  - PdfFieldType::ComboBox → `comboBox->SetSelectedIndex(idx)`
  - PdfFieldType::ListBox → `listBox->SetSelectedIndex(idx)`
  - **Calls dynamic_cast** to downcast from base field type
  - **Sets ReadOnly**: `field.SetReadOnly(true)` (line 130)

### Export to CSV/FDF (Lines 420-482)
- **Lines 426-427**: Iterate AcroForm fields
- **Lines 429-458**: Extract value by type (TextBox, CheckBox, ComboBox, etc.)
- **Lines 461-470**: CSV format with escaping
- **Lines 471-481**: FDF format with JavaScript escaping (backslash parens)

### Import from CSV/FDF (Lines 484-531)
- **Lines 486-487**: Open file, read content
- **Lines 490-507**: If FDF, regex parse `<< /T (name) /V (value) >>`
- **Lines 509-525**: If CSV, split on `","`
- **Line 528**: Call fillForm() with parsed data map

### FlattenForm (Lines 533-600)
- **Lines 537-540**: Load document, get AcroForm
- **Lines 541-545**: Get Helvetica font
- **Lines 547-600**: For each field:
  - Find page via /P reference (lines 551-566)
  - Extract current value based on type (lines 571-592)
  - If page found, use PdfPainter to draw value as text (lines 594-598)
  - Set ReadOnly and annotation flags (lines 600-601)
- **Lines 603-617**: Remove AcroForm from catalog and trailer, remove Widget annotations

---

## Integration Points Needed for v1.1

### 1. FormBuilderMode ↔ Document Access
**Currently missing**:
- No reference to active document file path
- No reference to FormManager instance
- No reference to current page index
- Canvas (PdfViewerWidget) created locally but not wired to document

**Needed for v1.1**:
```cpp
// In FormBuilderMode::FormBuilderMode(QWidget* parent)
FormManager* formManager;        // Injected or passed
QString currentPdfPath;           // From parent ModeController
int currentPageIndex = 0;         // From canvas/document
PdfViewerWidget* canvas;          // Reuse from parent, don't create locally
```

### 2. FormBuilderMode Toolbar → Document Commands
**Currently disabled**: All 9 field buttons + Auto-detect/Tab Order/Preview/Exit

**Signal/Slot wiring needed** (pseudo-code):
```cpp
// TextFieldButton clicked → addTextField()
connect(textFieldButton, &QToolButton::clicked, this, [this]() {
    // Wait for canvas mouse click to define rect
    // Then: formManager->addTextField(currentPdfPath, currentPageIndex, rect, "field_N", tempPath)
});

// Similar for CHECKBOX, RADIO, DROPDOWN, etc.
```

### 3. Rectangle Capture from Canvas
**Missing**: Method to capture user's click-drag on canvas to create QRectF

**Needed**:
- Mouse down on canvas → store start point
- Mouse drag → show preview rect
- Mouse release → confirm rect, pass to FormManager::addTextField() etc.
- Coordinate system: Canvas coords (0,0 top-left) → PDF coords (with Y-flip as FormManager does)

### 4. Undo/Redo Integration
**Missing**: No command object for AddFormFieldCommand

**Needed**:
- Derive AddFormFieldCommand from QUndoCommand
- Wrap FormManager::addTextField() calls
- Push to undo stack on field creation
- Support undo() = remove field, redo() = re-add field

### 5. FormBuilderMode ↔ ModeController Parent Access
**Currently broken**:
- ModeController lazy-creates FormBuilderMode: `new FormBuilderMode(this)`
- FormBuilderMode has no way to get back to parent or its context

**Solution** (v1.1):
```cpp
// Option A: Store parent as typed ModeController*
ModeController* modeCtrl = qobject_cast<ModeController*>(parent());
if (modeCtrl) {
    PdfViewerWidget* viewer = modeCtrl->viewer();
    // Access document from viewer
}

// Option B: Accept dependencies in constructor
FormBuilderMode::FormBuilderMode(
    PdfViewerWidget* canvas,
    FormManager* formMgr,
    const QString& pdfPath,
    int pageIndex,
    QWidget* parent = nullptr);
```

### 6. Connect/Disconnect Pattern
**Signals needed in FormBuilderMode**:
- `fieldAdded(int pageIndex, QRectF rect, QString fieldName)` → update canvas
- `fieldDeleted(QString fieldName)` → update UI
- `toolSelected(QString toolType)` → UI state

**Connections needed**:
- ModeController.screenChanged("form") → FormBuilderMode::onEnterMode()
  - Load current document, page count, existing fields
- FormBuilderMode buttons clicked → emit signals → canvas responds
- Canvas mouse events → FormBuilderMode::onCanvasClick() → rectangle capture

---

## Summary: Wiring Checklist for v1.1

| Component | Current State | Needed for v1.1 |
|-----------|--------------|-----------------|
| **FormBuilderMode.h** | Stub (12 lines) | + public slots for tool selection, canvas events; + private: FormManager*, QString path, int page |
| **FormBuilderMode.cpp** | Preview banner + disabled buttons (65 lines) | + Rectangle capture from canvas; + FormManager method calls; + Undo/redo integration |
| **ModeController** | Lazy-creates mode, no context passing | + Inject document/FormManager references to FormBuilderMode at creation |
| **FormManager** | Fully implemented (600 lines) | No changes needed; already handles all field types + FDF/CSV import/export |
| **IFormManager** | Interface defined (36 lines) | No changes needed |
| **Canvas PdfViewerWidget** | Created locally in FormBuilderMode | Reuse from parent ModeController, wire mouse events for rect capture |
| **Signals/Slots** | Missing | Add fieldAdded(), fieldDeleted(), toolSelected(), onCanvasClick(), onEnterMode() |
| **AddFormFieldCommand** | Not found in repo | Create as QUndoCommand wrapper for FormManager::addTextField() etc. |
