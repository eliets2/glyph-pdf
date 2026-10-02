# LANE REPORT — #15 wave 2b, Exportable accessibility results panel (2026-10-01)

Lane: `feat/a11y-export` @ worktree `D:/pdf/pdf-w2b-a11yexport`, base
`a4cc1522`. Mission: PARITY-SCORECARD-2026-09-30 §4 row 15 — "Exportable
accessibility results panel … CSV/PDF summary; `ReviewSummaryWriter` is the
in-repo pattern. Effort S-M. Dispatch target: `modes/AccessibilityPanel.cpp`."

## CHECK FIRST

No results export existed at the base: `AccessibilityPanel.cpp` (705 lines at
`a4cc1522`) contains no CSV/PDF/save affordance of any kind (grep for
export/csv/summary hits only the `a11yTagSummary` label). The lane is a real
implementation lane. No prior work duplicated.

## What was built

The accessibility checker's `A11yReport` is now exportable as a compliance
artifact in two forms, both carrying the checker's honest verdict — including
its capability degradations — never a greenwashed summary:

**1. `src/engines/A11yReportWriter.h/.cpp` (new)** — pure-static writer
modeled on `ReviewSummaryWriter` (same shape, same SafeSave candidate
transaction, same standard-14 discipline):

- `writeCsv(outPath, report, err)` / `csvPayload(report)` (testable seam) —
  machine-readable `glyphpdf-a11y-report/1`: a key/value report block
  (Format, Generated UTC ISO, Document, **Scan status** ok/**failed** +
  Load error, Tagged, Finding count, High/Medium/Low severity counts,
  Images/Fields reported "X of Y", **Truncated** flag, Verdict, Disclosure),
  a blank separator, then the findings table
  `Severity,Check,Page,Target,Where,Detail` in the checker's own order with
  1-based pages (the CommentsWidget CSV rule). UTF-8, CRLF (RFC-4180).
- `csvCell(raw)` — public single-contract escaping:
  `ConversionManager::csvFormulaSafeCell` (the PGR-16/M3 formula-injection
  hardening, shared with the conversion and comments exporters so the three
  cannot drift) + RFC-4180 conditional quoting with doubled inner quotes.
- `writePdf(outPath, docTitle, report, err)` — print-ready A4 summary via
  PoDoFo standard-14 Helvetica, delivered through the **SafeSave candidate
  transaction** (unique candidate → validate → atomic commit; a failed
  commit leaves an existing destination byte-identical), with per-sheet
  footers and the W1-04 substitution honesty note.
- `verdictLine(report)` / `disclosureLine()` / `severityLabel()` — the
  wording contract, testable without rendering:
  - failed scan → "Scan failed: <loadError> — no findings are available
    (nothing was checked)" — the artifact exports the failure, never a
    fabricated clean report;
  - zero findings → the panel's exact wording "No gaps found by these
    checks. This is not a PDF/UA verdict — content tagging is not checked."
  - findings → "%1 gap(s) found" + " — report truncated" when the bounded
    samples truncated;
  - standing disclosure carried into every artifact: detection only, never
    certifies PDF/UA, bounded samples, truncation disclosed.

**2. `src/modes/AccessibilityPanel.h/.cpp`** — the export exposed from the
panel UI: an "Export CSV…" / "Summary PDF…" button row. Gating is honest and
identity-tied: a `m_hasReport` flag arms the actions ONLY after a scan
delivered for the current identity (`updateDisplay`; ARC06 tie checked by
`onScanFinished`), `setDocument` disarms them (an old report may never
describe the new identity), and before that they are disabled with a
truthful tooltip (never dead controls). The dialogs are click adapters over
path-taking public seams `exportCsvTo(path)` / `exportSummaryPdfTo(path)`
(the `CommentsWidget::exportDisplayedCsv` idiom); both fail closed without a
delivered report. Suggested filenames follow the document
(`<base>-a11y-report.csv` / `<base>-a11y-summary.pdf`), confirmations/warnings
via QMessageBox (the CommentsWidget idiom; the status line keeps showing the
scan verdict).

**3. `src/engines/Standard14Text.h` (new)** — the W1-04 standard-14 toolkit
(`WinAnsiSafeString` + `sanitizeForStandard14` + `wrapText`) hoisted VERBATIM
out of `ReviewSummaryWriter.cpp` into a shared inline header. Rationale: the
repo's own anti-drift discipline (csvFormulaSafeCell was made public "so the
two exporters cannot drift"; the a11y writer draws the same untrusted
document-derived strings through the same font and needs the same guard).
`ReviewSummaryWriter.cpp` now includes the header with `using`-declarations —
zero behavior change, proven by its two suites (below).

**4. `CMakeLists.txt`** — engine sources (`A11yReportWriter.h/.cpp`,
`Standard14Text.h`) added to `pdfws_engines`; new `TestA11yReportWriter`
suite registered (offscreen, TIMEOUT 180, **RUN_SERIAL TRUE** — SafeSave seam
class FU-2, same as TestPrintableSummary/TestAccessibilityFixes) with the
qoffscreen plugin deploy step.

**5. Tests** —
- `tests/TestA11yReportWriter.cpp` (new, 11 pins): the csvCell escaping
  contract (=cmd/@SUM/+3+cmd escaped; -2/+3.14/-2,5 M3 number exemption; the
  -2+3+cmd fixture; RFC-4180 quoting), CSV verdict/degradation/findings
  content (scan status, tagged, severity counts, "Fields reported,60 of
  100", Truncated flag, "Verdict,4 gap(s) found — report truncated", the
  never-claims-"accessible"/"conformant" negatives), clean-scan honesty,
  failed-scan-not-greenwashed (scan status failed + zero invented finding
  rows), end-to-end formula-injection hardening of an attacker-shaped
  whyNot, PDF read-back via PDFium (title, document, verdict, disclosure,
  truncation totals, finding target lines, severity labels), clean+failed
  PDF honesty, pinned verdictLine wording, and the SafeSave overwrite guard
  (refused commit leaves the destination byte-identical).
- `tests/TestAccessibilityPanel.cpp` (+1 pin, 18 total):
  `exportActionsGateOnDeliveredReport` — buttons exist-but-disabled with a
  truthful tooltip before any scan; the seam refusal fail-closed (no files
  created); armed after a delivered scan; the CSV artifact of the DELIVERED
  report carries the format tag + the checker's findings; the PDF artifact
  is written; a document change disarms the actions again.

## Build + gate results (Release, ucrt64-first PATH, this worktree's build-rel)

- Configure: runbook flags exactly. Clean (initial configure 61.2s; re-run
  after adding sources).
- Build: `-k 0 -j 2`. **BUILD_RC=0** (true exit code, full tree) on
  `build-final2.log`. Incidents, all environmental, all resolved by runbook
  retries (recorded in `evidence-a11y-export/README.md`):
  1. two background builds killed externally (exit 137) under co-tenant load
     — resumed incrementally both times;
  2. one transient LTO link failure (`TestBatchOcrConfidence.exe`,
     "lto-wrapper failed", 29 LTRANS jobs) — clean on incremental retry;
  3. one `libpdfws_engines.a` "can't be removed and still exist" — caused by
     an earlier still-running background build holding the lock (cleared
     when that build completed; not a flake).
- Runtime DLLs verified present in the worktree (no copies needed).
- R7 evidence (`docs/audit/evidence-a11y-export/`): fail-before (BUILD_RC=1,
  both new suites RED with exactly the new-API errors) → NC once (10 passed /
  1 failed; failing pin was correct, fix was test-READER-side whitespace
  normalization; pin never weakened) → pass-after ×3 SERIAL (ctest, no -j):
  TestAccessibilityPanel 18/18 + TestA11yReportWriter 11/11, three
  consecutive clean runs.
- Neighbor suites after the ReviewSummaryWriter hoist (serial): TestReviewSummary,
  TestPrintableSummary, TestAccessibilityChecker, TestAccessibilityFixes,
  TestAccessibilityTagger — 5/5 Passed.

## Known limits (shipped scope)

- The checker's check set is unchanged (no new detection) — the lane
  exports EXISTING verdicts, per the scorecard row's framing.
- The PDF summary font is standard-14 Helvetica (W1-04 rules): non-WinAnsi
  characters (CJK, emoji) render as "?" with an in-artifact substitution
  note; CSV/UTF-8 carries them losslessly.
- The CSV report block is key/value with a second findings table (a
  sectioned artifact); consumers wanting only the findings table can anchor
  on its header row. This carries more honest context than flattening the
  verdict into columns.
- PDF-level export honors the report identity, not a live re-scan: the
  panel exports the LAST DELIVERED report (same contract as the displayed
  scope of the comments exporter). Re-run Check for a fresh artifact.

## Owner items for the integrator

1. Fold note for the scorecard row 15 + CHANGELOG at wave fold (this lane
   did not edit shared documents).
2. The `Standard14Text.h` hoist touched `ReviewSummaryWriter.cpp` (includes
   + two `using` declarations + deletion of the moved block). Behavior-neutral,
   proven by TestReviewSummary + TestPrintableSummary; merge order vs other
   lanes does not matter (self-contained file).
3. If a compliance consumer wants machine-readable JSON later, the
   `csvPayload` seam is the model to extend (same verdict/degradation block).

## Commits

- `feat(a11y)`: exportable checker results — A11yReportWriter (CSV with the
  shared csvFormulaSafeCell hardening + PDF summary through the SafeSave
  transaction), Standard14Text.h hoist, AccessibilityPanel export row with
  identity-tied arming, 12 new pins (TestA11yReportWriter 11 + panel gating 1).
- `docs(audit)`: lane report + `evidence-a11y-export/` (fail-before, NC-once,
  pass-after ×3 SERIAL, neighbor suites, incident log).
