# LANE REPORT — Compose Mode (§9.17 / SPEC-TRACEABILITY 9.18)

**Lane:** side-by-side visual composition · **Date:** 2026-10-04/05 · **Branch:** `feat/compose-mode`
**Worktree:** `D:/pdf/pdf-com` · **Base:** `93863d70` · **Commits:** `46cf53dd` (RED) → `4816b761` (GREEN) → this docs commit
**Contract:** PRD §9.17 bullets; SPEC-TRACEABILITY 9.18; brief deliverables 1–6 (mode shell, page transfer, image extraction + transfer, transactional save, a11y, R7 tests).

---

## 1. What shipped

### 1.1 Mode shell (`src/modes/ComposeMode.{h,cpp}`, registered end-to-end)
Two-pane workspace: left pane = SOURCE document, right pane = TARGET document, both live
documents opened by path — any two PDFs, or the same PDF twice. Each pane carries a page
thumbnail grid (checkbox picks, 36 dpi pdfium thumbnails) and a visual image picker listing
the selected page's embedded images. A transfer list previews every pending pick as it will
be applied; a composed-order label shows the live textual preview of the composed target
(`1 [S2] [S1] 3` — bracketed tokens are pending source picks). Picks made on either pane
queue a transfer INTO the other pane; picks from the TARGET pane transfer into the source,
each destination keeping its own compose history.

Registration follows the CompareMode idiom exactly, every layer:
- `ToolId::Compose` (`src/core/ToolId.h`, append-only after `PrepareSigningRequest`; canonical
  `"compose"` + aliases in `src/core/ToolId.cpp`),
- `TaskNav` table entry `{ "compose", "Compose", Workspace, ToolId::Compose, entryIsRoute=true,
  ribbonTab "Organize" }` + `screenForTool(Compose) == "compose"` (`src/shell/TaskNav.cpp`),
- `ModeController` lazy init + `composeStatusMessage` relay to the host status bar
  (`src/modes/ModeController.{h,cpp}`, the `redactStatusMessage` idiom),
- `HomeController` dispatch (`src/shell/controllers/HomeController.cpp`, the Compare pattern),
- Ribbon Organize ▸ Document entry (`src/shell/RibbonModel.cpp`),
- `commands.json` `"compose"` command (schema-1, sorted, `action "mode:compose"`),
- MenuBar Tools ▸ "Compose Documents…" (`src/shell/MenuBar.cpp`: actionSpecs Local entry,
  localHandlerIds, dispatch branch, menu item),
- ScreenNav button (`screenNav_compose`) is generated from the TaskNav table automatically.

### 1.2 Page transfer (order + position correctness)
Picked pages are extracted from the source with the §9.9 machinery
(`IPdfEditorEngine::extractPageAsBytes`) through an operation-owned engine, then inserted
into the destination at the chosen position with `insertPageFromBytes`. The position seam is
pure and pinned: `insertionIndexFor(afterPage0Based, k) == afterPage+1+k` ("after page N",
N = −1 = document start); the test drives the real UI checkbox path, inserts source pages
{2, 1} (pick order preserved) after target page 1 and proves the composed order by the text
each landed page carries (`T1, S2, S1, T2, T3`). Pick order — not page order — is the
applied order, disclosed in the transfer list.

### 1.3 Image extraction + transfer (geometry pin)
`src/engines/ImageExtractEngine.{h,cpp}` (the SPEC-TRACEABILITY-named file) walks the page's
`/Resources /XObject` dictionary for `/Subtype /Image` entries and reports, per object: the
resource name, `/Width` × `/Height`, the `/Filter` chain, and the DECODED pixels (PoDoFo
`PdfXObject::TryCreateFromObject` → `PdfImage::GetDecodedCopy(RGBA)`), guarded by the
codebase-wide 10 000 px/axis cap. `ComposeMode::imageInventory` surfaces it to the picker.

Placement reuses neither of the two existing image writers — the signature-Upload writer is
annotation-bound and `addImageWatermark` is page-range/fixed-position/no-alpha — so the brief's
fallback applies: a minimal `placeImageOnPage` engine op (interface default = honest `false`,
single implementer `PdfEditorEngine` → `PoDoFoBackend`), which packs RGB24 + an 8-bit
`/SMask` alpha plane (the §9.7 signature-appearance idiom, so signature rasters and any RGBA
pick keep their transparency), adds a clamped `ExtGState /ca /CA` for opacity, registers the
image under a UNIQUE resource name per placement (`CmpImg<n>`, no watermark-style collisions),
draws EXACTLY into the caller's rect (`q gs cm Do Q` appended through the shared
`appendPageContent` helper) and commits to `path` like the other path-addressed mutators.
Aspect preservation is the caller's contract: `ComposeMode::fittedRect` letterboxes the pick
inside the placement box and the geometry is pinned end-to-end — after a real Apply,
`listImages` on the committed target reports the placement at the fitted rect (±0.5 pt) with
the picked object's native pixel size.

