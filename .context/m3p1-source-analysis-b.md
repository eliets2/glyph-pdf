# M3-PROMPT-1 Source Analysis B — Canvas, Commands, Enums

## AddFormFieldCommand

**Location:** `src/commands/AddFormFieldCommand.h` (header-only, no .cpp)

**Constructor signature:**
```cpp
AddFormFieldCommand(IFormManager* engine, DocumentSession* doc, FieldType type,
                    int pageIndex, const QRectF& rect, const QString& name, 
                    const QStringList& options = {})
```

**Constructor parameters:**
- `IFormManager* engine` — form manager interface for field creation
- `DocumentSession* doc` — document session containing the file path
- `FieldType type` — enum specifying field type (see below)
- `int pageIndex` — zero-indexed page number
- `const QRectF& rect` — field bounding rectangle in PDF coordinates
- `const QString& name` — field name (unique identifier)
- `const QStringList& options = {}` — optional list for Dropdown/ListBox options

**Key fields stored in private section:**
- `IFormManager* m_engine` — form manager reference
- `DocumentSession* m_doc` — document reference
- `FieldType m_type` — field type (enum)
- `int m_page` — page index
- `QRectF m_rect` — field rectangle
- `QString m_name` — field name
- `QStringList m_options` — dropdown/listbox options

**FieldType enum (nested in AddFormFieldCommand):**
```cpp
enum class FieldType { Text, Checkbox, Radio, Dropdown, ListBox, Date, Numeric }
```

**undo() implementation status:** Not implemented (logs warning, field remains). See ROADMAP for removal via `removeFieldById()`.

---

## PdfViewerWidget Mouse Event Handling

**Location:** `src/ui/PdfViewerWidget.h` + `src/ui/PdfViewerWidget.cpp`

### mousePressEvent
**Signature (line 393):**
```cpp
void PdfViewerWidget::mousePressEvent(QMouseEvent *event) override
```

**Pattern:** 
- Checks if `m_toolMode == ToolMode::Crop` and left button
- If crop mode: creates `QRubberBand` (if not exists), sets geometry to origin point, shows it, sets `m_isSelectingCrop = true`, accepts event
- Otherwise: delegates to `QWidget::mousePressEvent(event)`

**Line range:** 393-407

### mouseMoveEvent
**Signature (line 409):**
```cpp
void PdfViewerWidget::mouseMoveEvent(QMouseEvent *event) override
```

**Pattern (rubber band + crosshair cursor):**
- If `m_isSelectingCrop && m_rubberBand` exists: updates rubber band geometry using `QRect(origin, currentPos).normalized()`, accepts event
- Otherwise: delegates to `QWidget::mouseMoveEvent(event)`
- **Rubber band implementation:** `QRubberBand::Rectangle` with dynamic geometry update on each move (line 412)
- **Crosshair cursor:** Set in `setToolMode()` for `ToolMode::Crop` (line 207): `m_pdfView->setCursor(Qt::CrossCursor)`

**Line range:** 409-417

### mouseReleaseEvent
**Signature (line 419):**
```cpp
void PdfViewerWidget::mouseReleaseEvent(QMouseEvent *event) override
```

**Pattern (rect finalization):**
- If `m_isSelectingCrop && left button released`:
  - Sets `m_isSelectingCrop = false`
  - Hides rubber band
  - Gets final `QRect` from rubber band geometry
  - If size > 10×10 pixels:
    - Gets current page index via `currentPage()`
    - Maps widget coords to PDF coords using `m_zoomFactor` (divides by zoom)
    - Creates `QRectF` and emits `cropRequested(page, pdfRect)` signal
  - Accepts event
- Otherwise: delegates to `QWidget::mouseReleaseEvent(event)`

**Coordinate transformation (lines 437-440):**
```cpp
QRectF pdfRect(selection.x() / m_zoomFactor, 
               selection.y() / m_zoomFactor, 
               selection.width() / m_zoomFactor, 
               selection.height() / m_zoomFactor);
```

**Line range:** 419-449

