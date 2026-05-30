# M3-PROMPT-5 Walkthrough — InspectorWidget Properties Tab Fully Bound

**Date:** 2026-05-30
**Result:** 21/21 ctest pass (TestInspector 6/6)
**Commits:** eb65984 (model updates) · 5bc2fbe (D1-D6 InspectorWidget binding + TestInspector)
**Author:** Gemini 3.1 Pro (only non-Claude commit in M3-M4 sprint)

---

## COMPLETED

### D1 — Identity tab bound to AnnotationItem
- Author, Created, Modified, Layer labels read from `AnnotationItem` fields.
- ObjectName-stamped labels: `propAuthor`, `propCreated`, `propModified`, `propLayer`.
- Locked QCheckBox (`propLocked`) reads/writes `AnnotationItem::isLocked`.
- All reads triggered by `InspectorWidget::setAnnotation(AnnotationItem*)`.

### D2 — Geometry tab bound (X/Y/W/H read/write)
- `QDoubleSpinBox` fields for X, Y, W, H (`propX`, `propY`, `propW`, `propH`).
- Write path: `QDoubleSpinBox::valueChanged` → `EditAnnotationCommand(AnnotationItem*, newRect)`.
- Command pushed onto `QUndoStack` via AppContext.

### D3 — Appearance tab bound (color swatches, opacity, blend, border)
- Color swatch buttons call `QColorDialog::getColor` → `EditAnnotationCommand` with color change.
- Opacity `QSlider` (0-100, `propOpacity`), Blend `QComboBox` (12 modes, `propBlend`), Border `QSpinBox` (`propBorder`).
- All write paths use `EditAnnotationCommand`.

### D4 — Contents tab bound (QTextEdit read/write)
- `QTextEdit` (`propText`) loads `AnnotationItem::contentsText()`.
- Write: `textChanged` signal → `EditAnnotationCommand`.
- HTML vs plain-text handling: `toPlainText()` for PDF /Contents; `toHtml()` reserved for /RC XHTML (M6-PROMPT-4).

### D5 — Replies sub-panel wired
- ThreadedRepliesWidget lists reply annotations (ISO 32000 /IRT linked items).
- Reply count label updates from `AnnotationItem::replies()`.

### D6 — TestInspector + CMakeLists registration
- 6 tests: testIdentityFieldsPopulated, testGeometrySpinboxes, testOpacitySlider, testBlendCombo, testContentsEditor, testLockedCheckbox.
- CMakeLists.txt: TestInspector registered with qoffscreen deploy (same pattern as TestBatchMode/TestControllers).
- 21/21 ctest pass confirmed.

---

## DEFERRED

| Item | Reason | Target |
|------|--------|--------|
| /RC XHTML transcoding in Contents editor | M6-PROMPT-4 (Djot annotation rich text) | M6 |
| Real PDFium thumbnail thumbnails in Replies | M6-PROMPT-6 | M6 |

---

## SKIPPED

None.

---

## CHANGELOG confirmation

Part of M3-P5 D6 entry in CHANGELOG.md (`feat: Implement InspectorWidget properties binding`).

---

## Patterns discovered

1. **ObjectName-stamped widget convention**: Setting `QWidget::objectName` on every bound control makes `TestInspector` able to find widgets by name (`findChild<QLabel*>("propAuthor")`), avoiding fragile positional indexing.
2. **Single-author session**: This prompt was executed by Gemini 3.1 Pro (not Claude Sonnet) — only non-Claude commit in the M3-M4 sprint. Result quality was equivalent.
3. **Two-commit delivery**: eb65984 (model changes to AnnotationItem) + 5bc2fbe (widget binding) — the model change needed to land first since binding depends on the new accessor methods.
