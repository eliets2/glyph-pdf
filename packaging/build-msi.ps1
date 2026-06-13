# 
#  GlyphPDF - Full MSI build pipeline (PowerShell)
#
#  cmake build -> deploy.ps1 (full payload incl. ONNX models + tessdata)
#  -> WiX MSI with license UI -> SHA-256 checksum.
#
#  Usage:  powershell -ExecutionPolicy Bypass -File packaging\build-msi.ps1
#          [-SkipBuild]   reuse the existing build/ output
# 
param([switch]$SkipBuild, [switch]$MsiOnly)
$ErrorActionPreference = 'Stop'

$ProjectRoot = Split-Path -Parent $PSScriptRoot
$BuildDir    = Join-Path $ProjectRoot 'build'
$DeployDir   = Join-Path $ProjectRoot 'deploy'
$PackDir     = $PSScriptRoot
$OutputDir   = Join-Path $ProjectRoot 'dist'
$Version     = '1.0.0'
$MsiName     = "GlyphPDF-$Version-x64.msi"
$ZipName     = "GlyphPDF-$Version-x64-portable.zip"

$env:PATH = "C:\msys64\ucrt64\bin;$env:PATH"

Write-Host '========================================'
Write-Host " GlyphPDF release build pipeline  v$Version"
Write-Host '========================================'

#  1. Build 
if (-not $SkipBuild) {
    if (-not (Test-Path (Join-Path $BuildDir 'build.ninja'))) {
        Write-Host '[1/4] Configuring CMake...'
        cmake -S $ProjectRoot -B $BuildDir -G Ninja -DCMAKE_BUILD_TYPE=Release
        if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed.' }
    }
    Write-Host '[1/4] Building Release...'
    cmake --build $BuildDir --parallel 8
    if ($LASTEXITCODE -ne 0) { throw 'Build failed.' }
} else {
    Write-Host '[1/4] Skipping build (-SkipBuild).'
}

#  2. Deploy 
Write-Host '[2/4] Deploying payload...'
& powershell -ExecutionPolicy Bypass -File (Join-Path $PackDir 'deploy.ps1')
if ($LASTEXITCODE -ne 0) { throw 'Deploy failed.' }

#  3. WiX MSI 
Write-Host '[3/4] Building MSI with WiX...'
if (-not (Get-Command wix -ErrorAction SilentlyContinue)) {
    throw 'WiX CLI not found. Install: dotnet tool install -g wix'
}
# Ensure the UI extension is available (idempotent). Pin to the CLI's own
# version - an unpinned add pulls the latest extension (e.g. 7.x), which is
# incompatible with an older installed CLI.
$wixVer = ((& wix --version) -split '\+')[0]
$extList = & wix extension list -g 2>$null | Out-String
if ($extList -notmatch 'WixToolset\.UI\.wixext') {
    Write-Host "      adding WixToolset.UI.wixext/$wixVer (global)..."
    & wix extension add -g "WixToolset.UI.wixext/$wixVer"
    if ($LASTEXITCODE -ne 0) { throw 'Failed to add WixToolset.UI.wixext.' }
}

if (-not (Test-Path $OutputDir)) { New-Item -ItemType Directory -Path $OutputDir | Out-Null }
$msiPath = Join-Path $OutputDir $MsiName

Push-Location $PackDir
try {
    & wix build -src 'GlyphPDF.wxs' `
        -d "DeployDir=$DeployDir" `
        -ext WixToolset.UI.wixext `
        -arch x64 `
        -out $msiPath
    if ($LASTEXITCODE -ne 0) { throw 'WiX build failed.' }
} finally { Pop-Location }

#  4. MSI SHA-256 checksum
Write-Host '[4/5] Computing MSI SHA-256...'
$msiHash = (Get-FileHash $msiPath -Algorithm SHA256).Hash
"$msiHash  $MsiName" | Set-Content -Path "$msiPath.sha256" -Encoding Ascii

#  5. Portable ZIP (reuses deploy/ already built in step 2)
if (-not $MsiOnly) {
    Write-Host '[5/5] Creating portable ZIP...'
    & powershell -ExecutionPolicy Bypass -File (Join-Path $PackDir 'build-portable.ps1') -SkipDeploy
    if ($LASTEXITCODE -ne 0) { throw 'Portable ZIP build failed.' }
} else {
    Write-Host '[5/5] Skipping portable ZIP (-MsiOnly).'
}

$msiSize = (Get-Item $msiPath).Length / 1MB
$zipPath = Join-Path $OutputDir $ZipName
Write-Host '========================================'
Write-Host (' MSI:    {0}' -f $msiPath)
Write-Host (' Size:   {0:N1} MB' -f $msiSize)
Write-Host (' SHA256: {0}' -f $msiHash)
if (-not $MsiOnly -and (Test-Path $zipPath)) {
    $zipHash = (Get-Content "$zipPath.sha256").Split(' ')[0]
    $zipSize = (Get-Item $zipPath).Length / 1MB
    Write-Host (' ZIP:    {0}' -f $zipPath)
    Write-Host (' Size:   {0:N1} MB' -f $zipSize)
    Write-Host (' SHA256: {0}' -f $zipHash)
}
Write-Host '========================================'
Write-Host ' Update docs/release/release-notes-v1.0.0.md with both SHA-256 hashes.'
Write-Host ' Upload both dist/ artifacts to the GitHub Release.'
