@echo off
cd /d "%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0get_sdl2_and_run.ps1"
if errorlevel 1 pause
