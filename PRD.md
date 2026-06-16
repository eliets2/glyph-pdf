# Product Requirements Document

# Advanced PDF Editor and Document Workflow Platform

## 1. Product overview

This product is a cross-platform PDF editor built for personal, academic, and business use. It lets users view, edit, annotate, convert, secure, sign, organize, compare, and automate PDF documents in one place.

It should feel fast, reliable, and familiar to users who work with PDFs every day. It must support both light tasks like highlighting and heavy tasks like OCR on scanned files, batch conversion, and enterprise document handling.

## 2. Problem statement

Users work with PDFs in fragmented ways.

* They open one app to read.
* They use another app to annotate.
* They use a separate tool to edit text.
* They use different tools again for signing, OCR, redaction, or form filling.

This creates friction:

* Too many apps.
* Too many subscriptions.
* Slow workflows.
* File quality loss during conversion.
* Weak support for scanned documents.
* Confusing interfaces.
* Poor mobile-to-desktop continuity.

The product solves this by giving users a single, integrated workspace for the full PDF lifecycle.

## 3. Product goals

### Primary goals

* Let users read and edit PDFs quickly and accurately.
* Support professional workflows without requiring multiple tools.
* Make scanned documents searchable and editable through OCR.
* Make forms easy to create, fill, and distribute.
* Make signing and review workflows simple and secure.
* Keep the interface approachable for new users and efficient for power users.

### Secondary goals

* Reduce document handling time.
* Improve file organization and batch processing.
* Support collaboration across teams and devices.
* Reduce errors in document conversion and markup.
* Offer enterprise-ready security and admin controls.

## 4. Non-goals

The product is not intended to:

* Replace a full word processor.
* Replace a full DMS or ECM platform.
* Become a general image editor.
* Become a note-taking app with arbitrary canvas freedom.
* Focus only on one platform.

## 5. Target users

### Persona 1: Student

Uses PDFs for class notes, textbooks, assignments, and scanned handouts.
Needs: Highlighting and comments, Search in scanned PDFs, Merge and split files, Fill forms, Convert PDFs to editable documents

### Persona 2: Office worker

Handles invoices, reports, contracts, and forms.
Needs: Fast document review, Redaction, E-signatures, Page organization, Compression, Secure sharing

### Persona 3: Legal or compliance user

Works with sensitive documents and version control.
Needs: Precise editing, Compare documents, Redaction, Audit trails, Permission controls, Digital signatures

### Persona 4: Small business owner

Sends proposals, contracts, and forms to customers.
Needs: Form creation, Signing workflows, Branding, Batch processing, Simple pricing

### Persona 5: IT or enterprise admin

Deploys and manages the tool across an organization.
Needs: Licensing controls, Security policies, SSO, Admin dashboards, Deployment support, Compliance features

## 6. Product principles

* Fast open, fast search, fast save.
* Preserve document fidelity.
* Minimize user clicks for common tasks.
* Be powerful without feeling crowded.
* Make advanced tools discoverable, not hidden.
* Handle scanned, native, and mixed PDFs equally well.
* Keep security visible and trustworthy.
* Support keyboard-heavy workflows.

## 7. Scope

### 7.1 In scope

* PDF viewing, Text editing, Object editing, Annotation tools, OCR
* Conversion to and from common formats, Form creation and filling
* E-signatures, Redaction, Document comparison, Page management
* Batch automation, Security features
* Accessibility support, Desktop primary + mobile/web companion

### 7.2 Out of scope for v1

* Full collaborative real-time coauthoring with simultaneous cursor presence
* Native CAD editing, Native video editing
* Full email client integration, Full DMS replacement

## 8. Core user journeys

### Journey A: Review a contract
1. Open PDF. 2. Search for clauses. 3. Highlight and comment. 4. Compare with previous version. 5. Redact sensitive sections. 6. Send for signature.

### Journey B: Edit a scanned form
1. Open scanned PDF. 2. Run OCR. 3. Correct detected text. 4. Fill fields. 5. Export and share.

### Journey C: Create a form from scratch
1. Import blank PDF. 2. Add fields. 3. Set validation rules. 4. Preview form. 5. Share for completion. 6. Collect responses.

### Journey D: Batch process hundreds of files
1. Select multiple PDFs. 2. Compress/convert/OCR. 3. Apply naming rules. 4. Export to destination.

## 9. Functional requirements

### 9.1 Document viewing
* Open PDFs quickly (<3s for 100-page standard PDF)
* Single page, continuous scroll, two-page, and presentation modes
* Render large files efficiently, preserve vector quality and fonts
* Bookmarks, hyperlinks, attachments, embedded media
* Dark mode and reduced eye strain reading options
* Keyboard navigation and search shortcuts

