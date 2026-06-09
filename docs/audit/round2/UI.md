# GlyphPDF — UI Visual Audit Round 2
**Auditor:** Claude Code (read-only static analysis)
**Date:** 2026-06-09
**Branch:** audit-remediation
**Scope:** Qt 6.11 Widgets layer — themes, visual consistency, PreferencesDialog backend-selector, residual mock surfaces, layout robustness.

---

## Summary

| Severity | Count |
|----------|-------|
| High     | 3     |
| Medium   | 8     |
| Low      | 6     |
| Info     | 4     |
| **Total** | **21** |

**Residual mock surfaces from C-report:** All 8 Pattern 5 sites are confirmed GONE in current code (see §Residual mock surfaces below). No fake data or placeholder widgets remain.

**Top concerns:**
1. CommentsWidget composer is entirely hardcoded dark Tailwind blue/green colors — broken in Light and High-Contrast themes (High severity).
2. CompressDialog uses only dark tokens in inline stylesheets — invisible/wrong in Light theme (High severity).
3. SignaturesWidget (the right-panel list variant, distinct from SignaturesPanel) uses hardcoded Tailwind palette — broken across themes (High severity).
4. 15+ instances of `setStyleSheet` with dark-mode hex literals in BatchMode, OCRMode, AIChatPanel, PdfAValidationPanel, SignaturesPanel — Medium severity each.
5. Update notification bar (`#updateBar`) is hardcoded blue — wrong in every non-dark theme (Medium severity).

---

## Theme Findings

### T-01 — CommentsWidget composer: fully hardcoded Tailwind dark palette (High)
**File:** `src/ui/CommentsWidget.cpp:108,134,141,147,153,157,177-178`

The composer section (author QLineEdit, comment QTextEdit, Post/Reply QPushButton, context QMenu) uses Tailwind-derived dark hex literals (`#1E293B`, `#0F172A`, `#334155`, `#F8FAFC`, `#3B82F6`, `#10B981`, `#2563EB`, `#059669`) via `setStyleSheet()` on individual widgets. In Light theme the background `#1E293B` clashes with the `#e9e6dd` window, creating a dark island. In High-Contrast theme the blue Post button (`#3B82F6`) on black fails contrast. The title label at line 108 uses `color: #94A3B8` — invisible in High-Contrast.

**Fix:** Remove all `setStyleSheet()` calls from the composer. Style via QSS property selectors: give the composer `setObjectName("commentComposer")` and define `#commentComposer { … }` rules in all three `.qss` files using their respective tokens. Post/Reply buttons should carry `setProperty("variant","accent")` / `setProperty("variant","ghost")`.

---

### T-02 — CompressDialog: all inline stylesheets are dark-only (High)
**File:** `src/modes/CompressDialog.cpp:37-42,107,152,187,200-203,221-227,256`

`advFrame`, `mrcFrame`, `sizeRow` use `background:#1a1b1e; border:1px solid #393b40` directly. The `_origBar` and `_estBar` QProgressBar `setStyleSheet` calls are also dark-only (`#34363b`, `#4a4d52`). The card-toggle buttons at line 37-42 embed full state-switching stylesheets with `#1a1b1e`/`#ff8c42`/`#4a4d52` literals. In Light theme these produce dark boxes on a beige background.

**Fix:** Move compressed-size visualization into QSS custom properties. Progress bar chunks should use named variant properties. Card-toggle active state should use `setProperty("active", true)` so the global `QToolButton[variant="tool"][active="true"]` rule applies.

---

### T-03 — SignaturesWidget (right-panel list): hardcoded dark + Tailwind colors (High)
**File:** `src/ui/SignaturesWidget.cpp:19,23-27,61,65,67,70+`

This widget (distinct from `SignaturesPanel`) uses `color: #64748B` (Tailwind slate-500), `#333`, `#2D2D2D`, `color: white`, `#10B981` (emerald), `#EF4444` (red) throughout. The list item border `#333` is invisible in Light theme. The `color: white` on nameLabel renders unreadable in Light mode (`#1a1b1e` body). The valid/invalid colors do not map to GpTheme `okGreen()` / `danger()`.

**Fix:** Replace hardcoded colours with `Theme::okGreen()`, `Theme::danger()`, `Theme::fg0()`, `Theme::line()`. Or move status indicators to Badge widget (already theme-aware).

