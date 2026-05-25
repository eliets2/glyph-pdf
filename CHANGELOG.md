# Changelog

All notable changes to GlyphPDF are documented in this file.

## [1.0.0] - 2026-05-23

### Architecture
- Dependency injection via `AppContext` with shared_ptr ownership
- Bootstrapper pattern for clean startup wiring
- Static library architecture: pdfws_core > pdfws_engines > pdfws_commands > pdfws_ui
- ToolId enum with ToolRegistry for type-safe action dispatch

### PDF Engine
- PoDoFo backend for editing, metadata, forms, annotations, encryption
- PDFium backend for high-quality rendering with 3-tier cache (LRU, 256 MB, tiled)
- qpdf backend for linearization, repair-on-open, and save pipeline
- Incremental save with PoDoFo WriteUpdate (preserves signatures)
- Error reporting via lastError()/clearError() pattern on IPdfEditorEngine

### Security
- PAdES B-LT/B-LTA digital signatures with DSS/VRI embedding
- OCSP client for certificate validation
- Certificate-based encryption (multi-recipient)
- SHA-256 only for signature hashing (no MD5/SHA-1)
- Redaction with content stream excision (never black rectangles)
- 15+ sanitization vectors (metadata, JavaScript, embedded files, etc.)

### Content
- Forms: text fields, checkboxes, radio buttons, combo boxes, list boxes
- Date fields, numeric fields, calculated fields
- FDF import/export
- Text editing with inline edit support
- Full annotation subtypes (highlights, underlines, notes, stamps, shapes)

### OCR
- Tesseract engine integration
- RapidOCR PP-OCRv5 engine
- Preprocessing pipeline
- ROVER word-level merge for multi-engine consensus

### Conversion & Batch
- PDF to HTML, images, CSV, text conversion
- Batch execution mode with inline error reporting
- Error log export (CSV/JSON)

### Page Operations
- Rotate, delete, insert, extract, split, reorder pages
- Crop and resize pages
- Headers, footers, page numbers, Bates numbering

### Watermark & Optimization
- Text and image watermarks
- PDF compression with optimization estimates
- Linearization for web-optimized delivery

### Accessibility
- Screen reader support with accessible names/descriptions on all UI elements
- F6 / Shift+F6 region cycling (ribbon, sidebars, viewer, status bar)
- F1 keyboard shortcuts help dialog
- Tab focus order across all regions
- WCAG-aligned high contrast theme

### Localization
- Qt Linguist translation framework wired (English only translated; Arabic, French, German .ts files exist as empty shells pending lupdate + translator pass)
- RTL layout flag applied at application root via Qt::RightToLeft when language is Arabic; per-widget RTL handling not yet audited
- Language selection in Preferences (restart required) — picks resource translator but non-English packs are currently no-ops

### Error Handling
- ErrorInfo struct with severity levels (Info, Warning, Error, Critical)
- ErrorDialog with collapsible technical details and action buttons
- Engine error wrapping across all 40+ methods
- Batch error handling with inline success/failure display
- Graceful PDF corruption handling with repair warnings
- Memory guards: 500 MB file warning, system RAM monitoring, auto-tiling
- Temp file cleanup with atexit handler

### Installer & Packaging
- windeployqt bundling script
- WiX v4 MSI installer with MajorUpgrade support
- File associations via OpenWithProgids (not hijacking defaults)
- Runtime dependency checker
- Build automation script

### Auto-Update
- JSON manifest-based update checker
- Async non-blocking startup check (3-second delay)
- SHA-256 verified downloads
- MSI silent upgrade via msiexec
- User consent required at every stage (check, download, install)
- Rollback safety: failed updates leave current version intact
- Preferences integration: toggle check-on-startup, channel selection (Stable/Beta)

### Print & Export
- Print preview via QPrintPreviewDialog
- Page setup dialog (paper size, orientation, margins, scaling)
- Export presets panel (High Quality PDF/A, Web Optimized, Legal Archive)
- Create/edit/delete custom export presets (stored in QSettings)

### UI
- Ribbon toolbar with 7 tab controllers (Home, View, Edit, Pages, Convert, Forms, Security)
- Mode strip with theme toggle and AI panel toggle
- Left/right sidebars (thumbnails, bookmarks, comments, inspector)
- Screen navigation (OCR, Redact, Compare, Signatures, PDF/A, Compress, Watermark)
- Status bar: mode, page, zoom, tool, selection, OCR language, encoding, PDF version, page size, file size, unsaved indicator
- Recent files (max 20) with Clear History
- Drag-and-drop PDF opening
- Welcome widget with action cards and recent files grid
- Dark, Light, and High Contrast themes
- Find & Replace bar with redact-all option
- AI Chat panel (toggle)
- Keyboard shortcuts dialog

### Testing
- 12 test targets: UnitTests, TestInterfaces, SmokeTest, TestSanitization, TestSignatureValidation, TestRedaction, TestThreadSafety, TestEncryption, TestResourceLimits, TestControllers, TestIntegration, TestPerformance
- Headless testing via qoffscreen platform plugin
- Thread safety tests with QRecursiveMutex verification
- Performance benchmarks with timing targets

### Known Issues
- MuPDF (AGPL) and Poppler (GPL) are never linked in-process (licensing constraint)
- OCR ensemble pipeline (PP-DocLayoutV2, Surya, LaneScheduler) not yet implemented
- MRC compression inside PDF/A not yet implemented
- Some Session 7-11 features may be interface stubs pending full implementation verification
- Translations: glyphpdf_{ar,fr,de}.ts are empty shells. Run `lupdate src/ -ts translations/glyphpdf_*.ts` then commission translators before claiming multilingual support.
- Autosave: ModeStrip displays an "AUTOSAVED · hh:mm:ss" label that is updated only when a Save action runs, not by a background timer. PRD §13 requires real interval autosave + crash recovery — not yet implemented.
- DiffEngine uses per-word set-difference rather than LCS/Myers — order changes appear as add+delete pairs, not moves. Affects legal/compliance comparison persona.
- Pattern redaction (email/phone/ID regex) not implemented. Only literal-string search-and-redact is available.
- Office→PDF import not implemented (only PDF→Office conversion paths exist).
- Send-for-signing workflow (remote signing order, reminders, audit trails) not implemented — only local certificate-based signing exists.
- CollaborationManager.cpp marks itself "Cloud Sync Stub (Simulation)"; ICollaboration interface exists with no real network backend.
- RapidOcrEngine.cpp line 112 marks PP-OCRv5 tensor pre/post processing as STUB.
- TestSignatureValidation links only pdfws_core + Qt6::Test — uses MockSignatureManager, does NOT exercise real OpenSSL/PoDoFo signing pipeline. Real-crypto E2E coverage gap.
- enterprise_installer.wxs at repo root (legacy WiX v3 with different product name) — superseded by packaging/GlyphPDF.wxs (WiX v4) and removed in this audit.
