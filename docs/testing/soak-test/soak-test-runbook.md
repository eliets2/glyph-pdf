# GlyphPDF 48-Hour Soak Test Runbook

**Milestone:** M7-PROMPT-2, D4  
**Purpose:** Detect memory leaks, resource exhaustion, and stability failures that only
manifest under sustained load over 48 wall-clock hours.  
**Owner:** Assigned developer / QA engineer  
**Estimated run time:** 48 hours continuous + ~2 hours setup + ~1 hour post-run analysis

---

## Prerequisites

Complete all items in this list before starting the soak. A partially-configured soak
produces ambiguous results.

### 1. Environment

- **Fresh Windows VM** (strongly recommended): use a clean Windows 10 21H2 or Windows
  11 22H2 VM with no other applications running. A fresh VM eliminates interference from
  background processes that could skew memory readings.
- If a VM is not available, use a dedicated physical machine. Close all other
  applications before starting.
- **RAM:** minimum 8 GB. The soak acceptance threshold assumes a working set that starts
  below 200 MB; less RAM increases likelihood of OS-level paging confusing measurements.
- **Disk:** minimum 5 GB free for output files, logs, and Dr. Memory snapshots.
- **Network:** connected (for TSA/OCSP during any signing operations in the loop);
  disconnect only if you are intentionally testing offline-mode resilience.

### 2. Software Installation

- Install `GlyphPDF-1.0.0-beta-x64.msi` on the clean VM (not the development build —
  use the same MSI that beta testers received).
- Install Dr. Memory from https://drmemory.org (choose the Windows installer).
  Default install path: `C:\Program Files (x86)\Dr. Memory\`.
  Verify: open PowerShell and run:
  ```powershell
  & "C:\Program Files (x86)\Dr. Memory\bin\drmemory.exe" --version
  ```
  Expected: version string printed. If not found, adjust the path.
- Install Windows Performance Monitor (built in to all Windows 10/11 — no install
  needed) — used for working-set counter logging as an independent cross-check.

### 3. Test PDF Corpus

Prepare a folder `C:\SoakTestPDFs\` containing exactly 10 representative PDFs:

| File | Type | Why included |
|------|------|-------------|
| `01_short_textrich.pdf` | 5-page born-digital | Fast open/save cycle |
| `02_medium_textrich.pdf` | 30-page born-digital | Standard annotation workload |
| `03_large_textrich.pdf` | 100-page born-digital | Large-file open/close stress |
| `04_scanned_clean.pdf` | 10-page scanned, high quality | OCR hot path |
| `05_scanned_noisy.pdf` | 10-page scanned, poor quality | OCR preprocessing + ROVER |
| `06_forms.pdf` | Interactive form PDF | Form fill / save cycle |
| `07_signed.pdf` | Pre-signed PDF | Signature validation path |
| `08_encrypted.pdf` | AES-256 encrypted PDF | Password dialog + decrypt path |
| `09_xobjects.pdf` | PDF with embedded XObjects | Redaction hot path (PoDoFoBackend) |
| `10_large_annotated.pdf` | 50-page with 20+ annotations | Annotation serialisation path |

If you cannot source all types, substitute with any real-world PDFs. The priority order
is: large file (03) > scanned (04/05) > XObjects (09) > forms (06) > others.

### 4. Monitoring Scripts

Copy `soak-monitor-script.ps1` and `soak-automation-script.ps1` (both in this
`docs/testing/soak-test/` directory) to `C:\SoakTest\` on the target machine.

### 5. Baseline Measurement

Before starting the soak, record a baseline memory reading for the installed GlyphPDF:
1. Launch GlyphPDF from Start Menu (do not open any file).
2. Wait 30 seconds for startup to settle.
3. Open Task Manager -> Details tab -> find `GlyphPDF.exe`.
4. Record "Working Set (Memory)" value. This is your idle baseline.
5. Note it in `C:\SoakTest\soak-results\baseline.txt` along with the timestamp.

---

## Soak Test Execution

### Step 1 — Create Output Directory

```powershell
New-Item -ItemType Directory -Force -Path C:\SoakTest\soak-results
New-Item -ItemType Directory -Force -Path C:\SoakTest\logs
```

### Step 2 — Launch the Monitor Script

Open a dedicated PowerShell window. Run:

```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
& C:\SoakTest\soak-monitor-script.ps1 -DurationHours 48 -OutputDir C:\SoakTest\soak-results
```

The monitor script will:
- Log working-set (RSS) to `C:\SoakTest\soak-results\memory-log.csv` every 60 seconds.
- Alert if working-set growth exceeds 50 MB/hour (leak indicator).
- Write a final summary report at the end of 48 hours.

Do NOT close this PowerShell window during the soak. Minimize it.

### Step 3 — Launch the Automation Script

Open a second PowerShell window. Run:

```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
& C:\SoakTest\soak-automation-script.ps1 -PdfFolder C:\SoakTestPDFs -LogFile C:\SoakTest\logs\automation.log
```

The automation script cycles through the 10 PDFs. Manually supervise the first 2-3
iterations to confirm the script is driving GlyphPDF correctly before walking away.
During the first 2 hours, check back every 30 minutes. After 2 hours of stable
operation, hourly checks are sufficient.

### Step 4 — Windows Performance Monitor (optional cross-check)

For an independent working-set counter:
1. Open `perfmon.exe` (run as administrator for full access).
2. In the Performance Monitor graph, click the green (+) button.
3. Add counter: `Process` -> `Working Set` -> select `GlyphPDF` from the instance list.
4. Click OK. The graph now tracks working set independently of the PowerShell script.
5. Screenshot the graph at Hours 0, 2, 6, 12, 24, 36, 48. Save screenshots to
   `C:\SoakTest\soak-results\perfmon-screenshots\`.

### Step 5 — Overnight Monitoring

Set a recurring phone alarm for every 4 hours. At each check:
1. Verify GlyphPDF is still running (Task Manager).
2. Verify the monitor PowerShell window shows no "LEAK ALERT" lines.
3. Verify the automation log (`C:\SoakTest\logs\automation.log`) shows recent activity.
4. Note the current working-set value in a paper log or a quick text file update.

If GlyphPDF has crashed: do NOT restart immediately. Capture the crash dump first
(see "Crash Handling" below), then restart the soak scripts.

### Step 6 — Dr. Memory Leak Attribution (Optional but Recommended)

Dr. Memory cannot run transparently alongside a GUI application in all configurations.
The recommended approach is to run a shorter Dr. Memory session at Hour 24 as a
spot-check rather than for the full 48 hours (Dr. Memory adds ~10x overhead; 48h under
Dr. Memory is impractical).

At Hour 24, stop the automation script (Ctrl+C), then relaunch GlyphPDF under Dr. Memory:

```powershell
# Adjust the path to match your GlyphPDF install location
& "C:\Program Files (x86)\Dr. Memory\bin\drmemory.exe" `
    -leaks_only `
    -logdir C:\SoakTest\soak-results\drmem `
    -- "C:\Program Files\GlyphPDF\GlyphPDF.exe"
```

