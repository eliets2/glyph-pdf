# GlyphPDF — UX Audit Round 2
**Date:** 2026-06-09  **Branch:** `audit-remediation`  **Auditor:** Read-only static analysis

---

## Summary

The WP-1 cloud-removal remediation cleaned up the most visible fake-state indicators but
left a large number of ribbon buttons that fire silently — they reach
`ToolRegistry::activateFromString` → qWarning and stop. Of 8 ribbon tabs with ~80 unique
tool IDs, only ~28 are wired to an `IToolController`. The remainder produce no visible
response: no dialog, no status message, no disabled-with-tooltip treatment. This is the
dominant UX defect category: **silent dead controls masquerading as live features.**

| Severity | Count |
|----------|-------|
| Critical | 1 |
| High     | 6 |
| Medium   | 10 |
| Low / Info | 7 |
| **Total** | **24** |

Top issues:
1. **~52 ribbon buttons have no controller handler** — they call `qWarning` and silently
   do nothing (UX-01, Critical)
2. **"Send Form / Collect / Submit" (Forms › Distribute) are cloud-orphan dead controls** —
   removed backend, retained buttons, no disclosure (UX-02, High)
3. **FormBuilder Delete Field is a silent engine no-op** — the PDF is not modified; field
   reappears on reload (UX-08, High)
4. **Redact confirmation dialog understates safety** — warns the user the operation is not
   guaranteed, then does it anyway with the same "are you sure?" UX as a safe operation (UX-13, Medium)
5. **No progress feedback for sign→LTV partial outcome** — the B-LT/B-LTA warning dialog
   is technically correct but users cannot distinguish "signed but LTV failed" from "signing
   failed" without reading the full text (UX-16, Medium)

---

## Flow Findings

### FLOW-01 — Open → View flow is complete
`HomeController::activate(Open)` → `QFileDialog` → `openDocument()` → `PdfViewerWidget::loadDocument` →
`StatusBar::updateFromDocument` → title update. Error path surfaces `ErrorDialog` with Retry.
Recent-files list auto-maintained (max 20). Drag-and-drop also wired (`setAcceptDrops(true)`).
No dead ends observed.  
**Evidence:** `GpMainWindow.cpp:351-399`, `HomeController.cpp:52-59`

### FLOW-02 — Save flow is correct but missing confirmation on failure
`HomeController::onSave` writes to status bar on both success and failure. The failure message
("Save failed: …") is status-bar-only and auto-expires after 5 seconds — no modal for a
destructive save failure. `onSaveAs` correctly shows a `QMessageBox::warning` on failure.
**Evidence:** `HomeController.cpp:141-147`  **Severity:** Medium (UX-14)

