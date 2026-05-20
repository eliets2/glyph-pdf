@echo off
set "QT_DIR=C:\Qt\6.10.2\mingw_64"
set "CMAKE_EXE=C:\Qt\Tools\CMake_64\bin\cmake.exe"
set "MINGW_DIR=C:\Qt\Tools\mingw1310_64\bin"

set "PATH=%QT_DIR%\bin;%MINGW_DIR%;C:\Qt\Tools\CMake_64\bin;C:\Users\User\msys64\mingw64\bin;%PATH%"
set "PKG_CONFIG_PATH=C:\Users\User\msys64\mingw64\lib\pkgconfig;%PKG_CONFIG_PATH%"
set "VCPKG_DEFAULT_TRIPLET=x64-mingw-dynamic"
set "VCPKG_TARGET_TRIPLET=x64-mingw-dynamic"

if not exist build mkdir build

echo Configuring...
"%CMAKE_EXE%" -S . -B build -G "MinGW Makefiles" -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-mingw-dynamic -DVCPKG_MANIFEST_MODE=OFF
if %ERRORLEVEL% NEQ 0 (
    echo Configuration failed.
    exit /b %ERRORLEVEL%
)

echo Building...
"%CMAKE_EXE%" --build build --parallel 8
if %ERRORLEVEL% NEQ 0 (
    echo Build failed.
    exit /b %ERRORLEVEL%
)

echo Build successful.
