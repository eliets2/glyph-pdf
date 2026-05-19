# PRD Summary

PDF Workstation is a professional desktop PDF editor targeting feature parity with commercial tools (Foxit, Nitro) while being fully native C++/Qt.

## Core Feature Areas

### Document Viewing
- Multi-tab document interface
- Thumbnail sidebar navigation
- Zoom (fit-width, fit-page, custom level, slider)
- Page rotation (clockwise/counter-clockwise)
- Single and continuous page modes
- Text search with match case / whole words
- Document comparison (side-by-side)

### Editing
- Inline text editing on PDF pages
- Object selection and deletion
- Embedded image manipulation (move, resize, rotate, replace, delete)
- Undo/redo via QUndoStack (per-tab)

### Annotations & Comments
- Freehand drawing, shapes (rectangle, ellipse, line, arrow)
- Text boxes, sticky note comments
- Highlight and underline
- Color and thickness customization
- Annotation save/load (JSON persistence)
- Redaction (mark + apply)

### Page Management
- Rotate, delete, insert blank pages
- Extract page ranges to new PDF
- Merge multiple PDFs
- Page management dialog for batch operations

### Forms
- Extract form fields from existing PDFs
- Fill form fields programmatically
- Add text fields and checkboxes

### Security
- AES-256 encryption with user/owner passwords
- Permission flags (print, copy, modify)
- Digital signatures (PKCS#12 signing + validation)
- Document sanitization (strip metadata, JavaScript, embedded files, auto-actions)

### Conversion
- Export to Word (.docx) via duckx
- Export to Excel (.xlsx) via OpenXLSX
- Export to PDF/A-1b, 2b, 3b
- Linearization for web optimization (via qpdf)

### OCR
- Tesseract-based text recognition
- Bounding box overlay on recognized text

### Other
- Dark/light theme toggle
- Drag-and-drop file opening
- Recent files list
- Autosave with crash recovery
- Print support
