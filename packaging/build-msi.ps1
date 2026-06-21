#
#  GlyphPDF - Full MSI build pipeline (PowerShell)
#
#  cmake build -> deploy.ps1 (full payload incl. ONNX models + tessdata)
#  -> sign EXE (Authenticode, RFC-3161) -> WiX MSI -> sign MSI
#  -> SHA-256 of the SIGNED MSI -> gate: refuse to publish unsigned artifact.
#
#  Signing configuration (via environment or parameters):
#    GLYPHPDF_SIGN_CERT_PFX  — path to PFX file (takes precedence over /a)
#    GLYPHPDF_SIGN_CERT_PASS — PFX password (if GLYPHPDF_SIGN_CERT_PFX is set)
#    GLYPHPDF_SIGN_THUMBPRINT — certificate thumbprint for /sha1 selection
#    GLYPHPDF_TIMESTAMP_URL  — RFC-3161 timestamp server URL
#                              (default: http://timestamp.digicert.com)
#
#  To skip signing (dev builds that will NEVER be published):
#    -SkipSigning
#  The pipeline will print a conspicuous warning and the artifact is marked
#  "UNSIGNED" — the PublishRelease step refuses to proceed.
#
#  To do a one-line swap to the real EV/OV cert:
#    set GLYPHPDF_SIGN_CERT_PFX=C:\certs\ev-cert.pfx
#    set GLYPHPDF_SIGN_CERT_PASS=secret
#  or via thumbprint (cert pre-imported into Windows CertStore):
#    set GLYPHPDF_SIGN_THUMBPRINT=AABBCC...
#
#  Usage:  powershell -ExecutionPolicy Bypass -File packaging\build-msi.ps1
#          [-SkipBuild]   reuse the existing build/ output
#          [-SkipSigning] dev/test only — NEVER publish unsigned artifacts
#
param(
    [switch]$SkipBuild,
    [switch]$MsiOnly,
    [switch]$SkipSigning
)
$ErrorActionPreference = 'Stop'

$ProjectRoot = Split-Path -Parent $PSScriptRoot
$BuildDir    = Join-Path $ProjectRoot 'build'
$DeployDir   = Join-Path $ProjectRoot 'deploy'
$PackDir     = $PSScriptRoot
$OutputDir   = Join-Path $ProjectRoot 'dist'
$Version     = '1.3.2'
$MsiName     = "GlyphPDF-$Version-x64.msi"
$ZipName     = "GlyphPDF-$Version-x64-portable.zip"

$env:PATH = "C:\msys64\ucrt64\bin;$env:PATH"

# ── Authenticode signing configuration ──────────────────────────────────────
$TimestampUrl = if ($env:GLYPHPDF_TIMESTAMP_URL) { $env:GLYPHPDF_TIMESTAMP_URL }
                else { 'http://timestamp.digicert.com' }

# Locate signtool.exe from Windows SDK (10.0 or later)
function Get-SignTool {
    # 1. In PATH already
    $st = Get-Command signtool.exe -ErrorAction SilentlyContinue
    if ($st) { return $st.Source }
    # 2. Standard Windows Kits locations
    $roots = @(
        "${env:ProgramFiles(x86)}\Windows Kits\10\bin",
        "${env:ProgramFiles}\Windows Kits\10\bin",
        "${env:ProgramFiles(x86)}\Windows Kits\11\bin"
    )
    foreach ($root in $roots) {
        if (Test-Path $root) {
            $found = Get-ChildItem $root -Recurse -Filter signtool.exe -ErrorAction SilentlyContinue |
                     Sort-Object -Property FullName -Descending |
                     Select-Object -First 1
            if ($found) { return $found.FullName }
        }
    }
    return $null
}

