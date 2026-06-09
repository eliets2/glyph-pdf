# GlyphPDF Round-2 Remediation — SUMMARY

Branch: `audit-remediation` (R2-5 branch: `r2-5-theme`)

| Finding | Status | Session | Evidence |
|---------|--------|---------|----------|
| **T-01** CommentsWidget hardcoded dark hex | **CLOSED** | R2-5 | `src/ui/CommentsWidget.cpp` — removed inline styleSheets with `#1E293B`, `#0F172A`, `#334155`, `#F8FAFC`; title uses `QPalette::PlaceholderText`; inputs inherit QSS; buttons use `gp::Theme::accent()` / `gp::Theme::okGreen()`; context menu clears inline style. |
| **T-02** CompressDialog hardcoded dark hex | **CLOSED** | R2-5 | `src/modes/CompressDialog.cpp` — preset cards, advFrame/mrcFrame/sizeRow, progress bars, detail/MRC labels, Go button all use `gp::Theme::bg0/bg3/line/lineStrong/fg0/fg2/accent()`. |
| **T-03** SignaturesWidget hardcoded dark hex | **CLOSED** | R2-5 | `src/ui/SignaturesWidget.cpp` — empty label `fg2()`; list border `bg3()`; selection bg `bg2()`; name label drops `color:white`; status uses `okGreen()`/`danger()`; reason uses `fg2()`. |
| **T-10** AnnotationLayer `Qt::blue` HC selection | **CLOSED** | R2-5 | `src/ui/AnnotationLayer.cpp:207` — selection ring uses `qApp->palette().color(QPalette::Highlight)`. EditImage overlay uses same. |
| **VS-02** No QSS for QTabWidget | **CLOSED** | R2-5 | `resources/theme_{dark,light,highcontrast}.qss` — added `QTabWidget::pane`, `QTabBar::tab`, `::tab:hover`, `::tab:selected` per theme. |
| **VS-03** No QSS for QGroupBox | **CLOSED** | R2-5 | Same three QSS files — added `QGroupBox` + `QGroupBox::title` rules per theme. |
| **VS-04** No QSS for QSpinBox | **CLOSED** | R2-5 | Same three QSS files — added `QSpinBox`, `:focus`, `::up-button`, `::down-button`, arrow rules per theme. |

## Build result

```
cmake --build build --parallel 8  →  14/14 targets, zero warnings (-Werror clean)
```

Pre-existing ctest failures (0xc0000135/0xc0000139 — DLL not found at test exe path) exist on
the branch baseline before R2-5; confirmed by stash/restore baseline comparison. No new failures
introduced by these changes.

## Commits

- `0009fa6` fix(R2-5): theme correctness — route hardcoded hex through GpTheme; add QSS for QTabWidget/QGroupBox/QSpinBox; fix AnnotationLayer HC selection
