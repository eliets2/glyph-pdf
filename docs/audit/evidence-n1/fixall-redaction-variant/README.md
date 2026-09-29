# Superseded N1 variant — feat/fixall-redaction @ 5461b72d (preserved 2026-09-29)

The three files in this directory are the evidence artifacts of the PARALLEL N1 implementation
committed on `feat/fixall-redaction` @ `5461b72d` ("fix(images): N1 — image edits address ONE
placement by (name, occurrence)", 2026-09-29 01:24, unpushed lane). They are absorbed here,
unmodified, so that no artifact of the superseded variant is lost when that branch is archived.

## Supersession disposition (the written content diff required by CONSOLIDATION-CRITIQUE E5/F3)

The landed N1 is `2ead0b17` ("fix(images): N1 — placements are addressed by occurrence index,
not name") on this branch. Line-level comparison of `5461b72d` vs this tree:

1. **File surface — the landed N1 is a strict superset.** `5461b72d` touches 13 files
   (+471/−105); `2ead0b17` touches 23 files (+704/−158) and includes EVERY file `5461b72d`
   touches (`src/core/ImageTypes.h`, `IPdfEditorEngine.h`, `PdfEditorEngine.{h,cpp}`,
   `ContentSpans.{h,cpp}`, `PoDoFoBackend.{h,cpp}`, `MockPdfEditorEngine.h`,
   `TestImageAppearance.cpp`) PLUS the layers `5461b72d` never reached: the four image commands
   (`MoveImageCommand.h`, `ResizeImageCommand.h`, `RotateImageCommand.h`,
   `ReplaceImageCommand.h`), `EditController.{h,cpp}`, `AnnotationLayer.{h,cpp}`,
   `TestCheckedMutationCoverage.cpp`. The lane variant threads occurrence through the engine
   only; the landed N1 threads it through commands, controller, and UI down to the user action.
2. **Test coverage — the landed suite covers every lane test and adds stronger ones.** The lane
   variant's six occurrence tests map 1:1 onto the landed seven
   (removeImagePlacementByOccurrence = same name; restackTargetsTheOccurrence →
   restackByOccurrence; wrapTargetsTheOccurrence + replaceMatrixTargetsTheOccurrence →
   wrapAndReplaceMatrixByOccurrence; listImagesNumbersTheOccurrences →
   listImagesReportsPlacementOccurrences; opacityTargetsTheOccurrence →
   opacityTargetsItsOccurrence), plus the landed suite adds `deleteTargetsItsOccurrence` and
   `editingTheSecondOccurrenceLeavesTheFirstByteIdentical` (isolation, which the lane variant
   lacks). The lane's own evidence recorded 41P/4F on its base; the landed variant is verified
   47P/0F (see CONSOLIDATION-HANDOFF-FIXALL-2026-09-25 §4).
3. **The 62 image-surface lines this tree lacked** (critique E5 measurement vs `2ead0b17`) are
   the lane variant's differently-named test bodies, its separate `findImageDo`-removal shape
   (the landed N1 keeps `findImageDoNth` as the single lookup with the same guarantee), and the
   evidence files preserved below. No functional hazard-fix from the lane variant is absent from
   the landed N1: both implement the same (name, occurrence) addressing with the same default-0
   compatibility; the landed one additionally separates opacity ExtGStates per occurrence and
   keeps `replaceImage` name-addressed by design (shared XObject pixels), exactly as the lane
   message specifies.

Verdict: **SUPERSEDED** — the lane variant's code is superseded by the landed, gates-verified
`2ead0b17`, and its evidence artifacts are preserved verbatim in this directory. Recorded by the
branch-consolidation executor 2026-09-29; source tip pinned by tag
`archive/final/feat/fixall-redaction` and by the consolidation bundle.
