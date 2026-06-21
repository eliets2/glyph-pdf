# GlyphPDF — Spec ↔ Code Traceability (v1.3.1)

Maps each PRD §9 functional requirement to the **actual implementing code**, verified
against the `audit-remediation` tree at v1.3.1. Status mirrors PRD §27.

Legend: ✅ Done · 🟡 Partial · ⬜ Planned. Line numbers are indicative (current at v1.3.1).

| § | Requirement | Status | Implementing code (verified) |
|---|-------------|--------|------------------------------|
| 9.1 | Viewing (single/continuous/two-page/presentation, dark mode, nav) | ✅ | `src/ui/PdfViewerWidget.{h,cpp}`; modes `ToolId.h:33-39` (SinglePage/Continuous/TwoPage/Presentation); `src/shell/controllers/ViewController.cpp` |
| 9.2 | Text & object editing, image edit, undo/redo | ✅ | `IPdfEditorEngine.h`: `editTextInline:127`, `deleteObjectAt:131`, `moveImage:171`, `rotateImage:173`; `EditController.cpp` |
| 9.3 | Annotation: highlight/underline/strike/squiggly, notes, callouts, stamps, shapes, freehand, attachments | 🟡 | `src/ui/AnnotationLayer.cpp` (Stamp/Callout render); `ToolId.h` Stamp/Callout/Erase; `AnnotationTypes.h` `attachmentPath`; `EditController.cpp` dispatch. **Gap:** Erase = placeholder `EditController.cpp:109-114`; comment review UI (summary/filterComm/statusComm/trackChanges/reply) in `RibbonModel.cpp:27` `plannedTools()` |
| 9.4 | OCR ensemble + searchable + Verify review screen | ✅ | `src/engines/ocr/OcrPipeline.*`; `src/modes/OCRMode.cpp`; nav `ScreenNav.cpp:27`; lazy-init `ModeController.cpp:42` |
| 9.5 | Conversion to Word/Excel/PPT/image/text/HTML/CSV + batch + presets | 🟡 | `IConversionEngine.h` TargetFormat (Word/Excel/Html/Image/Csv/Text/PPT); `src/engines/ConversionManager.cpp`. **Gap:** Markdown/EPUB (`toMD`/`toEPUB` in `RibbonModel.cpp:29` plannedTools) |
| 9.6 | Forms — 10 field types incl. calculated, tab order, flatten, data | ✅ | `IFormManager.h` `addTextField:22`…`createButton:37`, **`addCalculatedField:44`**; `FormManager.cpp`; `FormBuilderMode.cpp` (10 types). **Note:** data export CSV/FDF only (`IFormManager.h:70`) |
| 9.7 | E-signatures — local + certificate (PAdES) | 🟡 | `src/engines/SignatureManager.{h,cpp}` (PAdES B-LT/B-LTA, trust chain, OCSP). **Not started:** multi-party send-for-signing / order / reminders / status / audit trail (no API present) |
| 9.8 | Redaction — mark, pattern, permanent excision | ✅ | `IPdfEditorEngine.h` `applyRedactions:176`, `applyPatternRedactions:181`; `src/engines/PatternRedactor.cpp` |
| 9.9 | Page management — insert/delete/reorder/rotate/split/merge/extract/crop/resize, numbering, Bates | ✅ | `src/engines/podofo/PdfPageOps.h` `extractPages:19`/`deletePages:22`/`insertBlankPage:26`/`rotatePages:30`/`mergeDocuments:36`; `PagesController.cpp` |
| 9.10 | Comparison — visual + text diff, report export, page-reorder | ✅ | `src/engines/DiffEngine.*` (`pageMoves`); `CompareMode.cpp` `onExportReport:61`, `buildHtmlReport`, page moves `:144` |
| 9.11 | Security — passwords/permissions/AES-256/watermark/sanitize + encrypted package + expiry | 🟡 | `IPdfEditorEngine.h` `encryptDocument:148`/`sanitizeDocument:154`/`addTextWatermark:197`; `PdfEditorEngine.h` **`setExpiryDate:50`**; `HomeController.h` **`createEncryptedPackage:35`**. Out of scope (local-first): URL secure links, server revocation |
| 9.12 | Batch & automation — convert/OCR/compress/watermark/redact/merge + hot folder | 🟡 | `BatchMode.h` `OpMerge:162`/`OpOCR:163`/`OpRedact:164` (no longer disabled); hot folder `onToggleHotFolder:80`, `m_hotFolderWatcher:177`. **Gap:** named preset workflows |
| 9.13 | Compression & optimization | ✅ | `IPdfEditorEngine.h` `optimizeDocument:202`, `estimateOptimization:201` |
| 9.14 | Accessibility — tagged-PDF reading order | 🟡 | `PdfAValidationPanel.cpp` `onCheckReadingOrder:463`, `/StructTreeRoot` walk `:413`. **Gap:** tag preservation/repair on export, screen-reader labels |
| 9.15 | Search & navigation — full-text, thumbnails, bookmarks | 🟡 | `src/ui/FindBar.*`, `PdfViewerWidget::searchDocument`. **Gap:** regex + find-and-replace (`regex`/`findRep` in `RibbonModel.cpp:13` plannedTools) |
| 9.16 | File import & export | ✅ | `ToolId.h` `ImportOffice:98`; `ConvertController.cpp`; `ConversionManager.cpp` |

## How to re-verify

```sh
# Example: confirm the calculated form field exists end-to-end
grep -n addCalculatedField src/core/interfaces/IFormManager.h src/engines/FormManager.cpp \
  src/commands/AddFormFieldCommand.h src/modes/FormBuilderMode.cpp

# Confirm a "gap" is genuinely disabled (in plannedTools, not wired)
grep -n '"regex"\|"findRep"\|"toMD"\|"toEPUB"\|"reply"' src/shell/RibbonModel.cpp
```

The `plannedTools()` set in `src/shell/RibbonModel.cpp` is the source of truth for which ribbon
buttons are intentionally disabled (planned-but-unimplemented). `TestRibbonIntegrity` enforces
that every *enabled* ribbon tool resolves to a handled `ToolId` and that no planned tool is also
wired to a controller — so this table cannot silently drift from the code.
