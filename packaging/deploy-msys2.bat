@echo off
REM ────────────────────────────────────────────────────────────────────────
REM  GlyphPDF — Post-build DLL deploy for MSYS2 ucrt64 native build
REM  Copies all required runtime DLLs into build/ so ctest can run.
REM  Usage: call packaging\deploy-msys2.bat  (from project root)
REM ────────────────────────────────────────────────────────────────────────
setlocal

set "PROJECT_ROOT=%~dp0.."
set "BUILD_DIR=%PROJECT_ROOT%\build"
set "MSYS2_BIN=C:\msys64\ucrt64\bin"
set "QT6_PLUGINS=C:\msys64\ucrt64\share\qt6\plugins"

if not exist "%BUILD_DIR%\PdfWorkstation.exe" (
    echo [WARN] PdfWorkstation.exe not in build/ — skipping deploy.
    exit /b 0
)

echo Deploying Qt runtime DLLs via windeployqt6...
"%MSYS2_BIN%\windeployqt6.exe" ^
    --no-translations ^
    --no-system-d3d-compiler ^
    --no-quick-import ^
    "%BUILD_DIR%\PdfWorkstation.exe"

echo Copying MSYS2 ucrt64 dependency DLLs...
for %%F in (
    libpodofo.dll
    libcrypto-3-x64.dll
    libssl-3-x64.dll
    libqpdf30.dll
    libxml2-2.dll
    libtesseract-5.5.dll
    libleptonica-6.dll
    libfreetype-6.dll
    zlib1.dll
    libcurl-4.dll
    libpng16-16.dll
    libjpeg-62.dll
    libtiff-6.dll
    libgcc_s_seh-1.dll
    libstdc++-6.dll
    libwinpthread-1.dll
) do (
    if exist "%MSYS2_BIN%\%%F" (
        copy /y "%MSYS2_BIN%\%%F" "%BUILD_DIR%\" >nul
    )
)

echo Ensuring platforms\qoffscreen.dll is present for headless tests...
if not exist "%BUILD_DIR%\platforms" mkdir "%BUILD_DIR%\platforms"
if exist "%QT6_PLUGINS%\platforms\qoffscreen.dll" (
    copy /y "%QT6_PLUGINS%\platforms\qoffscreen.dll" "%BUILD_DIR%\platforms\" >nul
)

REM PDFium prebuilt
if exist "%PROJECT_ROOT%\third_party\pdfium\bin\pdfium.dll" (
    copy /y "%PROJECT_ROOT%\third_party\pdfium\bin\pdfium.dll" "%BUILD_DIR%\" >nul
)

REM ONNX Runtime bundled
if exist "%PROJECT_ROOT%\onnxruntime-win-x64-1.17.3\lib\onnxruntime.dll" (
    copy /y "%PROJECT_ROOT%\onnxruntime-win-x64-1.17.3\lib\onnxruntime.dll" "%BUILD_DIR%\" >nul
)

echo deploy-msys2 complete.
endlocal
