# UI Components

## MainWindow

**File:** `ui/MainWindow.h/.cpp`

- Inherits `QMainWindow`
- Receives `AppContext*` with all engine pointers
- Multi-tab document editing via `QTabWidget` (`m_documentTabs`)
- Per-tab `QUndoStack` stored in `m_tabUndoStacks` map
- Keyboard shortcuts: Ctrl+S (save), Ctrl+Z (undo), Ctrl+Y (redo)
- Drag-and-drop file opening
- Autosave timer, crash recovery
- Native window events (frameless border handling)
- Recent files list (max 10)

## Layout Structure

```
┌─ ModeStrip (top) ─────────────────────────────────┐
├─ CustomRibbon (8 tabs) ───────────────────────────┤
│  Home | View | Edit | Organize | Comment |        │
│  Convert | Forms | Protect                        │
├───────────────────────────────────────────────────┤
│  ┌─LeftSidebar─┐  ┌─CentralStack──┐  ┌─RightSB─┐│
│  │ Thumbnails   │  │ WelcomeWidget │  │Inspector ││
│  │              │  │  or           │  │          ││
│  │              │  │ DocumentTabs  │  │          ││
│  └──────────────┘  └───────────────┘  └──────────┘│
├─ GlyphStatusBar (bottom) ─────────────────────────┤
└───────────────────────────────────────────────────┘
```

## Controllers (7 ribbon-tab controllers)

| Controller | Ribbon Tab | Key Operations |
|-----------|-----------|----------------|
| `HomeController` | Home | Open, save, print, export |
| `ViewController` | View | Zoom, rotation, page mode |
| `EditController` | Edit | Text edit, object delete, image editing |
| `PagesController` | Organize | Rotate/delete/insert/extract pages |
| `ConvertController` | Convert | Word, Excel, PDF/A, linearize |
| `FormsController` | Forms | Form field extraction/creation |
| `SecurityController` | Protect | Encrypt, sign, sanitize, validate signatures |

## Key Widgets

| Widget | Purpose |
|--------|---------|
| `PdfViewerWidget` | QPdfView wrapper + annotation layer + search + zoom |
| `AnnotationLayer` | Transparent overlay for drawing/selecting annotations |
| `CustomRibbon` | Office-style ribbon with tab panels |
| `ModeStrip` | Top mode bar (view/edit/comment) + theme toggle |
| `GlyphStatusBar` | Bottom status: page nav, zoom slider, file info |
| `SidePanelContainer` | Collapsible sidebar wrapper (min 200px, max 400px) |
| `ThumbnailSidebar` | Page thumbnail navigation |
| `InspectorWidget` | Right panel: annotation properties + document info |
| `FindBar` | Search bar with match case/whole words + redact-all |
| `ZoomSlider` | Zoom percentage slider |
| `WelcomeWidget` | Start screen shown when no documents open |
| `CompareWidget` | Side-by-side document comparison |

## Dialogs

| Dialog | Purpose |
|--------|---------|
| `EncryptionDialog` | Set user/owner passwords + permission flags |
| `SignatureDialog` | Certificate selection for digital signing |
| `MetadataDialog` | Edit document title/author/subject/keywords |
| `PageManagementDialog` | Batch page operations (rotate/delete/insert) |

## InspectorWidget Detail

3-page `QStackedWidget`:
- Page 0: Empty placeholder
- Page 1: Annotation properties (color, thickness, text)
- Page 2: Document info (title, author, subject, creator, producer, dates, pages, file size, PDF version, signatures)
