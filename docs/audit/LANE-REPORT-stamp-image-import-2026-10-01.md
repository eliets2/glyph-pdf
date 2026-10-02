# LANE REPORT — Stamp tool: image-as-stamp import (wave 2b, lane #18)

Date: 2026-10-01
Branch: `feat/stamp-image-import` (worktree `D:/pdf/pdf-w2b-stampimport`, base `a4cc1522`)
Scorecard: PARITY-SCORECARD-2026-09-30 §4 row 18 (July §3 row 16, July P1 — CLOSED by this lane)
Status: **COMPLETE** — all work committed, R7 evidence contract satisfied.

## Mission

Let users import an image file as a reusable stamp: pick image → name it → it
lands in `StampLibrary` and places correctly. Reuse the existing `/Stamp` +
image-appearance writer from the signature Upload path. Honest failure
handling for unusable images (typed message, no silent accept). Keep the
existing stamp catalog semantics.

## What was found at base (check-first)

Confirmed NOT done: `StampTemplate` (src/core/StampLibrary.h) had no image
slot — only id/name/textTemplate/color; `loadCustomFrom` silently DROPPED any
entry without a non-empty text template; `StampLibraryDialog` had no import
entry point; grep for image-stamp/import-image surfaces found nothing.
Pins commit `eeb6f6c8` proves the gap behaviorally (RED at base).

The REUSE TARGET was verified present at base, end to end:
- Writer: `PoDoFoBackend.cpp` applyAnnotationsToDoc — `AddSignatureTyped`/
  `AddSignatureUpload` → `/Stamp` subtype + image XObject with 8-bit /SMask
  drawn by a form appearance stream (letterboxed, same math as the layer's
  `fitInsideRect`), `/GlyphSigMode` marker; failure degrades to a gray-box AP.
- Reader: `extractAnnotations` maps `/GlyphSigMode` back to the tool and
  restores the raster from the AP's image XObject.
- Placement: `AnnotationLayer` `AddSignatureUpload` + `setPendingSignatureImage`
  → committed item carries the image; plain click gets an aspect-correct
  default box.

## File-by-file changes

1. `src/core/StampLibrary.h`
   - `StampTemplate` gains `QString imagePath` — the image-variant slot.
     An entry carries EXACTLY ONE placement carrier (text template XOR image
     path). Stored as a path RELATIVE to the stamps.json directory
     (`stamp-images/<uuid>.png`) so the profile stays self-contained.
   - New statics: `defaultCustomPath()`, `imageStampsDirFor()`,
     `imageAbsolutePath()` (safe-relative gate: refuses absolute and
     parent-traversing paths — the second gate for in-memory callers),
     `addImageStampTo()` (ONE committed import step), `loadStampImage()`
     (decode for placement/preview, EXIF auto-transform; null = missing or
     corrupt — callers must surface that, never place a blank).
2. `src/core/StampLibrary.cpp`
   - `loadCustomFrom`: reads the optional `"image"` key; validation —
     non-empty id+name; unsafe image path refuses the entry regardless of the
     text carrier; exactly-one-carrier rule (both or neither → skip), same
     skip discipline as the existing built-in-shadowing defense.
   - `saveCustomTo`: writes `"template"`/`"image"` only when non-empty
     (image stamps stay image stamps on re-save; no empty-key noise).
   - `addImageStampTo`: typed refusals (empty name; missing file; source must
     FULLY decode via QImageReader — a file that merely sniffs as an image is
     refused with its reason); normalized PNG copy into `stamp-images/`;
     append + persist; a failed save removes the copied file (no orphans, no
     half import).
3. `src/ui/StampLibraryDialog.h/.cpp`
   - "Import Image…" button (`stampImportButton`): pick image → name it
     (pre-filled with the file base name) → lands in the library, selected.
     Cancelled picker/name is not an error; an unusable image shows its typed
     reason (`QMessageBox`). Programmatic seam `addCustomImageStamp()`
     (no modals) + `selectedStampImage()`.
   - List shows image stamps as `name — (image stamp)`; preview shows the
     scaled picture, or an honest "image file is missing or unreadable" line.
   - `deleteSelectedCustom` removes the catalog's OWN copy — only files inside
     the managed `stamp-images` folder are ever deleted.
