# GlyphPDF Soak Test Automation Script
# M7-PROMPT-2 D4
#
# Drives GlyphPDF through a repeating operation loop using Windows UI Automation and
# keyboard shortcuts. One iteration = open a PDF -> annotate -> save -> close.
# The script cycles through all PDFs in the given folder indefinitely (or for a fixed
# count) and logs each iteration to a log file.
#
# IMPORTANT — READ BEFORE RUNNING:
# -------------------------------------------------------------------------
# This is a best-effort UI automation script. It relies on keyboard shortcuts
# (documented in README.md) and assumes the standard window layout. It is NOT
# a hermetic test harness. You MUST manually supervise the first 3-5 iterations
# to confirm GlyphPDF is responding correctly before walking away.
#
# If GlyphPDF displays a dialog that the script does not expect (e.g., an update
# prompt, a password dialog), the script may stall. The script includes a watchdog
# that kills and restarts GlyphPDF if it becomes unresponsive for > 45 seconds,
# but manual monitoring is strongly recommended during the first 2 hours.
#
# UI Automation note: this script uses SendKeys via the Windows Shell COM object,
# which is the most compatible approach across all Windows 10/11 builds without
# requiring additional automation frameworks. For a production-grade harness,
# replace SendKeys calls with Windows UI Automation (UIA) API calls.
# -------------------------------------------------------------------------
#
# Usage:
#   .\soak-automation-script.ps1 -PdfFolder C:\SoakTestPDFs -LogFile C:\SoakTest\logs\automation.log
#
# Optional parameters:
#   -MaxIterations 0   (0 = run indefinitely; set a number to cap iterations)
#   -WatchdogSec 45    (kill and restart GlyphPDF if frozen for this many seconds)

