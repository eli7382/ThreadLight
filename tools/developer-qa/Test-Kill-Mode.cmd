@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0QA.ps1" -Wave 35 -Phase 4 -FightSeconds 300 -Build defense -Seed 7382
if errorlevel 1 pause
