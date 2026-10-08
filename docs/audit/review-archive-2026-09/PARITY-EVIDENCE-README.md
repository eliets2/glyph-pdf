# Parity branch review evidence — 5 September 2026

Reviewed commit: 06b542db7256e01260e607a96bcbf62205abb79d, feat/parity-glm.
Source worktree: C:/Users/User/Projects/pdf-parity. Original main was not merged or modified.
The archive contains generated test documents, not private user documents or screenshots.

## Checks and limits

Fresh configuration and selected build succeeded (PdfWorkstation plus 12 test executables).
All 12 targeted CTest suites passed in 37.11 seconds after supplying runtime dependency paths
and test-only Windows settings/local-server access. The initial DLL-loader failures and AI
timeout are preserved separately. 91 suites are registered; the other 79 were not run here.
The successful first build's completion and compilation tail were returned in the tool output.
A later no-op build overwrote the full build log; parity-build.log records that no-op verification,
not the original complete compiler output. No claim of a warning-free build is made.

## Reproduce in a new isolated audit workspace

Create a work/ directory and place the five scripts in it. review_setup.py expects a git archive
named work/parity-06b542d.zip. Generate that archive from the specified commit with git archive.
The source checkout is read only; do not reuse another session's build directory.
The setup copies the checkout's existing PoDoFo, PDFium and ONNX dependency trees; they are
not bundled in this evidence zip. See setup script for exact source/destination paths.

From the workspace parent, use C:/Python314/python.exe to run:
  work/review_setup.py
  work/review_build.py
  work/review_test.py
  work/compile_parity_probe.py

The scripts expect the installed C:/msys64/ucrt64 toolchain. review_test.py needs normal
test-profile Windows settings and loopback test-server access; it does not contact a remote AI.
The production source is not edited by the probe. Probe PDFs/CSV/XLSX are written beneath
work/parity-probe-data. Missing DLLs are an environment failure, not a test assertion failure.

## Decisive probe results

- Same-path form addition succeeds and leaves a readable field-bearing PDF.
- Two same-baseline columns collapse into one CSV/XLSX column (V03).
- A one-word edit in a one-page PDF also creates PageRemoved and PageAdded (V04).
- Injected commit failure during undo preserves the edited value but moves history to index 0
  and disables Undo (V02).
- Engine import to the temporary candidate succeeds. The separate controller path bug (V01)
  was established by source trace, not by deleting a loaded document in the UI.

parity-probe.log has the runtime output. table.xlsx can be inspected as a ZIP; sheet1.xml has
A1 and A2 only, containing merged column text. The source fixture positions columns at x=72
and x=300. See the report for exact code locations, severity, and acceptance instructions.
