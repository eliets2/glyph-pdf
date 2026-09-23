#
#  GlyphPDF  -  INF05 release define-gate.
#
#  Asserts, from REAL compiler-invocation evidence, that GLYPH_TESTING is not
#  defined in any production compilation of a release build. GLYPH_TESTING
#  guards an OCSP test-fixture bypass (SECFIX-1); it may exist only on
#  dedicated test executables, never on the shipped application/engine
#  targets.
#
#  Replaces the old release.yml step that grepped build/CMakeFiles/
#  CMakeOutput.log  -  a file that is not a record of target compile definitions
#  and whose absence was silently swallowed (-ErrorAction SilentlyContinue),
#  letting the check print PASS while inspecting NO data.
#
#  Evidence source: <BuildDir>/compile_commands.json (CMake generates it when
#  configured with -DCMAKE_EXPORT_COMPILE_COMMANDS=ON; the release pipeline
#  always passes that flag). A missing or empty evidence file is a FAILURE:
#  a gate without evidence must not pass.
#
#  Classification: a compilation is a TEST compilation iff its -o object path
#  is under CMakeFiles/Test*.dir (the repo's test executables are all named
#  Test*). Everything else  -  PdfWorkstation, pdfws_engines, pdfws_core,
#  pdfws_commands, pdfws_ui, pdfws_djot, docmodel, ...  -  is PRODUCTION and
#  must be free of GLYPH_TESTING.
#
#  Usage:  powershell -ExecutionPolicy Bypass -File packaging\check-release-defines.ps1
#          -BuildDir build-rel
#          [-CompileCommands <path>]   # override evidence file (for controls)
#
#  Exit codes: 0 = verified clean; 1 = missing evidence or forbidden define.
#
param(
    [Parameter(Mandatory = $true)][string]$BuildDir,
    [string]$CompileCommands = ''
)
$ErrorActionPreference = 'Stop'

if (-not $CompileCommands) {
    $CompileCommands = Join-Path $BuildDir 'compile_commands.json'
}
if (-not (Test-Path $CompileCommands)) {
    Write-Error "FAIL (INF05): evidence file '$CompileCommands' not found. Configure the release build with -DCMAKE_EXPORT_COMPILE_COMMANDS=ON; a define-gate without evidence must FAIL, not pass."
    exit 1
}

$raw = Get-Content $CompileCommands -Raw -Encoding UTF8
$entries = $raw | ConvertFrom-Json
if (-not $entries -or @($entries).Count -eq 0) {
    Write-Error "FAIL (INF05): '$CompileCommands' contains no compilation entries  -  no evidence, no pass."
    exit 1
}

$testDirRx    = 'CMakeFiles/Test[^/\\]*\.dir'
$appTargetRx  = 'CMakeFiles/PdfWorkstation\.dir'
$engineDirRx  = 'CMakeFiles/pdfws_engines\.dir'

$prodCount     = 0
$testCount     = 0
$prodWithMacro = New-Object System.Collections.Generic.List[string]
$haveApp       = $false
$haveEngine    = $false

foreach ($e in $entries) {
    $cmd = $e.command
    if (-not $cmd) { $cmd = ($e.arguments -join ' ') }
    $m = [regex]::Match($cmd, '-o\s+"?([^"\s]+\.(?:o|obj|res))"?')
    $outPath = if ($m.Success) { $m.Groups[1].Value } else { $e.file }
    # Windows CMake mixes path separators across entries (autogen vs main
    # targets) - normalise before matching, or Test* targets slip through as
    # production (caught by the real build-rel artifact control).
    $outPath = $outPath -replace '\\', '/'
    $isTest = $outPath -match $testDirRx
    if ($isTest) {
        $testCount++
        continue
    }
    $prodCount++
    if ($outPath -match $appTargetRx)  { $haveApp = $true }
    if ($outPath -match $engineDirRx)  { $haveEngine = $true }
    if ($cmd -match 'GLYPH_TESTING') {
        $prodWithMacro.Add($outPath)
    }
}

#  G17 (QUALITY-GATE-2026-09-09): BOTH required production targets must be
#  present in the evidence. The old condition (-and) only rejected evidence
#  missing BOTH, so a compile database containing just the application or
#  just the engine passed with half the shipped surface uninspected.
if (-not $haveApp -or -not $haveEngine) {
    $missing = @()
    if (-not $haveApp)    { $missing += 'PdfWorkstation (app)' }
    if (-not $haveEngine) { $missing += 'pdfws_engines (engine)' }
    Write-Error "FAIL (INF05): evidence file '$CompileCommands' is missing required production target compilations: $($missing -join ', ')  -  every shipped target must be inspected, so the gate cannot pass on partial evidence. Was this file produced by a FULL release configure?"
    exit 1
}
if ($prodWithMacro.Count -gt 0) {
    $list = ($prodWithMacro | Select-Object -First 10) -join "`n  "
    Write-Error "FAIL (INF05): GLYPH_TESTING is defined in $($prodWithMacro.Count) PRODUCTION compilation(s)  -  the OCSP test hook would ship in the release binary:`n  $list"
    exit 1
}

Write-Host "PASS (INF05): GLYPH_TESTING absent from all $prodCount production compilations (app target found: $haveApp, engine target found: $haveEngine); $testCount test-target compilations may keep test-only defines."
exit 0
