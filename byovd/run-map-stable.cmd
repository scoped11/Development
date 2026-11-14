@echo off
setlocal
cd /d "%~dp0"
if not exist "bin\mapper_nal.exe" (
  echo Run build.ps1 first.
  exit /b 1
)
if not exist "bin\Betest.sys" (
  echo Missing bin\Betest.sys
  exit /b 1
)
bin\mapper_nal.exe bin\Betest.sys --driver nal %*
echo Exit: %ERRORLEVEL%
echo Log: %TEMP%\pf_diag.tmp