### 9.2 Text and object editing
* Edit text in native PDFs
* Add, delete, modify text blocks
* Font, size, color, alignment, spacing, opacity adjustments
* Move, resize, rotate, layer objects
* Edit images inside PDFs
* Preserve layout, warn on reflow risk
* Undo/redo across edit sessions

### 9.3 Annotation and markup
* Highlight, Underline, Strikeout, Squiggly underline
* Sticky notes, Text box comments, Callouts, Stamps
* Shapes, Freehand drawing, Eraser
* File attachments on comments
* Comment threads, filters, review summaries
* Statuses: open, resolved, rejected

### 9.4 OCR
* Detect text from scanned pages, multi-language
* Preserve page layout, searchable text layer
* Output modes: Searchable only, Editable text, Text under image, High accuracy
* Deskew, orientation correction, noise reduction
* Mixed page detection, confidence warnings
* Progress display, review before save

### 9.5 Conversion
* PDF to Word, Excel, PowerPoint, image, text, HTML, CSV
* Office files, images, scans to PDF
* Batch conversion
* Quality presets: High fidelity, Balanced, Smaller file size
* Image format selection and DPI control

### 9.6 Forms
* Auto-detect form fields
* Create: text field, checkbox, radio button, dropdown, list box, signature field, date field, numeric field, calculated field, button field
* Validation rules, required fields, placeholder text
* Tab order and field alignment
* Import/export form data (CSV, XML)
* Flatten completed forms

### 9.7 E-signatures
* Typed, drawn, and image-based signatures
* Initials
* Send for signing, track status, signing order
* Reminders and notifications
* Audit trails
* Certificate-based digital signatures
* Signature validity and trust display
* Customizable signature appearance

### 9.8 Redaction
* Mark text and images for redaction
* Preview before permanent application
* Pattern redaction: emails, phone numbers, IDs, keywords
* Permanently removed content (not hidden)
* Redaction logs

### 9.9 Page management
* Insert, delete, reorder, rotate, split, merge, extract, replace, crop, resize pages
* Add page numbers, headers/footers
* Bates numbering for legal workflows
* Drag-and-drop page reordering

### 9.10 Document comparison
* Visual and textual comparison of two PDFs
* Highlight added, removed, modified content
* Page-by-page or full document comparison
* Compare reports, detect page reorder changes

### 9.11 Security
* Password protection (open + permission)
* Restrict printing, copying, editing, extraction
* Encryption (AES-256)
* Certificates and digital IDs
* Watermarks
* Secure sharing links
* Document expiration/access revocation
* Metadata sanitization

### 9.12 Batch processing and automation
* Batch: convert, OCR, compress, rename, watermark, redact, merge, export
* Preset workflow creation
* Folder watching / hot folder for enterprise

### 9.13 Compression and optimization
* Downsample images, remove unused objects/metadata
* Quality controls, size reduction estimates

### 9.14 Accessibility
* Screen reader support, keyboard shortcuts, high contrast
* Scalable UI, logical focus order, accessible labels
* Preserve accessibility tags in exports
* Reading order checks for tagged PDFs

### 9.15 Search and navigation
* Full-text search, search in comments/bookmarks
* Regex search, find and replace
* Page thumbnails, bookmark navigation, jump to page
* Recent pages and history

### 9.16 File import and export
* Open standard and linearized PDFs
* Import Word, Excel, PowerPoint, images, scans
* Export to supported formats
* Preserve metadata, bookmarks, hyperlinks, comments

## 10. UX requirements

* Top ribbon/toolbar for frequent actions
* Left pane: thumbnails, bookmarks, attachments, search
* Right panel: object, page, annotation properties
* Central document canvas with smooth zoom/scroll
* Mode switching: view, edit, comment, form, protect
* Progressive disclosure for advanced options
* Empty states with examples and next actions

## 11. Permissions and roles

* Consumer mode: Full local editing, basic sharing/signing
* Team mode: Shared templates, review workflows, admin controls, audit logs
* Enterprise mode: Central licensing, SSO, DLP integration, policy enforcement

## 12. Performance requirements

* Launch quickly, open small files instantly
* Smooth scrolling, responsive zoom
* Non-blocking OCR and conversions (background processing)
* Document open: <3s typical files
* Search: <1s on local files
* Autosave must not interrupt reading

## 13. Reliability requirements

