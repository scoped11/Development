# Build Betest.sys + mapper_nal.exe into .\bin
$ErrorActionPreference = "Stop"
$Root = $PSScriptRoot
$Bin = Join-Path $Root "bin"
$Obj = Join-Path $Bin "obj"
New-Item -ItemType Directory -Force -Path $Bin, $Obj | Out-Null

$VsDevCmd = Get-ChildItem "C:\Program Files\Microsoft Visual Studio" -Recurse -Filter "VsDevCmd.bat" -ErrorAction SilentlyContinue |
    Sort-Object FullName -Descending | Select-Object -First 1 -ExpandProperty FullName
if (-not $VsDevCmd) { throw "Visual Studio VsDevCmd.bat not found" }

$KitRoot = "C:\Program Files (x86)\Windows Kits\10"
$KitVersion = "10.0.26100.0"
$kmNtddk = Join-Path $KitRoot "Include\$KitVersion\km\ntddk.h"
if (-not (Test-Path $kmNtddk)) {
    $KitVersion = Get-ChildItem (Join-Path $KitRoot "Include") -Directory |
        Where-Object { Test-Path (Join-Path $_.FullName "km\ntddk.h") } |
        Sort-Object Name -Descending | Select-Object -First 1 -ExpandProperty Name
}
if (-not $KitVersion) { throw "WDK km headers not found (install Windows Driver Kit)" }
$KitInclude = Join-Path $KitRoot "Include\$KitVersion"
$KitLib = Join-Path $KitRoot "Lib\$KitVersion"

$DriverSrc = Join-Path $Root "src\driver\BetestDriver.c"
$RwTestSrc = Join-Path $Root "src\driver\RwTestDrv.c"
$DriverObj = Join-Path $Obj "BetestDriver.obj"
$RwTestObj = Join-Path $Obj "RwTestDrv.obj"
$DriverSys = Join-Path $Bin "Betest.sys"
$RwTestSys = Join-Path $Bin "RwTest.sys"
$MapperExe = Join-Path $Bin "mapper_nal.exe"
$ProcessRwExe = Join-Path $Bin "process_rw.exe"
$MapperDir = Join-Path $Root "src\mapper"

$cmd = @"
@echo on
call "$VsDevCmd" -arch=amd64 -host_arch=amd64
cl /nologo /c /kernel /GS- /W3 /D_AMD64_ /DAMD64 /DWIN64 /D_WIN64 /D_WIN32_WINNT=0x0A00 ^
  /I"$KitInclude\km" /I"$KitInclude\shared" /I"$KitInclude\ucrt" ^
  /Fo"$DriverObj" "$DriverSrc"
if errorlevel 1 exit /b 1
link /nologo /DRIVER /SUBSYSTEM:NATIVE /MACHINE:X64 /ENTRY:DriverEntry /OUT:"$DriverSys" ^
  "$DriverObj" /LIBPATH:"$KitLib\km\x64" /LIBPATH:"$KitLib\ucrt\x64" ^
  ntoskrnl.lib hal.lib BufferOverflowFastFailK.lib
if errorlevel 1 exit /b 1
cl /nologo /c /kernel /GS- /W3 /D_AMD64_ /DAMD64 /DWIN64 /D_WIN64 /D_WIN32_WINNT=0x0A00 ^
  /I"$KitInclude\km" /I"$KitInclude\shared" /I"$KitInclude\ucrt" ^
  /Fo"$RwTestObj" "$RwTestSrc"
if errorlevel 1 exit /b 1
link /nologo /DRIVER /SUBSYSTEM:NATIVE /MACHINE:X64 /ENTRY:DriverEntry /OUT:"$RwTestSys" ^
  "$RwTestObj" /LIBPATH:"$KitLib\km\x64" /LIBPATH:"$KitLib\ucrt\x64" ^
  ntoskrnl.lib hal.lib BufferOverflowFastFailK.lib
if errorlevel 1 exit /b 1
cl /nologo /EHsc /std:c++17 /O2 /MT /W3 /D_WIN32_WINNT=0x0A00 ^
  /I"$Root\include" /I"$KitInclude\um" /I"$KitInclude\shared" /I"$KitInclude\ucrt" ^
  "$Root\examples\process_rw.cpp" /Fe"$ProcessRwExe" /link ^
  /LIBPATH:"$KitLib\um\x64" /LIBPATH:"$KitLib\ucrt\x64" kernel32.lib user32.lib
if errorlevel 1 exit /b 1
cl /nologo /EHsc /std:c++17 /O2 /MT /W3 /D_WIN32_WINNT=0x0A00 ^
  /I"$MapperDir" /I"$KitInclude\um" /I"$KitInclude\shared" /I"$KitInclude\ucrt" ^
  "$MapperDir\main_nal.cpp" "$MapperDir\mapper.cpp" /Fe"$MapperExe" /link ^
  /LIBPATH:"$KitLib\um\x64" /LIBPATH:"$KitLib\ucrt\x64" kernel32.lib user32.lib advapi32.lib ntdll.lib
if errorlevel 1 exit /b 1
"@

$cmdPath = Join-Path $Obj "build.cmd"
Set-Content -LiteralPath $cmdPath -Value $cmd -Encoding ASCII
cmd.exe /d /c "`"$cmdPath`""
if ($LASTEXITCODE -ne 0) { throw "Build failed with exit code $LASTEXITCODE" }

Copy-Item $DriverSys (Join-Path $Bin "BetestDriver.sys") -Force
Write-Host "OK: $DriverSys"
Write-Host "OK: $RwTestSys"
Write-Host "OK: $MapperExe"
Write-Host "OK: $ProcessRwExe"
Write-Host "WDK kit: $KitVersion"
$Driver2Build = Join-Path $Root "driver2\build.ps1"
if (Test-Path $Driver2Build) {
    Write-Host "[*] Building driver2 (Driver2.sys)..."
    & $Driver2Build
}
