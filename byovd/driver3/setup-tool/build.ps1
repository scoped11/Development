# Build Driver3Setup.exe (usermode console — no admin required to launch setup)
param([switch]$CopyToByovdBin)

$ErrorActionPreference = 'Stop'
$Tool = $PSScriptRoot
$Driver3 = Split-Path $Tool -Parent
$OutDir = Join-Path $Driver3 'bin'
$Src = Join-Path $Tool 'Driver3Setup.cpp'
$Out = Join-Path $OutDir 'Driver3Setup.exe'
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

$VsDevCmd = Get-ChildItem 'C:\Program Files\Microsoft Visual Studio' -Recurse -Filter 'VsDevCmd.bat' -ErrorAction SilentlyContinue |
    Sort-Object FullName -Descending | Select-Object -First 1 -ExpandProperty FullName
if (-not $VsDevCmd) { throw 'Visual Studio not found — install VS Build Tools' }

$KitRoot = 'C:\Program Files (x86)\Windows Kits\10'
$KitVer = Get-ChildItem (Join-Path $KitRoot 'Include') -Directory -ErrorAction SilentlyContinue |
    Sort-Object Name -Descending | Select-Object -First 1 -ExpandProperty Name
if (-not $KitVer) { throw 'Windows SDK not found' }

$UmInc = Join-Path $KitRoot "Include\$KitVer\um"
$ShInc = Join-Path $KitRoot "Include\$KitVer\shared"
$UcrtInc = Join-Path $KitRoot "Include\$KitVer\ucrt"
$LibUm = Join-Path $KitRoot "Lib\$KitVer\um\x64"
$LibUcrt = Join-Path $KitRoot "Lib\$KitVer\ucrt\x64"

$bat = @"
@echo off
call "$VsDevCmd" -arch=amd64 -host_arch=amd64
cl /nologo /EHsc /std:c++17 /O2 /MT /W4 /D_UNICODE /DUNICODE ^
  /I"$UmInc" /I"$ShInc" /I"$UcrtInc" ^
  "$Src" /Fe"$Out" /link /SUBSYSTEM:CONSOLE ^
  /LIBPATH:"$LibUm" /LIBPATH:"$LibUcrt" Shell32.lib
exit /b %ERRORLEVEL%
"@
$tmp = Join-Path $env:TEMP "build_driver3setup.cmd"
$bat | Set-Content $tmp -Encoding ASCII
cmd /c $tmp
if ($LASTEXITCODE -ne 0) { throw 'cl failed' }

if ($CopyToByovdBin) {
    $shared = Join-Path (Split-Path $Driver3 -Parent) 'bin\Driver3Setup.exe'
    Copy-Item $Out $shared -Force
    Write-Host "[+] Also: $shared"
}

Write-Host "[+] $Out"
Write-Host "    Run: .\driver3\bin\Driver3Setup.exe --auto"
exit 0
