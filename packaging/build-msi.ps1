#
#  GlyphPDF - Full MSI build pipeline (PowerShell)
#
#  cmake build -> deploy.ps1 (full payload incl. ONNX models + tessdata)
#  -> sign EXE (Authenticode, RFC-3161) -> WiX MSI -> sign MSI
#  -> SHA-256 of the SIGNED MSI -> gate: refuse to publish unsigned artifact.
#
#  Signing configuration (via environment or parameters):
#    GLYPHPDF_SIGN_CERT_PFX  - path to PFX file (takes precedence over /a)
#    GLYPHPDF_SIGN_CERT_PASS - PFX password (if GLYPHPDF_SIGN_CERT_PFX is set)
#    GLYPHPDF_SIGN_THUMBPRINT - certificate thumbprint for /sha1 selection
#    GLYPHPDF_TIMESTAMP_URL  - RFC-3161 timestamp server URL
#                              (default: http://timestamp.digicert.com)
#
#  To skip signing (dev builds that will NEVER be published):
#    -SkipSigning
#  The pipeline will print a conspicuous warning and the artifact is marked
#  "UNSIGNED" - the PublishRelease step refuses to proceed.
#
#  To do a one-line swap to the real EV/OV cert:
#    set GLYPHPDF_SIGN_CERT_PFX=C:\certs\ev-cert.pfx
#    set GLYPHPDF_SIGN_CERT_PASS=secret
#  or via thumbprint (cert pre-imported into Windows CertStore):
#    set GLYPHPDF_SIGN_THUMBPRINT=AABBCC...
#
#  Usage:  powershell -ExecutionPolicy Bypass -File packaging\build-msi.ps1
#          [-Parallel N]  build parallelism (default 2  -  the ucrt64 linker
#                         runs out of memory at higher job counts)
#          [-SkipBuild]   reuse the existing build-rel/ output  -  ONLY after it
#                         was produced by this pipeline; the cached
#                         configuration and its stamp (source tree + commit +
#                         passing checks) are validated and a mismatch stops
#                         the pipeline
#          [-SkipSigning] dev/test only - NEVER publish unsigned artifacts
#
param(
    [switch]$SkipBuild,
    [switch]$MsiOnly,
    [switch]$SkipSigning,
    [int]$Parallel = 2
)
$ErrorActionPreference = 'Stop'

$ProjectRoot = Split-Path -Parent $PSScriptRoot
# INF02: a DEDICATED release build directory. The pipeline used to reuse
# <root>/build, configuring only when build.ninja was absent  -  an existing
# Debug (or feature-disabled) developer build was compiled and shipped while
# the log said "Building Release". build-rel is owned by this pipeline and
# always carries the release configuration (Release + GLYPHPDF_RELEASE_BUILD
# + GLYPHPDF_ENABLE_LTO), validated by packaging/validate-release-build.ps1
# before anything is staged or signed.
$BuildDir    = Join-Path $ProjectRoot 'build-rel'
$DeployDir   = Join-Path $ProjectRoot 'deploy'
$PackDir     = $PSScriptRoot
$OutputDir   = Join-Path $ProjectRoot 'dist'
$StampFile   = Join-Path $BuildDir 'glyphpdf-release-stamp.txt'

# INF03: ONE authoritative version  -  the root CMakeLists project() VERSION.
# The WiX package metadata, the MSI/ZIP names and the portable README all
# receive it from here; nothing hardcodes a release version any more (the
# portable child used to package the payload as 1.3.1 while this parent
# expected 1.3.2.3 and silently skipped the summary).
function Get-GlyphPdfVersion {
    $cmakeFile = Join-Path $ProjectRoot 'CMakeLists.txt'
    $m = Select-String -Path $cmakeFile -Pattern '^\s*project\(PdfWorkstation\s+VERSION\s+([0-9][0-9.]*)' |
         Select-Object -First 1
    if (-not $m) { throw "Cannot read the project VERSION from $cmakeFile." }
    return $m.Matches[0].Groups[1].Value
}
$Version     = Get-GlyphPdfVersion
$MsiName     = "GlyphPDF-$Version-x64.msi"
$ZipName     = "GlyphPDF-$Version-x64-portable.zip"

