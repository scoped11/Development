# Build Driver2.sys + Driver2Runner.exe (+ optional examples)
param([switch]$BuildExamples, [switch]$BuildLoaders, [switch]$BuildRobRunEsp, [switch]$MirrorToByovdBin)
$ErrorActionPreference = 'Stop'
$Driver2Root = $PSScriptRoot
$ByovdRoot = Split-Path $Driver2Root -Parent
$D2Bin = Join-Path $Driver2Root 'bin'
$D2Obj = Join-Path $Driver2Root 'obj'
$SharedBin = Join-Path $ByovdRoot 'bin'
New-Item -ItemType Directory -Force -Path $D2Bin, $D2Obj, $SharedBin | Out-Null

if (-not (Test-Path (Join-Path $SharedBin 'mapper_nal.exe'))) {
    Write-Host '[*] Running parent build.ps1 (mapper + RwTest)...'
    & (Join-Path $ByovdRoot 'build.ps1')
}

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
$PayloadSrc = Join-Path $Driver2Root 'src\Driver2Build.c'
$PayloadObj = Join-Path $D2Obj 'Driver2Build.obj'
$StompSrc = Join-Path $Driver2Root 'src\Driver2Stomp.c'
$StompObj = Join-Path $D2Obj 'Driver2Stomp.obj'
$Driver2Sys = Join-Path $D2Bin 'Driver2.sys'
$Driver2StompSys = Join-Path $D2Bin 'Driver2Stomp.sys'
$RunnerSrc = Join-Path $Driver2Root 'Driver2Runner.cpp'
$RunnerExe = Join-Path $D2Bin 'Driver2Runner.exe'
$ClientSrc = Join-Path $Driver2Root 'Driver2Client.cpp'
$ClientExe = Join-Path $D2Bin 'Driver2Client.exe'
$D2Sdk = Join-Path $Driver2Root 'sdk'
$ByovdInc = Join-Path $ByovdRoot 'include'

$cmd = @"
call "$VsDevCmd" -arch=amd64 -host_arch=amd64
cl /nologo /c /kernel /GS- /W3 /D_AMD64_ /DAMD64 /DWIN64 /D_WIN64 /D_WIN32_WINNT=0x0A00 /DDRIVER2_PRODUCT=1 ^
  /I"$KitInclude\km" /I"$KitInclude\shared" /I"$KitInclude\ucrt" ^
  /Fo"$PayloadObj" "$PayloadSrc"
if errorlevel 1 exit /b 1
link /nologo /DRIVER /SUBSYSTEM:NATIVE /MACHINE:X64 /ENTRY:DriverEntry /OUT:"$Driver2Sys" ^
  "$PayloadObj" /LIBPATH:"$KitLib\km\x64" /LIBPATH:"$KitLib\ucrt\x64" ^
  ntoskrnl.lib hal.lib BufferOverflowFastFailK.lib
if errorlevel 1 exit /b 1
cl /nologo /c /kernel /GS- /Os /W3 /D_AMD64_ /DAMD64 /DWIN64 /D_WIN64 /D_WIN32_WINNT=0x0A00 /DDRIVER2_STOMP=1 /DDRIVER2_STOMP_LAB_MIN=1 ^
  /I"$KitInclude\km" /I"$KitInclude\shared" /I"$KitInclude\ucrt" ^
  /Fo"$StompObj" "$StompSrc"
if errorlevel 1 exit /b 1
link /nologo /DRIVER /SUBSYSTEM:NATIVE /MACHINE:X64 /OPT:REF /OPT:ICF /ALIGN:64 /FILEALIGN:64 ^
  /MERGE:.rdata=.text /MERGE:.pdata=.text /MERGE:.data=.text /ENTRY:StompThunkEntry /OUT:"$Driver2StompSys" ^
  "$StompObj" /LIBPATH:"$KitLib\km\x64" /LIBPATH:"$KitLib\ucrt\x64" ^
  ntoskrnl.lib hal.lib BufferOverflowFastFailK.lib
if errorlevel 1 exit /b 1
cl /nologo /EHsc /std:c++17 /O2 /MT /W3 /D_WIN32_WINNT=0x0A00 ^
  /I"$ByovdInc" /I"$D2Sdk" /I"$Driver2Root" /I"$KitRoot\Include\$KitVersion\um" /I"$KitRoot\Include\$KitVersion\shared" ^
  "$RunnerSrc" /Fe"$RunnerExe" /link /MANIFESTUAC:level='requireAdministrator' ^
  /LIBPATH:"$KitRoot\Lib\$KitVersion\um\x64" kernel32.lib user32.lib advapi32.lib shlwapi.lib