* Autosave after meaningful changes
* Crash recovery for unsaved work
* File integrity checks
* Undo history across complex operations
* Safe handling of malformed PDFs
* Graceful fallback on low-memory devices

## 14. Security and privacy

* Local files remain local by default
* Encrypt data in transit and at rest
* Secure deletion of temporary files
* No silent content logging

## 15. Analytics and telemetry

* File open success/failure, feature usage, search usage
* OCR completion/error rate, export success/failure
* Crash and performance data
* Minimal collection, aggregated metrics, transparent

## 16. Accessibility and localization

* Multiple languages, RTL support
* Local date/number/currency formats in forms
* Accessible labels, keyboard cheatsheets, focus management
* Assistive technology compatibility, contrast-safe visuals

## 17. Platform strategy

* Desktop: Primary platform (editing, OCR, forms, redaction, enterprise)
* Mobile: Reading, annotating, signing, scanning, quick edits
* Web companion: File access, review, sharing, lightweight editing

## 18. Integrations

* Storage: Local filesystem, network drives
* Identity: Email login, SSO, MFA
* Ecosystem: Office formats, images, scanners, signature providers, DLP

## 19. Error handling

* Plain language errors
* No user work loss, preserve partial progress
* Retry, skip, or export logs options
* Expandable details for advanced users

## 20. Edge cases

* Very large PDFs, millions of vector objects
* Password-protected files, embedded files
* Mixed scanned and native pages
* Rotated/upside-down scans, corrupt fonts
* Broken form fields, nonstandard page sizes
* Multi-layer content, accessibility tags surviving edits

## 21. Competitive differentiation

* Faster than heavyweight tools
* Easier than enterprise-only editors
* More complete than basic viewers
* Strong OCR, forms, redaction
* Individual + organization suitability

## 22. Success metrics

* User: Task time, steps per task, conversion/OCR/form/signature rates
* Business: Trial-to-paid, MAU, retention, enterprise seats
* Quality: Crash-free sessions, failure rates, data loss incidents

## 23. Release plan

* Phase 1: Core editor (viewer, search, annotation, page mgmt, export, autosave)
* Phase 2: Professional workflows (editing, OCR, conversion, forms, signatures, security)
* Phase 3: Power and enterprise (compare, redaction, batch, admin, permissions)
* Phase 4: Optimization and scale (performance, accessibility, localization, automation, integrations)

## 24. Risks and mitigations

* PDF editing complexity → strong progressive disclosure
* Conversion fidelity → benchmarks on common document types
* OCR quality varies → preprocessing + confidence scoring
* Large file memory → streaming parser + cache limits
* Feature overload → good defaults + clear mode labels

## 25. Open questions

* Free vs paid feature split
* Mobile editing scope
* Enterprise admin depth in first release
* AI features timing
* Minimum OCR language set

## 26. Recommended MVP

Fast PDF viewing, Search, Annotation, Basic text editing, OCR, Conversion, Page tools, Form filling, E-signatures, Basic protection, Autosave and recovery.

## 27. Implementation status (as of v1.3.1 — 2026-06-16)

Legend: ✅ Done · 🟡 Partial · ⬜ Planned/not started

| § | Area | Status | Notes |
|---|------|--------|-------|
| 9.1 | Document viewing | ✅ | Single/continuous/two-page/presentation, dark mode, keyboard nav, bookmarks/attachments |
| 9.2 | Text & object editing | ✅ | Inline text edit, image edit, move/resize/rotate, undo/redo |
| 9.3 | Annotation & markup | 🟡 | Highlight/underline/strike/squiggly, notes, **callouts, stamps**, shapes, freehand, **comment attachments** done (v1.3.0). Gaps: **Erase** canvas hit-testing (placeholder only), comment threads/filters/review-summary UI |
| 9.4 | OCR | ✅ | ROVER ensemble (PP-DocLayout + Tesseract 5 + RapidOCR), searchable output, **OCR Verify review screen restored** (v1.3.0) |
| 9.5 | Conversion | 🟡 | Word/Excel/PPT/image/text/HTML/CSV + batch + presets. Gap: Markdown/EPUB targets |
| 9.6 | Forms | ✅ | All 10 field types incl. **calculated field** (v1.3.0), tab order, flatten, CSV/FDF data. Gap: XML data, richer validation-rule UI |
| 9.7 | E-signatures | 🟡 | Local typed/drawn/image + certificate (PAdES B-LT/B-LTA) signing & trust-chain/OCSP validation. **Not started: multi-party send-for-signing, signing order, reminders, status tracking, audit trail** |
| 9.8 | Redaction | ✅ | Mark + preview + pattern (regex) + permanent content excision + batch redact |
| 9.9 | Page management | ✅ | Insert/delete/reorder/rotate/split/merge/extract/crop/resize, page numbers, headers/footers, Bates |
| 9.10 | Document comparison | ✅ | Visual + Myers-LCS text diff, **HTML/text report export, page-reorder detection** (v1.3.0) |
| 9.11 | Security | 🟡 | Passwords, permissions, AES-256, watermarks, metadata sanitization, **encrypted-ZIP secure package + XMP document expiry → read-only** (v1.3.0). Out of scope (local-first): URL secure links, server-side access revocation |
| 9.12 | Batch & automation | 🟡 | Convert/compress/watermark + **OCR/Merge/Redact + hot-folder watching** (v1.3.0). Gap: named preset workflows |
| 9.13 | Compression | ✅ | Downsampling, object/metadata stripping, size estimates |
| 9.14 | Accessibility | 🟡 | **Tagged-PDF reading-order check** (v1.3.0). Gaps: tag preservation/repair on export, full screen-reader labeling |
| 9.15 | Search & navigation | 🟡 | Full-text search, thumbnails, bookmarks, jump-to-page. Gap: regex search + find-and-replace |
| 9.16 | File import & export | ✅ | Standard/linearized PDFs, Office/image/scan import, metadata/bookmark/hyperlink preservation |