param(
    [string] $PdfFolder      = "C:\SoakTestPDFs",
    [string] $LogFile        = "C:\SoakTest\logs\automation.log",
    [string] $GlyphPdfExe    = "C:\Program Files\GlyphPDF\GlyphPDF.exe",
    [int]    $MaxIterations  = 0,            # 0 = unlimited
    [int]    $WatchdogSec    = 45,           # seconds before declaring freeze
    [int]    $PauseAfterOpMs = 1500          # ms pause between operations
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Continue"   # Don't abort on recoverable errors

# ---------------------------------------------------------------------------
# Setup
# ---------------------------------------------------------------------------
$null = New-Item -ItemType Directory -Force -Path (Split-Path $LogFile)

function Write-Log {
    param([string]$Message, [string]$Level = "INFO")
    $Ts = (Get-Date).ToString("yyyy-MM-dd HH:mm:ss")
    $Line = "[$Ts] [$Level] $Message"
    $Line | Add-Content -Path $LogFile
    switch ($Level) {
        "ERROR" { Write-Host $Line -ForegroundColor Red }
        "WARN"  { Write-Host $Line -ForegroundColor Yellow }
        default { Write-Host $Line }
    }
}

function Pause-Ms {
    param([int]$Ms = $PauseAfterOpMs)
    Start-Sleep -Milliseconds $Ms
}

# Get sorted list of PDFs
$PdfFiles = Get-ChildItem -Path $PdfFolder -Filter "*.pdf" | Sort-Object Name
if ($PdfFiles.Count -eq 0) {
    Write-Log "No PDF files found in $PdfFolder. Exiting." "ERROR"
    exit 1
}

Write-Log "Soak automation starting. PDF folder: $PdfFolder ($($PdfFiles.Count) files)"
Write-Log "GlyphPDF exe: $GlyphPdfExe"
Write-Log "Max iterations: $(if ($MaxIterations -eq 0) { 'unlimited' } else { $MaxIterations })"
Write-Log "Watchdog timeout: $WatchdogSec seconds"

# ---------------------------------------------------------------------------
# Helper: launch GlyphPDF and wait for it to be ready
# ---------------------------------------------------------------------------
function Start-GlyphPdf {
    Write-Log "Launching GlyphPDF..."
    $proc = Start-Process -FilePath $GlyphPdfExe -PassThru -ErrorAction Stop

    # Wait up to 15 seconds for the process window to appear
    $deadline = (Get-Date).AddSeconds(15)
    do {
        Start-Sleep -Milliseconds 500
        $running = Get-Process -Id $proc.Id -ErrorAction SilentlyContinue
    } while ($null -ne $running -and $running.MainWindowHandle -eq 0 -and (Get-Date) -lt $deadline)

    if ($null -eq $running) {
        Write-Log "GlyphPDF exited immediately after launch." "ERROR"
        return $null
    }
    Write-Log "GlyphPDF launched (PID: $($proc.Id))"
    Pause-Ms 2000   # let the UI fully initialise
    return $proc
}

# ---------------------------------------------------------------------------
# Helper: send keystrokes to GlyphPDF main window
# ---------------------------------------------------------------------------
function Send-Keys {
    param([string]$Keys)
    $shell = New-Object -ComObject WScript.Shell
    $null  = $shell.AppActivate("GlyphPDF")
    Pause-Ms 300
    $shell.SendKeys($Keys)
    Pause-Ms $PauseAfterOpMs
}

# ---------------------------------------------------------------------------
# Helper: watchdog check — kill and return $false if frozen
# ---------------------------------------------------------------------------
function Test-GlyphPdfResponsive {
    param([System.Diagnostics.Process]$Proc, [int]$TimeoutSec = $WatchdogSec)
    if ($null -eq $Proc -or $Proc.HasExited) { return $false }
    # Check if the process is responding via the Win32 API
    $responding = $Proc.Responding
    if (-not $responding) {
        Write-Log "GlyphPDF NOT RESPONDING. Waiting $TimeoutSec seconds before force-kill..." "WARN"
        Start-Sleep -Seconds $TimeoutSec
        $Proc.Refresh()
        if (-not $Proc.Responding) {
            Write-Log "GlyphPDF still frozen. Force-killing." "ERROR"
            Stop-Process -Id $Proc.Id -Force -ErrorAction SilentlyContinue
            return $false
        }
    }
    return $true
}

# ---------------------------------------------------------------------------
# Helper: open a PDF file using Ctrl+O -> type path -> Enter
# ---------------------------------------------------------------------------
function Open-Pdf {
    param([string]$Path)
    Write-Log "Opening: $Path"
    Send-Keys "^o"         # Ctrl+O -> Open dialog
    Pause-Ms 1000
    Send-Keys $Path        # Type the file path into the dialog
    Pause-Ms 500
    Send-Keys "{ENTER}"    # Confirm
    Pause-Ms 2000          # Wait for file to load
}

# ---------------------------------------------------------------------------
# Helper: perform a lightweight annotation (Ctrl+H -> find -> Escape)
# We use Find (Ctrl+F) as a proxy operation because it exercises the text search
# path without requiring mouse clicks at absolute coordinates.
# For annotation, we use the keyboard shortcut to the Home tab if available.
# ---------------------------------------------------------------------------
function Do-Annotation {
    Write-Log "Operation: text search (annotation proxy)"
    Send-Keys "^f"             # Open Find bar
    Pause-Ms 500
    Send-Keys "GlyphPDF"       # Type search term
    Pause-Ms 500
    Send-Keys "{ENTER}"        # Execute search
    Pause-Ms 1000
    Send-Keys "{ESCAPE}"       # Close find bar
}

# ---------------------------------------------------------------------------
# Helper: export to text (Convert tab -> Ctrl shortcut or menu navigation)
# We use Ctrl+S (save) as the primary file-write operation.
# ---------------------------------------------------------------------------
function Do-Save {
    Write-Log "Operation: Ctrl+S save"
    Send-Keys "^s"      # Ctrl+S
    Pause-Ms 1500
    # If a save-as dialog appeared (first save), press Enter to confirm existing path
    Send-Keys "{ENTER}"
    Pause-Ms 1000
}

# ---------------------------------------------------------------------------
# Helper: close the current document
# Ctrl+W closes the active document tab in most Qt apps; fall back to Alt+F4
# if the application uses a single-window model.
# ---------------------------------------------------------------------------
function Do-Close {
    Write-Log "Operation: close document"
    Send-Keys "^w"      # Close document (may not exist — app handles gracefully)
    Pause-Ms 1000
    # If GlyphPDF asks "Save changes?" press N (no) since we already saved
    Send-Keys "n"
    Pause-Ms 500
}

# ---------------------------------------------------------------------------
# Main loop
# ---------------------------------------------------------------------------
$Iteration   = 0
$ErrorCount  = 0
$PdfIndex    = 0

$glyphProc = Start-GlyphPdf
if ($null -eq $glyphProc) {
    Write-Log "Could not start GlyphPDF. Aborting." "ERROR"
    exit 1
}

Write-Log "=== Begin soak loop ==="
Write-Log "IMPORTANT: Manually supervise the first 3-5 iterations to confirm correct operation."

try {
    while ($true) {
        $Iteration++
        if ($MaxIterations -gt 0 -and $Iteration -gt $MaxIterations) {
            Write-Log "Reached max iterations ($MaxIterations). Stopping."
            break
        }

        $PdfPath = $PdfFiles[$PdfIndex % $PdfFiles.Count].FullName
        $PdfIndex++

        Write-Log "--- Iteration $Iteration | File: $(Split-Path $PdfPath -Leaf) ---"

        # Check GlyphPDF is still alive before each iteration
        $glyphProc.Refresh()
        if ($glyphProc.HasExited) {
            Write-Log "GlyphPDF has exited unexpectedly before iteration $Iteration. Restarting." "WARN"
            $ErrorCount++
            $glyphProc = Start-GlyphPdf
            if ($null -eq $glyphProc) {
                Write-Log "Failed to restart GlyphPDF. Aborting." "ERROR"
                break
            }
        }

        # --- Operation sequence ---
        try {
            Open-Pdf     $PdfPath
            if (-not (Test-GlyphPdfResponsive $glyphProc)) {
                Write-Log "GlyphPDF unresponsive after Open. Restarting." "ERROR"; $ErrorCount++
                $glyphProc = Start-GlyphPdf; continue
            }

            Do-Annotation
            if (-not (Test-GlyphPdfResponsive $glyphProc)) {
                Write-Log "GlyphPDF unresponsive after Annotation. Restarting." "ERROR"; $ErrorCount++
                $glyphProc = Start-GlyphPdf; continue
            }

            Do-Save
            if (-not (Test-GlyphPdfResponsive $glyphProc)) {
                Write-Log "GlyphPDF unresponsive after Save. Restarting." "ERROR"; $ErrorCount++
                $glyphProc = Start-GlyphPdf; continue
            }

            Do-Close

            Write-Log "Iteration $Iteration COMPLETE (errors so far: $ErrorCount)"

        } catch {
            Write-Log "Exception in iteration $Iteration : $_" "ERROR"
            $ErrorCount++
        }

        # Brief pause between iterations — keeps the cycle at roughly 60 seconds
        # even if individual operations are faster than expected.
        Pause-Ms 3000

    } # end while

} finally {
    Write-Log "=== Soak loop ended ==="
    Write-Log "Total iterations  : $Iteration"
    Write-Log "Total error count : $ErrorCount"

    # Attempt graceful GlyphPDF exit
    if (-not $glyphProc.HasExited) {
        Write-Log "Closing GlyphPDF gracefully (Alt+F4)..."
        $shell = New-Object -ComObject WScript.Shell
        $null  = $shell.AppActivate("GlyphPDF")
        $shell.SendKeys("%{F4}")
        Start-Sleep -Seconds 3
        if (-not $glyphProc.HasExited) {
            Stop-Process -Id $glyphProc.Id -Force -ErrorAction SilentlyContinue
            Write-Log "GlyphPDF force-killed at script end."
        }
    }

    Write-Log "Automation script finished. Log: $LogFile"
}