function Invoke-SignArtifact {
    param([string]$Path, [string]$Description)

    $signtool = Get-SignTool
    if (-not $signtool) {
        throw "signtool.exe not found. Install Windows SDK (10.0+) or add signtool to PATH."
    }

    # Build signtool arguments
    $signArgs = @('sign', '/fd', 'sha256', '/td', 'sha256',
                  '/tr', $TimestampUrl,
                  '/d', "GlyphPDF — Open-Source PDF Workstation",
                  '/du', 'https://github.com/eliets2/glyph-pdf')

    if ($env:GLYPHPDF_SIGN_CERT_PFX) {
        if (-not (Test-Path $env:GLYPHPDF_SIGN_CERT_PFX)) {
            throw "GLYPHPDF_SIGN_CERT_PFX points to a non-existent file: $env:GLYPHPDF_SIGN_CERT_PFX"
        }
        $signArgs += '/f'
        $signArgs += $env:GLYPHPDF_SIGN_CERT_PFX
        if ($env:GLYPHPDF_SIGN_CERT_PASS) {
            $signArgs += '/p'
            $signArgs += $env:GLYPHPDF_SIGN_CERT_PASS
        }
    } elseif ($env:GLYPHPDF_SIGN_THUMBPRINT) {
        $signArgs += '/sha1'
        $signArgs += $env:GLYPHPDF_SIGN_THUMBPRINT
    } else {
        # Auto-select best cert from Windows CertStore (placeholder / dev cert path)
        $signArgs += '/a'
    }

    $signArgs += $Path

    Write-Host "      Signing: $([System.IO.Path]::GetFileName($Path))"
    Write-Host "      signtool: $signtool"
    & $signtool @signArgs
    if ($LASTEXITCODE -ne 0) {
        throw "signtool failed for: $Path (exit $LASTEXITCODE)"
    }

    # Verify the signature was written with RFC-3161 timestamp
    Write-Host "      Verifying signature + timestamp..."
    & $signtool verify /pa /v $Path 2>&1 | ForEach-Object {
        if ($_ -match 'timestamp|successfully verified|signed') { Write-Host "      $_" }
    }
    if ($LASTEXITCODE -ne 0) {
        throw "Signature verification failed for: $Path"
    }
    Write-Host "      OK: $([System.IO.Path]::GetFileName($Path)) signed + timestamp verified."
}

function Assert-Signed {
    param([string]$Path)
    # If signing was skipped, gate here — refuse to compute a "publish-ready" SHA.
    if ($script:Unsigned) {
        $name = [System.IO.Path]::GetFileName($Path)
        Write-Error @"
==========================================================================
PUBLISH GATE: $name is UNSIGNED.
-SkipSigning was passed or signing failed. This artifact MUST NOT be
published or submitted to winget/chocolatey/scoop. SHA-256 of an unsigned
MSI MUST NOT appear in any release manifest.

To publish: re-run build-msi.ps1 WITHOUT -SkipSigning, with a valid
code-signing cert configured via GLYPHPDF_SIGN_CERT_PFX /
GLYPHPDF_SIGN_THUMBPRINT.
==========================================================================
"@
        exit 1
    }
    # Paranoia check: verify signtool reports the file as signed
    $signtool = Get-SignTool
    if ($signtool) {
        & $signtool verify /pa $Path 2>$null
        if ($LASTEXITCODE -ne 0) {
            Write-Error "PUBLISH GATE: $Path failed Authenticode verify — artifact is NOT signed."
            exit 1
        }
    }
}

Write-Host '========================================'
Write-Host " GlyphPDF release build pipeline  v$Version"
Write-Host '========================================'

if ($SkipSigning) {
    Write-Warning '!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!'
    Write-Warning '!! -SkipSigning is set. THIS ARTIFACT MUST NEVER BE PUBLISHED.   !!'
    Write-Warning '!! The publish gate (Assert-Signed) will HARD-FAIL before SHA-256 !!'
    Write-Warning '!! is computed if you attempt to publish without signing.         !!'
    Write-Warning '!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!'
    $script:Unsigned = $true
} else {
    $script:Unsigned = $false
}

#  1. Build
if (-not $SkipBuild) {
    if (-not (Test-Path (Join-Path $BuildDir 'build.ninja'))) {
        Write-Host '[1/5] Configuring CMake...'
        cmake -S $ProjectRoot -B $BuildDir -G Ninja -DCMAKE_BUILD_TYPE=Release
        if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed.' }
    }
    Write-Host '[1/5] Building Release...'
    cmake --build $BuildDir --parallel 8
    if ($LASTEXITCODE -ne 0) { throw 'Build failed.' }
} else {
    Write-Host '[1/5] Skipping build (-SkipBuild).'
}

#  2. Deploy
Write-Host '[2/5] Deploying payload...'
& powershell -ExecutionPolicy Bypass -File (Join-Path $PackDir 'deploy.ps1')
if ($LASTEXITCODE -ne 0) { throw 'Deploy failed.' }

