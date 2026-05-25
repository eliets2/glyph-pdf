@echo off
REM ────────────────────────────────────────────────────────────────────────
REM  GlyphPDF — windeployqt bundling script (Session 17 D1)
REM  Copies PdfWorkstation.exe + all Qt/MinGW/vcpkg DLLs into deploy/
REM ────────────────────────────────────────────────────────────────────────
setlocal enabledelayedexpansion

set "PROJECT_ROOT=%~dp0.."
set "BUILD_DIR=%PROJECT_ROOT%\build"
set "DEPLOY_DIR=%PROJECT_ROOT%\deploy"
set "QT_DIR=C:\Qt\6.10.2\mingw_64"
set "MINGW_DIR=C:\Qt\Tools\mingw1310_64"
set "VCPKG_DIR=C:\vcpkg\installed\x64-mingw-dynamic"

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

echo [3/6] Running windeployqt...
"%QT_DIR%\bin\windeployqt.exe" ^
    --release ^
    --no-translations ^
    --no-system-d3d-compiler ^
    --no-opengl-sw ^
    --no-quick-import ^
    "%DEPLOY_DIR%\PdfWorkstation.exe"

if errorlevel 1 (
    echo ERROR: windeployqt failed
    exit /b 1
)

echo [4/6] Copying vcpkg runtime DLLs...
for %%F in (
    podofo.dll
    libcrypto-3-x64.dll
    libssl-3-x64.dll
    qpdf29.dll
    zlib1.dll
    libjpeg-62.dll
    freetype.dll
    libpng16.dll
    liblzma.dll
    libbz2.dll
    libbrotlidec.dll
    libbrotlicommon.dll
    libxml2.dll
    libiconv.dll
    libcharset.dll
    libintl.dll
) do (
    if exist "%VCPKG_DIR%\bin\%%F" (
        copy /y "%VCPKG_DIR%\bin\%%F" "%DEPLOY_DIR%\" >nul
    ) else if exist "%BUILD_DIR%\%%F" (
        copy /y "%BUILD_DIR%\%%F" "%DEPLOY_DIR%\" >nul
    )
)

echo [5/6] Copying MinGW runtime DLLs...
for %%F in (
    libgcc_s_seh-1.dll
    libstdc++-6.dll
    libwinpthread-1.dll
    libgomp-1.dll
) do (
    if exist "%MINGW_DIR%\bin\%%F" (
        copy /y "%MINGW_DIR%\bin\%%F" "%DEPLOY_DIR%\" >nul
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

REM Quick DLL dependency check
echo.
echo Checking for missing DLLs...
pushd "%DEPLOY_DIR%"
for /f "tokens=*" %%L in ('"%DEPLOY_DIR%\PdfWorkstation.exe" --version 2^>^&1') do (
    echo   %%L
)
popd

endlocal
