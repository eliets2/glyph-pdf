# PDF Workstation

**Version:** 1.0 | **Language:** C++17 | **Framework:** Qt 6.10.2 | **Status:** Backlog Remediation Complete

A professional desktop PDF editor built with Qt 6 and PoDoFo.

---

## Quick Links

- [[Architecture]] - Layer diagram, library split, data flow
- [[Backend Engines]] - PdfEditorEngine, OCR, Signatures, Conversion, Collaboration
- [[UI Components]] - MainWindow, Ribbon, Panels, Dialogs, Controllers
- [[Annotation System]] - AnnotationLayer, AnnotationItem, ToolModes
- [[Build + Environment]] - CMake, vcpkg, MinGW, Qt setup
- [[ToolMode Reference]] - All tool modes and their behaviors
- [[PRD Summary]] - Product requirements and feature scope
- [[Roadmap + Status]] - Phase plan and current progress
- [[Audit Report]] - Architecture/security/testing audit findings (Now includes Remediation Status)

---

## Source Layout

```
src/
  core/             - PdfEnums, AnnotationTypes, ImageTypes, AppContext
  core/interfaces   - IPdfEditorEngine, IOcrEngine, IFormManager,
                      ISignatureManager, IConversionEngine, ICollaboration
  engines/          - Concrete engine implementations + DocumentSession
  commands/         - QUndoCommand subclasses + SanitizeDocumentHelper
  shell/            - gp:: shell UI namespace (Ribbon, Sidebars, Toolbars)
  shell/controllers - Isolated tab logic (HomeController, EditController, etc)
  modes/            - Panel layouts for tools (e.g. OCRMode, FormBuilderMode)
  util/             - GpTheme and Icons parsers
  app/              - main.cpp entry point
tests/              - 9 robust QTest target suites
resources/          - Icons (SVG), QRC manifest, qss theme tokens
```