#  3. Sign EXE BEFORE wix build (so the signed EXE is embedded in the MSI)
$exeInDeploy = Join-Path $DeployDir 'GlyphPDF.exe'
if (-not $SkipSigning) {
    Write-Host '[3/5] Signing GlyphPDF.exe (Authenticode, RFC-3161 timestamp)...'
    if (-not (Test-Path $exeInDeploy)) {
        throw "GlyphPDF.exe not found in deploy/ ($exeInDeploy) — run deploy.ps1 first."
    }
    Invoke-SignArtifact -Path $exeInDeploy -Description 'GlyphPDF executable'
} else {
    Write-Host '[3/5] Skipping EXE signing (-SkipSigning).'
}

#  4. WiX MSI (built AFTER signing the EXE so the signed binary is packaged)
Write-Host '[4/5] Building MSI with WiX...'
if (-not (Get-Command wix -ErrorAction SilentlyContinue)) {
    throw 'WiX CLI not found. Install: dotnet tool install -g wix'
}
# Ensure the UI extension is available (idempotent). Pin to the CLI's own
# version - an unpinned add pulls the latest extension (e.g. 7.x), which is
# incompatible with an older installed CLI.
$wixVer = ((& wix --version) -split '\+')[0]
$extList = & wix extension list -g 2>$null | Out-String
foreach ($ext in @('WixToolset.UI.wixext', 'WixToolset.Util.wixext')) {
    if ($extList -notmatch [regex]::Escape($ext)) {
        Write-Host "      adding $ext/$wixVer (global)..."
        & wix extension add -g "$ext/$wixVer"
        if ($LASTEXITCODE -ne 0) { throw "Failed to add $ext." }
    }
}

if (-not (Test-Path $OutputDir)) { New-Item -ItemType Directory -Path $OutputDir | Out-Null }
$msiPath = Join-Path $OutputDir $MsiName

Push-Location $PackDir
try {
    & wix build -src 'GlyphPDF.wxs' `
        -d "DeployDir=$DeployDir" `
        -ext WixToolset.UI.wixext `
        -ext WixToolset.Util.wixext `
        -arch x64 `
        -out $msiPath
    if ($LASTEXITCODE -ne 0) { throw 'WiX build failed.' }
} finally { Pop-Location }

#  4b. Sign MSI AFTER wix build
if (-not $SkipSigning) {
    Write-Host '[4b] Signing MSI (Authenticode, RFC-3161 timestamp)...'
    Invoke-SignArtifact -Path $msiPath -Description 'GlyphPDF installer (MSI)'
} else {
    Write-Host '[4b] Skipping MSI signing (-SkipSigning).'
}

#  PUBLISH GATE: refuse to compute/publish SHA-256 for an unsigned artifact
Assert-Signed -Path $msiPath

#  5. MSI SHA-256 — computed AFTER signing (so the hash is of the signed file)
Write-Host '[5/5] Computing MSI SHA-256 (of signed artifact)...'
$msiHash = (Get-FileHash $msiPath -Algorithm SHA256).Hash
"$msiHash  $MsiName" | Set-Content -Path "$msiPath.sha256" -Encoding Ascii

#  5b. Portable ZIP (reuses deploy/ already built in step 2)
if (-not $MsiOnly) {
    Write-Host '[5b] Creating portable ZIP...'
    & powershell -ExecutionPolicy Bypass -File (Join-Path $PackDir 'build-portable.ps1') -SkipDeploy
    if ($LASTEXITCODE -ne 0) { throw 'Portable ZIP build failed.' }
} else {
    Write-Host '[5b] Skipping portable ZIP (-MsiOnly).'
}

$msiSize = (Get-Item $msiPath).Length / 1MB
$zipPath = Join-Path $OutputDir $ZipName
Write-Host '========================================'
Write-Host (' MSI:    {0}' -f $msiPath)
Write-Host (' Size:   {0:N1} MB' -f $msiSize)
Write-Host (' SHA256: {0}  (of SIGNED artifact)' -f $msiHash)
if (-not $MsiOnly -and (Test-Path $zipPath)) {
    $zipHash = (Get-Content "$zipPath.sha256").Split(' ')[0]
    $zipSize = (Get-Item $zipPath).Length / 1MB
    Write-Host (' ZIP:    {0}' -f $zipPath)
    Write-Host (' Size:   {0:N1} MB' -f $zipSize)
    Write-Host (' SHA256: {0}' -f $zipHash)
}
Write-Host '========================================'
Write-Host " Update docs/release/release-notes-v$Version.md with both SHA-256 hashes."
Write-Host ' Upload both dist/ artifacts to the GitHub Release.'
Write-Host ' Then update packaging/winget/Glyph.GlyphPDF.installer.yaml InstallerSha256.'
