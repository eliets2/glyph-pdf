@echo off
REM ────────────────────────────────────────────────────────────────────────
REM  GlyphPDF — windeployqt bundling script (MSYS2 ucrt64 native)
REM  Copies PdfWorkstation.exe + all Qt/MSYS2/dep DLLs into deploy/
REM ────────────────────────────────────────────────────────────────────────
setlocal enabledelayedexpansion

set "PROJECT_ROOT=%~dp0.."
set "BUILD_DIR=%PROJECT_ROOT%\build"
set "DEPLOY_DIR=%PROJECT_ROOT%\deploy"
set "MSYS2_BIN=C:\msys64\ucrt64\bin"

echo [1/6] Cleaning deploy directory...
if exist "%DEPLOY_DIR%" rmdir /s /q "%DEPLOY_DIR%"
mkdir "%DEPLOY_DIR%"

echo [2/6] Copying main executable...
if not exist "%BUILD_DIR%\PdfWorkstation.exe" (
    echo ERROR: PdfWorkstation.exe not found in %BUILD_DIR%
    echo Run cmake --build build --parallel first.
    exit /b 1
)
copy /y "%BUILD_DIR%\PdfWorkstation.exe" "%DEPLOY_DIR%\"

echo [3/6] Running windeployqt6...
"%MSYS2_BIN%\windeployqt6.exe" ^
    --release ^
    --no-translations ^
    --no-system-d3d-compiler ^
    --no-opengl-sw ^
    --no-quick-import ^
    "%DEPLOY_DIR%\PdfWorkstation.exe"

if errorlevel 1 (
    echo ERROR: windeployqt6 failed
    exit /b 1
)

echo [4/6] Copying MSYS2 ucrt64 runtime DLLs...
for %%F in (
    libpodofo.dll
    libcrypto-3-x64.dll
    libssl-3-x64.dll
    libqpdf30.dll
    zlib1.dll
    libjpeg-62.dll
    libfreetype-6.dll
    libpng16-16.dll
    liblzma-5.dll
    libbz2-1.dll
    libbrotlidec.dll
    libbrotlicommon.dll
    libxml2-2.dll
    libiconv-2.dll
    libintl-8.dll
    libtesseract-5.5.dll
    libleptonica-6.dll
) do (
    if exist "%MSYS2_BIN%\%%F" (
        copy /y "%MSYS2_BIN%\%%F" "%DEPLOY_DIR%\" >nul
    ) else if exist "%BUILD_DIR%\%%F" (
        copy /y "%BUILD_DIR%\%%F" "%DEPLOY_DIR%\" >nul
    )
)

echo [5/6] Copying MSYS2 MinGW runtime DLLs...
for %%F in (
    libgcc_s_seh-1.dll
    libstdc++-6.dll
    libwinpthread-1.dll
    libgomp-1.dll
) do (
    if exist "%MSYS2_BIN%\%%F" (
        copy /y "%MSYS2_BIN%\%%F" "%DEPLOY_DIR%\" >nul
    ) else if exist "%BUILD_DIR%\%%F" (
        copy /y "%BUILD_DIR%\%%F" "%DEPLOY_DIR%\" >nul
    )
)

REM Copy PDFium if present
if exist "%BUILD_DIR%\pdfium.dll" (
    copy /y "%BUILD_DIR%\pdfium.dll" "%DEPLOY_DIR%\" >nul
)

REM Copy ONNX Runtime if present
if exist "%PROJECT_ROOT%\onnxruntime-win-x64-1.17.3\lib\onnxruntime.dll" (
    copy /y "%PROJECT_ROOT%\onnxruntime-win-x64-1.17.3\lib\onnxruntime.dll" "%DEPLOY_DIR%\" >nul
)

echo [6/6] Copying resources...
if exist "%PROJECT_ROOT%\resources\icons\glyphpdf.ico" (
    copy /y "%PROJECT_ROOT%\resources\icons\glyphpdf.ico" "%DEPLOY_DIR%\" >nul
)

echo.
echo ========================================
echo  Deploy complete: %DEPLOY_DIR%
echo ========================================

endlocal
