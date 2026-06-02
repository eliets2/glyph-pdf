# GlyphPDF Soak Test Acceptance Criteria

**Milestone:** M7-PROMPT-2, D4  
**Applies to:** GlyphPDF 1.0.0-beta (and release candidate)  
**Version:** 2026-06-02

---

## Overview

The 48-hour soak test passes if and only if all criteria in the Hard Gates table below
are met. Any single Hard Gate failure is a release blocker. Soft Gate failures are
logged and triaged but do not automatically block ship.

---

## Hard Gates — All Must Pass

| ID | Criterion | Measurement Method | Threshold | Pass / Fail |
|----|-----------|-------------------|-----------|-------------|
| SG-01 | No crash in 48 hours | `soak-monitor-script.ps1` NOT_RUNNING count | 0 | Record actual count: ____ |
| SG-02 | No freeze > 30 seconds | Monitor script watchdog + automation log | 0 freeze events | Record actual count: ____ |
| SG-03 | Working set at Hour 4 | CSV row nearest ElapsedMinutes=240 | <= 500 MB | Record actual MB: ____ |
| SG-04 | Avg hourly WS growth (Hours 2-48) | Final monitor report "Avg hourly growth" field | <= 10 MB/hour | Record actual MB/hr: ____ |
| SG-05 | No leaked temp files | `Get-ChildItem $env:TEMP -Filter "GlyphPDF*"` after clean exit | 0 files | Record actual count: ____ |
| SG-06 | ctest regression | `ctest --output-on-failure` on release candidate build after soak | All 12 targets pass | Record failing targets if any: ____ |
| SG-07 | Dr. Memory definite leaks | Dr. Memory `-leaks_only` session report | 0 "definitely leaked" blocks | Record actual count: ____ |

To mark a gate: write "PASS" or "FAIL + [actual value]" in the Pass/Fail column when
completing the soak.

---

## Gate Definitions and Rationale

### SG-01 — No crash in 48 hours

A crash is any unplanned exit of `GlyphPDF.exe`. The monitor script records these as
NOT_RUNNING samples. Even one crash in 48 hours is a soak failure because:
- It means the application cannot sustain a professional workflow session without
  intervention.
- Crashes under sustained use typically indicate use-after-free, stack overflow,
  or unhandled exception conditions that are also present but less frequently
  triggered in normal interactive use.

If a crash occurs: collect the minidump (see runbook crash handling section), identify
the faulting module and stack trace, and fix before re-running the soak.

### SG-02 — No freeze > 30 seconds

A freeze is `Process.Responding == false` for more than 30 seconds. The watchdog in
`soak-automation-script.ps1` implements this check. Freezes indicate:
- Deadlock between the UI thread and a background worker.
- An operation blocking the event loop (e.g., a synchronous network call on the
  UI thread).
- A background thread spinning in an infinite loop.

### SG-03 — Working set <= 500 MB at Hour 4

The first 4 hours represent the initial warm-up and steady-state establishment period.
By Hour 4, the application should have reached its operational memory footprint. A
reading above 500 MB at Hour 4 indicates that the initial load itself is excessive
before any leak growth is even calculated.

Basis for 500 MB threshold:
- The RenderCache is configured to 256 MB maximum (README.md architecture section).
- The PoDoFo document object graph for a 100-page PDF is typically < 20 MB.
- Application UI, Qt framework overhead, and OCR engine models (ONNX Runtime) together
  add approximately 100-150 MB on first load.
- 500 MB provides a 25% headroom margin above the expected ~400 MB maximum reasonable
  footprint after first-hour warm-up.

### SG-04 — Average hourly growth <= 10 MB/hour (Hours 2-48)

The first 2 hours are excluded from the growth rate calculation because:
- ONNX Runtime lazy-loads model weights on first inference call.
- Qt font and glyph caches fill on first use.
- OS prefetcher warms file-system caches.

These are one-time allocations, not leaks. After Hour 2, memory should plateau or
grow only linearly with cached rendered pages (which are bounded by the LRU cap).

10 MB/hour threshold: over 46 active hours (Hours 2-48), this allows up to 460 MB of
growth, which is generous. In a healthy application with no leaks and a 256 MB LRU
cap, growth should be near 0 MB/hour after warm-up. If growth exceeds 10 MB/hour
consistently, a structural leak is present.

**Interpretation guidance:**

| Measured growth rate | Interpretation |
|---------------------|----------------|
| 0-3 MB/hour | Expected. LRU working correctly. |
| 3-10 MB/hour | Borderline. Investigate with Dr. Memory. PASS but flag for v1.1. |
| 10-50 MB/hour | Definite leak. SG-04 FAIL. Block ship. |
| > 50 MB/hour | Severe leak. SG-04 FAIL. Correlated with monitor script LEAK ALERT. |

### SG-05 — No leaked temp files in %TEMP%\GlyphPDF*

GlyphPDF creates temporary files in `%TEMP%` during operations that require intermediate
disk space (e.g., incremental PDF saves, OCR preprocessing images, export intermediates).
These files must be deleted on clean application exit.

Command to verify after clean exit:
```powershell
$leaks = Get-ChildItem -Path $env:TEMP -Filter "GlyphPDF*" -Recurse -ErrorAction SilentlyContinue
if ($leaks.Count -gt 0) {
    Write-Host "FAIL: $($leaks.Count) temp file(s) remain:" -ForegroundColor Red
    $leaks | ForEach-Object { Write-Host "  $_" }
} else {
    Write-Host "PASS: No GlyphPDF temp files remain." -ForegroundColor Green
}
```

