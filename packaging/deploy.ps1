# 
#  GlyphPDF - Canonical deployment script (PowerShell)
#
#  Produces a complete, self-contained deploy/ directory ready for MSI
#  packaging. Replaces the hand-maintained DLL lists in deploy-qt.bat with an
#  automatic dependency closure computed via objdump, so new dependencies can
#  never be silently missing from the installer.
#
#  Usage:  powershell -ExecutionPolicy Bypass -File packaging\deploy.ps1
# 
$ErrorActionPreference = 'Stop'

$ProjectRoot = Split-Path -Parent $PSScriptRoot
$BuildDir    = Join-Path $ProjectRoot 'build'
$DeployDir   = Join-Path $ProjectRoot 'deploy'
$PackDir     = $PSScriptRoot
$Msys2Bin    = 'C:\msys64\ucrt64\bin'

Write-Host '========================================'
Write-Host ' GlyphPDF deploy pipeline'
Write-Host '========================================'

#  1. Validate inputs 
$exeSrc = Join-Path $BuildDir 'PdfWorkstation.exe'
if (-not (Test-Path $exeSrc)) {
    throw "PdfWorkstation.exe not found in $BuildDir - run cmake --build build first."
}
if (-not (Test-Path (Join-Path $Msys2Bin 'objdump.exe'))) {
    throw "objdump.exe not found in $Msys2Bin - MSYS2 ucrt64 toolchain required."
}

#  2. Clean deploy dir 
if (Test-Path $DeployDir) { Remove-Item -Recurse -Force $DeployDir }
New-Item -ItemType Directory -Path $DeployDir | Out-Null

#  3. Main executable (installed name: GlyphPDF.exe) 
Write-Host '[1/8] Copying GlyphPDF.exe...'
Copy-Item $exeSrc (Join-Path $DeployDir 'GlyphPDF.exe')

