@echo off
cd /d "%~dp0.."
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0..\driver2\map-host-lab.ps1" -Profile universal %*
pause
