# Regenerate docs/updates/latest.json from the built MSI's SHA-256 sidecar.
#
# Run AFTER packaging/build-msi.ps1 (which produces dist/*.msi.sha256), then
# commit docs/updates/latest.json to the GitHub Pages branch so the in-app
# UpdateChecker sees the new version.
#
# Usage:  powershell -ExecutionPolicy Bypass -File packaging\gen-update-manifest.ps1 -Version 1.3.1
param(
    [Parameter(Mandatory=$true)][string]$Version,
    [string]$MinVersion = '1.0.0',
    [string]$Channel    = 'latest'   # 'latest' (stable) or 'beta'
)
$ErrorActionPreference = 'Stop'

$ProjectRoot = Split-Path -Parent $PSScriptRoot
$msiName     = "GlyphPDF-$Version-x64.msi"
$sidecar     = Join-Path $ProjectRoot "dist\$msiName.sha256"
if (-not (Test-Path $sidecar)) { throw "Missing $sidecar - run build-msi.ps1 first." }

$sha = ((Get-Content $sidecar -Raw).Trim() -split '\s+')[0].ToUpper()

$manifest = [ordered]@{
    version      = $Version
    releaseDate  = (Get-Date -Format 'yyyy-MM-dd')
    downloadUrl  = "https://github.com/eliets2/glyph-pdf/releases/download/v$Version/$msiName"
    sha256       = $sha
    releaseNotes = "https://github.com/eliets2/glyph-pdf/releases/tag/v$Version"
    minVersion   = $MinVersion
}

$outDir = Join-Path $ProjectRoot 'docs\updates'
if (-not (Test-Path $outDir)) { New-Item -ItemType Directory -Path $outDir | Out-Null }
$outFile = Join-Path $outDir "$Channel.json"
$json = $manifest | ConvertTo-Json
# Write UTF-8 WITHOUT BOM — a BOM breaks QJsonDocument::fromJson in the app.
[System.IO.File]::WriteAllText($outFile, $json, (New-Object System.Text.UTF8Encoding($false)))

Write-Host "Wrote $outFile"
Write-Host "  version: $Version  sha256: $sha"
Write-Host "Commit it to the GitHub Pages branch to publish the update."
