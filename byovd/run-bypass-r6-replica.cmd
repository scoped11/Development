@echo off
cd /d "%~dp0"
echo R6 BattlEye replica live-test bypass — no game launch
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\bypass-ac-replica-live.ps1 -Profile r6 %*
exit /b %ERRORLEVEL%
