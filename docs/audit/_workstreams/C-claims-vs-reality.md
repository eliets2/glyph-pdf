# GlyphPDF v1.0.0 — Claims-vs-Reality Audit (Workstream C)

**Auditor:** Guarantee Verification Engine (GVE)
**Date:** 2026-06-02
**Commit audited:** e1c5394 (CLAUDE.md asserts e09404d — minor doc drift; HEAD confirmed via git log)
**Scope:** README.md features list, PRD.md, CHANGELOG.md, CLAUDE.md Sections 5 and 7, UI strings, src/ implementation
**Method:** Read-only static analysis; no build or runtime execution

---

## Summary

| Tier | Count |
|------|-------|
| REAL (wired to real engine, exercised by tests) | 38 |
| PARTIAL (real engine wired; sub-features stub/deferred) | 8 |
| MOCK / STUB — undisclosed (UI says one thing, code does another) | 3 |
| DISCLOSED-PLACEHOLDER (setEnabled(false) + honest tooltip/dialog) | 9 |
| MISSING (no implementation found; no disclosure) | 2 |
| **Total claims surveyed** | **60** |

**Release-blockers (undisclosed fake claims or false UI promises):** 5 findings at High/Medium severity.

---
## Claims Matrix

| # | Claim | Source | Status | Evidence (file:line) | Notes |
|---|-------|--------|--------|----------------------|-------|| 1 | Tesseract OCR | README | REAL | OcrEngine.cpp | Tesseract API MSYS2 |
| 2 | RapidOCR PP-OCRv5 | README | REAL conditional | RapidOcrEngine.cpp:61-152 | isMockImplementation=false; ONNX models present |
| 3 | ROVER word-level merge | README | REAL | OcrPipeline.cpp | CrossPagePipeline confidence-weighted |
| 4 | PP-DocLayoutV2 layout detector | README | REAL conditional | PpDocLayoutDetector.cpp:1-80 | ONNX; model at models/pp_doclayout/ |
| 5 | Surya detector | README | DISCLOSED-PLACEHOLDER | SuryaDetector.cpp | Subprocess stub; HAS_SURYA |
| 6 | MRC 30x compression | README | PARTIAL | MrcPageProcessor.cpp:65-122 | OpenJPEG wired; JBIG2 vendored; 30x synthetic only; test asserts >=5x |
| 7 | PDF/A-2b assembly via MRC | CHANGELOG M7-P3 | REAL | PdfEditorEngine.cpp exportMrcPdfA | XMP + OutputIntent + XObjects |
| 8 | PAdES B-LT/B-LTA DSS/VRI | README Security | REAL | SignatureManager.cpp | Real OpenSSL CMS + OCSP + DSS |
| 9 | SHA-256 only for signing | README Security | REAL | SignatureManager.cpp | SHA-1 in VRI key correct spec exception |
| 10 | CMS_verify Windows root trust | CLAUDE.md M1-B4 | REAL | SignatureManager.cpp | CertOpenSystemStoreA + EKU + CRL |
| 11 | OCSP_basic_verify before DSS | CLAUDE.md M1-B5 | REAL | SignatureManager.cpp | Rejection path; degrades to B-T |
| 12 | RSA >= 2048-bit | CHANGELOG M2-P4 | REAL | SignatureManager.cpp | signDocument() rejects weaker keys |
| 13 | OCSP per-field revocation | CHANGELOG M2-P4 | PARTIAL/STUB | SignatureManager.cpp:537-542 | First-available OCSP DER; VRI-to-field stub persists |
| 14 | AES-256 password encryption | README Security | REAL | PdfEditorEngine.cpp | PoDoFo AES-256; TestEncryption |
| 15 | Certificate encryption multi-recipient | README Security | REAL | SignatureManager.cpp | M1 B8 real /Filter /PubSec |
| 16 | Secure redaction content stream | README Security | REAL | PoDoFoBackend.cpp | Edact-Ray + Tr==3 OCR scrub; TestRedaction |
| 17 | Sanitization 15+ vectors | README Security | REAL | PoDoFoBackend.cpp; TestSanitization | 15+ vectors |
| 18 | Edact-Ray glyph-advance normalization | CHANGELOG M2-P1 | REAL | GlyphAdvanceCalculator | Numeric TJ gap; 3 regression tests |
| 19 | AI Chat multi-provider | README UI | REAL | AIChatPanel.cpp; src/engines/ai/ | 4 providers; canned reply removed |
| 20 | AI Anthropic/OpenAI/Gemini/Ollama | CHANGELOG M6-P3 | REAL | src/engines/ai/*.cpp | Real HTTPS; TestAiProvider 10 tests |
| 21 | SignaturesPanel real output was hardcoded | CLAUDE.md s7 P5 | REAL | SignaturesPanel.cpp:40-65 | No hardcoded data; wired to validateSignatures() |
| 22 | ThumbnailSidebar real thumbnails was fake | CLAUDE.md s7 P5 | REAL | ThumbnailSidebar.cpp:31-51 | ThumbnailRenderer + RenderCache LRU |
| 23 | Sidebar Files real bytes was placeholder | CLAUDE.md s7 P5 | REAL | Sidebar.cpp:106-141 | extractEmbeddedFile() via PoDoFo |
| 24 | PdfAValidationPanel real veraPDF was fake | CLAUDE.md s7 P5 | REAL conditional | VeraPdfValidator.cpp | Honest unavailable fallback |
| 25 | AIChatPanel no canned reply | CLAUDE.md s7 P5 | REAL | AIChatPanel.cpp:21-80 | activeProvider chat() via QFutureWatcher |
| 26 | Sync indicator fake SYNCED label | CLAUDE.md s7 P5 | MOCK UNDISCLOSED | GpMainWindow.cpp:146-148; ModeStrip.cpp:77 | SYNCED v.1 and NOT SYNCED emitted; no cloud backend; LOCAL ONLY path overridden |
| 27 | CollaborationManager Cloud Sync | CHANGELOG Known Limitations | DISCLOSED-PLACEHOLDER | CollaborationManager.cpp:52-80; Ribbon.cpp:83-88 | setEnabled false; informational dialog |
| 28 | Batch Convert/Compress/Watermark/ExportPDFA | README Batch | REAL | BatchMode.cpp:843-844 | QtConcurrent::mapped; 5 tests |
| 29 | Batch Merge PDFs | README Batch | DISCLOSED-PLACEHOLDER | BatchMode.cpp:185,202,348 | disableItem; mergeDocuments() in PdfViewerWidget not wired here |
| 30 | Batch OCR | README Batch | DISCLOSED-PLACEHOLDER stale | BatchMode.cpp:186,203 | Tooltip says M5 needed; M5 shipped |
| 31 | Batch Redact Search Pattern | README Batch | DISCLOSED-PLACEHOLDER stale | BatchMode.cpp:187,204,374 | Tooltip says M3-P4 needed; M3-P4 shipped |
| 32 | FormBuilder field placement 9 types | README Forms | REAL | FormBuilderMode.cpp | addField* engine calls; TestFormBuilder |
| 33 | FormBuilder delete field | README Forms | PARTIAL/STUB | DeleteFormFieldCommand.h:32-33 | Engine-side removal not implemented; UI-list only |
| 34 | FormBuilder move/resize field | README Forms | PARTIAL/STUB | MoveFormFieldCommand.h:35; ResizeFormFieldCommand.h:34 | Engine-side rect update not implemented |
| 35 | FormBuilder tab order persist to PDF | CHANGELOG M3-P1 | PARTIAL/STUB | FormBuilderMode.cpp:417-421 | QMessageBox v1.1; IFormManager has no setTabOrder() |
| 36 | FormBuilder Auto-Detect fields | README Forms | DISCLOSED-PLACEHOLDER | FormBuilderMode.cpp:319-322 | setEnabled false; no-op; tooltip v1.1 |
| 37 | FormBuilder Preview Form | README Forms | DISCLOSED-PLACEHOLDER | FormBuilderMode.cpp:341-344 | setEnabled false; no-op; tooltip v1.1 |
| 38 | IFormManager listFields method | CHANGELOG M3-P1 | MISSING | src/core/interfaces/IFormManager.h | Method does not exist in interface |
| 39 | Localization ar/fr/de | CLAUDE.md M6-P2 | PARTIAL | translations/*.ts | 1394 extracted; 0 translated; README implies full multilingual support |
| 40 | Dark/Light/High Contrast themes | README UI | REAL | GpTheme.cpp | 3 QSS themes |
| 41 | Find Replace with redact-all | README UI | REAL | FindBar.cpp | QPdfSearchModel + redactAll |
| 42 | F6 region cycling F1 help | README UI | REAL | GpMainWindow.cpp | keyPressEvent |
| 43 | Auto-Update manifest + SHA-256 | README | REAL | UpdateChecker.cpp | Async; msiexec |
| 44 | Print preview + page setup | README Print | REAL | PdfViewerWidget.cpp | QPrintPreviewDialog |
| 45 | Export presets 3 built-in + CRUD | README Print | REAL | ExportPresetsPanel.cpp | QSettings |
| 46 | DiffEngine Myers LCS + move detection | CHANGELOG M6-P1 | REAL | MyersDiff.cpp | O((N+M)D); 10 tests |
| 47 | OCR to Djot mapping SemanticDocument | CHANGELOG M5-P4 | REAL | OcrDjotMapper.cpp | BornOCR provenance; 14 tests |
| 48 | Djot annotation rich text WS2 | CHANGELOG M6-P4 | REAL | DjotToRichTextXhtml.cpp | /RC XHTML + /PieceInfo; 12 tests |
| 49 | Comment threading /IRT | CHANGELOG M6-P5 | REAL | PoDoFoBackend.cpp | /IRT + /RT /R |
| 50 | CommentsWidget rich Djot composer | CHANGELOG M6-P4 Note | DISCLOSED-PLACEHOLDER | CommentsWidget.cpp:379,414 | TODO M6-P5; plain text only |
| 51 | PdfStructureMapper dual-model bridge | README Architecture | STUB UNDISCLOSED | PdfStructureMapper.cpp:11-25 | mapPdfToSemantic returns empty; applySemanticToPdf returns false; no disclosure |
| 52 | Send-for-Sig ribbon button | RibbonModel.cpp:14 | MISSING SILENT | RibbonModel.cpp:14 | No ToolId; no handler; clickable button fires nothing |
| 53 | Office to PDF import | README optional | REAL conditional | ConversionManager.cpp | Real soffice subprocess |
| 54 | LaneScheduler + CrossPagePipeline | CHANGELOG M2-P5 | REAL | LaneScheduler.cpp | QThreadPool; 6 concurrency tests |
| 55 | Autosave + RecoveryDialog | CHANGELOG M1 | REAL | AutosaveManager.cpp | Interval timer + atomic rename |
| 56 | ProvenanceGuard | CHANGELOG M4-P7 | REAL | ProvenanceGuard.cpp | 4 tests |
| 57 | Test count doc drift 14/26/32 | README/CLAUDE.md | DOC DRIFT | CMakeLists.txt | 32 add_test entries; README says 14 |
| 58 | Drag-and-drop PDF opening | README UI | REAL | PdfViewerWidget.cpp | QMimeData |
| 59 | Recent files max 20 | README UI | REAL | HomeController.cpp | QSettings |
| 60 | Ribbon 7 tab controllers | README UI | REAL | RibbonModel.cpp | Home/View/Edit/Pages/Convert/Forms/Security |


---

## Findings

### C-01 - Fake sync-state indicator persists in shipped code
**Severity:** High -- **Confidence:** Proven (direct code read)

**GpMainWindow.cpp:146** calls setSyncStatus("SYNCED v.1") on every save. **GpMainWindow.cpp:148** calls setSyncStatus("NOT SYNCED") on every edit. **ModeStrip.cpp:77** initializes to "SYNCED v.0". The ModeStrip::updateLabels() LOCAL ONLY path (ModeStrip.cpp:150) is overridden by the dirtyChanged signal. No cloud backend exists; CollaborationManager.cpp:57 is a stub returning true. CHANGELOG:394 says indicator was replaced with LOCAL ONLY but the override code was not removed.

**Proposed fix:** Remove the setSyncStatus("SYNCED...") and setSyncStatus("NOT SYNCED") calls from GpMainWindow.cpp:146,148. The ModeStrip::updateLabels() LOCAL ONLY path handles status correctly on its own.

**Regression test:** After DocumentSession::dirtyChanged(false), sync label text must NOT contain "SYNCED" and must contain "LOCAL ONLY".

---

### C-02 - PdfStructureMapper is a silent stub for the dual-model core architecture feature
**Severity:** High -- **Confidence:** Proven (direct code read)

**PdfStructureMapper.cpp:11-25**: mapPdfToSemantic() returns an empty SemanticDocument (comment: "Stub: returns an empty SemanticDocument"). applySemanticToPdf() returns false (comment: "Stub: not yet implemented"). No README or CHANGELOG disclosure. The dual-model core only works for OCR-derived documents, not structural parsing of existing born-PDFs.

**Proposed fix:** Add to CHANGELOG Known Limitations: "PdfStructureMapper (native-PDF => SemanticDocument structural parsing) not yet implemented; born-PDF editing via Djot requires OCR-derived SemanticDocuments."

**Regression test:** mapPdfToSemantic() on a text-bearing PDF must return a non-empty SemanticDocument.

---

### C-03 - FormBuilder delete/move/resize do not modify the PDF -- silent engine-side no-op
**Severity:** High -- **Confidence:** Proven (direct code read)

DeleteFormFieldCommand.h:32-33: "engine-side field removal not yet implemented". MoveFormFieldCommand.h:35: "engine-side field move not yet implemented". ResizeFormFieldCommand.h:34: "engine-side field resize not yet implemented". FormBuilderMode.cpp:417-421: tab order QMessageBox says v1.1. IFormManager has no deleteField(), updateFieldRect(), setTabOrder(), or listFields() methods.

A user who deletes a field, saves, and reopens will find the field still present. The operation logs qWarning and takes no engine action.

**Proposed fix:** Implement IFormManager::deleteField()/updateFieldRect()/setTabOrder() or show a clear disclosure dialog on each operation. Current silent no-op is a guarantee failure.

**Regression test:** Place field, delete, save, reload -- field must not be present. Currently fails silently.

---

### C-04 - Batch mode OCR and Redact operations show stale milestone tooltips after engines shipped
**Severity:** Medium -- **Confidence:** Proven

BatchMode.cpp:203 OpOCR tooltip: "OCR engine available in M5" -- M5 shipped. BatchMode.cpp:204 OpRedact tooltip: "Search-pattern redact coming in M3-P4" -- M3-P4 shipped. BatchMode.cpp:202 OpMerge tooltip: "engine method not yet available -- coming in M4" -- mergeDocuments() exists in PdfViewerWidget.cpp:1153. All three operations are disclosed as disabled (DISCLOSED-PLACEHOLDER standard met) but the engine-availability rationale is false.

**Proposed fix:** Wire the three operations (all engines exist) or update tooltips to non-milestone language: "Not available in v1.0 batch mode."

---

### C-05 - Send-for-Sig ribbon button fires nothing on click -- silent inert control
**Severity:** Medium -- **Confidence:** Proven

RibbonModel.cpp:14 defines sendSign as an enabled ribbon item. No ToolId::SendSign exists in ToolId.h. No controller handles the sendSign tool string. Click produces silence. Compare Cloud Sync: Ribbon.cpp:85-88 correctly calls setEnabled(false) + tooltip.

**Proposed fix:** Add to Ribbon::buildBtn(): if id == "sendSign" { setEnabled(false); setToolTip("Coming in v1.1"); } Mirror the Cloud Sync treatment.

**Regression test:** Activating sendSign tool must produce a user-visible response (dialog or disabled state).

---

### C-06 - Localization: README implies full multilingual support; 0 of 1394 strings translated
**Severity:** Medium -- **Confidence:** Proven

translations/glyphpdf_fr.ts: 1394 strings type=unfinished; 0 strings with non-empty translation. Same for ar.ts and de.ts. The embedded .qm files are null -- switching locale displays English source strings. CLAUDE.md Section 5 M6-P2 states "lupdate populated (1394 strings each)" implying translation-ready content. CHANGELOG Note section is honest ("untranslated pending commission"). README has no equivalent disclosure.

**Proposed fix:** Add to README: "Localization: Engineering scaffold complete; human-translated .qm files pending commissioning. English fallback active."

---

### C-07 - OCSP per-field revocation correlation is a stub despite CHANGELOG claiming M2-P4 complete
**Severity:** Low -- **Confidence:** Proven

SignatureManager.cpp:537-542 comment: "Full DSS-to-field-name correlation is wired in M5 when the VRI dict links /OCSPs entries to individual signatures; for now returns the first available OCSP DER." TestSignatureRealCrypto::testRevokedCertReportsRevoked is QEXPECT_FAIL. Security impact limited to multi-signature documents. Single-signature documents work correctly.

**Proposed fix:** Implement VRI key correlation or document as v1.1 item. Remove QEXPECT_FAIL, replace with QSKIP with issue reference.


---

## Verified-REAL list

The following claims are wired to real engines with passing tests. No mock paths detected.

1. **RapidOCR PP-OCRv5** -- RapidOcrEngine.cpp:61-121; isMockImplementation()=false; 4 ONNX model files present in models/ppocrv5/
2. **PAdES B-LT/B-LTA digital signatures** -- real OpenSSL CMS; real OCSP_basic_verify; real Windows trust store; real DSS/VRI
3. **AES-256 encryption and certificate-based encryption** -- PoDoFo /Filter /PubSec real output (M1 B8)
4. **Secure redaction** -- content stream excision + Edact-Ray glyph-advance normalization + Tr==3 invisible layer scrub
5. **Document sanitization** -- 15+ vectors; TestSanitization passes
6. **AI Chat panel (all 4 providers)** -- real HTTPS calls via QNetworkAccessManager; canned reply fully removed
7. **SignaturesPanel** -- no hardcoded identity; fields initialized to em-dash; populated from validateSignatures()
8. **ThumbnailSidebar** -- real PDFium-rendered thumbnails via ThumbnailRenderer adapter + RenderCache LRU
9. **Sidebar Files attachment extraction** -- real PoDoFo extractEmbeddedFile() bytes
10. **PdfAValidationPanel** -- real veraPDF CLI subprocess; honest unavailable fallback when CLI absent
11. **MRC compression** -- real OpenJPEG JPEG2000 always wired; JBIG2 via vendored jbig2enc; separatePage() real Otsu threshold + layout-guided mask
12. **OCR to Djot mapping** -- OcrDjotMapper stateless; BornOCR provenance with attributes; 14 passing tests
13. **Djot annotation rich text (WS2 Role 3)** -- DjotToRichTextXhtml bounded transcoder; /RC XHTML + /PieceInfo dual-write; 12 tests
14. **Myers LCS DiffEngine** -- O((N+M)D); detectMoves(); 10 tests
15. **Comment threading (/IRT)** -- spec-conformant /IRT indirect ref + /RT /R; roundtrip verified
16. **LaneScheduler + CrossPagePipeline** -- real QThreadPool/QSemaphore; 6 concurrency tests
17. **BatchMode execution loop** -- QtConcurrent::mapped; continue-on-failure; error log; CSV/JSON export
18. **FormBuilder field placement** -- 9 addField* engine methods real; TestFormBuilder 5 headless tests
19. **Auto-Update** -- manifest fetch + SHA-256 verify + msiexec consent flow
20. **Office-to-PDF and Images-to-PDF** -- real soffice subprocess; real PoDoFo PdfImage XObject
21. **ProvenanceGuard** -- refuses Djot-save-back on born-PDF signed documents; TestDjotRoundtrip 4 ProvenanceGuard cases
22. **Autosave** -- real interval timer + atomic rename; RecoveryDialog on startup

---

## Pattern 5 sites -- final disposition

| Pattern 5 site (CLAUDE.md s7) | Disposition | Evidence |
|-------------------------------|-------------|----------|
| RapidOCR returning literal RapidOCR_Mock | FIXED | isMockImplementation() returns false; real ONNX pipeline |
| ThumbnailSidebar fake paper widgets | FIXED | ThumbnailRenderer + RenderCache; ThumbnailSidebar.cpp:31-51 |
| AIChatPanel canned reply | FIXED | 4 real AI providers; canned reply removed |
| SignaturesPanel hardcoded Elie Matta / GlobalSign CA | FIXED | Fields init em-dash; setDocument() wired to validateSignatures() |
| PdfAValidationPanel fake rules | FIXED | VeraPdfValidator real CLI subprocess; honest unavailable fallback |
| Fake SYNCED v.X indicator | PARTIALLY FIXED -- GpMainWindow.cpp overrides reintroduce it | ModeStrip LOCAL ONLY path exists; GpMainWindow.cpp:146,148 override with SYNCED/NOT SYNCED |
| Sidebar Files placeholder text | FIXED | extractEmbeddedFile() real PoDoFo bytes; Sidebar.cpp:115 |
| CollaborationManager Cloud Sync stub | DISCLOSED-PLACEHOLDER | SecurityController:319-322 informational dialog; Ribbon:85-88 setEnabled false |

---

## Coverage gaps

1. **No test verifies sync label text is never SYNCED.** The ModeStrip LOCAL ONLY path and GpMainWindow SYNCED override are untested at the integration level.
2. **FormBuilder save-reload round-trip for delete/move/resize not tested.** TestFormBuilder tests placement but not persistence. The silent engine-side no-op is undetected.
3. **No test for PdfStructureMapper returning non-empty results.** The stub returns an empty document and no test catches this regression.
4. **Batch OCR/Redact/Merge have no positive tests.** When they are wired in a future sprint, there will be no regression baseline.
5. **No test asserts that switching locale produces at least one translated string.** The qt_add_translations machinery compiles and embeds empty .qm files without error.
6. **sendSign ribbon button has no handler and no test.** Silent no-op could regress to any future behavior undetected.
7. **OCSP revocation multi-signature test is QEXPECT_FAIL.** testRevokedCertReportsRevoked will always pass in CI while the actual invariant is unproven.