4. `src/shell/controllers/EditController.h/.cpp`
   - Pure seam `stampArmModeForTemplate` (image → `AddSignatureUpload`, text →
     `Stamp`); `armDynamicStamp` branches: image stamps load the raster and
     arm the EXISTING signature-Upload placement (mode FIRST, image second —
     `setMode` discards a pending image for non-signature tools), so the
     placed annotation rides the §9.7 P0 `/Stamp` + image-appearance writer
     and the `GlyphSigMode` roundtrip unchanged — reused, not duplicated.
     Missing/unreadable image at arm time: typed status message ("re-import
     it"), no placement. The dialog's `placeRequested` already routed through
     `armDynamicStamp`, so image stamps need no new wiring.
5. `tests/TestStampImageImport.cpp` (+ CMakeLists.txt block mirroring
   TestDynamicStamps)
   - Pins: dialog import entry point; image-entry catalog round trip (load
     AND save→load, no text-carrier growth); tamper refusals (absolute,
     traversal, both-carriers); built-in catalog semantics unchanged; typed
     import refusals (garbage bytes, truncated PNG, missing file, empty name)
     with zero file/catalog residue; happy path (copy inside stamp-images,
     decodes at 40×20, dialog seam places via `placeRequested`, delete removes
     entry + copy); arm seam; REUSE PROOF (image item through
     AnnotationLayer → embed → extract: mode AND raster restored).
6. Fix iteration (honest): the tamper pin caught a real bug in my first
   validation (`hasText == safeImage` kept a text entry carrying an UNSAFE
   image path) — corrected to `hasImage && !safeImage → refuse` +
   `hasText == hasImage → skip`. The delete pin's assertion was fixed to
   capture the id BEFORE the delete (delete reloads and moves the selection).

## R7 evidence (docs/audit/evidence-stamp-image-import/)

| Gate | Result | Evidence |
|---|---|---|
| 1. Fail-before (RED at base `a4cc1522`) | 4 passed, 3 FAILED — no import button; image catalog entry silently dropped; tampered image path loaded as a text stamp. Guards green by design: built-in semantics + REUSE PROOF (writer already works). | `red-before-pins-run1.log` |
| 2. Negative control (recorded ONCE) | Scoped revert: the OLD validation rule restored inside the new signature ⇒ exactly the 3 catalog-dependent pins RED (entry dropped, tamper kept, import doesn't land); all other pins green — honest scoping. | `negative-control-scoped-revert.log` |
| 3. Pass-after ×3 consecutive SERIAL | 3/3 green (1.10 s / 1.04 s / 1.08 s), plus a 4th green after the NC restore (0.27 s). | `green-after-fix-run1.log`, `run2`, `run3`, `run4-post-negative-control.log` |
| Ripple sanity | TestControllers (16.9 s) + TestDynamicStamps — 2/2 passed. | `ripple-sanity.log` |
| Full build | BUILD_RC=0 (first full impl build and first post-fix build each hit the transient LTO-under-contention class on UNRELATED targets — 8 co-tenant builds on the box, "serial compilation of 90 LTRANS jobs"; incremental retries went BUILD_RC=0, 0 failed targets; NC and post-NC builds BUILD_RC=0). | `build-evidence.log` |

All ctest runs SERIAL (no -j), `QT_QPA_PLATFORM=offscreen`, ucrt64-first PATH.
One ctest invocation aborted early by a stale NC-reverted binary mid-cycle is
NOT in the evidence set (superseded before recording); every recorded log is a
complete run.

## Commits

- `eeb6f6c8` test(stamps): row-18 image-as-stamp pins (RED at base) + RED evidence log.
- `fb045010` feat(stamps): row-18 image-as-stamp import — images land in the
  catalog and place through the existing /Stamp image-appearance writer.

No merges, no pushes, no branch switches, no stash/gc. Working tree clean at
report time (build logs live under build-rel/, untracked by design).

## Known limits (honest)

- The placed image annotation persists as the §9.7 signature-upload shape
  (`/Stamp` + `/GlyphSigMode Upload`) — indistinguishable from an uploaded
  signature once placed, and the stamp's catalog name is not written into the
  annotation. That is the row's named reuse target; a distinct marker/name
  carrier would touch the frozen PdfEnums ordinals and the writer (owner
  follow-up if wanted).
- Image stamps are stored as re-encoded PNG: animated GIF/WebP land as their
  first frame; CMYK/16-bit sources are flattened by the decode. Placed
  geometry letterboxes exactly like the signature path.
- Import accepts whatever Qt's image plugins decode (PNG/JPEG/BMP/GIF/WebP/
  TIFF on this build). No pixel-count cap, matching the existing signature
  Upload path; the fully-decode-first rule is the honesty gate.
- User-facing strings are `tr()`-wrapped English; .ts files untouched (out of
  lane scope).

## Owner items

1. Same as the fdf lane noted: `third_party/podofo/install/bin/libpodofo.dll`
   and `third_party/pdfium/bin/pdfium.dll` WERE present in this worktree at
   base (runbook accurate here); recording only that machine contention
   produced recurring transient LTO link failures across lanes — integrator
   may want a rebuild pass on a quiet box before tagging.
