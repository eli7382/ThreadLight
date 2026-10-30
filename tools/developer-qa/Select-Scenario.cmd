@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0QA.ps1" -Menu
if errorlevel 1 pause
