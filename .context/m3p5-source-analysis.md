# M3-PROMPT-5 Source Analysis — InspectorWidget Properties Tab

## InspectorWidget.cpp Properties tab (lines 331-553)

**Header section (lines 311-357):**
- Type name QLabel: lines 331-337, hardcoded to "HIGHLIGHT"
  - Bound to real data: **YES** — updated in refreshProperties() line 681-696 via ToolMode enum
- Annotation ID QLabel: lines 339-344, hardcoded to "#001"
  - Bound to real data: **NO** — not connected to `m_currentAnnotation->id`
- Sub-header QLabel: lines 350-355, hardcoded to "Page 1 · Default Layer · Unlocked"
  - Bound to real data: **PARTIAL** — page index updated in refreshProperties() line 702-703, layer/lock status hardcoded

**Identity section (lines 359-370):**
- 4 field rows (Author, Created, Modified, Subject): lines 365-368
  - Created with `createFieldRow()` utility, all show placeholder values ("Unknown", "--")
  - Bound to real data: **NO** — fields never populated from `m_currentAnnotation`
  - Should bind to: author, creationDate, (no modified date field), (no subject field in AnnotationItem)

**Appearance section (lines 372-407):**
- 6 color swatches: lines 378-391, hardcoded colors "#f5c542", "#42a5f5", "#ef5350", "#66bb6a", "#ab47bc", "#ff8c42"
  - Type: QLabel (via `createColorSwatch()`)
  - Wired: **NO** — click handlers not connected; no binding to `m_currentAnnotation->color`
- Opacity slider: line 396, shows "100%"
  - Type: created via `createFieldRow("Opacity", "100%")` — likely QLabel, not interactive
  - Wired: **NO** — not a QSlider; no connection to data
- Blend mode combo: line 400, shows "Normal"
  - Type: created via `createFieldRow("Blend", "Normal")` — likely QLabel, not a QComboBox
  - Wired: **NO** — read-only display
- Border width spinbox: line 404, shows "2px"
  - Type: created via `createFieldRow("Border", "2px")` — likely QLabel, not a QSpinBox
  - Wired: **NO** — not editable; not connected to `m_currentAnnotation->thickness`

**Position section (lines 409-475):**
- X/Y/W/H grid: lines 431-438
  - All created as QLabel pairs (label "X"/"Y"/"W"/"H" + value "0.00")
  - Bound: **NO** — hardcoded to "0.00"; not connected to `m_currentAnnotation->rect`
  - Should bind to: rect.x(), rect.y(), rect.width(), rect.height()
- Align buttons: lines 442-471
  - 6 QPushButton instances, lines 459-470
  - Wired: **NO** — tooltip text set, no click handlers connected; buttons are decorative
  - Should trigger: alignment operations (none implemented)

**Contents section (lines 477-510):**
- QTextEdit: lines 483-500, placeholder "Annotation text content..."
  - Bound to annotation.text: **PARTIAL** — editor exists but refreshProperties() does NOT populate it
  - Line 667-705 refreshProperties() only updates header (color, type, page), NOT text content
  - Contents tab (lines 512-551): reply thread stub

**Reply thread section (lines 512-551):**
- QLabel placeholder "No replies yet": line 519
- "Add reply" button: lines 527-545
- Shows real replies: **NO** — hardcoded empty state; `m_currentAnnotation->replies` never displayed
- Section hidden by default: line 549 (`setMaximumHeight(0)` + `setVisible(false)`)

---

## AnnotationItem
**File:** `C:\Users\User\Projects\pdf\src\core\AnnotationTypes.h` (lines 19-35)

**Fields (exact types and names):**
```cpp
int pageIndex = 0;                    // Page number (0-indexed)
ToolMode mode = ToolMode::HandTool;  // Annotation type (Highlight, etc.)
QList<QPointF> points;               // Free-form drawing points
QColor color = Qt::yellow;           // Display color
int thickness = 2;                   // Line/stroke width
QString text;                        // Annotation content text
QRectF rect;                         // Bounding rectangle (x, y, w, h)
QString id;                          // Unique identifier
QString parentId;                    // Parent annotation ID (threading)
QList<QString> replies;              // List of reply IDs
QString author;                      // Creator name
QString creationDate;                // ISO timestamp or human-readable
ReviewState reviewState = ReviewState::None;  // Open/Accepted/Rejected/Cancelled/Completed
```

