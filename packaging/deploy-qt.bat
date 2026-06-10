@echo off
REM ────────────────────────────────────────────────────────────────────────
REM  GlyphPDF — deploy entry point.
REM  Forwards to deploy.ps1 (canonical since R4). The old hand-maintained DLL
REM  list was replaced by an objdump dependency closure, and the payload now
REM  includes models/ (ppocrv5 + pp_doclayout) and tessdata/.
REM ────────────────────────────────────────────────────────────────────────
powershell -ExecutionPolicy Bypass -File "%~dp0deploy.ps1" %*
exit /b %errorlevel%
