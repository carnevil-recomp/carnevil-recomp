@echo off
rem Drag your carnevil.raw onto this file once; afterwards just double-click it.
rem Options can follow, e.g.  play.bat C:\games\carnevil.raw --violence 2
cd /d "%~dp0"
if exist "%~1" (
  >disk.path echo %~1
  shift
)
if not exist disk.path (
  echo Drag your carnevil.raw onto play.bat, or run: play.bat C:\path\to\carnevil.raw
  pause
  exit /b 1
)
set /p RAW=<disk.path
carnevil.exe --raw "%RAW%" %1 %2 %3 %4 %5 %6 %7 %8 %9
if errorlevel 1 pause
