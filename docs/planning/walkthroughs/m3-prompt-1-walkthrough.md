# M3-PROMPT-1 Walkthrough — FormBuilderMode Wiring

**Date:** 2026-05-29
**Commits:** D1 `faac7f2` · D2 `2e5ed03` · D3 `ce6df8c` · D4 `fdba6a8` · D6 `9e2cc42` · D7 `aea843a`
**Test result:** 18/18 ctest pass (0 failures)

---

## 1. What is complete

### D1 — Remove preview banner; connect to active document (`faac7f2`)
- Orange v1.0.0 preview banner (QLabel) and disable-all-children loop deleted from FormBuilderMode.cpp
- 9 new `ToolMode` enum values added to `src/core/PdfEnums.h`: `FormAddText`, `FormAddCheckbox`, `FormAddRadio`, `FormAddDropdown`, `FormAddListBox`, `FormAddDate`, `FormAddNumeric`, `FormAddSignature`, `FormAddButton`
- `ModeController::setAppContext(const AppContext*)` added; `GpMainWindow` calls it after `_modes` construction
- `FormBuilderMode` constructor signature changed to `(const AppContext*, PdfViewerWidget*, QWidget*)` — receives context and shared canvas without taking ownership
- All 9 field-type toolbar buttons enabled when a document path is set in AppContext; `no-document` placeholder label shown otherwise
- Left sidebar: `QListWidget` tracking placed field names in session; Delete Field button wired to `DeleteFormFieldCommand`
- Right sidebar: `FormFieldPropertiesPanel` (hidden until field selected) + tab order panel
- ESC shortcut cancels active field placement, restores `HandTool`

### D2 — Click-to-place rubber band workflow (`2e5ed03`)
- `PdfViewerWidget`: `fieldPlacementRequested(int, QRectF, ToolMode)` signal added
- `isFormBuilderMode(ToolMode)` static helper covers all 9 FormAddXxx values
- `setToolMode()`: all FormAddXxx modes set `Qt::CrossCursor`
- `mousePressEvent/mouseMoveEvent/mouseReleaseEvent`: separate `m_formRubberBand` / `m_isPlacingField` state for form placement, replicating the Crop mode pattern exactly; on release emits `fieldPlacementRequested` with PDF-space rect (divided by `m_zoomFactor`)
- `FormBuilderMode::onFieldPlacementRequested`: maps `ToolMode` → `AddFormFieldCommand::FieldType` via `toolModeToFieldType()`; generates unique field name; pushes `AddFormFieldCommand` to `QUndoStack`; resets canvas to `FormAddText` after placement

### D3 — FormFieldPropertiesPanel + EditFormFieldCommand (`ce6df8c`)
- `src/modes/FormFieldPropertiesPanel.h/.cpp`: QWidget with 6 editable properties (name/tooltip/required/default/placeholder/regex); live red-border validation for empty name and invalid QRegularExpression; Apply pushes EditFormFieldCommand
- `src/commands/EditFormFieldCommand.h`: QUndoCommand (id 0x105); stores old+new `EditFormFieldProperties`; applies default value via `IFormManager::fillForm` in redo(); undo restores old default; full name/tooltip/JS-action metadata persistence deferred to v1.1

### D4 — Field move/resize/delete commands (`fdba6a8`)
- `DeleteFormFieldCommand` (id 0x106): removes from UI list; engine-side `removeField()` deferred to v1.1
- `MoveFormFieldCommand` (id 0x107): records old/new QRectF; engine-side move deferred to v1.1
- `ResizeFormFieldCommand` (id 0x108): records old/new QRectF; engine-side resize deferred to v1.1
- Simplified D4 approach documented: field list QListWidget + Delete button (no drag-handle overlay); move/resize via commands reserved for v1.1 overlay implementation

### D5 — Tab order editor (integrated in D1 commit `faac7f2`)
- `m_tabOrderPanel`: QFrame with `QListWidget` (drag-drop reorder) + "Apply Order" button
- Tab order list populated from field list when "Tab Order" button checked
- `onTabOrderApplyClicked()`: shows informational dialog that tab order is recorded in the view; PDF AcroForm `/CO` array persistence deferred to v1.1 (IFormManager::setTabOrder not yet exposed)

