# SWEEP-W3-PERF measurement suite orchestrator (PowerShell 5.1 — no && / ternary).
#
# Drives the tools/perf drivers offscreen and records, for EVERY scenario block,
# the concurrent machine-load context (CPU %, build-process count, free RAM MiB)
# — the LOAD-HONESTY contract: loaded-box provisional numbers are never quoted
# as quiet-box baselines. Aggregates median/p95/min/max per metric.
#
# Usage (from the repo root):
#   powershell -File tools\perf\run_perf_suite.ps1 -BuildDir build-perf `
#     -OutDir build-perf\perf-results\loaded -Label loaded [-IterationsProfile smoke]
#
# Output: <OutDir>\raw\        one JSON file per scenario invocation
#         <OutDir>\summary.json
param(
    [string]$BuildDir = "build-perf",
    # Default run root is on C: — the D: volume (repo + msys64 + build trees)
    # runs at 100% capacity; fixtures (~125 MB) and transient op outputs do
    # not fit there. Pass an explicit -OutDir to override.
    [string]$OutDir = "",
    [string]$Label = "loaded",
    [ValidateSet("full", "smoke")]
    [string]$IterationsProfile = "full"
)

$ErrorActionPreference = "Stop"
$Repo = (Get-Location).Path
if (-not $OutDir) {
    $OutDir = Join-Path $env:SYSTEMDRIVE "perftmp\glyphpdf-perf\$Label"
}
$Exe = Join-Path $Repo $BuildDir
$Raw = Join-Path $OutDir "raw"
New-Item -ItemType Directory -Force -Path $Raw | Out-Null

# Child temp + op-output isolation: keep everything off the full D: volume.
$RunTmp = Join-Path $env:SYSTEMDRIVE "perftmp\tmp"
New-Item -ItemType Directory -Force -Path $RunTmp | Out-Null
$env:TMP = $RunTmp
$env:TEMP = $RunTmp
$env:GLYPHPDF_PERF_OUTDIR = Join-Path $OutDir "op-outputs"
New-Item -ItemType Directory -Force -Path $env:GLYPHPDF_PERF_OUTDIR | Out-Null

# ── load-context sampling ────────────────────────────────────────────────────
function Get-LoadContext {
    $cpu = $null
    try {
        $c = Get-CimInstance Win32_Processor -ErrorAction Stop | Select-Object -First 1
        $cpu = $c.LoadPercentage
    } catch { $cpu = $null }
    $names = @("g++", "cc1plus", "gcc", "cc1", "ninja", "cmake", "ctest")
    $buildProcs = 0
    foreach ($n in $names) {
        $p = Get-Process -Name $n -ErrorAction SilentlyContinue
        if ($p) { $buildProcs += ($p | Measure-Object).Count }
    }
    $freeMiB = [long]((Get-CimInstance Win32_OperatingSystem).FreePhysicalMemory / 1024)
    return @{
        cpuPercent     = $cpu
        buildProcesses = $buildProcs
        freeRamMiB     = $freeMiB
        at             = (Get-Date).ToString("o")
    }
}

function Get-Stats([double[]]$v) {
    if ($v.Count -eq 0) { return $null }
    $sorted = $v | Sort-Object
    $median = $sorted[[int][math]::Floor(($sorted.Count - 1) / 2)]
    # nearest-rank p95; for small N it approaches max (disclosed in the doc)
    $idx = [int][math]::Ceiling(0.95 * $sorted.Count) - 1
    if ($idx -lt 0) { $idx = 0 }
    return @{
        n      = $sorted.Count
        median = [math]::Round($median, 3)
        p95    = [math]::Round($sorted[$idx], 3)
        min    = [math]::Round($sorted[0], 3)
        max    = [math]::Round($sorted[-1], 3)
    }
}