Manually run one full operation cycle (open PDF -> annotate -> sign -> save -> close).
Exit GlyphPDF. Dr. Memory writes a leak report to the `-logdir`. Review it for
"LEAK" or "POSSIBLE LEAK" entries. Record the count in your results.

Restart the automation script for the remaining 24 hours using only Task Manager /
Performance Monitor tracking.

### Step 7 — End-of-Soak Procedure

At Hour 48:
1. Allow the current automation loop iteration to complete (do not kill mid-operation).
2. The monitor script will exit automatically after 48 hours and write the final report.
3. Stop the automation script (Ctrl+C if still running).
4. Close GlyphPDF.
5. Run ctest on the installed build source to confirm all test targets still pass:
   ```powershell
   cd C:\Users\User\Projects\pdf\build
   $env:QT_QPA_PLATFORM = "offscreen"
   ctest --output-on-failure
   ```
6. Check `%TEMP%\` for any leftover GlyphPDF temp files:
   ```powershell
   Get-ChildItem -Path $env:TEMP -Filter "GlyphPDF*" -Recurse
   ```
   Any files found here are temp-file leaks — record the count and file names.
7. Collect all results into `C:\SoakTest\soak-results\` for analysis.

---

## Crash Handling

If GlyphPDF crashes at any point during the soak:

1. Do NOT dismiss the crash dialog yet.
2. Open Task Manager -> right-click `GlyphPDF.exe` -> "Create dump file".
   Windows saves the dump to `%TEMP%\GlyphPDF.exe.DMP`. Move it immediately to
   `C:\SoakTest\soak-results\crash-TIMESTAMP.dmp`.
3. Note the iteration number from the automation log (which PDF and which operation
   was in progress).
4. Dismiss the crash dialog. GlyphPDF exits.
5. Record in `C:\SoakTest\soak-results\crash-log.txt`:
   ```
   Timestamp: <datetime>
   Iteration: <number from automation log>
   PDF in use: <filename>
   Operation: <annotate / sign / redact / export>
   Dump file: crash-TIMESTAMP.dmp
   ```
6. Restart GlyphPDF and the automation script. The soak continues; the crash itself
   is a soak failure but you want the remaining hours of data for leak analysis.

---

## Failure Conditions

The soak is considered FAILED if any of these occur:

| Condition | Threshold |
|-----------|-----------|
| Application crash | Any crash in 48 hours |
| Freeze | GlyphPDF unresponsive for > 30 seconds at any point |
| Memory at Hour 4 | Working set > 500 MB after first 4 hours of continuous use |
| Memory growth rate | Working set grows > 10 MB/hour averaged over Hours 2-48 (first 2h warm-up excluded from average) |
| Leaked temp files | Any file matching `%TEMP%\GlyphPDF*` present after clean exit |
| ctest regression | Any of the 12 ctest targets fail on the release candidate build after the soak |
| Dr. Memory definite leaks | Any "definitely leaked" block reported by Dr. Memory |

See `soak-acceptance-criteria.md` for the full pass/fail decision matrix.
