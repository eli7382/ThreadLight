@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0QA.ps1" -Wave 29 -Build mixed -Seed 7382
if errorlevel 1 pause
