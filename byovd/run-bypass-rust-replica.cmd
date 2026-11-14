@echo off
cd /d "%~dp0"
echo Rust EAC replica live-test bypass — no game launch
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\bypass-ac-replica-live.ps1 -Profile rust %*
exit /b %ERRORLEVEL%
