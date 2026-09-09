# Feature-command matrix — companion notes (9 September 2026)

Companion to `docs/audit/FEATURE-COMMAND-MATRIX-2026-09-09.csv` (300 rows, 21 columns).
Author: code-archaeologist lane, Wave 0. Base: worktree `C:\Users\User\Projects\pdf-parity`,
branch `feat/parity-glm`, HEAD `de77d46` (3 measure fixes G21–G23 on top of `48087ad`, which the
UI review used as its read-only tip). Matrix rows marked `traced-2026-09-09` are this session's
source-level tracing, never `verified`. No production files were touched; the only written
artifacts are the two files under `docs/audit/` and scratch under `.context/matrix-wip/`.

## Scope of the inventory

| Surface | Rows | Notes |
|---|---:|---|
| ribbon | 144 | extended from the reviewer's `ribbon-command-baseline.csv` (92 rendered + 52 hidden by `Ribbon.cpp:216–236`) |
| welcome | 8 | 6 cards + recent-file click + remove-from-recent |
| screen-nav | 12 | bottom TaskNav strip, one row per TaskNav table entry |
| mode-strip | 7 | 5 mode pills + AI toggle + Tools chooser |
| side-pane | 7 | left Pages/Bookmarks/Comments/Files + right Properties/Comments/Layers |
| menu-bar | 94 | every `MenuBar::actionSpecs()` entry + dynamic submenus (recent, stamps) |
| shortcut | 28 | registered QShortcut / QAction QKeySequence bindings |

## Disposition counts (the five classes)

Ribbon hidden IDs (52):

| Status | Home | View | Edit | Organize | Comment | Convert | Forms | Protect | Total |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| implemented+missing-context (wire target exists) | 0 | 4 | 3 | 0 | 3 | 2 | 2 | 0 | **16** |
| obsolete-alias-duplicate | 2 | 0 | 1 | 0 | 0 | 2 | 1 | 0 | **8** |
| planned-unimplemented | 2 | 2 | 12 | 3 | 4 | 6 | 2 | 1 | **28** |

Per-tab hidden counts match the review's UI01 table exactly (4/6/16/3/7/10/5/1).

Whole-matrix status distribution (300 rows): implemented+available 226,
implemented+missing-context 27 (16 hidden + 3 visible: `delete`, `applyRedact`, `validateSig`),
implemented+dependency-unavailable 2 (welcome Import Office; ribbon `fromFile`),
obsolete-alias-duplicate 8, planned-unimplemented 37 (28 hidden ribbon + 9 menu items that are
Disabled "Planned" with no traced implementation: new-window, paste, select-all, rulers, guides,
grid, tile, updates + Active-Document label).

## A. Hidden IDs WITH a traced implementation — the wire-up target list (16)

These are the entries an implementation wave can wire to existing code; none needs a new engine
subsystem (the caveat per row is in the matrix):

| # | Hidden ID (tab/group) | Wire target (evidence) |
|---|---|---|
| 1 | `thumbs` (View/Panes) | Left Sidebar "Pages" tab = ThumbnailSidebar; drag-reorder already wired to PagesController (`Sidebar.cpp:85–94`, `GpMainWindow.cpp:500–502`). Need: a public Sidebar tab-raise API. |
| 2 | `bookmarks` (View/Panes) | Left Sidebar "Bookmarks" tab = BookmarkPanel (`Sidebar.cpp:96–99`). |
| 3 | `comments` (View/Panes) | Left Sidebar "Comments" tab = CommentsWidget with filters/reply/status (`Sidebar.cpp:101–110`); menu mirror `toggle-comments` is currently Disabled (`MenuBar.cpp:106`). |
| 4 | `layers` (View/Panes) | Right Sidebar "Layers" tab, populated via `IPdfDocumentIO::getLayers` (`Sidebar.cpp:196–210`). CAVEAT: unchecking only posts a status message — no engine set-layer-visibility op exists yet. |
| 5 | `ocrSettings` (Edit/OCR) | PreferencesDialog OCR Engine group with honest ONNX availability (`PreferencesDialog.cpp:197–260`); Ctrl+, already registered. |
| 6 | `measure` (Edit/Measure) | MeasureMode right-dock panel header exists but is UNTRACKED WIP of the measure lane (`src/modes/MeasureMode.h`, no .cpp yet); ToolModes MeasureDistance/Perimeter/Area committed (`PdfEnums.h:50–53`); AnnotationLayer T1 calibration/snap/readout (measurement-lane file, dirty). Menu mirrors Disabled (`MenuBar.cpp:88–89`). |
| 7 | `distance` (Edit/Measure) | As `measure`; G21 fixed measurement /Rect persistence (commit `de77d46`). |
| 8 | `area` (Edit/Measure) | As `measure`; G22 /PolyLine closing edge (`83706cf`), G23 calibration parser (`69f8a94`). |
| 9 | `filterComm` (Comment/Review) | CommentsWidget status/author/date filters (`CommentsWidget.h:38–46,79–99`). |
| 10 | `statusComm` (Comment/Review) | ReviewState model (`AnnotationTypes.h:10–19`) + status filter; edits route through EditAnnotationCommand on the shared undo stack. |
| 11 | `reply` (Comment/Review) | `CommentsWidget::replyToComment` + composer, wired by `setContext/setViewer` (`Sidebar.cpp:103–106`). |
| 12 | `batchConv` (Convert/Batch) | Full BatchMode workspace (7 operations, per-item preflight, async merge worker — `BatchMode.cpp`, 1724 lines), reachable only via bottom ScreenNav "Batch". |
| 13 | `watch` (Convert/Batch) | BatchMode hot-folder option (`BatchMode.cpp:745–752`). |
| 14 | `required` (Forms/Validate) | FormFieldPropertiesPanel required checkbox + `IFormManager::setFieldMetadata(required)` (`FormFieldPropertiesPanel.h:50`, `FormManager.cpp:957`). |
| 15 | `flatten` (Forms/Data) | `IFormManager::flattenForm` implemented at `FormManager.cpp:1137` with ZERO UI callers — pure wire-up. |
| 16 | `poly` (Comment/Drawing) | Partial: /PolyLine geometry + serialization real via MeasurePerimeter (G22); no standalone polyline ToolMode yet; icon exists. |