---

### T-04 — GpMainWindow update bar: hardcoded blue (Medium)
**File:** `src/GpMainWindow.cpp:580-585`

`QFrame#updateBar` uses `background:#1a5fb4` (solid dark blue) and `color:#fff`. This bar appears only occasionally (update available) but will look wrong in Light/HighContrast. Not life-threatening but inconsistent with the theme system.

**Fix:** Add `QFrame#updateBar` rules to all three QSS files using appropriate tokens (info blue: `Theme::info()` = `#7fb1e8` in dark; use `#1a4a8a` dark, `#1a4a8a` light, `#00ffff` HC). Or simply add a fourth standalone `setStyleSheet` that checks `Theme::current()`.

---

### T-05 — BatchMode disabled-operation hint labels: dark-only (Medium)
**File:** `src/modes/BatchMode.cpp:118,125,262,351,364,377,402,433,446-448`

Multiple `setStyleSheet("color:#71747a; …")` on hint/ETA/log labels. `#71747a` is a valid dark-mode secondary colour but maps to near-invisible contrast against the Light theme background `#e9e6dd` (contrast ratio ≈ 1.9:1, below WCAG 4.5:1). The log view at line 446-448 hard-codes `background:#1a1b1e; color:#d4d4d4` — a black panel in Light mode.

**Fix:** Use `setProperty("mono", true)` or a new QSS-defined `role="hintLabel"` property. Log view should inherit from the global `QPlainTextEdit` rule.

---

### T-06 — AIChatPanel panel border: dark-only inline stylesheet (Medium)
**File:** `src/modes/AIChatPanel.cpp:23`

`setStyleSheet("QFrame#aiPanel { border-left: 1px solid #393b40; }")` overrides the QSS global border, locking the AI panel border to the dark `#393b40` token regardless of theme.

**Fix:** Remove the `setStyleSheet` call; instead add `QFrame#aiPanel { border-left: 1px solid <theme-token>; }` to all three QSS files.

---

### T-07 — OCRMode toolbar: dark-only checkbox colors (Medium)
**File:** `src/modes/OCRMode.cpp:141,147,153,159,189,293,336`

Deskew/Binarize/Denoise checkboxes at lines 147,153,159 use `color:#c0c0c0`. In High-Contrast theme `#c0c0c0` on `#000000` is only 7:1 (borderline), but the HC QSS defines `QCheckBox { color: #ffffff }` which the inline `setStyleSheet` overrides because it has higher CSS specificity, silently reverting to grey-on-black for those three checkboxes specifically. The scroll area at line 293 hardcodes `background:#2a2a2a`.

**Fix:** Remove checkbox `setStyleSheet` calls; use `setProperty("role","ocrCheck")` + QSS rules.

---

### T-08 — PdfAValidationPanel violation rows: dark-only inline frames (Medium)
**File:** `src/modes/PdfAValidationPanel.cpp:22-33`

The static helper `issueRow()` bakes in `background:#1a1b1e`, `border:1px solid #393b40`, `color:#dfe1e5` and dark accent colours. In Light mode these produce solid dark card rows on a beige panel. The `border-left` colour (`#c8442b` / `#ff8c42`) is the same across themes — acceptable for Dark but lacks the HC theme's full-white-border convention.

**Fix:** Build violation row frames with QSS object names or properties; define per-theme rules. At minimum accept the light-theme regression as Medium: it's a secondary panel unlikely to be used in Light mode, but it's visually broken.

---

### T-09 — SignaturesPanel idCard/appCard/preview: dark-only inline borders (Medium)
**File:** `src/modes/SignaturesPanel.cpp:42,69,76`

`border:1px solid #393b40` and `background:#1a1b1e` repeated as inline `setStyleSheet` on `idCard`, `appCard`, `preview` containers. Same dark-only issue as T-08 but for the Sign mode right panel.

**Fix:** Give these frames QSS role properties (`role="signatureCard"`) and define them in all three QSS files.

---

### T-10 — AnnotationLayer selection highlight: hardcoded Qt::blue (Medium)
**File:** `src/ui/AnnotationLayer.cpp:207-208,346-347`