### 1.4 Transactional save + full undo
Every apply is ONE `ComposeApplyCommand` (a `CheckedUndoCommand`) per destination session,
following the R01 transaction shape:
`SafeSave::makeUniqueCandidate` → destination bytes copied to the candidate → page inserts +
image placements by an operation-owned engine → independent re-open validation (fresh pdfium
renderer: exact page count + every mutated page really decodes) →
`SafeSave::commitFileToDestination` guarded by the E-6 `captureDestinationIdentity`
precondition. No partial writes are possible; the destination is byte-identical on any
refusal and the transfer stays pending (retryable). The refusal test injects
`SafeSave::CommitFaultForTesting::FailBeforeCommit` and pins byte-exactness by SHA-256, no
history step, then a successful retry.

Undo is the exact inverse under the same transaction shape: placed-image pages restored from
their pre-draw page backups (`restorePageFromBytes`, the ImageAppearanceCommand/G08 idiom —
backups captured inside the transaction after the inserts, before any draw), inserted pages
deleted in descending order, page count validated, identity-guarded commit. Traversal goes
through `CheckedHistory::undo/redo` (a failed restore never moves the history position).
Images land on the first destination page AFTER the whole inserted block — disclosed in the
preview — so an image pick never lands underneath pages inserted by the same apply.

**Handle discipline (found the hard way, see §3):** the operation engine's lazy-parse device,
the validation renderer and the pane's pdfium loader all keep OS handles; every one of them
is released before the atomic replace (engine reset, scoped validation renderer, pane
renderer parked + reloaded), mirroring the GUI-held-handle coordination SafeSave installs for
viewers.

**Source protection:** a transfer aimed at the target only ever READS the source (extraction
through an operation-owned engine); the test pins the source SHA-256 unchanged. A source is
written only by an explicit Apply of user-picked transfers aimed at it — the brief's "never
modified unless the user explicitly chooses" holds by construction.

### 1.5 Mismatched page sizes — honest scaling disclosure
`ComposeMode::pageSizeDisclosure` returns "" for sizes equal within 0.5 pt, otherwise names
BOTH sizes and states the contract: "the page keeps its own size, never stretched to match
the target" (inserted pages keep their own MediaBox — no silent stretching anywhere).
`pendingSummary()` folds the disclosure into the transfer list BEFORE anything is written;
the test drives a real Letter → A4 transfer and pins the disclosure row. Image placements
carry the parallel `imagePlacementDisclosure` ("aspect ratio preserved, never stretched").

### 1.6 A11y (the r4-ux accessibleName discipline)
Every thumbnail/canvas surface names itself with document role + file + page numbers:
pane grids carry `accessibleName` ("SOURCE document pages — a.pdf (3 pages)") and a
description of the pick affordance; the image picker names role + page + file; the transfer
list announces "Pending transfers"; and every grid/picker ITEM carries `AccessibleTextRole`
("Source document a.pdf, page 2 of 3, not picked") kept in step with the checkbox
("picked" / "not picked"). The test drives the real checkbox path and pins role, page
position, and pick-state wording on both panes.

## 2. R7 evidence (fail-before RED → NC once → pass-after ×3 SERIAL)

All runs offscreen (`QT_QPA_PLATFORM=offscreen`), Release, LTO, `build-rel`, Windows;
serial per the brief; logs in `docs/audit/evidence-compose/`.

| Stage | Log | Result |
|---|---|---|
| RED (fail-before, commit `46cf53dd`) | `red-final.log` | **4 passed / 7 failed, RC=8** — every failure names a missing §9.17 behavior: insertion seam unset; apply refuses ("Composition is not available yet — nothing was changed") for page transfer, image placement, transactional save and undo; empty size disclosure; missing thumbnail AccessibleTextRole. Passing already: registration end-to-end (ToolId/TaskNav/Ribbon/commands.json/ScreenNav/HomeController/ModeController) and the two-document open. |
| NC (intermediate, not committed) | `intermediate-refusal-access-denied.log` | 8 passed / 4 failed — first working apply refused by Windows file locking: `commit to destination failed: Access is denied.` The operation engine's lazy-parse device + validation renderer + pane pdfium loader held the candidate/destination. Fixed by the handle-release discipline of §1.4 (engine reset, scoped validation renderer, pane renderer parking) and surfaced the command's `lastError()` through the mode's refusal wording. One test-arithmetic bug fixed in the same pass (undo test expected 4 pages after inserting one). No flakes; re-run-on-flake rule never triggered. |
| GREEN ×1 | `green-pass1.log` | **12/12 passed, RC=0** |
| GREEN ×2 | `green-pass2.log` | **12/12 passed, RC=0** |
| GREEN ×3 | `green-pass3.log` | **12/12 passed, RC=0** |

