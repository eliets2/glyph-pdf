@echo off
REM ────────────────────────────────────────────────────────────────────────
REM  GlyphPDF — Runtime dependency checker (MSYS2 ucrt64 native)
REM  Verifies all required DLLs are present in deploy/ before MSI build.
REM ────────────────────────────────────────────────────────────────────────
setlocal enabledelayedexpansion

set "DEPLOY_DIR=%~dp0..\deploy"
set "MISSING=0"
set "FOUND=0"

echo ======================================
echo  GlyphPDF Dependency Check
echo ======================================
echo  Deploy dir: %DEPLOY_DIR%
echo.

if not exist "%DEPLOY_DIR%\PdfWorkstation.exe" (
    echo [FATAL] PdfWorkstation.exe not found. Run deploy-qt.bat first.
    exit /b 1
)

REM ── Required: Qt Core ──
echo --- Qt Runtime ---
for %%F in (
    Qt6Core.dll
    Qt6Gui.dll
    Qt6Widgets.dll
    Qt6Pdf.dll
    Qt6PdfWidgets.dll
    Qt6Network.dll
    Qt6Svg.dll
    Qt6PrintSupport.dll
    Qt6Concurrent.dll
) do (
    if exist "%DEPLOY_DIR%\%%F" (
        set /a FOUND+=1
        echo   [OK]   %%F
    ) else (
        set /a MISSING+=1
        echo   [MISS] %%F
    )
)

REM ── Required: Qt Plugins ──
echo.
echo --- Qt Plugins ---
for %%F in (
    platforms\qwindows.dll
) do (
    if exist "%DEPLOY_DIR%\%%F" (
        set /a FOUND+=1
        echo   [OK]   %%F
    ) else (
        set /a MISSING+=1
        echo   [MISS] %%F
    )
)

REM ── Required: MSYS2 ucrt64 Runtime ──
echo.
echo --- MSYS2 ucrt64 Runtime ---
for %%F in (
    libgcc_s_seh-1.dll
    libstdc++-6.dll
    libwinpthread-1.dll
) do (
    if exist "%DEPLOY_DIR%\%%F" (
        set /a FOUND+=1
        echo   [OK]   %%F
    ) else (
        set /a MISSING+=1
        echo   [MISS] %%F
    )
)

REM ── Required: PoDoFo + OpenSSL ──
echo.
echo --- PoDoFo + OpenSSL ---
for %%F in (
    libpodofo.dll
    libcrypto-3-x64.dll
    libssl-3-x64.dll
) do (
    if exist "%DEPLOY_DIR%\%%F" (
        set /a FOUND+=1
        echo   [OK]   %%F
    ) else (
        set /a MISSING+=1
        echo   [MISS] %%F
    )
)

REM ── Required: Vendored 7-Zip (PARITY-SCORECARD-2026-09-30 §4 row 14) ──
REM  The encrypted-package feature resolves its 7z.exe from the app dir FIRST;
REM  an official install without the bundled pair depends on a system 7-Zip
REM  again, which is the dependency this vendoring removed.
echo.
echo --- Vendored 7-Zip ---
for %%F in (
    7z.exe
    7z.dll
) do (
    if exist "%DEPLOY_DIR%\%%F" (
        set /a FOUND+=1
        echo   [OK]   %%F
    ) else (
        set /a MISSING+=1
        echo   [MISS] %%F
    )
)

REM ── Optional: PDFium, ONNX, qpdf ──
echo.
echo --- Optional ---
for %%F in (
    pdfium.dll
    onnxruntime.dll
    libqpdf30.dll
) do (
    if exist "%DEPLOY_DIR%\%%F" (
        set /a FOUND+=1
        echo   [OK]   %%F  (optional)
    ) else (
        echo   [SKIP] %%F  (optional — feature disabled)
    )
)

echo.
echo ======================================
echo  Result: !FOUND! found, !MISSING! missing
echo ======================================

if !MISSING! GTR 0 (
    echo [FAIL] Cannot build MSI — resolve missing dependencies first.
    exit /b 1
) else (
    echo [PASS] All required dependencies present.
    exit /b 0
)

endlocal
