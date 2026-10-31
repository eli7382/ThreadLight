@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0QA.ps1" -Wave 35 -Phase 3 -Build mixed -Mechanic 6 -Seed 7382
if errorlevel 1 pause