If temp files are found, record their names and sizes. Files matching `*ocr*`, `*render*`,
or `*export*` patterns indicate which feature area is leaking. File a bug with severity
High (these accumulate on disk across sessions — a user running GlyphPDF daily for a
week could lose gigabytes of disk space).

### SG-06 — All ctest targets pass after the soak

Run the full ctest suite on the same build that survived the 48-hour soak. This confirms
that the soak did not leave the application state or any shared resources in a condition
that breaks test assumptions.

```powershell
cd C:\Users\User\Projects\pdf\build
$env:QT_QPA_PLATFORM = "offscreen"
ctest --output-on-failure
```

All 12 listed test targets must pass (see README.md Testing section for the full list:
UnitTests, TestInterfaces, SmokeTest, TestSanitization, TestSignatureValidation,
TestRedaction, TestThreadSafety, TestEncryption, TestResourceLimits, TestControllers,
TestIntegration, TestPerformance).

If a test that was passing before the soak now fails, the soak revealed a state
mutation — a resource, file, or registry entry that GlyphPDF modified and did not
restore. This is a High severity bug.

### SG-07 — Dr. Memory reports 0 definite leaks

Dr. Memory distinguishes between:
- **Definitely leaked**: memory allocated and never freed, with no remaining pointer.
  These are true leaks. Zero is required.
- **Possibly leaked**: memory with a pointer, but not via a clear allocation path.
  These are flagged but may include intentional singletons or OS-managed resources.
  Non-zero is acceptable for release as long as the count is stable across runs.
- **Still reachable**: memory reachable at exit via a global/static. Not a leak.

Dr. Memory command (run on a manual session, not the 48h run):
```powershell
& "C:\Program Files (x86)\Dr. Memory\bin\drmemory.exe" `
    -leaks_only `
    -logdir C:\SoakTest\soak-results\drmem `
    -- "C:\Program Files\GlyphPDF\GlyphPDF.exe"
```

Run one full operation cycle manually (open a PDF, annotate, save, close), then exit
GlyphPDF. Dr. Memory writes the report to the `-logdir`. Search the report for the
string `LEAK SUMMARY`:

```
LEAK SUMMARY:
   definitely lost: X bytes in N blocks
    possibly lost:  Y bytes in M blocks
  still reachable: Z bytes in P blocks
```

The required value for X (definitely lost) is **0**.

---

## Soft Gates — Tracked, Not Blocking

| ID | Criterion | Threshold | Notes |
|----|-----------|-----------|-------|
| SS-01 | Automation error count | < 5% of iterations | SendKeys-based automation is best-effort; occasional failures are expected |
| SS-02 | "Possibly leaked" (Dr. Memory) | Stable across 2 runs | Non-zero acceptable if count does not grow between runs |
| SS-03 | Handle count growth | < 50 handles/hour | Track via CSV HandleCount column; gradual growth may indicate GDI or kernel object leaks |
| SS-04 | Thread count stability | No unbounded growth | Thread count should stabilize after warm-up; sustained growth = thread-pool leak |

---

## Results Record

Complete this table after the soak:

| Gate | Result | Measured Value | Notes |
|------|--------|---------------|-------|
| SG-01 Crash count | | | |
| SG-02 Freeze count | | | |
| SG-03 WS at Hour 4 | | MB | |
| SG-04 Avg hourly growth | | MB/hour | |
| SG-05 Temp file leaks | | count | |
| SG-06 ctest targets | | X/12 pass | |
| SG-07 Dr. Memory definite leaks | | count | |
| **OVERALL SOAK RESULT** | **PASS / FAIL** | | |

**Sign-off:**

| Role | Name | Date | Signature |
|------|------|------|-----------|
| Soak test runner | | | |
| Bash lead / QA owner | | | |
| Developer sign-off | | | |

---

## What to Do on Failure

| Failing gate | Immediate action |
|-------------|-----------------|
| SG-01 (crash) | Collect minidump. Identify faulting function. Fix and re-run soak. |
| SG-02 (freeze) | Check automation log for last operation before freeze. Attach debugger to find deadlock. |
| SG-03 (WS at Hour 4) | Profile startup allocations with Samply or Dr. Memory allocation tracking. Check ONNX model loading. |
| SG-04 (growth rate) | Run Dr. Memory with `-leaks_only` for 2 hours. Cross-reference leaked allocation sites with the 5 hot paths identified in `docs/performance/hot-path-analysis.md`: RenderCache LRU, redactCanvasRecursively, roverMerge, buildDssDictionary, PpOcrDecoder preprocessing. |
| SG-05 (temp files) | Search codebase for `QTemporaryFile` and `QDir::tempPath()` usage. Ensure all temporary files are created via RAII guards that delete on destruction. |
| SG-06 (ctest) | Run failing tests in isolation. Add debug logging. The most likely failure after a soak is TestEncryption or TestRedaction if a shared document state was not cleaned up. |
| SG-07 (Dr. Memory leaks) | The Dr. Memory report names the allocation site (source file + line). Fix the allocation at the named site. Re-run Dr. Memory session to confirm zero. |
