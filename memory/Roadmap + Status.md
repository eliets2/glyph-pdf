# Roadmap + Status

## Current Status: GSD Phase 1 Complete

All core architecture, engines, UI, and tests are implemented and building.
5/5 test suites pass (as of 2026-05-17).

## Completed Work

### Architecture
- 4-layer library split: core (INTERFACE) -> engines (STATIC) -> commands (STATIC) -> ui (STATIC)
- 6 pure-virtual interfaces for engine abstraction
- AppContext dependency injection
- Multi-tab document editing with per-tab undo stacks
- Controller-per-ribbon-tab pattern (7 controllers)

### Engines
- PdfEditorEngine: full PoDoFo integration (edit, redact, encrypt, sanitize, PDF/A, page ops, image ops)
- SignatureManager: OpenSSL PKCS#12 signing + validation
- FormManager: AcroForm extraction + filling
- ConversionManager: Word/Excel export
- CollaborationManager: WebSocket scaffold
- OcrEngine: Tesseract wrapper (conditional)

### UI
- Office-style ribbon with 8 tabs
- ModeStrip (top) + GlyphStatusBar (bottom)
- InspectorWidget with document properties + signature info
- AnnotationLayer with full drawing/selection/image-editing
- All dialogs (Encryption, Signature, Metadata, PageManagement)
- Theme toggle, welcome screen, find bar, compare view

### Testing
- 5 test targets all passing
- Mock-based unit tests for interfaces and commands
- Smoke tests covering all engine operations
- Conditional QSKIP for optional features (qpdf)

### Build System
- CMake with optional dependency handling
- Conditional compilation guards for all optional libraries
- vcpkg integration (x64-mingw-dynamic)
- Precompiled headers for build speed

## Next Phases (Planned)

### Phase 2 - Polish & Hardening
- Real-world PDF testing with complex documents
- Performance profiling (large PDFs, render cache tuning)
- Error handling improvements
- Accessibility review

### Phase 3 - Advanced Features
- Install Tesseract for OCR support
- Install qpdf for linearization
- Collaboration features (real-time editing)
- Plugin/extension system

### Phase 4 - Distribution
- WiX installer (enterprise_installer.wxs exists)
- Auto-update mechanism
- Code signing
