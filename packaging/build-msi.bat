@echo off
REM ────────────────────────────────────────────────────────────────────────
REM  GlyphPDF — Full build automation: cmake → deploy → check → MSI
REM  (Session 17 D5)
REM ────────────────────────────────────────────────────────────────────────
setlocal enabledelayedexpansion

set "PROJECT_ROOT=%~dp0.."
set "BUILD_DIR=%PROJECT_ROOT%\build"
set "DEPLOY_DIR=%PROJECT_ROOT%\deploy"
set "PACK_DIR=%~dp0"
set "OUTPUT_DIR=%PROJECT_ROOT%\dist"
set "QT_DIR=C:\Qt\6.10.2\mingw_64"
set "MINGW_DIR=C:\Qt\Tools\mingw1310_64"
set "CMAKE=C:\Qt\Tools\CMake_64\bin\cmake.exe"

set "PATH=%QT_DIR%\bin;%MINGW_DIR%\bin;%PATH%"

echo ========================================
echo  GlyphPDF MSI Build Pipeline
echo ========================================
echo.

REM ── Step 1: CMake configure (if needed) ──
if not exist "%BUILD_DIR%\Makefile" (
    echo [1/5] Configuring CMake...
    "%CMAKE%" -S "%PROJECT_ROOT%" -B "%BUILD_DIR%" ^
        -G "MinGW Makefiles" ^
        -DCMAKE_BUILD_TYPE=Release ^
        -DCMAKE_PREFIX_PATH="%QT_DIR%" ^
        -DVCPKG_TARGET_TRIPLET=x64-mingw-dynamic ^
        -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
    if errorlevel 1 (
        echo [FAIL] CMake configure failed.
        exit /b 1
    )
) else (
    echo [1/5] CMake already configured — skipping.
)

REM ── Step 2: Build ──
echo [2/5] Building Release...
"%CMAKE%" --build "%BUILD_DIR%" --parallel --config Release
if errorlevel 1 (
    echo [FAIL] Build failed.
    exit /b 1
)

REM ── Step 3: Deploy (windeployqt + DLLs) ──
echo [3/5] Deploying Qt runtime...
call "%PACK_DIR%\deploy-qt.bat"
if errorlevel 1 (
    echo [FAIL] Deployment failed.
    exit /b 1
)

REM ── Step 4: Dependency check ──
echo [4/5] Checking dependencies...
call "%PACK_DIR%\check-deps.bat"
if errorlevel 1 (
    echo [FAIL] Dependency check failed.
    exit /b 1
)

REM ── Step 5: Build MSI with WiX v4 ──
echo [5/5] Building MSI installer...

REM Check for wix CLI
where wix >nul 2>&1
if errorlevel 1 (
    echo [WARN] WiX v4 CLI not found in PATH.
    echo        Install: dotnet tool install -g wix
    echo        The deploy/ directory is ready for manual MSI packaging.
    goto :skipMsi
)

if not exist "%OUTPUT_DIR%" mkdir "%OUTPUT_DIR%"

wix build ^
    -src "%PACK_DIR%\GlyphPDF.wxs" ^
    -d DeployDir="%DEPLOY_DIR%" ^
    -out "%OUTPUT_DIR%\GlyphPDF-0.2.0-x64.msi" ^
    -arch x64

if errorlevel 1 (
    echo [FAIL] WiX build failed.
    exit /b 1
)

echo.
echo ========================================
echo  MSI built: %OUTPUT_DIR%\GlyphPDF-0.2.0-x64.msi
echo ========================================
goto :done

:skipMsi
echo.
echo ========================================
echo  Deploy complete: %DEPLOY_DIR%
echo  Install WiX v4 to build MSI:
echo    dotnet tool install -g wix
echo ========================================

:done
endlocal