function Add-Metric([string]$name, [double[]]$samples, [object]$extra,
                    [object]$loadBefore, [object]$loadAfter, [string]$jsonFile) {
    $st = Get-Stats $samples
    $script:metrics[$name] = @{
        stats      = $st
        samples_ms = $samples
        extra      = $extra
        loadBefore = $loadBefore
        loadAfter  = $loadAfter
        rawFile    = $jsonFile
    }
    Write-Host ("  {0}: median={1} p95={2} min={3} max={4} (n={5})" -f `
        $name, $st.median, $st.p95, $st.min, $st.max, $st.n)
}

# ── child env ────────────────────────────────────────────────────────────────
$env:QT_QPA_PLATFORM = "offscreen"

$results = [ordered]@{}
$results.phase = $Label
$results.startedAt = (Get-Date).ToString("o")
$results.machine = @{
    computerName = $env:COMPUTERNAME
    processors   = [int]$env:NUMBER_OF_PROCESSORS
    os           = (Get-CimInstance Win32_OperatingSystem).Caption
}
$results.iterationsProfile = $IterationsProfile
$metrics = [ordered]@{}

# ── fixtures ─────────────────────────────────────────────────────────────────
$FixtureDir = Join-Path $OutDir "fixtures"
$Small = Join-Path $FixtureDir "small-text-20p.pdf"
if (-not (Test-Path $Small)) {
    Write-Host "== generating fixtures"
    $fx = & (Join-Path $Exe "perf_fixtures.exe") $FixtureDir 2>&1 | Where-Object { $_ -like "{*" }
    if ($LASTEXITCODE -ne 0) { throw "fixture generation FAILED" }
    $fx | Select-Object -Last 1 | Set-Content -Encoding utf8 (Join-Path $Raw "fixtures.json")
    Write-Host ($fx | Select-Object -Last 1)
}
$Medium = Join-Path $FixtureDir "medium-20mb.pdf"
$Large = Join-Path $FixtureDir "large-100mb.pdf"
$CmpA = Join-Path $FixtureDir "compare-a-50p.pdf"
$CmpB = Join-Path $FixtureDir "compare-b-50p.pdf"
# Two byte-identical copies for the app-level open round-robin (alternating
# paths force a REAL load every iteration).
$OpenA = Join-Path $OutDir "open-copy-a.pdf"
$OpenB = Join-Path $OutDir "open-copy-b.pdf"
Copy-Item $Small $OpenA -Force
Copy-Item $Small $OpenB -Force

$p12 = Join-Path $Repo "tests\fixtures\signing\test_signer.p12"
if (-not (Test-Path $p12)) { throw "P12 fixture missing: $p12" }

$it = @{
    startup = 20; openEngineSmall = 20; openEngineMedium = 10; openEngineLarge = 3
    openApp = 20; redact = 20; ocrskip = 20; compare = 5
    convert = 10; sign = 10; save = 20; batch = 3
}
if ($IterationsProfile -eq "smoke") {
    $it = @{
        startup = 3; openEngineSmall = 3; openEngineMedium = 2; openEngineLarge = 1
        openApp = 3; redact = 3; ocrskip = 3; compare = 1
        convert = 2; sign = 2; save = 3; batch = 1
    }
}

# Runs a driver that prints exactly ONE JSON line; returns parsed json + load ctx.
function Invoke-Driver([string]$exeName, [string[]]$driverArgs, [string]$tag) {
    $loadBefore = Get-LoadContext
    $out = & (Join-Path $Exe $exeName) @driverArgs 2>&1 | Where-Object { $_ -like "{*" }
    $rc = $LASTEXITCODE
    $loadAfter = Get-LoadContext
    if ($rc -ne 0) { throw "driver $exeName $driverArgs failed rc=$rc" }
    $line = @($out) | Select-Object -Last 1
    if (-not $line) { throw "driver $exeName produced no JSON line" }
    $file = "$tag.json"
    $line | Set-Content -Encoding utf8 (Join-Path $Raw $file)
    return @{ json = ($line | ConvertFrom-Json); before = $loadBefore; after = $loadAfter; file = $file }
}

# Runs a driver that prints MULTIPLE JSON lines; returns all parsed + load ctx.
function Invoke-DriverMulti([string]$exeName, [string[]]$driverArgs, [string]$tag) {
    $loadBefore = Get-LoadContext
    $out = & (Join-Path $Exe $exeName) @driverArgs 2>&1 | Where-Object { $_ -like "{*" }
    $rc = $LASTEXITCODE
    $loadAfter = Get-LoadContext
    if ($rc -ne 0) { throw "driver $exeName $driverArgs failed rc=$rc" }
    $parsed = @()
    $i = 0
    foreach ($ln in @($out)) {
        $f = ("{0}-{1}.json" -f $tag, $i)
        $ln | Set-Content -Encoding utf8 (Join-Path $Raw $f)
        $parsed += ($ln | ConvertFrom-Json)
        $i++
    }
    return @{ parsed = $parsed; before = $loadBefore; after = $loadAfter }
}

function Add-OpenEngineMetrics([string]$pdf, [int]$n, [string]$sizeTag) {
    $r = Invoke-DriverMulti "perf_docops.exe" @("open-engine", $pdf, "$n") "open-engine-$sizeTag"
    $first = $r.parsed | Where-Object { $_.scenario -eq "open-engine-first" } | Select-Object -First 1
    $full = $r.parsed | Where-Object { $_.scenario -eq "open-engine-full-pagination" } | Select-Object -First 1
    Add-Metric "open-$sizeTag-first-page" [double[]]@($first.samples_ms) $first.extra $r.before $r.after "open-engine-$sizeTag-0.json"
    Add-Metric "paginate-$sizeTag-full" [double[]]@($full.samples_ms) $full.extra $r.before $r.after "open-engine-$sizeTag-1.json"
}

# ── startup: fresh process per run (external wall + internal samples) ────────
Write-Host "== startup ($($it.startup) spawns)"
{
    $wall = @(); $shown = @(); $context = @(); $ctor = @()
    for ($i = 0; $i -lt $it.startup; $i++) {
        $before = Get-LoadContext
        $sw = [System.Diagnostics.Stopwatch]::StartNew()
        $out = & (Join-Path $Exe "perf_startup.exe") --json 2>&1 | Where-Object { $_ -like "{*" }
        $sw.Stop()
        if ($LASTEXITCODE -ne 0) { throw "perf_startup rc=$LASTEXITCODE" }
        $wall += $sw.ElapsedMilliseconds
        $line = @($out) | Select-Object -Last 1
        $last = $line | ConvertFrom-Json
        $shown += [double]$last.shown_ms
        $context += [double]$last.context_ms
        $ctor += [double]$last.ctor_ms
        # first and last runs carry the full load context
        if ($i -eq 0 -or $i -eq ($it.startup - 1)) {
            $last | Add-Member -NotePropertyMembers @{
                loadBefore     = $before
                loadAfter      = (Get-LoadContext)
                externalWallMs = $sw.ElapsedMilliseconds
            }
        }
        $last | ConvertTo-Json -Depth 5 -Compress | Set-Content -Encoding utf8 `
            (Join-Path $Raw ("startup-run-{0}.json" -f $i))
    }
    Add-Metric "startup-external-wall" [double[]]$wall @{ note = "whole child process incl. loader" } $null $null "startup-runs.json"
    Add-Metric "startup-main-to-window-shown" [double[]]$shown @{ note = "internal, offscreen expose" } $null $null "startup-runs.json"
    Add-Metric "startup-createContext" [double[]]$context $null $null $null "startup-runs.json"
    Add-Metric "startup-mainWindow-ctor" [double[]]$ctor $null $null $null "startup-runs.json"
}.Invoke()

