@echo off
REM ────────────────────────────────────────────────────────────────────────
REM  GlyphPDF — Release build entry point.
REM
REM  Builds BOTH artifacts in one pass:
REM    dist\GlyphPDF-1.0.0-x64.msi          (installer, with WixUI dialog)
REM    dist\GlyphPDF-1.0.0-x64-portable.zip (no-install, unzip-and-run)
REM
REM  Both include VC++ 2022 runtime, Qt 6, ONNX models, tessdata.
REM
REM  Flags (forwarded to build-msi.ps1):
REM    -SkipBuild   reuse existing build\ output (skip cmake --build)
REM    -MsiOnly     skip portable ZIP creation
REM ────────────────────────────────────────────────────────────────────────
powershell -ExecutionPolicy Bypass -File "%~dp0build-msi.ps1" %*
exit /b %errorlevel%
