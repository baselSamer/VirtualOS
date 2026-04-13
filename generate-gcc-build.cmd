@echo off
setlocal

set "SCRIPT_DIR=%~dp0"
set "PS1_PATH=%SCRIPT_DIR%scripts\generate-gcc-build.ps1"

if not exist "%PS1_PATH%" (
  echo Script not found: "%PS1_PATH%"
  exit /b 1
)

powershell -NoProfile -ExecutionPolicy Bypass -File "%PS1_PATH%" %*
exit /b %ERRORLEVEL%
