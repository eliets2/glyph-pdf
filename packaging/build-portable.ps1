#
#  GlyphPDF — Portable ZIP build pipeline (PowerShell)
#
#  Produces a self-contained deploy/ directory via deploy.ps1 then zips it
#  into GlyphPDF-<Version>-x64-portable.zip.  No installation required —
#  users unzip anywhere and run GlyphPDF.exe directly.
#
#  INF03: the version is no longer hardcoded here. It comes from the caller
#  (build-msi.ps1 passes the pipeline's single authoritative version read
#  from the root CMakeLists project() VERSION); when run standalone without
#  -Version it is read from that same source. Every produced artifact —
#  archive name, README banner, SHA-256 sidecar — carries that version, and
#  the pipeline fails if the expected ZIP is not produced.
#
#  Usage:  powershell -ExecutionPolicy Bypass -File packaging\build-portable.ps1
#          [-Version 1.3.2.3]  override the version (normally passed by build-msi.ps1)
#          [-SkipDeploy]       reuse existing deploy/ (deploy.ps1 already ran)
#
param(
    [string]$Version,
    [switch]$SkipDeploy
)
$ErrorActionPreference = 'Stop'

$ProjectRoot = Split-Path -Parent $PSScriptRoot
$DeployDir   = Join-Path $ProjectRoot 'deploy'
$PackDir     = $PSScriptRoot
$OutputDir   = Join-Path $ProjectRoot 'dist'

# INF03: one authoritative version — the root CMakeLists project() VERSION.
function Get-GlyphPdfVersion {
    $cmakeFile = Join-Path $ProjectRoot 'CMakeLists.txt'
    $m = Select-String -Path $cmakeFile -Pattern '^\s*project\(PdfWorkstation\s+VERSION\s+([0-9][0-9.]*)' |
         Select-Object -First 1
    if (-not $m) { throw "Cannot read the project VERSION from $cmakeFile." }
    return $m.Matches[0].Groups[1].Value
}
if (-not $Version) { $Version = Get-GlyphPdfVersion }

$ZipName = "GlyphPDF-$Version-x64-portable.zip"

Write-Host '========================================'
Write-Host " GlyphPDF portable build  v$Version"
Write-Host '========================================'

#  1. Deploy (full payload incl. VC++ runtime, models, tessdata)
if (-not $SkipDeploy) {
    Write-Host '[1/3] Running deploy pipeline...'
    & powershell -ExecutionPolicy Bypass -File (Join-Path $PackDir 'deploy.ps1')
    if ($LASTEXITCODE -ne 0) { throw 'deploy.ps1 failed.' }
} else {
    if (-not (Test-Path $DeployDir)) { throw "deploy/ missing and -SkipDeploy was set." }
    Write-Host '[1/3] Skipping deploy (-SkipDeploy).'
}

#  2. Write portable README into deploy/
Write-Host '[2/3] Writing portable README...'
@"
GlyphPDF $Version — Portable Edition
=====================================
No installation required.

HOW TO USE
  1. Unzip this folder to any location (e.g. C:\GlyphPDF or a USB drive).
  2. Double-click GlyphPDF.exe to launch.
  3. Optional: pin GlyphPDF.exe to your taskbar or Start menu.

REQUIREMENTS
  Windows 10 (version 1607 or later) or Windows 11, x64.
  4 GB RAM recommended for OCR on large documents.

PRIVACY
  GlyphPDF processes documents entirely on your machine.
  No telemetry. No cloud. Documents never leave your device.

LICENSES
  See LICENSE.txt (MIT) and LICENSE-3RD-PARTY.md for third-party notices.

SUPPORT
  https://github.com/eliets2/glyph-pdf/issues
"@ | Set-Content -Path (Join-Path $DeployDir 'README-PORTABLE.txt') -Encoding UTF8

#  3. Create ZIP
Write-Host '[3/3] Creating portable ZIP...'
if (-not (Test-Path $OutputDir)) { New-Item -ItemType Directory -Path $OutputDir | Out-Null }
$zipPath = Join-Path $OutputDir $ZipName

if (Test-Path $zipPath) { Remove-Item $zipPath -Force }
Compress-Archive -Path "$DeployDir\*" -DestinationPath $zipPath -CompressionLevel Optimal

#  INF03: the expected portable output must exist and be non-empty — a
#  missing archive must FAIL this pipeline, not pass silently.
if (-not (Test-Path $zipPath) -or (Get-Item $zipPath).Length -eq 0) {
    throw "INF03: expected portable archive '$zipPath' was not produced (or is empty)."
}

$hash = (Get-FileHash $zipPath -Algorithm SHA256).Hash
"$hash  $ZipName" | Set-Content -Path "$zipPath.sha256" -Encoding Ascii

$zipSize = (Get-Item $zipPath).Length / 1MB
Write-Host '========================================'
Write-Host (' ZIP:    {0}' -f $zipPath)
Write-Host (' Size:   {0:N1} MB' -f $zipSize)
Write-Host (' SHA256: {0}' -f $hash)
Write-Host (' Version: {0} (source: CMakeLists project VERSION{1})' -f $Version, $(if ($Version -eq (Get-GlyphPdfVersion)) { '' } else { ' - OVERRIDDEN' }))
Write-Host '========================================'