## B. Hidden IDs with NO implementation — the honest Planned list (28)

`snapshot, history, splitWin, newWin, deleteText, link, attach, alignL, alignC, alignR, distribute,
group, layerOrder, replace, reverse, background, customStamp, summary, trackChanges, toMD, toEPUB,
fromScan, fromWeb, extractTables, detectTables, rules, reset, trust`

Method note: for each, the trace covered `ToolId.cpp/.h` (string map), all seven controllers,
`PdfEnums.h` ToolModes, engine interfaces (`IPdfEditorEngine.h` role interfaces, `IConversionEngine.h`,
`IFormManager.h`, `ISignatureManager.h`), modes/ and ui/ panels, and grep across `src/`. None of the
52 hidden string IDs is in the canonical ToolId map — where a backend existed it was found as a
panel/dialog/pref, not as a command. Near-neighbors worth exploiting when these get implemented are
recorded per-row in the matrix (e.g. `reverse` is one `reorderAllPages` permutation away;
page `replace` can compose `insertPageFromBytes` + `deletePage`).

## C. Duplicates / aliases found (canonicalization map)

| Surface id | Canonical stable_command_id | Evidence |
|---|---|---|
| ribbon `findRep`, menu `find-replace`, `find` | `search` | FindBar is the one implementation; Ctrl+H (`MenuBar.cpp:292`, `FindBar.h:32–59`) |
| ribbon `regex` | `search` | FindBar regex checkbox (`FindBar.h:59`) |
| ribbon `insertText` | `addText` | both mean ToolMode::AddTextBox (`EditController.cpp:112–113`) |
| ribbon `ocrVerify` | `ocr` | U02 entry route: the OCR button opens the OCR Verify screen; F8/Shift+F8 review lives there |
| ribbon `ocrLang` | `ocr` | OCRMode language picker persists `ocr/language` (`OCRMode.cpp:116–126`) |
| ribbon `reduce` | `compress` | no separate implementation; CompressDialog is the size-reduction path (execution prompt: must not be duplicates pretending separate capabilities) |
| ribbon `preset` | `exportPresets` | ExportPresetsPanel + `HomeController::onExportPresets` |
| ribbon `calc` | `calcField` | FormsController handles ToolId::CalcField; menu Forms▸Add Calculated Field — the ribbon has no visible calculated-field button, so `calc` is its natural home |
| ribbon `pageNums`/menu `page-numbers` | `addPageNumbers` | `ToolId.cpp:227` |
| ribbon `header`/menu `headers-footers` | `addHeader` | `ToolId.cpp:225` |
| ribbon `bates` | `batesNumber` | `ToolId.cpp:228` |
| ribbon `strike`/menu `strike` | `strikeout` | `ToolId.cpp:191` |
| ribbon `eraser` | `erase` | `ToolId.cpp:197` |
| ribbon `delete`/menu `delete` | `deleteSelection` | `ToolId.cpp:212` |
| ribbon `fromFile` | `importOffice` | `ToolId.cpp:242` |
| ribbon `combinePDF`, ribbon `merge`, welcome "Merge files" | `combine` | `ToolId.cpp:231` |
| ribbon `compareDocs`, ribbon View `compare` | `compare` | `ToolId.cpp:163` |
| ribbon `exportCSV` | `toCSV` | `ToolId.cpp:234` (RibbonModel.cpp:33 comment says the same) |
| ribbon `export`/`import` | `exportData`/`importData` | `ToolId.cpp:259–260` |
| menu `extract-page`, `two-page`, `date-field`, `num-field`, `calc-field`, `signature-field`, `expiry-date`, `autodetect`, `tab-order`, `import-data`, `export-data`, `save-copy` | respective ToolIds | alias table in `ToolId.cpp:139–286` |

Implemented commands with NO ribbon surface (found during tracing; matrix rows on their real
surfaces): `linearize` (Export Presets panel only), `ExportAnno`/`ImportAnno` (SecurityController
handles them; no menu/ribbon/shortcut surface traced), `SelectObject`/`EditObject` (controller
cases; no visible surface), `Freehand`/`comment` (aliases of pencil/note).