if errorlevel 1 exit /b 1
cl /nologo /EHsc /std:c++17 /O2 /MT /W3 /D_WIN32_WINNT=0x0A00 ^
  /I"$ByovdInc" /I"$D2Sdk" /I"$Driver2Root" /I"$KitRoot\Include\$KitVersion\um" /I"$KitRoot\Include\$KitVersion\shared" ^
  "$ClientSrc" /Fe"$ClientExe" /link /MANIFESTUAC:level='requireAdministrator' ^
  /LIBPATH:"$KitRoot\Lib\$KitVersion\um\x64" kernel32.lib user32.lib advapi32.lib
if errorlevel 1 exit /b 1
"@
$bat = Join-Path $D2Obj 'build.cmd'
Set-Content -LiteralPath $bat -Value $cmd -Encoding ASCII
cmd.exe /d /c "`"$bat`""
if ($LASTEXITCODE -ne 0) { throw "driver2 build failed $LASTEXITCODE" }

if ($MirrorToByovdBin) {
    Copy-Item $Driver2Sys (Join-Path $SharedBin 'Driver2.sys') -Force
    Copy-Item $RunnerExe (Join-Path $SharedBin 'Driver2Runner.exe') -Force
    Copy-Item $ClientExe (Join-Path $SharedBin 'Driver2Client.exe') -Force
}
& (Join-Path $Driver2Root 'sync-driver2-bin.ps1')

if ($BuildExamples) {
    $ExSrc = Join-Path $Driver2Root 'examples\cheat_dev_minimal.cpp'
    $ExOut = Join-Path $D2Bin 'cheat_dev_minimal.exe'
    $exCmd = @"
call "$VsDevCmd" -arch=amd64 -host_arch=amd64
cl /nologo /EHsc /std:c++17 /O2 /MT /W3 /D_WIN32_WINNT=0x0A00 ^
  /I"$ByovdInc" /I"$D2Sdk" /I"$Driver2Root" /I"$KitRoot\Include\$KitVersion\um" /I"$KitRoot\Include\$KitVersion\shared" ^
  "$ExSrc" /Fe"$ExOut" /link /LIBPATH:"$KitRoot\Lib\$KitVersion\um\x64" kernel32.lib user32.lib
if errorlevel 1 exit /b 1
"@
    $exBat = Join-Path $D2Obj 'build-example.cmd'
    Set-Content -LiteralPath $exBat -Value $exCmd -Encoding ASCII
    cmd.exe /d /c "`"$exBat`""
    if ($LASTEXITCODE -ne 0) { throw 'Example build failed' }
    Write-Host "[+] $ExOut"
}

if ($BuildRobRunEsp) {
    $DevRoot = Split-Path $ByovdRoot -Parent
    $DmaSrc = Join-Path $DevRoot 'dma\src'
    $RobSrc = Join-Path $Driver2Root 'robrun\robrun_esp_external.cpp'
    $RobOut = Join-Path $D2Bin 'robrun_esp_external.exe'
    $robCmd = @"
call "$VsDevCmd" -arch=amd64 -host_arch=amd64
cl /nologo /EHsc /std:c++17 /O2 /MT /W3 /D_WIN32_WINNT=0x0A00 ^
  /I"$ByovdInc" /I"$D2Sdk" /I"$Driver2Root" /I"$DmaSrc" /I"$KitRoot\Include\$KitVersion\um" /I"$KitRoot\Include\$KitVersion\shared" ^
  "$RobSrc" /Fe"$RobOut" /link /LIBPATH:"$KitRoot\Lib\$KitVersion\um\x64" kernel32.lib user32.lib gdi32.lib psapi.lib gdiplus.lib ole32.lib
if errorlevel 1 exit /b 1
"@
    $robBat = Join-Path $D2Obj 'build-robrun-esp.cmd'
    Set-Content -LiteralPath $robBat -Value $robCmd -Encoding ASCII
    cmd.exe /d /c "`"$robBat`""
    if ($LASTEXITCODE -ne 0) { throw 'Rob Run ESP build failed' }
    if ($MirrorToByovdBin) {
        Copy-Item $RobOut (Join-Path $SharedBin 'robrun_esp_external.exe') -Force
    }
    Write-Host "[+] $RobOut"
}

if ($BuildLoaders) {
    & (Join-Path $Driver2Root 'loader-tool\build-loader.ps1') -WithRobRunEsp
}

Write-Host "[+] $Driver2Sys"
Write-Host "[+] $Driver2StompSys"
Write-Host "[+] $RunnerExe"
Write-Host "[+] $ClientExe"
if ($MirrorToByovdBin) { Write-Host "[+] Mirrored to $SharedBin" }
Write-Host "[+] Canonical: $D2Bin"
