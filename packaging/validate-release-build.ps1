#
#  GlyphPDF - INF02 release-build configuration validator.
#
#  Verifies that a build directory is a genuine dedicated Release
#  configuration of THIS source tree:
#    1. CMakeCache.txt exists and says CMAKE_BUILD_TYPE=Release and
#       GLYPHPDF_RELEASE_BUILD=ON (a cached Debug / feature-disabled build is
#       rejected, not silently reused - the INF02 finding);
#    2. with -RequireStamp: a release stamp recorded by build-msi.ps1 exists
#       and its SourceDir is THIS tree (wrong-source builds are rejected);
#    3. with -RequireCommitMatch (-SkipBuild path): the stamped commit equals
#       the current HEAD of the source tree, so signing/staging can never
#       ship binaries built from a different revision.
#
#  Usage:  powershell -ExecutionPolicy Bypass -File packaging\validate-release-build.ps1
#          -BuildDir build-rel [-ProjectRoot <dir>]
#          [-RequireStamp] [-RequireCommitMatch] [-ExpectedCommit <sha>]
#
#  Exit codes: 0 = validated; 1 = rejected (with the reason).
#
param(
    [Parameter(Mandatory = $true)][string]$BuildDir,
    [string]$ProjectRoot = '',
    [switch]$RequireStamp,
    [switch]$RequireCommitMatch,
    [string]$ExpectedCommit = ''
)
$ErrorActionPreference = 'Stop'

function Fail([string]$msg) {
    Write-Error "REJECTED (INF02): $msg"
    exit 1
}

if (-not $ProjectRoot) { $ProjectRoot = Split-Path -Parent $PSScriptRoot }
if (-not (Test-Path $ProjectRoot)) {
    Fail "ProjectRoot '$ProjectRoot' does not exist."
}
$ProjectRoot = (Resolve-Path $ProjectRoot).Path

$cache = Join-Path $BuildDir 'CMakeCache.txt'
if (-not (Test-Path $cache)) {
    Fail "no CMakeCache.txt in '$BuildDir' - not a configured build directory."
}
$cacheText = Get-Content $cache -Raw

if ($cacheText -notmatch 'CMAKE_BUILD_TYPE:STRING=Release') {
    $t = [regex]::Match($cacheText, 'CMAKE_BUILD_TYPE:STRING=(\S+)').Groups[1].Value
    Fail "cached CMAKE_BUILD_TYPE is '$t', not Release. Re-run the pipeline without -SkipBuild so the dedicated release configuration is (re)written."
}
if ($cacheText -notmatch 'GLYPHPDF_RELEASE_BUILD:BOOL=ON') {
    Fail "cached GLYPHPDF_RELEASE_BUILD is not ON - this build lacks the release feature gate (shipped-feature hard fail). Re-run the pipeline without -SkipBuild."
}

$stampPath = Join-Path $BuildDir 'glyphpdf-release-stamp.txt'
if ($RequireStamp -or $RequireCommitMatch -or (Test-Path $stampPath)) {
    if (-not (Test-Path $stampPath)) {
        Fail "no release stamp at '$stampPath' - this build directory was not produced/validated by build-msi.ps1. Re-run the pipeline without -SkipBuild."
    }
    $stamp = @{}
    Get-Content $stampPath | ForEach-Object {
        $i = $_.IndexOf('=')
        if ($i -gt 0) { $stamp[$_.Substring(0, $i)] = $_.Substring($i + 1) }
    }
    $stampSource = ''
    $src = ''
    if ($stamp.ContainsKey('SourceDir')) { $src = $stamp['SourceDir'] }
    if (-not $src -or -not (Test-Path $src)) {
        Fail "stamp SourceDir '$src' does not exist on this machine - not this tree's build. Re-run the pipeline without -SkipBuild."
    }
    $stampSource = (Resolve-Path $src).Path
    if ($stampSource -ne $ProjectRoot) {
        Fail "stamp SourceDir '$stampSource' is not this tree ('$ProjectRoot') - wrong-source build. Re-run the pipeline without -SkipBuild."
    }
    if ($RequireCommitMatch) {
        $head = ''
        try {
            $head = (& git -C $ProjectRoot rev-parse HEAD 2>$null)
            if ($LASTEXITCODE -ne 0) { $head = '' }
        } catch { $head = '' }
        if (-not $ExpectedCommit -and $head) { $ExpectedCommit = $head }
        if (-not $ExpectedCommit) {
            Fail "cannot determine the expected commit for -RequireCommitMatch (no git HEAD?) - refusing to validate."
        }
        if ($stamp['Commit'] -ne $ExpectedCommit) {
            Fail "stamp commit '$($stamp['Commit'])' != expected '$ExpectedCommit' - the cached release build is not from the revision being released. Re-run the pipeline without -SkipBuild."
        }
    }
    Write-Host ("Stamp OK: SourceDir={0} Commit={1} TestsPassed={2}" -f $stamp['SourceDir'], $stamp['Commit'], $stamp['TestsPassed'])
}

Write-Host "VALID (INF02): '$BuildDir' is a dedicated Release configuration with GLYPHPDF_RELEASE_BUILD=ON of '$ProjectRoot'."
exit 0