### Rubber Band Implementation Details
- **Member variable (line 126):** `QRubberBand *m_rubberBand = nullptr`
- **Origin tracking (line 127):** `QPoint m_rubberBandOrigin`
- **State flag (line 128):** `bool m_isSelectingCrop = false`
- **Pattern:** Create on first press, update geometry on every move, hide/clear on release
- **Yes, rubber band exists:** Fully implemented via Qt's `QRubberBand::Rectangle` class

### Cursor Setting
- **HandTool:** `Qt::OpenHandCursor` (line 196)
- **EditObject:** `Qt::ArrowCursor` (line 199)
- **SelectText:** `Qt::IBeamCursor` (line 202)
- **DrawFreehand, DrawShape, Crop:** `Qt::CrossCursor` (lines 204-208)
- **Default:** `Qt::ArrowCursor` (line 210)

---

## PdfEnums.h

**Location:** `src/core/PdfEnums.h`

### FieldType Values
**Note:** There are two different enums named `FieldType`:
1. **In AddFormFieldCommand.h (nested enum):**
   - `Text`
   - `Checkbox`
   - `Radio`
   - `Dropdown`
   - `ListBox`
   - `Date`
   - `Numeric`

2. **In PdfEnums.h:** No `FieldType` enum exists here (this file only contains `ToolMode`)

### ToolMode enum values (all 27 values):
```
HandTool
SelectText
EditText
EditObject
Highlight
Underline
Strikeout
Squiggly
DrawShape
DrawFreehand
AddTextBox
AddComment
Redact
AddSignature
DrawRectangle
DrawEllipse
DrawLine
DrawArrow
AddTextField       ← relevant for form fields
AddCheckbox        ← relevant for form fields
EditImage
Stamp
Callout
Crop
```

---

## Missing Files (Not Yet Created)

- **EditFormFieldCommand.h:** ABSENT — not found in `src/commands/`
- **MoveFormFieldCommand.h:** ABSENT — not found in `src/commands/`
- **FormFieldPropertiesPanel.h:** ABSENT — not found in `src/modes/`

All three are architectural pieces referenced in requirements but not yet implemented.

---

## CMakeLists.txt Test Registration Pattern

**Location:** `CMakeLists.txt` (lines 386-694)

### Standard test registration pattern (guard-first, then add_executable + link + add_test):

```cmake
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestXXXName.cpp")
    add_executable(TestXXXName
        tests/TestXXXName.cpp
        [optional source files]
    )
    target_include_directories(TestXXXName PRIVATE src [optional paths])
    target_link_libraries(TestXXXName PRIVATE
        pdfws_engines pdfws_core [other libs as needed]
        Qt6::Test Qt6::Widgets [other Qt modules]
    )
    add_test(NAME TestXXXName COMMAND TestXXXName)
    set_tests_properties(TestXXXName PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 60
        LABELS "unit;descriptive;labels;here;qt;headless"
    )
endif()
```

### To add TestFormBuilder:

```cmake
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestFormBuilder.cpp")
    add_executable(TestFormBuilder
        tests/TestFormBuilder.cpp
        src/commands/AddFormFieldCommand.h
    )
    target_include_directories(TestFormBuilder PRIVATE src tests)
    target_link_libraries(TestFormBuilder PRIVATE
        pdfws_engines pdfws_core pdfws_commands
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets
    )
    add_test(NAME TestFormBuilder COMMAND TestFormBuilder)
    set_tests_properties(TestFormBuilder PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 60
        LABELS "unit;forms;form-fields;builder;qt;headless"
    )
endif()
```

**Key observations:**
- `if(EXISTS ...)` guard prevents CMake from failing if test file doesn't exist yet
- `target_include_directories` must include `src` at minimum
- `target_link_libraries` requires Qt6::Test + Qt6::Widgets for QTest framework
- Add `pdfws_commands` if test uses command classes (AddFormFieldCommand)
- Always set `QT_QPA_PLATFORM=offscreen` for headless CI/testing
- TIMEOUT typically 60s for unit tests, 120s for integration tests
- LABELS should describe test purpose (used for CTest filtering)

