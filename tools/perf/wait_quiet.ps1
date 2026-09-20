# SWEEP-W3-PERF quiet-box gate (PowerShell 5.1).
#
# Blocks until the machine has been QUIET (< the process-count threshold) for
# the full quiet window -- the precondition for the FINAL baseline run (the
# load-honesty contract: only the quiet run is the baseline).
#
# Usage: powershell -File tools\perf\wait_quiet.ps1 [-QuietMinutes 10] [-MaxBuildProcs 1]
param(
    [int]$QuietMinutes = 10,
    [int]$MaxBuildProcs = 1
)

$ErrorActionPreference = "Stop"
$names = @("g++", "cc1plus", "gcc", "cc1", "ninja", "cmake", "ctest", "link")
$quietStart = $null
Write-Host "Waiting for quiet box: <$MaxBuildProcs build processes for $QuietMinutes minutes..."
while ($true) {
    $count = 0
    foreach ($n in $names) {
        $p = Get-Process -Name $n -ErrorAction SilentlyContinue
        if ($p) { $count += ($p | Measure-Object).Count }
    }
    $now = Get-Date
    if ($count -lt $MaxBuildProcs) {
        if ($null -eq $quietStart) {
            $quietStart = $now
            Write-Host ("[{0}] quiet period started (buildProcs={1})" -f $now.ToString("HH:mm:ss"), $count)
        }
        $quietFor = ($now - $quietStart).TotalMinutes
        Write-Host ("[{0}] quiet for {1:n1} min (buildProcs={2})" -f $now.ToString("HH:mm:ss"), $quietFor, $count)
        if ($quietFor -ge $QuietMinutes) {
            Write-Host "QUIET GATE PASSED"
            exit 0
        }
    } else {
        if ($null -ne $quietStart) {
            Write-Host ("[{0}] busy again (buildProcs={1}) -- quiet timer reset" -f $now.ToString("HH:mm:ss"), $count)
        }
        $quietStart = $null
    }
    Start-Sleep -Seconds 30
}