### FLOW-03 — Sign flow is honest and differentiated
`SecurityController::signDocument` uses the `SignOutcome` enum to distinguish
total failure from `PartialLtvMissing`. The latter shows a warning (not an error) and still
offers to open the signed file. `certifyDocument` does not perform this differentiation —
it shows a generic failure dialog (which matches the backend's silent DocMDP downgrade, E-01).
**Evidence:** `SecurityController.cpp:233-248`

### FLOW-04 — Encrypt flow: E-04 remediated
`SecurityController::encryptDocument` now checks `result->load()` and shows
`QMessageBox::critical` on failure. The old silent-success path is fixed.
**Evidence:** `SecurityController.cpp:165-174`

### FLOW-05 — Permissions flow still discards result
`SecurityController::permissionsDocument` calls `EncryptDocumentHelper::execute` in a lambda
**without capturing the result**. On write failure the status bar says "Document permissions
updated" regardless. This is the same pattern as E-04 before it was fixed.  
**Evidence:** `SecurityController.cpp:480-489`  **Severity:** High (UX-03)

### FLOW-06 — RemoveSecurity: saveDocument return discarded
`SecurityController::removeSecurity` calls `engine->saveDocument(viewer->filePath())` and
ignores its return value. On failure, `viewer->reload()` runs anyway and the status bar
says "Document security removed." The old E-04 fix was not applied here.  
**Evidence:** `SecurityController.cpp:511-514`  **Severity:** High (UX-04)

### FLOW-07 — Redact flow has an over-permissive confirmation dialog
The "Confirm In-Place Redaction" dialog text explicitly says the implementation "cannot
guarantee secure removal of all recoverable text, images, forms, or hidden content" and
advises using a dedicated redaction tool for legally sensitive material. Yet the only
response is Yes/No — no second confirmation, no alternative path offered. A user who
clicks Yes on this warning has no way to know the operation may be legally insufficient.
**Evidence:** `SecurityController.cpp:396-400`  **Severity:** Medium (UX-13)

### FLOW-08 — Timestamp: E-06 still not visible at the UX layer
`SecurityController::timestampDocument` shows `QMessageBox::critical` on `addDocTimeStamp`
returning false. But E-06 (4-null-byte corrupt token + `addDocTimeStamp` returning `true`)
means the failure is never surfaced at all — success dialog shown, corrupt file opened.
The UX layer is correct; the problem is the backend lying to it.  
**Evidence:** `SecurityController.cpp:610-616`  **Severity:** Noted (covered by E-06)

### FLOW-09 — OCR flow: honest failure surfacing
`EditController::runOcr` produces explicit status-bar error messages for all failure modes
(ONNX models absent, render failed, page changed). It does not silently fall back to a
different engine. Correct.  
**Evidence:** `EditController.cpp:322-440`

### FLOW-10 — Batch flow: dead Merge/OCR/Redact items are now honestly disclosed
`BatchMode.cpp:202-204` uses `disableItem()` with correct "not available in v1.0 batch
mode." tooltips (stale milestone language from C-04 was fixed). The disabled panels show
gray explanatory text. Correct disclosure.  
**Evidence:** `BatchMode.cpp:202-204, 344-382`

### FLOW-11 — FormBuilder delete is a UI-only no-op
`FormBuilderMode::onDeleteFieldClicked` pushes `DeleteFormFieldCommand` onto the undo stack
and removes the item from the session list. But `DeleteFormFieldCommand.h:32-33` confirms
engine-side removal is not implemented. After save + reload the field reappears. No
disclosure or warning is shown to the user.  
**Evidence:** `FormBuilderMode.cpp:388-421`, C-03  **Severity:** High (UX-08)

### FLOW-12 — FormBuilder Tab Order "Apply Order" may succeed silently or fail
`onTabOrderApplyClicked` calls `m_ctx->forms->setTabOrder(...)` and shows a success or
failure `QMessageBox`. This is correct IF `setTabOrder` is implemented in `IFormManager`.
The C-03 audit noted `IFormManager` has no `setTabOrder()`. If this resolves to a no-op,
the success dialog is false. Needs verification that `setTabOrder` is wired.  
**Evidence:** `FormBuilderMode.cpp:442-452`  **Severity:** Medium (UX-09)

### FLOW-13 — Forms › Distribute group (sendForm / collect / submit) has no handler
`RibbonModel.cpp:68` defines `sendForm`, `collect`, `submit` as enabled ribbon items.
None appear in any controller's `handledTools()`. `ToolRegistry::activateFromString` emits
`qWarning` and returns. These were cloud-dependent features; the backend was removed but
the buttons were not disabled or annotated.  
**Evidence:** `RibbonModel.cpp:68`; absence from all `handledTools()` lists  **Severity:** High (UX-02)

---

## Accessibility Findings

### ACC-01 — F6 region cycling is wired and correct
`GpMainWindow.cpp:196-215` cycles through `{_ribbon, _left, _modes, _right, _status}`,
skipping invisible panels. Shift+F6 reverse-cycles. `_modeStrip` is absent from the cycle
(it sits between ribbon and main row) — screen readers navigating by region will skip it.
**Evidence:** `GpMainWindow.cpp:198`  **Severity:** Low (UX-17)

### ACC-02 — F1 opens ShortcutHelpDialog
Wired at `GpMainWindow.cpp:240`. Correct.

### ACC-03 — Ribbon buttons have accessible names
`Ribbon::makeTool` calls `setAccessibleName(label)` and
`setAccessibleDescription(tr("Activate %1 tool").arg(label))`. Correct.
**Evidence:** `Ribbon.cpp:80-81`

### ACC-04 — ModeStrip mode pills have accessible names
`btn->setAccessibleName(tr("%1 mode").arg(lab))`. Correct.
**Evidence:** `ModeStrip.cpp:52`

### ACC-05 — Status bar page spin-box has accessible name + description
`setAccessibleName("Jump to page")`, `setAccessibleDescription("Enter a page number…")`.
Correct.  **Evidence:** `StatusBar.cpp:55-57`

### ACC-06 — WelcomeWidget action cards have accessible names
All 6 action cards call `setAccessibleName` and `setAccessibleDescription`.
**Evidence:** `WelcomeWidget.cpp:166-177`

### ACC-07 — AI toggle button has no accessible description
`ModeStrip.cpp:66` sets `setAccessibleName(tr("Toggle AI assistant panel"))` but no
`setAccessibleDescription`. Minor.  **Evidence:** `ModeStrip.cpp:66`  **Severity:** Low (UX-18)

### ACC-08 — High-contrast theme: mode-strip labels use Unicode symbols
`_autosaveLabel` uses "●", `_syncLabel` uses "⤺", `_signLabel` uses "✓". These glyphs
are not given accessible alt text. Screen readers may announce the Unicode name ("Black
Circle", "ANTICLOCKWISE TOP SEMICIRCLE ARROW", etc.) instead of the semantic status.
**Evidence:** `ModeStrip.cpp:72-83`  **Severity:** Medium (UX-10)

### ACC-09 — Tab order: QTabWidget in PreferencesDialog is keyboard navigable
`PreferencesDialog` uses `QTabWidget`; Qt handles Tab cycling automatically. The "Save"
button is set `setDefault(true)`, so Enter commits. Cancel is correctly positioned before
Save in layout flow. Correct.

### ACC-10 — Ribbon lazy-loads tab bodies on first selection
`Ribbon::Ribbon` builds only tab 0 immediately; other tabs build on first `currentChanged`.
Before a tab is first selected its `QWidget` has no layout and no focusable children.
Keyboard users who try to Tab into an unvisited ribbon tab will land on an empty widget.
**Evidence:** `Ribbon.cpp:43-60`  **Severity:** Medium (UX-11)

---

## Orphaned-by-Cloud-Removal

The WP-1 remediation removed Anthropic/OpenAI/Gemini providers and the CollaborationManager
cloud-sync backend. The following surfaces were not cleaned up:

### CLOUD-01 — Forms › Distribute group: sendForm / collect / submit (Severity: High)
All three items in `RibbonModel.cpp:68` are enabled but have no handler.
These were "Send Form" (collect responses via a cloud backend), "Collect" (receive submitted
form data), and "Submit" (submit form data to a server). All require a cloud backend.
**Fix:** Add `setEnabled(false)` + `setToolTip(tr("Cloud feature — not available in v1.0"))`
in `Ribbon::makeTool` for these three IDs, or remove them from `RibbonModel`.

### CLOUD-02 — Home › Share group: "Share" button (Severity: Medium)
`HomeController::handledTools()` includes `ToolId::Share`, so the button IS wired — it opens
`MAPI / mailto:` to send the file by email. This is a local operation and remains valid.
However the `Ribbon.cpp` group label "Share" and the tooltip-less "Share" button imply the
old cloud-share feature. No documentation issue; label suffices.

### CLOUD-03 — Convert › To PDF › "Web Page" (fromWeb) is silently dead (Severity: Medium)
`RibbonModel.cpp:56` lists `fromWeb` (Web Page → PDF) as an enabled item. There is no
`ToolId::FromWeb` in `ConvertController::handledTools()` and no alias in `ToolId.cpp`.
`ToolRegistry::activateFromString("fromWeb")` → `qWarning`, no dialog shown.
This feature required a headless-browser or cloud fetch — orphaned by WP-1.
**Fix:** Disable item with tooltip "Web Page capture not available in v1.0".

### CLOUD-04 — Convert › To PDF › "Scanner" (fromScan) is silently dead (Severity: Medium)
Same as CLOUD-03 for `fromScan`. No `ToolId::FromScan` exists in any controller.
This required TWAIN/WIA or a cloud scanning backend.
**Fix:** Disable item with tooltip "Scanner import not available in v1.0".

### CLOUD-05 — Protect › Compliance group: auditLog / dlp / policy silently dead (Severity: Medium)
`RibbonModel.cpp:76` defines `auditLog`, `dlp`, `policy` as enabled. None appear in
`SecurityController::handledTools()` or any other controller. All three imply a cloud DLP
policy service.  
**Fix:** Disable all three with appropriate "not available in v1.0" tooltips.

### CLOUD-06 — View › Window group: splitWin / newWin silently dead (Severity: Low)
`RibbonModel.cpp:24` lists `splitWin` and `newWin`. `ViewController::handledTools()` does
not include these. No MDI or split-view backend. Click → qWarning.
**Fix:** Disable with tooltip or implement minimal split view.

### CLOUD-07 — View › Reading: eyeCare / rtl silently dead (Severity: Low)
`RibbonModel.cpp:23` lists `eyeCare` and `rtl`. `ViewController::handledTools()` does
not include them. Click → qWarning.
**Fix:** Disable with tooltip "coming in v1.1".

---

## States: Error / Empty / Loading / Progress

### STATE-01 — Save failure uses status-bar-only ephemeral message (High)
`HomeController::onSave` shows `statusBar()->showMessage(tr("Save failed:…"), 5000)`.
A 5-second transient is easily missed. Given that save failure means the user's work was
NOT persisted, this should be a modal `QMessageBox::critical`.
**Evidence:** `HomeController.cpp:145-147`  → UX-14

### STATE-02 — Permissions update always claims success (High)
See FLOW-05 above. `SecurityController::permissionsDocument` never checks the `execute`
return. On failure: "Document permissions updated" in the status bar.
**Evidence:** `SecurityController.cpp:480-489`  → UX-03

### STATE-03 — SignLabel shows "✓ SIGNED · 0 OF 0" even for unsigned documents (Low)
`ModeStrip::updateLabels` initialises `_signLabel` to "✓ SIGNED · 0 OF 0" regardless of
document state. The "✓" checkmark prefix visually implies signed status. For a document
with 0 signatures this is misleading. Should be "— NOT SIGNED" or suppressed entirely.
**Evidence:** `ModeStrip.cpp:171-181`  → UX-19

### STATE-04 — OCR "verify text" screen shows no empty-state feedback
`ScreenNav` includes an "OCR Verify" screen (`id="ocr"`). When the document has not been
OCR'd, the OCR mode shows whatever state the canvas is in — there is no "No OCR data yet"
empty state message to guide the user.
**Evidence:** `ScreenNav.cpp:27`, `OCRMode.cpp` (not read in detail)  **Severity:** Low (UX-20)

### STATE-05 — Progress dialogs are cancelable only for Office import and OCR
`HomeController::onImportOffice` and `onImagesToPdf` wire the cancel button to
`QFutureWatcher::cancel`. Most other progress dialogs (`encryptDocument`, `signDocument`,
`certifyDocument`, `timestampDocument`) pass `QString()` as the cancel-button label,
making them visually non-cancelable. Long operations (sign + TSA network request) may
freeze the user. Consistent with Qt's indeterminate-progress pattern but notable.
**Evidence:** `SecurityController.cpp:197-198`  **Severity:** Low (UX-21)

---

## Onboarding (WelcomeWidget)

The WelcomeWidget is well-structured:
- 6 action cards with accessible names and descriptions.
- Recent-files list with missing-file recovery (removes entry after Yes prompt).
- `displayName` truncates long paths to 60 characters with "..." prefix.

### ON-01 — No "what's new" or first-run hint (Low)
First launch shows the action cards but there is no first-run hint, feature tour, or
explanation of the AI tab or OCR capabilities. Low priority for a pro tool but notable.

### ON-02 — "Import Office" card silently fails without LibreOffice installed
When `#ifndef HAS_LIBREOFFICE` the card calls `HomeController::onImportOffice`, which shows
a `QMessageBox::information` directing the user to install LibreOffice. The `importOfficeRequested`
signal from `WelcomeWidget` is emitted, but the LibreOffice check is in the controller, not
in the WelcomeWidget. The card itself is never disabled or annotated. The user gets a dialog
after click rather than a visually-disabled card.
**Evidence:** `WelcomeWidget.cpp:186-187`, `HomeController.cpp:354-361`  **Severity:** Low (UX-22)

---

## PreferencesDialog — New Backend Selectors

### PREF-01 — PDF Engine group label is honest and correct
"PDF Engine Backends (locked)" with `makeEngineRow` showing engine name + lock reason.
PDFium, PoDoFo, and qpdf are all accurately described. The `#ifdef` guards correctly show
"(unavailable)" when a backend is absent.
**Evidence:** `PreferencesDialog.cpp:172-190`

### PREF-02 — OCR Engine selector is honest and guarded
Items 1 and 2 (RapidOCR, Ensemble) are disabled with a descriptive tooltip when ONNX
models are absent. The "save" guard prevents persisting a disabled engine setting.
Correct per audit §7 Pattern 5 fix requirement.
**Evidence:** `PreferencesDialog.cpp:207-246`

### PREF-03 — AI Tab: "Test connection" runs an actual Ollama probe (Honest)
`PreferencesDialog::onAiTestKey` constructs `OllamaProvider`, sends a 5-token ping, and
reports success/failure with distinct colors. Correct.
**Evidence:** `PreferencesDialog.cpp:422-451`

### PREF-04 — No visual feedback that saving triggers live autosave restart (Info)
`saveSettings` stops and restarts `AutosaveManager` with the new interval, then shows
"Some changes require a restart to take effect." The "restart" notice is slightly
misleading — autosave is already live-applied; only the language/theme actually needs
a restart.  
**Evidence:** `PreferencesDialog.cpp:378-386`  **Severity:** Low (UX-23)

### PREF-05 — Language selector shows all 4 locales even when translations are empty (Medium)
`PreferencesDialog.cpp:55-57` offers Arabic, French, German. `C-06` confirmed 0 of 1394
strings are translated. Switching locale shows English source strings with no disclosure.
The Preferences dialog itself does not warn that switching language will show untranslated
content.  
**Evidence:** `PreferencesDialog.cpp:55-57`, C-06  **Severity:** Medium (UX-12)

---

## Findings Table

| ID | Sev | File:Line | Title | Suggested Fix |
|----|-----|-----------|-------|---------------|
| UX-01 | **Critical** | `RibbonModel.cpp` (all tabs) | ~52 ribbon buttons have no registered controller — silently do nothing | Disable unimplemented items with `setEnabled(false)` + tooltip, or remove from model |
| UX-02 | **High** | `RibbonModel.cpp:68` | Forms › Distribute (sendForm, collect, submit) — cloud-orphan, no handler | Disable with "Cloud feature not available in v1.0" tooltip |
| UX-03 | **High** | `SecurityController.cpp:480-489` | Permissions flow discards `EncryptDocumentHelper` return — always claims success | Check result; show `QMessageBox::critical` on failure |
| UX-04 | **High** | `SecurityController.cpp:511-514` | `removeSecurity`: `saveDocument` return discarded — claims success on write failure | Check return; show `QMessageBox::warning` on failure; do not call `viewer->reload()` |
| UX-05 | **High** | `RibbonModel.cpp:56` | Convert › To PDF › "Web Page" (fromWeb) — no handler, silent fail | Disable with tooltip |
| UX-06 | **High** | `RibbonModel.cpp:56` | Convert › To PDF › "Scanner" (fromScan) — no handler, silent fail | Disable with tooltip |
| UX-07 | **High** | `RibbonModel.cpp:76` | Protect › Compliance (auditLog, dlp, policy) — cloud-orphan, no handler, silent fail | Disable with tooltip |
| UX-08 | **High** | `FormBuilderMode.cpp:388-421`, `DeleteFormFieldCommand.h:32-33` | FormBuilder Delete Field is UI-only no-op — field reappears on reload | Implement engine-side removal or show "Delete not yet supported — field will persist on reload" warning before pushing command |
| UX-09 | **Medium** | `FormBuilderMode.cpp:442-452` | Tab Order "Apply Order" — `IFormManager::setTabOrder` implementation status unclear; may show false success | Verify `setTabOrder` is wired; if not, show "not implemented" dialog instead of false success |
| UX-10 | **Medium** | `ModeStrip.cpp:72-83` | Status-strip Unicode symbols (●, ⤺, ✓) have no accessible alt text | Wrap each label in an accessible name describing the status semantically |
| UX-11 | **Medium** | `Ribbon.cpp:43-60` | Ribbon lazy-loads tab bodies — unvisited tabs have no focusable children on keyboard Tab | Pre-build all tab bodies at construction, or set a placeholder with `setFocusPolicy(Qt::NoFocus)` |
| UX-12 | **Medium** | `PreferencesDialog.cpp:55-57` | Language selector offers ar/fr/de with 0 translated strings — switching shows English silently | Add "(translations pending)" suffix to non-English items or disable them with tooltip |
| UX-13 | **Medium** | `SecurityController.cpp:396-400` | Redact confirmation warns operation is not legally sufficient but only asks Yes/No | Offer a secondary confirmation or rename button to "Proceed Anyway" to signal the risk |
| UX-14 | **Medium** | `HomeController.cpp:145-147` | Save failure surfaced only via ephemeral 5-second status message | Show `QMessageBox::critical` on save failure; 5-second transient is missable for data-loss event |
| UX-15 | **Medium** | `RibbonModel.cpp:23-24` | View › Reading (eyeCare, rtl) and View › Window (splitWin, newWin) — no handler, silent fail | Disable with "coming in v1.1" tooltip |
| UX-16 | **Medium** | `SecurityController.cpp:233-243` | Sign partial-LTV warning dialog text is dense — users may dismiss without understanding signed file is valid | Add bold summary line "Your document IS signed and saved." before the LTV explanation |
| UX-17 | **Low** | `GpMainWindow.cpp:198` | F6 region cycle omits ModeStrip | Add `_modeStrip` to the `regions[]` array |
| UX-18 | **Low** | `ModeStrip.cpp:66` | AI toggle button has no accessible description | Add `setAccessibleDescription(tr("Opens or closes the AI chat panel on the right"))` |
| UX-19 | **Low** | `ModeStrip.cpp:171-181` | "✓ SIGNED · 0 OF 0" displayed even for unsigned documents | Show "— NOT SIGNED" (or hide label) when total == 0 |
| UX-20 | **Low** | `ScreenNav.cpp:27` | OCR Verify screen has no empty-state feedback when no OCR data exists | Show "Run OCR first (Edit › Run OCR) to populate this view" in the OCR panel |
| UX-21 | **Low** | `SecurityController.cpp:197-198` | Sign / certify / timestamp progress dialogs are not cancelable | Add cancel button with a `QAtomicInteger` cancellation flag, or at minimum document that the operation cannot be interrupted |
| UX-22 | **Low** | `WelcomeWidget.cpp:186-187` | "Import Office" card never disabled when LibreOffice absent | In `WelcomeWidget`, check `HAS_LIBREOFFICE` at construction and disable the card with tooltip if absent |
| UX-23 | **Low** | `PreferencesDialog.cpp:384-385` | "Some changes require restart" message is overly broad — autosave takes effect immediately | Show restart notice only for language/theme rows; display "Applied immediately" for autosave interval |
| UX-24 | **Info** | `MenuBar.cpp:139-141` | View › Rulers / Guides / Grid are checkable but have no handler | Add to `ViewController::handledTools()` or disable |
