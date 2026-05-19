# Project Refresh & Debloat Roadmap (GSD Stage 1)

Following the **Orchestrator's GSD Methodology**, this roadmap defines the cleanup and modernization phase to eliminate "context rot" and technical debt accumulated during rapid UI transitions.

## Phase 1: Structural Audit & Debloat (Code Archaeology)
- [x] **Auditing MainWindow**: Cleaned up 34KB of dense setup code. Extracted `createFileActions`, `createViewActions`, `createEditActions`, and `createBetaActions` into a modular structure (Completed).
- [x] **Resource Verification**: Verified `resources.qrc` and consolidated all corporate styling (Completed).
- [x] **Dead Code Removal**: Checked `main.cpp` and `WelcomeWidget` for remaining "Modern/Dark Mode" hacks (Completed).
- [x] **Auditing build.log**: Debloat old build artifacts and logs. (Completed via Backlog Remediation).

## Phase 2: Premium UI Refresh (UI Specialist)
- [x] **Ribbon TabBar Integration**: Ensure seamless tab transitions between toolsets (Completed).
- [x] **Icon Suite**: Replace standard Qt system icons with professional custom-themed SVG icons for the Ribbon. (Completed via SVG resource embedding).
- [x] **WelcomeWidget Refinement**: Match PDF-XChange's flat neutral gray background (Completed).
- [x] **Visual Audit**: Confirm pixel-perfect alignment with "Dark Obsidian" layout (Completed via Backlog Remediation).

## Phase 3: Engine Handoff (Backend Specialist)
- [x] **OCR Pipeline**: Connect rendered page capture to the `OcrEngine` (Completed).
- [x] **Annotation Drag-to-Move**: Implement interactive mouse manipulation for all markup objects (Completed).
- [x] **Save Logic Fix**: Transition from `QPrinter` render-save to native PDF structure preserve-save where applicable. (Completed via `pdfws_engines` PoDoFo implementation).
- [x] **Security Engine API implementation**: Implementing thread-safe encryption, signature validation, and redaction (Completed via Backlog Remediation).

## Phase 4: Verification (Testing Specialist/Verifier)
- [x] **Build Validation**: Verified application compiles and links all beta engines (Completed).
- [x] **Test Coverage expansion**: Validate sanitization, encryption, thread safety, redaction, and interfaces (Completed via Backlog Remediation).

---

### Remediation Note (May 2026)
A 7-session remediation phase successfully closed out the technical debt backlog. The old UI classes were cleanly migrated to the new `gp::` namespace. Security flaws like thread data races, ineffective redactions, missing signature validations, and buffer overflows were resolved using PoDoFo/OpenSSL integrations. The test suite was dramatically expanded to cover all new features and security requirements. All temporary/audit files were purged. The project is now fully prepared to begin Phase 5+ roadmap tracking.

---
*Reference Agent: Orchestrator.md*
*Methodology: GSD Pipeline*
