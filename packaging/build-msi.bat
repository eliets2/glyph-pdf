@echo off
REM ────────────────────────────────────────────────────────────────────────
REM  GlyphPDF — MSI build entry point.
REM  Forwards to build-msi.ps1 (canonical pipeline since R4: full payload
REM  including ONNX models + tessdata, WixUI license dialog, SHA-256).
REM ────────────────────────────────────────────────────────────────────────
powershell -ExecutionPolicy Bypass -File "%~dp0build-msi.ps1" %*
exit /b %errorlevel%