Write-Host "== doc opens (engine floor + full pagination)"
Add-OpenEngineMetrics $Small $it.openEngineSmall "small"
Add-OpenEngineMetrics $Medium $it.openEngineMedium "medium"
Add-OpenEngineMetrics $Large $it.openEngineLarge "large"

Write-Host "== app-level open (real MainWindow::openDocument)"
{
    $r = Invoke-Driver "perf_docops.exe" @("open-app", $OpenA, $OpenB, "$($it.openApp)") "open-app"
    Add-Metric "open-app-real-mainwindow" [double[]]@($r.json.samples_ms) $r.json.extra $r.before $r.after $r.file
}.Invoke()

Write-Host "== core ops"
{
    $r = Invoke-Driver "perf_docops.exe" @("redact", $Small, "$($it.redact)") "redact"
    Add-Metric "redact-apply" [double[]]@($r.json.samples_ms) $r.json.extra $r.before $r.after $r.file

    $r = Invoke-Driver "perf_docops.exe" @("ocrskip", $Small, "$($it.ocrskip)") "ocrskip"
    Add-Metric "ocr-skip-decision" [double[]]@($r.json.samples_ms) $r.json.extra $r.before $r.after $r.file

    $r = Invoke-Driver "perf_docops.exe" @("compare", $CmpA, $CmpB, "$($it.compare)") "compare"
    Add-Metric "compare-50p" [double[]]@($r.json.samples_ms) $r.json.extra $r.before $r.after $r.file

    $r = Invoke-Driver "perf_docops.exe" @("convert", $Small, "word", "$($it.convert)") "convert-word"
    Add-Metric "convert-export-docx" [double[]]@($r.json.samples_ms) $r.json.extra $r.before $r.after $r.file

    $r = Invoke-Driver "perf_docops.exe" @("convert", $Small, "excel", "$($it.convert)") "convert-excel"
    Add-Metric "convert-export-xlsx" [double[]]@($r.json.samples_ms) $r.json.extra $r.before $r.after $r.file

    $r = Invoke-Driver "perf_docops.exe" @("sign", $Small, $p12, "$($it.sign)") "sign"
    Add-Metric "sign-local-p12" [double[]]@($r.json.samples_ms) $r.json.extra $r.before $r.after $r.file

    $r = Invoke-Driver "perf_docops.exe" @("save", $Small, "$($it.save)") "save"
    Add-Metric "save-roundtrip" [double[]]@($r.json.samples_ms) $r.json.extra $r.before $r.after $r.file
}.Invoke()