## D. Icon gaps — the 5 "missing" SVG names map to EXISTING assets

The five unresolvable names in `RibbonModel.cpp` are camelCase spellings of icons that ARE
registered in `resources.qrc` under kebab-case names. `Icons::svg()` does an exact
`:resources/icons/<name>.svg` lookup (`Icons.cpp:16–22`), fails, and the factory paints the
circle fallback (`Icons.cpp:26–33`). **No new SVGs are needed — rename the five references:**

| RibbonModel name (broken) | Existing registered asset | Command(s) affected |
|---|---|---|
| `zoomIn` | `zoom-in.svg` | View▸Zoom In |
| `zoomOut` | `zoom-out.svg` | View▸Zoom Out |
| `editText` | `edit-text.svg` | Edit▸Edit Text |
| `insertPage` | `insert-page.svg` | Organize▸Insert |
| `deletePage` | `delete-page.svg` | Organize▸Delete |

Generic-icon debt: exactly **62 of 144** ribbon entries reference the `form` icon (verified by
count against the baseline CSV; 32 of the 92 rendered ones) — matching review UI05. All 164 qrc
entries and all RibbonModel references were cross-checked; the five names above are the only
unresolvable ones.

## E. Known problem entries — explicit notes

- **Edit's 16 hidden** (incl. Arrange + Measure): Arrange's 6 are all Planned (no alignment model);
  Measure's 3 are wire-ups to the measure lane's backend (see A6–A8); Text's `insertText` is a
  duplicate of `addText`, `deleteText` is Planned (nearest: Erase `deleteObjectAt`); Objects'
  `link`/`attach` have READ paths implemented (engine `extractLinks`, Sidebar Files tab) but no
  CREATE path.
- **View Panes group** (whole group hidden today): all four panes EXIST as passive Sidebar tabs;
  the missing piece is a tab-raise API + commands (and a real layer-toggle engine op).
- **Comment Review group** (whole group hidden): `filterComm`/`statusComm`/`reply` already exist in
  the Comments pane; `summary`/`trackChanges` are Planned.
- **Convert Batch group** (whole group hidden): BatchMode + hot folder are fully implemented and
  reachable only from the bottom ScreenNav — the hidden ribbon group is a routing duplicate, not a
  missing feature. `reduce` must consolidate into `compress`, `preset` into `exportPresets`.
- **Forms Validate group** (whole group hidden): `required` is a wire-up (properties panel +
  `setFieldMetadata`), `calc` duplicates the implemented-but-unlisted-on-ribbon CalcField, `rules`
  is Planned; Data's `flatten` has an implemented engine op with zero callers, `reset` is Planned.
- **Welcome Convert/Protect → Open-only routing** confirmed at `GpMainWindow.cpp:215–218`
  (review UI03). Merge correctly routes through `ToolId::Combine` (`:223–224`); Office/Images cards
  route to their own imports (`:209–212`).
- **Menu Stamps submenu**: Approved/Draft/Confidential are UNCONNECTED QActions — silent no-ops
  that bypass both `actionSpecs()` and TestMenuBarIntegrity (`MenuBar.cpp:363–367`). Should join
  the matrix as Disabled-Planned or be wired to a stamp command.
- **Enablement vs canonical state (UI02)**: visible ribbon buttons are standalone QToolButtons;
  the registry QAction path (`ToolRegistry::actionFor`, `refreshEnabledActions`) is the only
  read-only-aware enablement. The matrix records, per command, the real prerequisite; the 3 visible
  CTX rows (`delete`, `applyRedact`, `validateSig`) are the ones where context genuinely gates
  usefulness today.
- **View-vs-persisted semantics** recorded in `output_or_state_change`: zoom/layout/panes are view
  state; Organize `rotate` persists page /Rotate via RotatePageCommand (not a view rotation);
  header-footer/Bates/watermark/redaction/flatten/importData are direct engine writes with
  explicit NOT-undoable contracts; page-label vs printed-number distinction noted on
  `pageNums` (prints numbers; PDF page labels untouched).
- **Concurrent-lane caveat**: `src/modes/MeasureMode.h` is untracked and `src/ui/AnnotationLayer.*`
  are dirty — both belong to the active measure lane and were used read-only as evidence. The
  measure wire-up rows will need rebaseline confirmation when that lane lands.

## F. Verification statement

- Row/tab/hidden-count checks, disposition counts, and the alias/shared-identity check (60 stable
  ids shared across ribbon+menu rows) were run against the generated CSV with Python (outputs in
  the session log). 144 ribbon rows, 52 hidden, per-tab 4/6/16/3/7/10/5/1 — machine-checked.
- Icon cross-check (`RibbonModel` names vs `resources.qrc` vs `resources/icons/*.svg`) was run:
  164 svgs, 164 qrc entries, 5 unresolvable names — machine-checked.
- Everything else is source tracing at `de77d46` with file:line citations in the matrix;
  per execution-prompt protocol none of it is `verified`, and behaviors were not executed.
