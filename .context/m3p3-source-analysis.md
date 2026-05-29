# M3-PROMPT-3 Source Analysis — PagesMode

## PagesMode.cpp preview shell

**Preview banner:** lines 24-30
- Orange warning box displaying: "Preview — not wired in v1.0.0\n\nUse the ribbon toolbar for production-ready operations.\nThis mode page is scheduled for v1.1."
- Self-identified as preview-only, explicitly instructs users to use ribbon toolbar instead

**Hardcoded fake page names:** lines 53-57
- 12 fake pages generated with labels: "Cover", "Contents", "Exec", "Q4 Perf", "Revenue", "Geo", "Op Margin", "Outlook", "Risk", "Appx A", "Appx B", "Glossary"
- Each item shows "page [NNN]\n[FakeName]" format, with icons sized 180x220px, spaced 8px apart
- Pages are displayed in a `QListWidget` with `IconMode` and `InternalMove` drag-drop enabled (line 52)
- Row 3 preset as current (line 58)

**QLabel placeholder:** lines 67-70
- Right panel contains placeholder text: "Form layout: SPLIT AT radio,\nN PAGES input,\nNAMING pattern,\nDESTINATION,\nPreview · Split"
- Property `mono` set to true, margin 14px
- This is a stub for the split/document reordering form that will be implemented later

**Real widgets (if any):**
- `QListWidget` grid with drag-drop (line 46-57): **not real** — contains fake data only
- Toolbar buttons (lines 32-42): **not real** — all disabled by lines 76-84
- **All interactive controls disabled** (lines 75-84): checks for QPushButton, QLineEdit, QComboBox, QCheckBox, QRadioButton, QToolButton and disables them

---

## PdfEditorEngine page operation signatures

```cpp
// From PdfEditorEngine.h (lines 40-50)

// Extract page as raw bytes (for insertion elsewhere)
QByteArray extractPageAsBytes(const QString &path, int pageIndex);

// Insert extracted page bytes at specified index
bool insertPageFromBytes(const QString &path, int atIndex, const QByteArray &pageData);

// Delete single page
bool deletePage(const QString &path, int pageIndex);

// Insert blank page at index
bool insertBlankPage(const QString &path, int atIndex);

// Reorder: move page from one index to another
bool reorderPages(const QString &path, int fromIndex, int toIndex);

// Rotate page by degrees (multiple calls for successive rotations)
bool rotatePage(const QString &path, int pageIndex, int degrees);
```

**No `splitDocument` or `mergePdfs` methods found** — these operations are absent from the engine as of this snapshot. The controller message (line 67-68) confirms "multi-page page arranger scheduled for the upcoming engine update."

---

## ThumbnailSidebar

**Drag-drop reorder:** Yes, fully implemented
- Signal emitted: `pageReordered(int sourceIndex, int targetIndex)` (line 23)
- Infrastructure in place:
  - `setAcceptDrops(true)` (line 29)
  - Drag event handlers: `dragEnterEvent`, `dragMoveEvent`, `dropEvent` (lines 29-31)
  - Member tracking: `m_dragStartPos`, `m_dropIndicatorIndex` (lines 48-49)
- **Already wired** — ready to receive reorder signals and emit them

---

## PagesController stubs vs real implementations

**Stubbed/deferred (lines 65-81):**
- `ToolId::Split` (line 65): MessageBox stub — "multi-page page arranger scheduled for the upcoming engine update"
- `ToolId::Reorder` (line 66): Grouped with Split, same stub message
- `ToolId::Resize` (line 74): MessageBox stub — "Advanced page formatting options will be available in the detailed dialogs being finalized"
- `ToolId::AddHeader` (line 75): Same stub as Resize
- `ToolId::AddFooter` (line 75): Same stub
- `ToolId::AddPageNumbers` (line 77): Same stub
- `ToolId::BatesNumber` (line 77): Same stub

**Real/working implementations:**
- `ToolId::RotateCW` (line 40): Calls `rotateRight()` → pushes `RotatePageCommand` to undoStack (lines 94-98)
- `ToolId::RotateCCW` (line 43): Calls `rotateLeft()` → pushes `RotatePageCommand` with -90° (lines 87-92)
- `ToolId::DeletePage` (line 46): Pushes `DeletePageCommand` (lines 47-54)
- `ToolId::InsertPage` (line 55): Pushes `InsertPageCommand` (lines 56-60)
- `ToolId::Extract` (line 62): Calls `showPageManagement()` (not shown but implied real)
- `ToolId::Crop` (line 70): Sets `viewer->setToolMode(ToolMode::Crop)` (lines 71-72)

---

## AppContext access pattern

From `AppContext.h` (lines 19-29):

```cpp
struct AppContext {
    std::shared_ptr<IPdfEditorEngine>   pdfEditor;
    std::shared_ptr<DocumentSession>    document;
    std::shared_ptr<QUndoStack>         undoStack;
    // ... other services
};
```

**Access from a mode:**
1. Mode receives `AppContext* ctx` (passed in constructor or via MainWindow)
2. Get active document path: `ctx->document->getPath()` (assumed method on DocumentSession)
3. Get engine: `ctx->pdfEditor.get()` — shared pointer dereference
4. Push undo command: `ctx->undoStack->push(new YourCommand(...))`

**Example from PagesController (lines 47-49):**
```cpp
_ctx->document->setPath(viewer->filePath());
_ctx->undoStack->push(new DeletePageCommand(_ctx->pdfEditor.get(), _ctx->document.get(), viewer->currentPage()));
```

---

## Integration approach for D1-D3 (Drag-Reorder, Delete-Page, Split-Document)

### D1: Drag-Reorder (pages in sidebar)
1. **Signal already emitted:** `ThumbnailSidebar::pageReordered(int, int)` exists
2. **PagesController needs slot:** `onPageReordered(int from, int to)` exists (line 24) but unimplemented
3. **Call reorderPages:** `_ctx->pdfEditor->reorderPages(filePath, fromIndex, toIndex)`
4. **Push undo command:** Likely new `ReorderPageCommand` (already imported line 9 suggests it exists)

### D2: Delete-Page (via toolbar/keyboard)
- **Already working:** `ToolId::DeletePage` triggers `DeletePageCommand` (lines 46-54)
- PagesMode toolbar can wire to same controller if connected

### D3: Split-Document
1. **Form needs:** Radio button (split at page N vs. every N pages), input field, naming pattern, destination
2. **Currently stubbed:** Lines 65-69 show stub message
3. **Call needed:** `PdfEditorEngine` method signature **not found** — will require:
   - New method: `bool splitDocument(const QString &path, int atPageIndex, const QString &outputPath)` (and variant for "every N")
   - Or separate calls to `extractPageAsBytes` + `mergePdfs` (which also doesn't exist)
4. **Integration:** PagesMode needs to emit signal → PagesController slot → execute with progress dialog

---

## Summary: What's stubbed vs. ready

| Component | Status | Notes |
|-----------|--------|-------|
| PagesMode UI | Stub | Preview banner, all controls disabled, fake pages only |
| Drag-reorder signal | Ready | ThumbnailSidebar emits `pageReordered(int, int)` |
| Reorder backend | Partial | Engine has `reorderPages(path, from, to)`, controller slot exists but unimplemented |
| Delete page | Ready | Full chain: toolbar → controller → DeletePageCommand |
| Split document | Stub | No engine method, no form logic, stub MessageBox |
| Rotate pages | Ready | Full chain implemented (CW/CCW rotation) |
| Crop tool | Ready | ToolMode::Crop set, form implied |

