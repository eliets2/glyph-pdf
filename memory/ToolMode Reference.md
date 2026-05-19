# ToolMode Reference

Defined in `core/PdfEnums.h`. Controls `AnnotationLayer` behavior and cursor mode.

## All 19 Modes

| Mode | Category | Behavior |
|------|----------|----------|
| `HandTool` | Navigation | Pan/scroll document |
| `SelectText` | Navigation | Native text selection via QPdfView |
| `EditText` | Editing | Click to edit text inline (triggers `textEditRequested`) |
| `EditObject` | Editing | Select and delete objects at point |
| `EditImage` | Editing | Select, move, resize embedded images |
| `Highlight` | Annotation | Yellow highlight over text regions |
| `Underline` | Annotation | Underline annotation |
| `DrawFreehand` | Annotation | Freehand pen drawing |
| `DrawShape` | Annotation | Generic shape (legacy, use specific shapes) |
| `DrawRectangle` | Annotation | Rectangle shape |
| `DrawEllipse` | Annotation | Ellipse shape |
| `DrawLine` | Annotation | Straight line |
| `DrawArrow` | Annotation | Arrow line |
| `AddTextBox` | Annotation | Text box annotation |
| `AddComment` | Annotation | Sticky note comment |
| `Redact` | Security | Mark regions for redaction |
| `AddSignature` | Security | Place signature field |
| `AddTextField` | Forms | Insert form text field |
| `AddCheckbox` | Forms | Insert form checkbox |

## Mode Selection Flow

1. User clicks ribbon tool or ModeStrip button
2. `MainWindow::setActiveToolMode(ToolMode)` called
3. `PdfViewerWidget::setToolMode(mode)` propagates to `AnnotationLayer::setMode(mode)`
4. `AnnotationLayer` adjusts mouse handling based on active mode
