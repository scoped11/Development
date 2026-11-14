# Build VmLabDrv.sys + VmLabClient.exe into driver-lab\vm\bin (host or guest with WDK+VS).
$ErrorActionPreference = 'Stop'
$Root = $PSScriptRoot
$Bin = Join-Path $Root 'bin'
$Obj = Join-Path $Bin 'obj'
New-Item -ItemType Directory -Force -Path $Bin, $Obj | Out-Null

$VsDevCmd = Get-ChildItem 'C:\Program Files\Microsoft Visual Studio' -Recurse -Filter 'VsDevCmd.bat' -ErrorAction SilentlyContinue |
    Sort-Object FullName -Descending | Select-Object -First 1 -ExpandProperty FullName
if (-not $VsDevCmd) { throw 'Visual Studio not found' }

$KitRoot = 'C:\Program Files (x86)\Windows Kits\10'
$KitVersion = Get-ChildItem (Join-Path $KitRoot 'Include') -Directory |
    Where-Object { Test-Path (Join-Path $_.FullName 'km\ntddk.h') } |
    Sort-Object Name -Descending | Select-Object -First 1 -ExpandProperty Name
if (-not $KitVersion) { throw 'WDK not installed' }

$KitInclude = Join-Path $KitRoot "Include\$KitVersion"
$KitLib = Join-Path $KitRoot "Lib\$KitVersion"
$DrvSrc = Join-Path $Root 'driver\VmLabDrv.c'
$DrvObj = Join-Path $Obj 'VmLabDrv.obj'
$DrvSys = Join-Path $Bin 'VmLabDrv.sys'
$CliExe = Join-Path $Bin 'VmLabClient.exe'

$cmd = @"
call "$VsDevCmd" -arch=amd64 -host_arch=amd64
cl /nologo /c /kernel /GS- /W3 /D_AMD64_ /DAMD64 /DWIN64 /D_WIN64 /D_WIN32_WINNT=0x0A00 ^
  /I"$KitInclude\km" /I"$KitInclude\shared" /I"$KitInclude\ucrt" ^
  /Fo"$DrvObj" "$DrvSrc"
if errorlevel 1 exit /b 1
link /nologo /DRIVER /SUBSYSTEM:NATIVE /MACHINE:X64 /ENTRY:DriverEntry /OUT:"$DrvSys" ^
  "$DrvObj" /LIBPATH:"$KitLib\km\x64" /LIBPATH:"$KitLib\ucrt\x64" ^
  ntoskrnl.lib hal.lib BufferOverflowFastFailK.lib
if errorlevel 1 exit /b 1
cl /nologo /EHsc /std:c++17 /O2 /MT /W3 /D_WIN32_WINNT=0x0A00 ^
  /I"$KitInclude\um" /I"$KitInclude\shared" /I"$KitInclude\ucrt" ^
  "$Root\driver\VmLabClient.cpp" /Fe"$CliExe" /link ^
  /LIBPATH:"$KitLib\um\x64" /LIBPATH:"$KitLib\ucrt\x64" kernel32.lib user32.lib
if errorlevel 1 exit /b 1
"@

$cmdPath = Join-Path $Obj 'build.cmd'
Set-Content -LiteralPath $cmdPath -Value $cmd -Encoding ASCII
cmd.exe /d /c "`"$cmdPath`""
if ($LASTEXITCODE -ne 0) { throw "Build failed $LASTEXITCODE" }
Write-Host "OK: $DrvSys"
Write-Host "OK: $CliExe"
Write-Host "WDK: $KitVersion"
