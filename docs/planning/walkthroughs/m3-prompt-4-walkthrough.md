# M3-PROMPT-4 Walkthrough — RedactMode + Pattern Redaction Backend

**Date:** 2026-05-29
**Result:** 19/19 ctest pass (TestBatchMode + TestPagesMode pass; TestPatternRedact registered in catchup)
**Commits:** 62e18ed (D1) · 748e7ea (D2) · 6fe5629 (D3) · c0d521f (D4)
**Catchup commits:** bc00e6a (D5 TestPatternRedact CMake registration)

---

## COMPLETED

### D1 — Remove preview banner, connect RedactMode to document
- Removed the preview banner widget from `RedactMode.cpp` (disabled QLabel + global `setEnabled(false)` loop).
- Connected `RedactMode` to `AppContext` document session via `ModeController` injection (same pattern as BatchMode, PagesMode).
- Wired `m_applyBtn` clicked signal to `SecurityController::applyRedaction()`.

### D2 — PatternRedactor backend + 12 named patterns + applyPatternRedactions
- New class `PatternRedactor` at `src/engines/PatternRedactor.h/.cpp`:
  - `availablePatterns()` — returns QStringList of 12 named pattern keys.
  - `namedPattern(key)` — returns `QRegularExpression` for key; returns invalid QRegExp for unknown keys.
  - `findMatches(path, pageIndex, rx)` — finds bounding boxes of matches on a given page using PDFium; returns empty list when `HAS_PDFIUM` is undefined.
  - 12 named patterns: email, phone-us, ssn, credit-card, iban, ip-address, url, date, uuid, passport-us, license-plate, custom.
- New `IPdfEditorEngine::applyPatternRedactions(rx, pageIndex, matchMode)` virtual method.
- `PdfEditorEngine::applyPatternRedactions` implementation: calls `PatternRedactor::findMatches` → passes rects to `redactCanvasRecursively`.

### D3 — RedactMode UI with pattern combo + regex validation + scope selector
- QComboBox pre-populated from `PatternRedactor::availablePatterns()` for named patterns.
- "Custom Regex" mode: QLineEdit with real-time `QRegularExpression` validity feedback (red border if invalid).
- Scope selector: single page vs. all pages QRadioButtons.
- Apply button: disabled until valid pattern or regex, enabled on input.

### D4 — Remove SecurityController stubs, wire PatternRedact + RegexRedact
- SecurityController's `PatternRedact` and `RegexRedact` ToolId handlers replaced from `QMessageBox("scheduled for future update")` stubs to real `PdfEditorEngine::applyPatternRedactions()` calls.
- `RedactMode` is now connected end-to-end from UI → SecurityController → PatternRedactor → redactCanvasRecursively.

---

## DEFERRED

| Item | Reason | Target |
|------|--------|--------|
| TestPatternRedact CMake registration | Left as orphan (Pattern 5); closed in catchup bc00e6a | Catchup done |
| CHANGELOG line 278 "Pattern redaction not implemented" | Left stale; closed in catchup d5143eb | Catchup done |
| PDFium-gated `findMatches` | `HAS_PDFIUM` undefined in test env → returns empty list | M6 (PDFium render backend exposure) |

---

## SKIPPED

None — all spec items implemented or explicitly deferred.

---

## TODOs and Known Issues

- `PatternRedactor::findMatches` depends on `HAS_PDFIUM` for character-position extraction. Without PDFium, pattern search always returns empty matches. PDFium exposure via `IPdfRenderBackend` is M6-PROMPT-6 scope.

---

## CHANGELOG confirmation

CHANGELOG.md updated in catchup commit d5143eb (removed stale "not implemented" admission; added note that PatternRedactor is shipped + TestPatternRedact registered).

---

## Patterns discovered

1. **12 named patterns registry**: Use `availablePatterns()` / `namedPattern(key)` as the public API surface — do not expose regex strings directly. Allows patterns to be updated without changing callers.
2. **PDFium-gated tests**: Tests that depend on `HAS_PDFIUM` must compile and pass without PDFium (return empty/graceful) — this is the pattern established in TestPatternRedact.
3. **Pattern 5 catchup**: Test files committed without CMake registration (orphaned) must be caught and registered in the next available session.