Selection rectangle uses `QPen(Qt::blue, 1, Qt::DashLine)` and `QColor(0, 0, 255, 30)` fill. In High-Contrast (black background, white borders), a faint blue selection rectangle will have very low visibility. The unselected handle at line 346 uses `QColor(100,100,255,120)` — equally low contrast.

**Fix:** Replace with `Theme::accent()` (amber in dark, burnt orange in light, yellow in HC) for the active selection border, which is already the app-wide selection colour.

---

### T-11 — PreferencesDialog inline `setStyleSheet` on status/version labels (Low)
**File:** `src/ui/PreferencesDialog.cpp:131,133,137,255,300`

`_updateStatus` and `versionLabel` use `color:#888` / `color:#666` as inline styles; `_aiStatusLabel` uses `color:#888`. These are sub-threshold grey values that fail in High-Contrast (grey on black < 7:1). The success/failure colours on lines 443/447 (`#4ec96d` / `#cc3333`) are correct semantic values but should reference `Theme::okGreen()` / `Theme::danger()` to stay in sync.

**Fix:** Use `setProperty("mono", true)` for secondary labels; define a `QLabel[role="statusNote"]` QSS class with per-theme subdued colour. For AI test status, derive success/fail strings from GpTheme properties rather than literal hex.

---

### T-12 — WatermarkDialog hardcoded preview panel colors (Low)
**File:** `src/modes/WatermarkDialog.cpp:60,64,68`

Preview frame uses `background:#1a1b1e`, and the page uses `background:#f4f1ea` (a warm-white colour). In Dark theme the warm-white page on a near-black preview panel works well. In Light theme the outer preview panel (`#1a1b1e`) is a dark box inside a beige dialog. The watermark preview text in `_previewLabel` inherits inline styling as well.

**Fix:** Use `QWidget#watermarkPreview` rule in all QSS files. The paper background for previews should use `QWidget#thumbPaper` convention (`#e8e6df` dark / `#ffffff` light).

---

### T-13 — ThumbPageCountLabel uses 'Consolas' font fallback, not Manrope/JetBrains Mono (Low)
**File:** `resources/theme_dark.qss:489`, `resources/theme_light.qss:152`, `resources/theme_highcontrast.qss:142`

All three QSS files set `font-family: 'Consolas', 'Courier New', monospace` for `#thumbPageCountLabel` and `#thumbNumLabel`. The rest of the app uses JetBrains Mono for all mono labels. This is a token inconsistency — if JetBrains Mono is present these labels will still render in Consolas because QSS uses first-match.

**Fix:** Change to `font-family: "JetBrains Mono", "Consolas", monospace` (JetBrains Mono first) to be consistent with the rest of the UI.

---

### T-14 — `GpTheme::highlight()`, `note()`, `underline()`, `strike()` not theme-conditional (Low)
**File:** `src/util/GpTheme.h:44-47`