v1.3.0 closed nine PRD gaps (annotation tools §9.3, OCR Verify §9.4, calculated field §9.6,
batch OCR/Merge/Redact §9.12, compare export + page-reorder §9.10, hot folder §9.12, secure
sharing + expiry §9.11, reading-order check §9.14).

## 28. Roadmap — next versions

### v1.4.x — finish the partials
* **Annotation Erase** — dedicated `ToolMode::Erase` + canvas hit-testing/removal (replace the v1.3.0 placeholder)
* **Batch OCR hardening** — end-to-end runtime validation of the render→OCR→MRC path; progress/cancel correctness under load
* **Comment review UI** — threads, filters, status (open/resolved/rejected) and review summaries over the existing `AnnotationItem` data model
* **Accessibility export** — preserve and repair tags on export; surface labels for screen readers (pairs with the §9.14 reading-order check)
* **Search & navigation** — wire regex search and find-and-replace (currently disabled ribbon tools)

### v1.5.x — workflow depth
* **E-signature workflow (§9.7)** — send-for-signing, signing order, reminders, status tracking, and audit trail (the largest remaining Phase-2 gap)
* **Batch preset workflows (§9.12)** — named, reusable multi-step batch pipelines
* **Form polish (§9.6)** — XML form data import/export, validation-rule editor
* **Conversion targets (§9.5)** — Markdown and EPUB export

### Foundational / M5
* **Djot semantic round-trip** — implement `LuaDjotCodec::djotToDocument` AST-walking (currently a stub guarded by `QEXPECT_FAIL` in TestDjotFuzz); unblocks full Djot rich-text interchange

### Platform expansion (deferred / strategic)
* **Mobile companion (§17)** — reading, annotating, signing, scanning, quick edits
* **Web companion (§17)** — file access, review, sharing, lightweight editing
* **Enterprise (§11/§18)** — SSO/MFA, DLP integration, admin dashboards, central licensing, policy enforcement

### Explicitly out of scope (unchanged)
* Cloud sync / account-based storage (privacy-first, local-only stance — §9 "Cloud and sync" was removed)
* Real-time collaborative coauthoring with live cursor presence (§7.2)
* Native CAD/video editing, full DMS/email-client replacement (§7.2)

---

## Architecture — Target Stack

### Future Full-Stack Architecture
* Frontend: React + TypeScript, TailwindCSS, Zustand, PDF.js, Fabric.js/Konva, Tauri shell
* Backend: FastAPI, Python OCR workers, Redis, PostgreSQL, MinIO, OpenSearch, Keycloak
* PDF Engine: PDFium/MuPDF rendering, qpdf/pikepdf for manipulation, Ghostscript compression

### Current Implementation (v1.3.1 Desktop — Active Codebase)
* C++17 / Qt 6.10.2 (MinGW 13.1.0 / MSYS2 ucrt64) / PoDoFo 1.1 / PDFium / OpenSSL 3.x
* OCR: Tesseract 5 + RapidOCR (ONNX Runtime) + PP-DocLayout, ROVER fusion
* Native Windows desktop application (MSI + portable ZIP); privacy-first, fully offline
* Project path: C:\Users\User\Projects\pdf