#  4. Qt runtime via windeployqt6 
Write-Host '[2/8] Running windeployqt6...'
& (Join-Path $Msys2Bin 'windeployqt6.exe') `
    --release --no-translations --no-system-d3d-compiler `
    --no-opengl-sw --no-quick-import `
    (Join-Path $DeployDir 'GlyphPDF.exe')
if ($LASTEXITCODE -ne 0) { throw 'windeployqt6 failed.' }

#  5. Third-party engine binaries
Write-Host '[3/8] Copying engine binaries (PDFium, ONNX Runtime, PoDoFo)...'
$pdfiumCandidates = @(
    (Join-Path $BuildDir 'pdfium.dll'),
    (Join-Path $ProjectRoot 'third_party\pdfium\bin\pdfium.dll')
)
$pdfium = $pdfiumCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if ($pdfium) { Copy-Item $pdfium $DeployDir }
else { throw 'pdfium.dll not found (checked build/ and third_party/pdfium/bin/) - rendering engine missing.' }

$onnxLib = Join-Path $ProjectRoot 'onnxruntime-win-x64-1.17.3\lib'
foreach ($f in 'onnxruntime.dll', 'onnxruntime_providers_shared.dll') {
    $p = Join-Path $onnxLib $f
    if (Test-Path $p) { Copy-Item $p $DeployDir }
    else { throw "$f not found in $onnxLib - OCR inference runtime missing." }
}

# Stage vendored libpodofo.dll (1.1.0) BEFORE the objdump closure so the
# closure's already-present guard prevents MSYS2 ucrt64's podofo 0.10.4
# from overwriting it. The two versions are ABI-incompatible; shipping 0.10.4
# against an exe linked with 1.1.0 causes an "Entry Point Not Found" crash on
# startup because PdfContent::PdfContent() is not exported by 0.10.4.
$vendorPodofo = Join-Path $ProjectRoot 'third_party\podofo\install\bin\libpodofo.dll'
if (Test-Path $vendorPodofo) {
    Copy-Item $vendorPodofo $DeployDir
    Write-Host '      Staged vendored libpodofo.dll (1.1.0).'
} else {
    # No vendored podofo — CMake fell back to the MSYS2 ucrt64 copy (0.10.4).
    # The objdump closure will copy it from ucrt64\bin, which is correct because
    # GlyphPDF.exe was also linked against that same version.
    Write-Host '      No vendored podofo found; objdump closure will stage the MSYS2 ucrt64 copy.'
}

#  6. MinGW DLL dependency closure via objdump
# Iteratively scan every PE binary in deploy/ for "DLL Name:" imports and copy
# anything that exists in the MSYS2 ucrt64 bin dir until a fixpoint is reached.
Write-Host '[4/8] Resolving MinGW DLL dependency closure...'
$objdump = Join-Path $Msys2Bin 'objdump.exe'
$rounds = 0
do {
    $copied = 0
    $binaries = Get-ChildItem $DeployDir -Recurse -Include '*.exe', '*.dll'
    $present = @{}
    foreach ($b in (Get-ChildItem $DeployDir -Recurse -Include '*.dll')) {
        $present[$b.Name.ToLower()] = $true
    }
    foreach ($bin in $binaries) {
        $imports = & $objdump -p $bin.FullName 2>$null |
                   Select-String 'DLL Name: (.+)$' |
                   ForEach-Object { $_.Matches[0].Groups[1].Value.Trim() }
        foreach ($dep in $imports) {
            $depLower = $dep.ToLower()
            if ($present.ContainsKey($depLower)) { continue }
            $src = Join-Path $Msys2Bin $dep
            if (Test-Path $src) {
                Copy-Item $src $DeployDir
                $present[$depLower] = $true
                $copied++
            }
            # System DLLs (kernel32 etc.) are not in ucrt64\bin - skipped naturally.
        }
    }
    $rounds++
} while ($copied -gt 0 -and $rounds -lt 10)
Write-Host "      closure complete after $rounds round(s)."

#  7. Visual C++ runtime DLLs (required by onnxruntime.dll, which is MSVC-compiled)
# onnxruntime.dll imports VCRUNTIME140.dll, VCRUNTIME140_1.dll, MSVCP140.dll.
# These are NOT in ucrt64\bin so the objdump closure misses them. Users without
# a VC++ 2019+ app installed will get "procedure entry point cannot be found".
#
# AR-11 D4: Prefer the official vc_redist.x64.exe payload over raw System32 copies.
# System32 DLLs are the version the build machine has — they may not be the baseline
# 2022 Redistributable version that ships with onnxruntime. The preferred approach is
# to locate the vc_redist payload from an installed Visual Studio or Redistributable
# (contains the exact version the MSVC compiler targeted) and copy those DLLs.
#
# If vc_redist.x64.exe is not found, fall back to System32 with a warning. The WiX
# installer separately includes a MergeModule for the VC++ runtime, but since GlyphPDF
# uses WiX v5 with a Files harvester, we stage them explicitly here to ensure they
# reach the deploy tree regardless of which WiX variant is used.
Write-Host '[5/8] Staging Visual C++ 2022 runtime DLLs (AR-11 D4)...'

# Try to locate the VC++ Redistributable payload (official Microsoft redistributable)
$vcRedistPayloadDirs = @(
    "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community\VC\Redist\MSVC\*\x64\Microsoft.VC143.CRT",
    "${env:ProgramFiles}\Microsoft Visual Studio\2022\Professional\VC\Redist\MSVC\*\x64\Microsoft.VC143.CRT",
    "${env:ProgramFiles}\Microsoft Visual Studio\2022\Enterprise\VC\Redist\MSVC\*\x64\Microsoft.VC143.CRT",
    "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\*\VC\Redist\MSVC\*\x64\Microsoft.VC143.CRT",
    "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2019\*\VC\Redist\MSVC\*\x64\Microsoft.VC142.CRT"
)

$vcRedistDir = $null
foreach ($pattern in $vcRedistPayloadDirs) {
    $found = Resolve-Path $pattern -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($found) { $vcRedistDir = $found.Path; break }
}

$vcDlls = 'VCRUNTIME140.dll', 'VCRUNTIME140_1.dll', 'MSVCP140.dll', 'MSVCP140_1.dll', 'MSVCP140_2.dll'
$vcDllsStaged = 0
if ($vcRedistDir) {
    Write-Host "      Using official VC++ Redist payload: $vcRedistDir"
    foreach ($dll in $vcDlls) {
        $src = Join-Path $vcRedistDir $dll
        if (Test-Path $src) {
            Copy-Item $src $DeployDir
            $vcDllsStaged++
        }
    }
    if ($vcDllsStaged -gt 0) {
        Write-Host "      Staged $vcDllsStaged VC++ runtime DLLs from official Redist payload."
    }
}

# Fallback: copy from System32 (build-host version — acceptable for dev builds)
if ($vcDllsStaged -eq 0) {
    Write-Warning 'AR-11 D4: VC++ Redist payload not found. Falling back to System32 DLLs.'
    Write-Warning '          For release builds, install Visual Studio 2022 or the VC++ Redistributable'
    Write-Warning '          so the official versioned DLLs are staged (not the build-host System32 copy).'
    $sys32 = [System.Environment]::SystemDirectory
    foreach ($dll in $vcDlls) {
        $src = Join-Path $sys32 $dll
        if (Test-Path $src) {
            Copy-Item $src $DeployDir
        } else {
            Write-Warning "VC++ runtime DLL not found in System32: $dll"
        }
    }
}

#  8. ML models (ROVER ensemble + layout detection)
Write-Host '[6/8] Copying ONNX models...'
$modelsSrc = Join-Path $ProjectRoot 'models'
$modelsDst = Join-Path $DeployDir 'models'

# PP-OCRv5 (RapidOCR secondary engine - required for default ROVER ensemble)
$ppocrSrc = Join-Path $modelsSrc 'ppocrv5'
$required = 'PP-OCRv5_mobile_det_infer.onnx',
            'PP-OCRv5_mobile_rec_infer.onnx',
            'PP-LCNet_x1_0_textline_ori_infer.onnx',
            'ppocrv5_rec_dict.txt'
foreach ($f in $required) {
    if (-not (Test-Path (Join-Path $ppocrSrc $f))) {
        throw "Required OCR model missing: models/ppocrv5/$f - ROVER ensemble would be dead in the installed app."
    }
}
New-Item -ItemType Directory -Path (Join-Path $modelsDst 'ppocrv5') -Force | Out-Null
Copy-Item (Join-Path $ppocrSrc '*') (Join-Path $modelsDst 'ppocrv5') -Recurse

# PP-DocLayoutV2 (layout detection for Djot/MRC pipelines)
$layoutSrc = Join-Path $modelsSrc 'pp_doclayout'
if (Test-Path (Join-Path $layoutSrc 'pp_doclayout_v2.onnx')) {
    New-Item -ItemType Directory -Path (Join-Path $modelsDst 'pp_doclayout') -Force | Out-Null
    Copy-Item (Join-Path $layoutSrc '*') (Join-Path $modelsDst 'pp_doclayout') -Recurse
} else {
    throw 'models/pp_doclayout/pp_doclayout_v2.onnx missing - layout detection would be dead in the installed app.'
}

#  9. Tesseract language data (bundled - avoids first-run network fetch)
Write-Host '[7/8] Copying tessdata...'
$tessSrc = Join-Path $PackDir 'stage\tessdata'
if (-not (Test-Path (Join-Path $tessSrc 'eng.traineddata'))) {
    throw 'packaging/stage/tessdata/eng.traineddata missing - Tesseract would hit the network on first OCR.'
}
New-Item -ItemType Directory -Path (Join-Path $DeployDir 'tessdata') -Force | Out-Null
Copy-Item (Join-Path $tessSrc '*.traineddata') (Join-Path $DeployDir 'tessdata')

#  10. Optional: bundle veraPDF for out-of-the-box PDF/A validation.
# veraPDF is AGPL-3.0 — bundled as a SEPARATE program invoked via subprocess only
# (never linked in-process), which is license-safe aggregation. The app finds it at
# runtime under <appdir>/verapdf/ (VeraPdfValidator::locateCli). Drop a veraPDF CLI
# tree (with its bundled JRE) at third_party/verapdf to bundle it; otherwise this is
# skipped and PDF/A validation degrades gracefully until the user installs veraPDF.
$veraSrc = Join-Path $ProjectRoot 'third_party\verapdf'
if (Test-Path (Join-Path $veraSrc 'verapdf.bat')) {
    Write-Host '[8/8] Bundling veraPDF (PDF/A validator)...'
    $veraDst = Join-Path $DeployDir 'verapdf'
    New-Item -ItemType Directory -Path $veraDst -Force | Out-Null
    Copy-Item (Join-Path $veraSrc '*') $veraDst -Recurse -Force
    # AGPL/GPL compliance gate: ensure source offers and license texts travel with
    # the binary. Hard-fail if they are missing — distributing AGPL/GPL software
    # without a source offer is a license violation.
    $sourceOffer = Join-Path $PackDir 'VERAPDF-SOURCE-OFFER.txt'
    if (-not (Test-Path $sourceOffer)) {
        throw "AGPL COMPLIANCE GATE: packaging/VERAPDF-SOURCE-OFFER.txt is missing. " +
              "This file is required by AGPL-3.0 §6 before distributing veraPDF. " +
              "Create it before running deploy.ps1."
    }
    Copy-Item $sourceOffer $veraDst -Force
    Copy-Item $sourceOffer $DeployDir -Force
    # veraPDF's own license
    if (Test-Path (Join-Path $veraSrc 'LICENSE.txt')) {
        Copy-Item (Join-Path $veraSrc 'LICENSE.txt') (Join-Path $veraDst 'LICENSE-veraPDF.txt') -Force
    }
    # OpenJDK legal tree (GPL-2.0+CPE notices per module)
    $jreLegal = Join-Path $veraSrc 'jre\legal'
    if (Test-Path $jreLegal) {
        $jreLegalDst = Join-Path $veraDst 'jre\legal'
        # Already copied by the wildcard above; verify it's present
        if (-not (Test-Path (Join-Path $jreLegalDst 'java.base\LICENSE'))) {
            Write-Warning 'veraPDF jre/legal tree not staged — OpenJDK per-module notices missing.'
        }
    }
} else {
    Write-Host '[8/8] veraPDF not present at third_party/verapdf - skipping (PDF/A validation will prompt the user to install it).'
    Write-Host '      Note: if veraPDF is present in the final build, re-run deploy.ps1 with it staged.'
}

#  11. Branding + licenses
Write-Host 'Copying icon and licenses...'
Copy-Item (Join-Path $PackDir 'stage\glyphpdf.ico') $DeployDir
Copy-Item (Join-Path $ProjectRoot 'LICENSE') (Join-Path $DeployDir 'LICENSE.txt')
Copy-Item (Join-Path $ProjectRoot 'LICENSE-3RD-PARTY.md') $DeployDir

# Stage the full upstream license texts directory (required for AGPL/GPL
# compliance — a summary table is not the license text).
$licensesSrc = Join-Path $PackDir 'licenses'
if (Test-Path $licensesSrc) {
    $licensesDst = Join-Path $DeployDir 'licenses'
    New-Item -ItemType Directory -Path $licensesDst -Force | Out-Null
    Copy-Item (Join-Path $licensesSrc '*') $licensesDst -Recurse -Force
    Write-Host '      Staged packaging/licenses/ tree into deploy/licenses/'
} else {
    throw "COMPLIANCE GATE: packaging/licenses/ directory not found. " +
          "This directory must contain the full upstream LICENSE/COPYING files " +
          "for all bundled dependencies. See packaging/licenses/README.txt."
}

# AGPL/GPL compliance: VERAPDF-SOURCE-OFFER.txt must always be staged, whether
# or not veraPDF itself is bundled, because the installer carries the licenses/ tree.
$sourceOffer = Join-Path $PackDir 'VERAPDF-SOURCE-OFFER.txt'
if (-not (Test-Path $sourceOffer)) {
    throw "AGPL COMPLIANCE GATE: packaging/VERAPDF-SOURCE-OFFER.txt is missing. " +
          "Required even when veraPDF is not bundled, because LICENSE-3RD-PARTY.md " +
          "references it. Create this file before running deploy.ps1."
}
Copy-Item $sourceOffer $DeployDir -Force

#  12. Final validation gate
Write-Host 'Validating deploy tree...'
$critical = 'GlyphPDF.exe', 'Qt6Core.dll', 'Qt6Widgets.dll', 'Qt6Pdf.dll',
            'libpodofo.dll', 'pdfium.dll', 'onnxruntime.dll',
            'libtesseract-5.5.dll', 'libcrypto-3-x64.dll',
            'VCRUNTIME140.dll', 'VCRUNTIME140_1.dll', 'MSVCP140.dll',
            'models\ppocrv5\PP-OCRv5_mobile_det_infer.onnx',
            'models\ppocrv5\PP-OCRv5_mobile_rec_infer.onnx',
            'models\ppocrv5\PP-LCNet_x1_0_textline_ori_infer.onnx',
            'models\ppocrv5\ppocrv5_rec_dict.txt',
            'models\pp_doclayout\pp_doclayout_v2.onnx',
            'tessdata\eng.traineddata',
            'glyphpdf.ico', 'LICENSE.txt', 'LICENSE-3RD-PARTY.md',
            # AGPL/GPL compliance: source offer + at least the two AGPL/GPL license texts
            'VERAPDF-SOURCE-OFFER.txt',
            'licenses\LICENSE-veraPDF.txt',
            'licenses\LICENSE-OpenJDK.txt'
$missing = @()
foreach ($f in $critical) {
    if (-not (Test-Path (Join-Path $DeployDir $f))) { $missing += $f }
}
if ($missing.Count -gt 0) {
    throw "Deploy validation FAILED - missing: $($missing -join ', ')"
}

$size = (Get-ChildItem $DeployDir -Recurse | Measure-Object Length -Sum).Sum / 1MB
Write-Host '========================================'
Write-Host (' Deploy complete: {0}' -f $DeployDir)
Write-Host (' Total size: {0:N1} MB' -f $size)
Write-Host '========================================'