$env:PATH = "C:\msys64\ucrt64\bin;$env:PATH"

# -- Authenticode signing configuration --------------------------------------
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
                  '/d', "GlyphPDF - Open-Source PDF Workstation",
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
    # If signing was skipped, gate here - refuse to compute a "publish-ready" SHA.
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
            Write-Error "PUBLISH GATE: $Path failed Authenticode verify - artifact is NOT signed."
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

#  1. Build (dedicated, validated Release configuration  -  INF02)
if (-not $SkipBuild) {
    # ALWAYS configure with the release gate and the shipped-optimisation
    # settings  -  re-running configure on a cached wrong-configuration
    # directory RECONFIGURES it (last cache assignment wins); a fresh dir
    # starts clean. This is what makes a cached Debug build impossible here.
    Write-Host '[1/5] Configuring dedicated Release build (build-rel)...'
    & cmake -S $ProjectRoot -B $BuildDir -G Ninja `
        -DCMAKE_BUILD_TYPE=Release `
        -DGLYPHPDF_RELEASE_BUILD=ON `
        -DGLYPHPDF_ENABLE_LTO=ON `
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed.' }
    Write-Host ('[1/5] Building Release (-Parallel {0})...' -f $Parallel)
    & cmake --build $BuildDir --parallel $Parallel
    if ($LASTEXITCODE -ne 0) { throw 'Build failed.' }
} else {
    Write-Host '[1/5] Skipping build (-SkipBuild): validating the CACHED release configuration...'
    # -SkipBuild must never silently reuse an unvalidated developer build:
    # the cached directory must already be a validated Release configuration
    # of THIS source tree at THIS commit with passing checks.
    & powershell -NoProfile -ExecutionPolicy Bypass `
        -File (Join-Path $PackDir 'validate-release-build.ps1') `
        -BuildDir $BuildDir -ProjectRoot $ProjectRoot -RequireStamp -RequireCommitMatch
    if ($LASTEXITCODE -ne 0) { throw 'Cached build-rel configuration is not a validated Release build of this tree - refusing to continue (run without -SkipBuild).' }
    Write-Host '[1/5] Cached build-rel validated.'
}

#  1b. INF02: the release checks run HERE, before anything is staged or signed.
#  A failing test must stop the pipeline before artifacts are published.
Write-Host '[1b/5] Staging runtime DLLs + running release checks (ctest)...'
& cmake --build $BuildDir --target stage_runtime_dlls
if ($LASTEXITCODE -ne 0) { throw 'Runtime DLL staging failed.' }
$env:QT_QPA_PLATFORM = 'offscreen'
& ctest --test-dir $BuildDir --output-on-failure
if ($LASTEXITCODE -ne 0) {
    throw 'Release checks (ctest) FAILED - refusing to deploy, sign or publish anything.'
}