Build: configure RC=0 (runbook flags verbatim); full-tree build **BUILD_RC=0**
(`build-green2/3/4.log` stages; incremental retries after two externally killed/failed
intermediate builds are part of the recorded sequence). One pre-RED build failure
(`ImageExtractEngine.cpp`: PoDoFo `GetName().GetString()` returns `std::string_view`, not
`std::string`) fixed before any test run.

## 3. Neighbor suite (no existing test weakened)

`docs/audit/evidence-compose/neighbor-registration-suite.log` — 10/10, RC=0, serial:
TestControllers, TestRibbonIntegrity, TestMenuBarIntegrity, TestCommandBinding,
TestComposeMode, TestTaskNavRegistry, TestScreenStateSync, TestModeStripPins,
TestViewParity, TestCommandRegistry.

Two pinned fixtures were extended ADDITIVELY for the new registered screen (same assertion
strength, both updated in the same commit as the registration they mirror):
- `TestTaskNavRegistry::kWorkspaceScreens` — the set mirrors the ModeController ctor's
  registered screens verbatim; `compose` joins it (exact-equality check unchanged).
- `TestViewParity::everyTaskScreenIsReachable` — the count pin 13 → 14; the test still drives
  EVERY task screen (compose included) through the real MainWindow with unchanged
  reachability/snap-back assertions.

No plannedTools entry was added (compose is wired, never hidden); `TestRibbonIntegrity`
stays green because the ribbon entry resolves to a handled ToolId.

## 4. Deliberate scope decisions (honest disclosures)

- **Canvas marquee picking** is delivered at the inventory level the PRD names: the image
  picker is a visual, checkable surface over the page's rendered image inventory (decoded
  pixels, dimensions, filters). A free-form marquee over the rendered canvas would need a
  canvas overlay system of its own; the picker covers the same capability (choose an embedded
  image, place it at a chosen page + position) without it.
- **Live preview** of the composed result is textual + structural before Apply
  (`composedOrderPreview`, `pendingSummary` with disclosures); the first PIXEL preview of the
  composed result is the committed page itself. No pixel-accurate pre-save composite render.
- **Thumbnail rendering is synchronous** at 36 dpi for the first 200 pages (the PagesMode
  grid is off-thread; compose kept the synchronous path for determinism and bounded scope —
  large-document smoothness is a follow-up).
- **Image placement opacity** defaults to 1.0 (transparency still honored via /SMask); the
  placement-box UI is fixed to the centered half-page auto box or an explicit rect — there is
  no drag-to-position gesture yet.
- **Commands.json `action` strings are presentation metadata** (loaded by CommandRegistry,
  never dispatched); the real route is the ToolId path, which is fully wired.

## 5. Security / hard-rule compliance

- Work confined to `D:/pdf/pdf-com` (worktree created at base `93863d70` per brief); no other
  worktree touched; runtime DLLs copied OUT of the main checkout's `third_party/` into this
  worktree's (read-only source).
- No push, no merge/rebase to main, no stash/gc, no worktree/branch deletion.
- No CLAUDE.md/SECURITY.md created; no existing test weakened (§3).
- SafeSave invariants held end-to-end: candidate-only writes, independent validation,
  identity-guarded atomic commit, no direct-write fallback; E-6 destination precondition
  captured per transaction.

## 6. Handoff notes

- `placeImageOnPage` has an interface default (`false`) — only `PdfEditorEngine` implements
  the engine surface today, so future backends must opt in explicitly.
- ComposeMode keeps per-destination `QUndoStack`s inside the mode (compose history is a
  property of the compose session, not of the main document session); if the mode later needs
  to fold compose steps into the main window's undo stack, the command class moves unchanged.
- The transfer list caps at ~110 px height in the toolbar flow; with very large pick sets it
  scrolls. The composed-order preview string is O(target pages).
- `ImageExtractEngine` decodes pixels eagerly per picker refresh (bounded by the 10 000 px
  cap); a page with many large images pays one decode per image per selection change — the
  cache point, if ever needed, is `fillImagePicker`.
