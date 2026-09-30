evidence-csv-sinks — PROGRAM-CONSOLIDATION-2026-09-25 §1.1 (the PGR-16/17 class), feat/csv-sinks lane, 2026-09-30

SITUATION (why this dir is verification evidence, not a fix):
  The §1.1 fix ALREADY LANDED on main inside the consolidation — commit b1854eae
  "fix(csv): the three remaining CSV sinks neutralize formula leads" — and this
  lane's base (main @ 2ccfd5ba) already contains it. All three sinks
  (MeasureMode::measureCsvEscape, BatchMode::runReportCsv, ErrorLog::exportCsv)
  route every field through ConversionManager::csvFormulaSafeCell including the
  M3 plain-number exemption. This lane therefore REDID NO CODE; it independently
  verified the landed fix on a fresh worktree build and produced fresh evidence.

fail-before-*.txt
  VERBATIM COPIES of the secfix lane's genuine RED runs on the UNFIXED code
  (docs/audit/evidence-secfix-v150/11-fail-before-*.txt, pre-b1854eae), because
  the unfixed code does not exist at this lane's base and reproducing it here
  would be a simulation, not the original fail-before. Totals: TestMeasureCsvExport
  9P/2F, TestBatchPresetsP2 2P/1F, TestBatchMode 2P/1F — every injection pin red.

nc-bypassed-*.txt
  THIS LANE's negative control on the CURRENT tree: the body of
  ConversionManager::csvFormulaSafeCell scoped to `return cell;` (bypass only,
  pins kept, ConversionManager.cpp otherwise untouched), rebuilt, three suites
  run. Totals: TestMeasureCsvExport 9P/2F, TestBatchPresetsP2 41P/1F,
  TestBatchMode 17P/1F — the failures are EXACTLY the §1.1 injection pins
  (formulaLeadLabelIsNeutralized,
  plainNumberLabelKeepsTheExemptionButOperatorLeadEscapes,
  runReportCsvFormulaLeadsNeutralized, errorLogCsvNeutralizesFormulaLeads)
  failing for the right reason (missing apostrophe neutralizer). The bypass was
  reverted after capture; the committed tree carries NO source diff.

pass-after-run{1,2,3}.txt
  THREE consecutive SERIAL runs (QtTest, QT_QPA_PLATFORM=offscreen, no -j) after
  the revert, on the base-equal tree:
  TestMeasureCsvExport 11P/0F ×3, TestBatchPresetsP2 42P/0F ×3 (42 vs the
  secfix lane's 34 — main gained later pins; every test passes),
  TestBatchMode 18P/0F ×3.

Build: fresh worktree configure (Ninja, UCRT64 GCC, CMAKE_BUILD_TYPE=Release,
GLYPHPDF_RELEASE_BUILD=OFF — the ON flavor refuses to configure without ONNX
Runtime, which is not installed), gitignored libpodofo.dll/pdfium.dll copied
from the primary checkout, pdfium.dll additionally deployed next to the test
executables (the POST_BUILD rule deploys podofo but not pdfium; without it the
suites die 0xc0000135).