Write-Host "== batch (files through the real preset pipeline)"
{
    $outRoot = Join-Path $OutDir "batch-out"
    $samples = @(); $extra = $null; $lb = $null; $la = $null; $f = "batch-total.json"
    for ($i = 0; $i -lt $it.batch; $i++) {
        $r = Invoke-Driver "perf_docops.exe" @("batch", (Join-Path $FixtureDir "batch-corpus"), $outRoot) "batch-run-$i"
        $samples += [double]($r.json.samples_ms[0])
        $extra = $r.json.extra; $lb = $r.before; $la = $r.after; $f = $r.file
    }
    Add-Metric "batch-preset-compress" $samples $extra $lb $la $f
}.Invoke()

Write-Host "== memory (fresh process, peak working set)"
{
    $r = Invoke-Driver "perf_docops.exe" @("mem-open", $Large) "mem-open-large"
    Add-Metric "mem-open-large" @(0) $r.json $r.before $r.after $r.file
    $r = Invoke-Driver "perf_docops.exe" @("mem-compare", $CmpA, $CmpB) "mem-compare-50"
    Add-Metric "mem-compare-50" @(0) $r.json $r.before $r.after $r.file
    $r = Invoke-Driver "perf_docops.exe" @("pagescale", $Small) "page-scale"
    Add-Metric "page-render-per-scale" @(0) $r.json $r.before $r.after $r.file
}.Invoke()

$results.finishedAt = (Get-Date).ToString("o")
$results.metrics = $metrics
$results | ConvertTo-Json -Depth 8 | Set-Content -Encoding utf8 (Join-Path $OutDir "summary.json")
Write-Host "== summary written: $(Join-Path $OutDir 'summary.json')"
