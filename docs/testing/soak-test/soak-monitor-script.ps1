# GlyphPDF Soak Test Monitor Script
# M7-PROMPT-2 D4
#
# Records GlyphPDF working-set (RSS) to CSV every 60 seconds for the specified duration.
# Alerts to the console if the growth rate exceeds the leak-indicator threshold.
# Writes a final summary report at end of run.
#
# Usage:
#   .\soak-monitor-script.ps1 -DurationHours 48 -OutputDir C:\SoakTest\soak-results
#
# Prerequisites:
#   - GlyphPDF.exe must already be running before this script starts.
#   - PowerShell 5.1+ (built in to Windows 10/11).
#   - Run with: Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass

param(
    [int]    $DurationHours     = 48,
    [string] $OutputDir         = "C:\SoakTest\soak-results",
    [string] $ProcessName       = "GlyphPDF",

    # Alert threshold: if working-set grows more than this many MB in one hour,
    # print a LEAK ALERT to the console and log it.
    [double] $LeakAlertMbPerHour = 50.0,

    # Sampling interval in seconds.
    [int]    $SampleIntervalSec  = 60
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# ---------------------------------------------------------------------------
# Setup
# ---------------------------------------------------------------------------
$null = New-Item -ItemType Directory -Force -Path $OutputDir

$CsvPath     = Join-Path $OutputDir "memory-log.csv"
$ReportPath  = Join-Path $OutputDir "soak-monitor-report.txt"
$AlertLog    = Join-Path $OutputDir "alert-log.txt"

$StartTime       = Get-Date
$EndTime         = $StartTime.AddHours($DurationHours)
$TotalSamples    = 0
$AlertCount      = 0
$CrashCount      = 0
$MaxWorkingSetMB = 0.0
$MinWorkingSetMB = [double]::MaxValue

# Hourly tracking for growth-rate computation
$HourlySnapshots = [System.Collections.Generic.List[PSCustomObject]]::new()
$LastHourStartMB = $null
$LastHourStart   = $StartTime

# CSV header
"Timestamp,ElapsedMinutes,WorkingSetMB,PagedMemoryMB,HandleCount,ThreadCount,Status" |
    Out-File -FilePath $CsvPath -Encoding UTF8

# Alert and report log header
$Header = @"
GlyphPDF Soak Test Monitor
===========================
Start      : $StartTime
Duration   : $DurationHours hours
End (sched): $EndTime
OutputDir  : $OutputDir
Process    : $ProcessName
Alert thresh: $LeakAlertMbPerHour MB/hour growth
Sample rate : every $SampleIntervalSec seconds

"@
$Header | Out-File -FilePath $ReportPath -Encoding UTF8
$Header | Write-Host

# ---------------------------------------------------------------------------
# Helper: get process memory snapshot
# ---------------------------------------------------------------------------
function Get-GlyphPdfMemory {
    $proc = Get-Process -Name $ProcessName -ErrorAction SilentlyContinue
    if ($null -eq $proc) {
        return $null
    }
    return [PSCustomObject]@{
        WorkingSetMB  = [math]::Round($proc.WorkingSet64 / 1MB, 2)
        PagedMemoryMB = [math]::Round($proc.PagedMemorySize64 / 1MB, 2)
        HandleCount   = $proc.HandleCount
        ThreadCount   = $proc.Threads.Count
    }
}

# ---------------------------------------------------------------------------
# Main sampling loop
# ---------------------------------------------------------------------------
Write-Host "Monitor started. Sampling every $SampleIntervalSec seconds for $DurationHours hours."
Write-Host "CSV log: $CsvPath"
Write-Host "Press Ctrl+C to stop early (partial results will be written)."
Write-Host ""

while ((Get-Date) -lt $EndTime) {
    $Now          = Get-Date
    $ElapsedMin   = [math]::Round(($Now - $StartTime).TotalMinutes, 1)
    $snapshot     = Get-GlyphPdfMemory

    if ($null -eq $snapshot) {
        # Process not found — either crashed or not yet started
        $CrashCount++
        $Status = "NOT_RUNNING"
        $WsMB   = 0.0
        Write-Host "[$Now] WARNING: $ProcessName not found (crash count: $CrashCount)" -ForegroundColor Yellow
        "$Now,CRASH_OR_NOT_RUNNING" | Add-Content -Path $AlertLog
        "$($Now.ToString('o')),$ElapsedMin,0,0,0,0,$Status" | Add-Content -Path $CsvPath
    } else {
        $WsMB   = $snapshot.WorkingSetMB
        $Status = "OK"

        # Track min/max
        if ($WsMB -gt $MaxWorkingSetMB) { $MaxWorkingSetMB = $WsMB }
        if ($WsMB -lt $MinWorkingSetMB) { $MinWorkingSetMB = $WsMB }

        # Write CSV row
        "$($Now.ToString('o')),$ElapsedMin,$WsMB,$($snapshot.PagedMemoryMB),$($snapshot.HandleCount),$($snapshot.ThreadCount),$Status" |
            Add-Content -Path $CsvPath

        # Console heartbeat every 10 samples (~10 minutes at 60s interval)
        if ($TotalSamples % 10 -eq 0) {
            Write-Host "[$($Now.ToString('HH:mm:ss'))] Elapsed: ${ElapsedMin}min | WS: ${WsMB} MB | Handles: $($snapshot.HandleCount)" -ForegroundColor Cyan
        }
    }

    $TotalSamples++

    # ---------------------------------------------------------------------------
    # Hourly growth-rate check (skip the first 2 hours — warm-up period)
    # ---------------------------------------------------------------------------
    $ElapsedHours = ($Now - $StartTime).TotalHours
    if ($ElapsedHours -ge 2) {
        $MinutesSinceHourStart = ($Now - $LastHourStart).TotalMinutes
        if ($MinutesSinceHourStart -ge 60) {
            if ($null -ne $LastHourStartMB -and $WsMB -gt 0) {
                $GrowthMB = $WsMB - $LastHourStartMB
                $HourlySnapshots.Add([PSCustomObject]@{
                    Hour      = [math]::Round($ElapsedHours, 1)
                    StartMB   = $LastHourStartMB
                    EndMB     = $WsMB
                    GrowthMB  = [math]::Round($GrowthMB, 2)
                })

                if ($GrowthMB -gt $LeakAlertMbPerHour) {
                    $AlertCount++
                    $AlertMsg = "[LEAK ALERT] Hour $([math]::Round($ElapsedHours,1)): WS grew ${GrowthMB} MB in last hour (threshold: $LeakAlertMbPerHour MB/hour)"
                    Write-Host $AlertMsg -ForegroundColor Red
                    $AlertMsg | Add-Content -Path $AlertLog
                }
            }
            $LastHourStart   = $Now
            $LastHourStartMB = $WsMB
        }
    } else {
        # Keep resetting the hourly baseline during warm-up so Hour 2 comparison is accurate
        $LastHourStart   = $Now
        $LastHourStartMB = $WsMB
    }

    Start-Sleep -Seconds $SampleIntervalSec
}

# ---------------------------------------------------------------------------
# Final report
# ---------------------------------------------------------------------------
$EndActual  = Get-Date
$ActualHours = [math]::Round(($EndActual - $StartTime).TotalHours, 2)

# Compute average hourly growth from the hourly snapshots (exclude warm-up)
$AvgGrowthMB = 0.0
if ($HourlySnapshots.Count -gt 0) {
    $AvgGrowthMB = [math]::Round(($HourlySnapshots | Measure-Object GrowthMB -Average).Average, 2)
}

# Read first and last valid working-set values from the CSV for trend line
$CsvRows = Import-Csv -Path $CsvPath | Where-Object { $_.Status -eq "OK" }
$FirstWsMB = if ($CsvRows.Count -gt 0) { [double]($CsvRows[0].WorkingSetMB) } else { 0 }
$LastWsMB  = if ($CsvRows.Count -gt 0) { [double]($CsvRows[-1].WorkingSetMB) } else { 0 }
$TotalGrowthMB = [math]::Round($LastWsMB - $FirstWsMB, 2)

$Report = @"

===========================================
SOAK TEST MONITOR — FINAL REPORT
===========================================
Start time         : $StartTime
End time (actual)  : $EndActual
Actual duration    : $ActualHours hours
Total samples      : $TotalSamples
Crash / not-found  : $CrashCount occurrences

MEMORY SUMMARY
--------------
First WS reading   : $FirstWsMB MB
Last WS reading    : $LastWsMB MB
Total WS growth    : $TotalGrowthMB MB over $ActualHours hours
Peak WS            : $MaxWorkingSetMB MB
Minimum WS         : $MinWorkingSetMB MB
Avg hourly growth  : $AvgGrowthMB MB/hour (warm-up first 2h excluded)

ALERTS
------
Total LEAK ALERTs  : $AlertCount
  (Threshold: $LeakAlertMbPerHour MB/hour; first 2h warm-up excluded)
See alert-log.txt for details.

HOURLY GROWTH TABLE (warm-up excluded)
---------------------------------------
Hour | Start MB | End MB | Growth MB
"@

foreach ($row in $HourlySnapshots) {
    $Report += "`n{0,4:F1} | {1,8:F2} | {2,6:F2} | {3,9:F2}" -f $row.Hour, $row.StartMB, $row.EndMB, $row.GrowthMB
}

$Report += @"


PASS / FAIL INDICATORS
-----------------------
$(if ($CrashCount -gt 0) { "FAIL — Application was not running at $CrashCount sample points (crash detected)" } else { "PASS — No crashes detected" })
$(if ($MaxWorkingSetMB -gt 500) { "FAIL — Peak working set $MaxWorkingSetMB MB exceeds 500 MB threshold" } else { "PASS — Peak working set $MaxWorkingSetMB MB is within 500 MB threshold" })
$(if ($AvgGrowthMB -gt 10) { "FAIL — Average hourly growth $AvgGrowthMB MB/hour exceeds 10 MB/hour threshold" } else { "PASS — Average hourly growth $AvgGrowthMB MB/hour is within 10 MB/hour threshold" })
$(if ($AlertCount -gt 0) { "WARNING — $AlertCount hourly leak alerts fired. Review alert-log.txt and run Dr. Memory for attribution." } else { "PASS — No hourly growth alerts" })

NOTE: Run ctest and check %TEMP%\GlyphPDF* after the soak to complete the full pass/fail assessment.
See soak-acceptance-criteria.md for the complete decision matrix.
===========================================
"@

$Report | Tee-Object -FilePath $ReportPath -Append | Write-Host

Write-Host ""
Write-Host "Monitor complete. Results in: $OutputDir" -ForegroundColor Green

# ---------------------------------------------------------------------------
# Dr. Memory invocation reminder
# ---------------------------------------------------------------------------
Write-Host ""
Write-Host "To run Dr. Memory leak attribution on a fresh GlyphPDF session:" -ForegroundColor Yellow
Write-Host '  & "C:\Program Files (x86)\Dr. Memory\bin\drmemory.exe" -leaks_only -logdir C:\SoakTest\soak-results\drmem -- "C:\Program Files\GlyphPDF\GlyphPDF.exe"' -ForegroundColor Yellow
Write-Host "(Run one manual operation cycle, then exit GlyphPDF. Dr. Memory writes the leak report.)" -ForegroundColor Yellow