#  1c. INF05/INF02: confirm from real compile evidence that GLYPH_TESTING is
#  not defined in any production compilation of the artifact about to ship.
Write-Host '[1c/5] Asserting GLYPH_TESTING absent from production compilations...'
& powershell -NoProfile -ExecutionPolicy Bypass `
    -File (Join-Path $PackDir 'check-release-defines.ps1') -BuildDir $BuildDir
if ($LASTEXITCODE -ne 0) { throw 'Production define gate failed - refusing to continue.' }

#  1d. INF02: record the validated build identity (source tree + commit).
$commit = ''
try {
    $commit = (& git -C $ProjectRoot rev-parse HEAD 2>$null)
    if ($LASTEXITCODE -ne 0) { $commit = '' }
} catch { $commit = '' }
if (-not $commit) { $commit = 'no-git' }
@"
SourceDir=$ProjectRoot
Commit=$commit
BuildType=Release
ReleaseBuild=ON
LTO=ON
TestsPassed=1
StampTime=$(Get-Date -Format o)
"@ | Set-Content -Path $StampFile -Encoding Ascii
Write-Host ('[1d/5] Release build identity stamped: commit {0}' -f $commit)

#  2. Deploy (the actual release build directory is passed through  -  INF02:
#  deploy.ps1 must not infer build/ independently)
Write-Host '[2/5] Deploying payload...'
& powershell -ExecutionPolicy Bypass -File (Join-Path $PackDir 'deploy.ps1') -BuildDir $BuildDir
if ($LASTEXITCODE -ne 0) { throw 'Deploy failed.' }

#  3. Sign EXE BEFORE wix build (so the signed EXE is embedded in the MSI)
$exeInDeploy = Join-Path $DeployDir 'GlyphPDF.exe'
if (-not $SkipSigning) {
    Write-Host '[3/5] Signing GlyphPDF.exe (Authenticode, RFC-3161 timestamp)...'
    if (-not (Test-Path $exeInDeploy)) {
        throw "GlyphPDF.exe not found in deploy/ ($exeInDeploy) - run deploy.ps1 first."
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
        -d "GlyphPDFVersion=$Version" `
        -ext WixToolset.UI.wixext `
        -ext WixToolset.Util.wixext `
        -arch x64 `
        -out $msiPath
    if ($LASTEXITCODE -ne 0) { throw 'WiX build failed.' }
} finally { Pop-Location }

# INF03: the expected MSI must exist  -  wix exiting 0 without the artifact is a
# pipeline failure, not a summary footnote.
if (-not (Test-Path $msiPath)) {
    throw "INF03: expected MSI '$msiPath' was not produced."
}

#  4b. Sign MSI AFTER wix build
if (-not $SkipSigning) {
    Write-Host '[4b] Signing MSI (Authenticode, RFC-3161 timestamp)...'
    Invoke-SignArtifact -Path $msiPath -Description 'GlyphPDF installer (MSI)'
} else {
    Write-Host '[4b] Skipping MSI signing (-SkipSigning).'
}

#  PUBLISH GATE: refuse to compute/publish SHA-256 for an unsigned artifact
Assert-Signed -Path $msiPath

#  5. MSI SHA-256 - computed AFTER signing (so the hash is of the signed file)
Write-Host '[5/5] Computing MSI SHA-256 (of signed artifact)...'
$msiHash = (Get-FileHash $msiPath -Algorithm SHA256).Hash
"$msiHash  $MsiName" | Set-Content -Path "$msiPath.sha256" -Encoding Ascii

#  5b. Portable ZIP (reuses deploy/ already built in step 2)
if (-not $MsiOnly) {
    Write-Host '[5b] Creating portable ZIP...'
    # INF03: pass the single authoritative version down; the child packages
    # and names everything with it and verifies its own output.
    & powershell -ExecutionPolicy Bypass -File (Join-Path $PackDir 'build-portable.ps1') -SkipDeploy -Version $Version
    if ($LASTEXITCODE -ne 0) { throw 'Portable ZIP build failed.' }
    # INF03: the archive the parent expects must exist and match the version  - 
    # the old parent silently omitted the ZIP summary when the child produced
    # a differently-versioned archive.
    $zipPath = Join-Path $OutputDir $ZipName
    if (-not (Test-Path $zipPath) -or -not (Test-Path "$zipPath.sha256")) {
        throw "INF03: expected portable archive '$zipPath' (+.sha256) missing after build-portable.ps1  -  versions are out of sync."
    }
} else {
    Write-Host '[5b] Skipping portable ZIP (-MsiOnly).'
}

$msiSize = (Get-Item $msiPath).Length / 1MB
$zipPath = Join-Path $OutputDir $ZipName
Write-Host '========================================'
Write-Host (' MSI:    {0}' -f $msiPath)
Write-Host (' Size:   {0:N1} MB' -f $msiSize)
Write-Host (' SHA256: {0}  (of SIGNED artifact)' -f $msiHash)
if (-not $MsiOnly) {
    # INF03: existence is enforced above; the summary reports the archive
    # actually produced for THIS version.
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