---

## AnnotationLayer
**File location:** `C:\Users\User\Projects\pdf\src\ui\AnnotationLayer.h`

**Key signals:**
- `annotationsChanged()` — fired when annotation list modified
- `selectionChanged(int index)` — fired when user selects/deselects annotation
- `textEditRequested(int pageIndex, QPointF pos)` — request inline text editing
- `imageSelected(const QString &xobjectName, const QRectF &placement)` — image selection
- `imageMoved(const QString &xobjectName, double dx, double dy)` — image drag
- `imageResized(const QString &xobjectName, double newW, double newH)` — image resize

**How it populates InspectorWidget:**
- **No existing connection found.** AnnotationLayer stores `m_annotations` (line 56) but does NOT emit annotation detail signals.
- InspectorWidget must be manually notified when selection changes (via external caller invoking `setAnnotation(item)`).
- AnnotationLayer.h does NOT have a method to retrieve annotation by index for Inspector binding.

---

## EditAnnotationCommand
**Exists:** **YES** — `C:\Users\User\Projects\pdf\src\commands\EditAnnotationCommand.h`

**Constructor signature:**
```cpp
EditAnnotationCommand(PdfViewerWidget* viewer,
                      DocumentSession* doc,
                      const QList<AnnotationItem>& oldAnns,
                      const QList<AnnotationItem>& newAnns)
```

**Undo/Redo pattern:**
- `redo()` — calls `applyAnnotations(m_newAnns)` (line 22-23)
- `undo()` — calls `applyAnnotations(m_oldAnns)` (line 26-27)
- `id()` override — returns `0x105` (line 30), allows command merging
- Internal method `applyAnnotations(const QList<AnnotationItem>&)` applies full annotation list
- Uses `QPointer<PdfViewerWidget>` for safe null-checking if viewer is deleted

---

## AppContext annotation + undo stack access
**File:** `C:\Users\User\Projects\pdf\src\core\AppContext.h` (lines 19-32)

**Relevant fields:**
```cpp
std::shared_ptr<QUndoStack> undoStack;        // Line 27 — for undo/redo commands
std::shared_ptr<DocumentSession> document;    // Line 28 — holds PDF document state
```

- **No annotation layer field.** AppContext does not directly expose the AnnotationLayer.
- Must be accessed through DocumentSession or via PdfViewerWidget.
- EditAnnotationCommand takes `DocumentSession* doc` parameter to persist changes.

---

## What needs binding (summary)

**Identity section:**
1. Author QLabel → `m_currentAnnotation->author`
2. Created QLabel → `m_currentAnnotation->creationDate` (format as human-readable)
3. Modified QLabel → **(no field in AnnotationItem — stub or calculate from creationDate)**
4. Subject QLabel → **(no field in AnnotationItem — stub or remove section)**

**Appearance section:**
5. Color swatches → bind click handlers; set active swatch to `m_currentAnnotation->color`; emit color change signals
6. Opacity slider → convert QLabel to QSlider; bind value to color alpha or opacity layer (currently not in AnnotationItem — add field or compute from color)
7. Blend mode combo → convert QLabel to QComboBox; bind to (no field in AnnotationItem — add or stub)
8. Border width spinbox → convert QLabel to QDoubleSpinBox; bind to `m_currentAnnotation->thickness`

**Position section:**
9. X QLabel → `m_currentAnnotation->rect.x()`
10. Y QLabel → `m_currentAnnotation->rect.y()`
11. W QLabel → `m_currentAnnotation->rect.width()`
12. H QLabel → `m_currentAnnotation->rect.height()`
13. Align buttons → wire click handlers; emit alignment signals; implement alignment logic (not in layer or command yet)

**Contents section:**
14. QTextEdit → populate with `m_currentAnnotation->text` on `setAnnotation()`; connect `textChanged()` to annotation update + undo command

**Reply thread section:**
15. Reply list → populate from `m_currentAnnotation->replies`; fetch and render each reply ID (no reply storage structure yet)
16. Add reply button → wire to reply creation dialog; emit signal to append to replies list

**Header:**
- Annotation ID label → bind to `m_currentAnnotation->id`
- Layer name → currently hardcoded "Default Layer" (no field in AnnotationItem or layer stack)
- Lock state → currently hardcoded "Unlocked" (no field in AnnotationItem)
