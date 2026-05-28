@echo off
setlocal

set "MSYS2_UCRT64=C:\msys64\ucrt64"
set "PATH=%MSYS2_UCRT64%\bin;%PATH%"

if not exist build mkdir build

echo Configuring...
cmake -S . -B build -G "Ninja"
if %ERRORLEVEL% NEQ 0 (
    echo Configuration failed.
    exit /b %ERRORLEVEL%
)

echo Building...
cmake --build build --parallel 8
if %ERRORLEVEL% NEQ 0 (
    echo Build failed.
    exit /b %ERRORLEVEL%
)

echo Build successful.
endlocal
