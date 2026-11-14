@echo off
setlocal
cd /d "%~dp0"
if not exist "bin\mapper_nal.exe" (
  echo Run build.ps1 first.
  exit /b 1
)
if not exist "bin\RwTest.sys" (
  echo Missing bin\RwTest.sys — run build.ps1
  exit /b 1
)
echo Mapping RwTest.sys (BYOVD only — no test-signing)...
if exist "bin\RTCore64.sys" (
  bin\mapper_nal.exe bin\RwTest.sys --driver rtcore --no-auth --ac-safe %*
) else (
  bin\mapper_nal.exe bin\RwTest.sys --driver nal --no-auth --ac-safe %*
)
echo.
echo Test: bin\process_rw.exe  (scans blend device names, PID-bound session)
echo Or: powershell -File map-secure-rwtest.ps1
echo Log: %%TEMP%%\pf_diag.tmp
