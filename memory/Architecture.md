# Architecture

## Layer Diagram

```
┌─────────────────────────────────────────────────┐
│  app/main.cpp                                   │
│  Creates AppContext, engines, gp::MainWindow     │
├─────────────────────────────────────────────────┤
│  pdfws_ui (STATIC)                              │
│  gp::MainWindow, gp::PdfViewerWidget, gp::Ribbon│
│  gp::Sidebar, gp::StatusBar, gp::ModeStrip      │
│  gp::ModeController, isolated gp:: modes        │
├─────────────────────────────────────────────────┤
│  pdfws_commands (STATIC)                        │
│  QUndoCommand subclasses: RotatePage, DeletePage│
│  InsertPage, SetMetadata, AddFormField,         │
│  EditAnnotation, Encrypt, Sign, Redact,         │
│  MoveImage, ResizeImage, RotateImage,           │
│  ReplaceImage, DeleteImage                      │
│  Static helper: SanitizeDocumentHelper          │
├─────────────────────────────────────────────────┤
│  pdfws_engines (STATIC)                         │
│  PdfEditorEngine (PoDoFo), OcrEngine (Tesseract)│
│  FormManager, SignatureManager (OpenSSL),       │
│  ConversionManager, CollaborationManager,       │
│  DocumentSession                                │
├─────────────────────────────────────────────────┤
│  pdfws_core (INTERFACE / header-only)           │
│  PdfEnums, AnnotationTypes, ImageTypes,         │
│  AppContext, all I* interfaces                  │
└─────────────────────────────────────────────────┘
```

## CMake Libraries

| Target | Type | Key Dependencies |
|--------|------|------------------|
| `pdfws_core` | INTERFACE | Qt6::Core, Qt6::Gui |
| `pdfws_engines` | STATIC | podofo, OpenSSL, Qt6::Network, Qt6::Widgets, (optional: Tesseract, qpdf, OpenXLSX, duckx) |
| `pdfws_commands` | STATIC | pdfws_core, Qt6::Widgets |
| `pdfws_ui` | STATIC | pdfws_core, pdfws_commands, Qt6::Pdf, Qt6::PdfWidgets, Qt6::PrintSupport, Qt6::Svg, podofo |
| `PdfWorkstation` | EXECUTABLE | pdfws_ui, pdfws_engines, pdfws_commands, pdfws_core |

## AppContext (Dependency Injection)

All engines are injected via the `AppContext` struct passed to `gp::MainWindow` and `gp::ModeController`:

```
AppContext {
    IPdfEditorEngine*   pdfEditor
    IOcrEngine*         ocr
    IFormManager*       forms
    ISignatureManager*  signing
    IConversionEngine*  conversion
    ICollaboration*     collab
    QUndoStack*         undoStack
    DocumentSession*    document
}
```

The shell layer receives `AppContext*` and operates on the interface pointers, enabling mock-based testing and clean separations.

## Key Design Patterns

- **Interface segregation**: Pure-virtual interfaces in `core/interfaces/` decouple the UI from engines.
- **Command pattern**: `QUndoCommand` subclasses wrap PDF manipulations for undo/redo capability.
- **Mode Controller**: A single `gp::ModeController` orchestrates the ribbon, panels, and central view based on the current context (e.g., View, Edit, Pages, Security).
- **Single Window Shell**: `gp::MainWindow` houses the shell components (`Ribbon`, `ModeStrip`, `Sidebar`, `StatusBar`) which dynamically react to engine state changes.
- **Conditional compilation**: `HAS_TESSERACT`, `HAS_QPDF`, `HAS_OPENXLSX`, `HAS_DUCKX`, and `GLYPH_HAS_SVG` guards adapt the build to available backend libraries.