### D6 — Tests (`9e2cc42`)
- `tests/TestFormBuilder.cpp`: 5 headless tests (QT_QPA_PLATFORM=offscreen)
  - `testPlaceTextField`: AddFormFieldCommand::Text push; document still exists post-push
  - `testPlaceAllFieldTypes`: all 7 FieldType values placed; stack count = 7
  - `testEditFieldProperties`: place + edit + undo/redo index transitions
  - `testDeleteField`: place + delete + undo/redo; fieldName/pageIndex accessors verified
  - `testTabOrderClientSide`: 3 fields, full undo/redo cycle; stack index verified
- CMakeLists.txt: `TestFormBuilder` target registered; `EditFormFieldCommand.h`, `DeleteFormFieldCommand.h`, `MoveFormFieldCommand.h`, `ResizeFormFieldCommand.h` added to `pdfws_commands`; `FormFieldPropertiesPanel.h/.cpp` added to `pdfws_ui`

### D7 — Documentation (`aea843a`)
- `CHANGELOG.md`: Form Builder section added under `[Unreleased]`
- `ROADMAP.md`: Progress table updated with M2 + M3-P1 entries; M3-P1 marked ✅

---

## 2. What is deferred and why

| Item | Deferred to | Reason |
|------|-------------|--------|
| Engine-side `removeField()` in `DeleteFormFieldCommand` | v1.1 | `IFormManager` does not expose `removeFieldById()` |
| Engine-side `updateFieldRect()` in `MoveFormFieldCommand` / `ResizeFormFieldCommand` | v1.1 | `IFormManager` does not expose `updateFieldRect()` |
| Full name/tooltip/JS-action persistence in `EditFormFieldCommand` | v1.1 | Requires PoDoFo direct access; blocked on `IFormManager::updateFieldMetadata` API |
| Drag-handle overlay for move/resize on canvas | v1.1 | Complex QGraphicsItem overlay; D4 uses simplified QListWidget path as documented |
| `FormManager::setTabOrder()` + PDF `/CO` array write | v1.1 | Tab order recorded client-side only |
| "Preview Form" button functionality | v1.1 | Button is `setEnabled(false)` with tooltip "Preview filled form — Coming in v1.1" |
| "Auto-detect Fields" button | v1.1 | Button is `setEnabled(false)` with tooltip "Auto-detect form fields from document structure — Coming in v1.1" |
| `FormAddSignature` / `FormAddButton` mapped to `FieldType::Text` | v1.1 | `AddFormFieldCommand::FieldType` does not have Signature/Button values; these are mapped to Text fields; proper PoDoFo PdfPushButton / PdfSignatureField creation is v1.1 |
| Reload canvas after field placement | v1.1 | `m_doc->markReload()` signals reload intent but `PdfViewerWidget` does not yet react to `DocumentSession::reloadRequested` in FormBuilder context |

---

## 3. Tests added vs skipped

| Test | Status | Notes |
|------|--------|-------|
| `testPlaceTextField` | Added, passes | Verifies AddFormFieldCommand::Text executes without crash |
| `testPlaceAllFieldTypes` | Added, passes | Covers all 7 FieldType enum values |
| `testEditFieldProperties` | Added, passes | Undo/redo index verification |
| `testDeleteField` | Added, passes | Delete command metadata + undo/redo |
| `testTabOrderClientSide` | Added, passes | Undo/redo cycle for 3 fields |
| UI integration (FormBuilderMode widget) | Skipped | Requires QApplication; TestControllers pattern would work but was not specified in D6 scope |
| Canvas rubber band signal/slot round-trip | Skipped | Requires loading a real PDF into PdfViewerWidget; integration test left for v1.1 |

---

## 4. TODO comments inserted

Zero. No `TODO` comments were inserted into production-bound source files.

---

## 5. CHANGELOG updated confirmation

`CHANGELOG.md` updated in commit `aea843a` (D7). Section added under `[Unreleased]`:
```
### Form Builder (M3-PROMPT-1 — 2026-05-29)
```
