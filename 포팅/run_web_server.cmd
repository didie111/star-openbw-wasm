@echo off
setlocal

REM Run a simple local web server for the OpenBW web build.
REM Requires Python 3 installed and available on PATH.

set "ROOT=%~dp0"
pushd "%ROOT%" >nul

echo [WEB] Serving: %CD%
echo [WEB] Open: http://127.0.0.1:8080/
echo.

python -m http.server 8080

popd >nul
endlocal