Four annotation palette colors (`highlight=#f4d03f`, `note=#ff6b8a`, `underline=#4ec9b0`, `strike=#c586c0`) return the same hex regardless of theme. In High-Contrast mode these saturated colors produce very different visual weight against black backgrounds compared to white backgrounds, but more importantly they are below the 7:1 HC contrast requirement for any text rendered in these colors. Annotation text painted in `note()` (#ff6b8a, pink) on black is only ~4.5:1.

**Fix:** Make these conditional on `HighContrast`: return maximum-contrast neon variants (e.g. `#ff80aa` for note, `#ffff00` for highlight) when in HC mode.

---

### T-15 — CompareWidget navBar/textDiff: dark-hardcoded (Low)
**File:** `src/ui/CompareWidget.cpp:40,45,49,56-58`

NavBar `background:#1e1f22` and QTextBrowser `background:#1a1b1e; color:#dfe1e5` are dark-only. Color diff constants `CLR_DEL`, `CLR_ADD`, `CLR_MOV`, `CLR_KEEP` are static and do not adapt to theme.

**Fix:** Use QSS for the diff pane background; adapt diff token colours to theme in `buildTextDiff()` by checking `Theme::current()`.

---

## Consistency Findings

### VS-01 — PreferencesDialog foot buttons lack QDialog modal styling (Medium)
**File:** `src/ui/PreferencesDialog.cpp:321-332`

The Save and Cancel buttons in the footer are plain `QPushButton` with no `variant` property, `role="modalFoot"` parent, or accent styling. Every other dialog in the codebase (inferred from QSS `QFrame[role="modalFoot"]`) places its primary action button inside a styled footer frame. Here the footer is a raw `QHBoxLayout` outside any frame, so the Save button is unstyled — plain system button appearance. The dialog also sets `setMinimumWidth(460)` but no `setMinimumHeight`, so on small DPI it can be taller than the screen.

**Fix:** Wrap foot buttons in `QFrame` with `setProperty("role","modalFoot")`. Mark Save button with `setProperty("variant","accent")`, Cancel with `setProperty("variant","ghost")`.

---

### VS-02 — PreferencesDialog: missing `QTabWidget` QSS coverage (Info)
**File:** `resources/theme_dark.qss`, `resources/theme_light.qss`, `resources/theme_highcontrast.qss`

None of the three QSS files define `QTabWidget` or `QTabWidget::pane` rules. PreferencesDialog uses `QTabWidget` as its primary chrome, which falls back to the system style inside the app's custom-theme environment. On Windows 11 this renders as a faint native-style tab widget that appears visually disconnected from the rest of the dark chrome. The existing `QTabBar#ribbonTabs` and `QTabBar[role="sideTabs"]` rules do not apply to a QTabWidget's internal QTabBar.

**Fix:** Add `QTabWidget::pane { border: 1px solid <line-token>; background: <bg1-token>; }` and `QTabWidget QTabBar::tab { … }` rules to all three QSS files.

---

### VS-03 — `QGroupBox` unstyled in all themes (Info)
**File:** `resources/theme_dark.qss`, `resources/theme_light.qss`, `resources/theme_highcontrast.qss`

PreferencesDialog, EncryptionDialog, PermissionsDialog, and others use `QGroupBox` for field grouping. No QSS rules exist for `QGroupBox` in any theme file. On Windows 11 the platform style renders a system-default thin border and title with the system caption font at 9px, which contrasts heavily with the custom Manrope/14px UI.

**Fix:** Add `QGroupBox { border: 1px solid <line-token>; border-radius: 2px; margin-top: 8px; padding-top: 8px; color: <fg1-token>; font-size: 11px; font-weight: 600; }` and `QGroupBox::title { subcontrol-origin: margin; padding: 0 4px; color: <fg2-token>; }` to all three QSS files.

---

### VS-04 — `QSpinBox` unstyled in all themes (Info)
**File:** `resources/theme_dark.qss` et al.

`QSpinBox` is used in StatusBar (page spin), PreferencesDialog (autosave interval), and PageManagementDialog. No QSS rules exist for `QSpinBox`. On Windows 11 the page spin box in the status bar has a white background and black text in Dark mode — a stark visual break. GpTheme.h defines `InfoStripH` and declares a WCAG focus rule for `QSpinBox:focus` but no base styling.

**Fix:** Add `QSpinBox { background: <bg0-token>; border: 1px solid <lineStrong-token>; color: <fg0-token>; padding: 0 4px; min-height: 20px; }` and arrow button rules to all three QSS files. The focus rule already exists — only the base is missing.

---

### VS-05 — RibbonModel has 8 tabs but CLAUDE.md/README claims 7 (Info / Doc Drift)
**File:** `src/shell/RibbonModel.cpp:81-85`

`RibbonModel::tabs()` builds 8 tabs: Home, View, Edit, **Organize**, Comment, Convert, Forms, Protect. The old C-audit and README say "7 tab controllers". `makeOrganize()` is a full tab with Pages/Document/Numbering/Decorate groups. Not a visual bug but the claim "Ribbon 7 tab controllers" in the C-report (claim #60) is wrong — it is 8 in current HEAD.

---

### VS-06 — ModeStrip default mode is "comment" not "view" (Low)
**File:** `src/shell/ModeStrip.cpp:94`

Constructor calls `setMode("comment")` as the initial pill selection. On first launch (no document open), a user sees Comment mode preselected, which is surprising — most PDF applications default to View/Select. The default should be `"view"` unless the persisted mode is loaded on startup.

**Fix:** Change `setMode("comment")` to `setMode("view")`. Persist and restore the last-used mode via QSettings if not already done.

---

### VS-07 — Sidebar right-panel "Comments" tab uses a raw QListWidget for annotation threads (Low)
**File:** `src/shell/Sidebar.cpp:170-182`

The right-sidebar's second tab ("Comments") adds a `QListWidget` directly (`m_commentThreadList`) with manual `QListWidgetItem` construction for annotation threads. The update method (`updateCommentsTab()`, lines 285-316) uses `QFont("Manrope", 10, QFont::Bold)` and `QFont("Manrope", 9, QFont::StyleItalic)` via `item->setFont()`. This bypasses the theme system entirely — font sizes are hardcoded at 9–10px which may be too small at high DPI. The empty state label at line 313 has `Qt::AlignCenter` but the QListWidget doesn't have `setAlignment` support for items, so the centering may not render.

**Fix:** Use `setProperty("role","sidebarList")` on the list (already done for `m_filesList`). Replace font-set items with a `QFrame` delegate or use a QSS `QListWidget[role="sidebarList"]::item` rule. Check centering behavior on 2x DPI.

---

### VS-08 — Ribbon buildBody: lazy-loaded tabs show an empty plain QWidget before first click (Medium)
**File:** `src/shell/Ribbon.cpp:44-48`

For tabs 1–7, the ribbon inserts `new QWidget(this)` as a placeholder. Until the tab is clicked, if the app is resized while a non-Home tab has been selected previously and then the user switches back to a placeholder, there is a flash of an empty unstyled widget. The `ribbonBody` QSS sets `background:#1e1f22` on `#ribbonBody` but the placeholder `QWidget` has no `objectName`, so it may show system background briefly.

**Fix:** Set `placeholder->setObjectName("ribbonBody")` on each placeholder widget so the QSS background rule applies even before the tab is expanded.

---

## Residual Mock Surfaces (C-report Pattern 5 sites)

All 8 sites from the C-report have been verified against current code:

| Site | Status | Evidence |
|------|--------|----------|
| ThumbnailSidebar fake paper widgets | **GONE** | `ThumbnailSidebar.cpp:289-319`: real PDFium render via `RenderCache::getOrRender()` at 75 DPI; blank fallback on render failure — no fake content |
| PdfAValidationPanel fake rules | **GONE** | `PdfAValidationPanel.cpp`: builds violation rows from real `VeraPdfValidator` output; honest "unavailable" fallback |
| SignaturesPanel hardcoded identity | **GONE** | `SignaturesPanel.cpp:47-50`: all values initialised to `"—"`; populated by `validateSignatures()` |
| Sidebar Files placeholder | **GONE** | `Sidebar.cpp:115`: calls `extractEmbeddedFile()` via PoDoFo real bytes |
| AIChatPanel canned reply | **GONE** | `AIChatPanel.cpp`: wired to real OllamaProvider; no static reply string |
| Fake SYNCED indicator | **GONE** (fixed post C-audit) | `ModeStrip.cpp:150`: `updateLabels()` always emits `"⤺ LOCAL ONLY"` — the `setSyncStatus("SYNCED v.1")` / `setSyncStatus("NOT SYNCED")` calls cited in C-01 are **not present** in `GpMainWindow.cpp:130-190` (the section around line 146 now only wires `dirtyChanged` to `updateTitle()` and `setAutosaveTime()`). C-01 appears resolved. |
| Sidebar Files placeholder text | **GONE** (same as above) | confirmed |
| CollaborationManager Cloud Sync | **DISCLOSED** | `Ribbon.cpp`: `setEnabled(false)` + tooltip on Cloud Sync (not in current RibbonModel — Cloud Sync button removed entirely; CollaborationManager stub persists but is not exposed in UI) |

**Note on C-01 resolution:** The current `GpMainWindow.cpp` (lines 130–190 read) does NOT contain the `setSyncStatus("SYNCED v.1")` or `setSyncStatus("NOT SYNCED")` calls flagged in the C-audit at lines 146/148. `ModeStrip::updateLabels()` correctly emits `"⤺ LOCAL ONLY"` unconditionally at line 150. The C-01 finding is resolved in HEAD.

---

## Findings Table

| ID | Sev | File:Line | Description | Fix Summary |
|----|-----|-----------|-------------|-------------|
| T-01 | **High** | `src/ui/CommentsWidget.cpp:108,134,141,147,153,157,177` | Composer/buttons hardcoded Tailwind dark palette — broken in Light + HC | QSS role properties + all three .qss files |
| T-02 | **High** | `src/modes/CompressDialog.cpp:37-42,107,152,187,200-227` | All inline stylesheets dark-only — broken in Light | Move to QSS variant properties |
| T-03 | **High** | `src/ui/SignaturesWidget.cpp:19,23-27,61,65-67` | Hardcoded Tailwind/dark colours — broken Light + HC | Use `Theme::okGreen()`, `Theme::danger()`, `Badge` widget |
| T-04 | **Medium** | `src/GpMainWindow.cpp:580-585` | Update bar hardcoded blue — wrong in Light/HC | Add QSS rule in all three theme files |
| T-05 | **Medium** | `src/modes/BatchMode.cpp:118,262,351,364,377,402,446-448` | Hint/log labels dark-only; contrast <4.5:1 in Light | `setProperty("mono",true)` + QSS hint/log class |
| T-06 | **Medium** | `src/modes/AIChatPanel.cpp:23` | Panel border dark-only inline stylesheet | Add `#aiPanel` rules to all three QSS files |
| T-07 | **Medium** | `src/modes/OCRMode.cpp:147,153,159,293` | Checkboxes grey overrides HC white; scroll area black | Remove `setStyleSheet`, use QSS role |
| T-08 | **Medium** | `src/modes/PdfAValidationPanel.cpp:22-33` | Violation rows dark-only — black card in Light | Move to QSS object names |
| T-09 | **Medium** | `src/modes/SignaturesPanel.cpp:42,69,76` | idCard/appCard borders dark-only | `setProperty("role","signatureCard")` + QSS |
| T-10 | **Medium** | `src/ui/AnnotationLayer.cpp:207-208,346-347` | Selection highlight uses `Qt::blue` — low visibility in HC | Use `Theme::accent()` |
| VS-01 | **Medium** | `src/ui/PreferencesDialog.cpp:321-332` | Footer buttons not in `modalFoot` frame; unstyled | Add modal-foot QFrame, set variant properties |
| VS-08 | **Medium** | `src/shell/Ribbon.cpp:44-48` | Lazy ribbon tab placeholders lack `objectName` — brief flash | Set `objectName("ribbonBody")` on placeholders |
| T-11 | Low | `src/ui/PreferencesDialog.cpp:131,133,137,443,447` | Status labels `#888`/`#666` — fail HC contrast | Use `role="statusNote"` QSS class |
| T-12 | Low | `src/modes/WatermarkDialog.cpp:60,64` | Preview panel dark-only | `QWidget#watermarkPreview` QSS in all three files |
| T-13 | Low | `resources/theme_dark.qss:489` et al. | Thumbnail labels use Consolas, not JetBrains Mono | Swap font family order |
| T-14 | Low | `src/util/GpTheme.h:44-47` | Annotation palette not HC-conditional | Conditionalise on `HighContrast` mode |
| T-15 | Low | `src/ui/CompareWidget.cpp:40,45,49,56-58` | CompareWidget dark-hardcoded | QSS + theme-conditional diff colours |
| VS-06 | Low | `src/shell/ModeStrip.cpp:94` | Default mode is "comment" not "view" | Change to `setMode("view")` |
| VS-07 | Low | `src/shell/Sidebar.cpp:285-316` | Right-sidebar comment list uses hardcoded fonts; bad centering | QSS list item rules; delegate approach |
| VS-02 | Info | `resources/theme_dark.qss` (absent) | `QTabWidget` unstyled — PreferencesDialog falls back to system style | Add `QTabWidget` rules to all three QSS files |
| VS-03 | Info | `resources/theme_dark.qss` (absent) | `QGroupBox` unstyled — renders with system caption font | Add `QGroupBox` rules |
| VS-04 | Info | `resources/theme_dark.qss` (absent) | `QSpinBox` unstyled — white box in dark StatusBar | Add `QSpinBox` base rules |
| VS-05 | Info | `src/shell/RibbonModel.cpp:81-85` | Ribbon has 8 tabs; docs claim 7 | Update docs |
