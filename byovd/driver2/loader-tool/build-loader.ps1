# Build DriverLoader.exe + CheatLoader.exe -> driver2\bin (canonical)
param([switch]$WithRobRunEsp, [switch]$MirrorToByovdBin)
$ErrorActionPreference = 'Stop'
$Tool = $PSScriptRoot
$Driver2 = Split-Path $Tool -Parent
$Byovd = Split-Path $Driver2 -Parent
$D2Bin = Join-Path $Driver2 'bin'
$Shared = Join-Path $Byovd 'bin'
New-Item -ItemType Directory -Force -Path $D2Bin, $Shared, (Join-Path $Shared 'games'), (Join-Path $D2Bin 'games') | Out-Null

if ($WithRobRunEsp) {
    & (Join-Path $Driver2 'build.ps1') -BuildRobRunEsp
}

$VsDevCmd = Get-ChildItem 'C:\Program Files\Microsoft Visual Studio' -Recurse -Filter 'VsDevCmd.bat' -ErrorAction SilentlyContinue |
    Sort-Object FullName -Descending | Select-Object -First 1 -ExpandProperty FullName
if (-not $VsDevCmd) { throw 'Visual Studio not found' }

$KitRoot = 'C:\Program Files (x86)\Windows Kits\10'
$KitVer = (Get-ChildItem (Join-Path $KitRoot 'Include') -Directory | Sort-Object Name -Descending | Select-Object -First 1).Name
$Inc = Join-Path $Driver2 'sdk'
$ByovdInc = Join-Path $Byovd 'include'
$Lib = Join-Path $KitRoot "Lib\$KitVer\um\x64"

$common = @(
    "/nologo", "/EHsc", "/std:c++17", "/O2", "/MT", "/DUNICODE", "/D_UNICODE", "/D_WIN32_WINNT=0x0A00",
    "/I`"$Inc`"", "/I`"$ByovdInc`"", "/I`"$Driver2`"", "/I`"$Tool`"",
    "/I`"$KitRoot\Include\$KitVer\um`"", "/I`"$KitRoot\Include\$KitVer\shared`""
)

$driverOut = Join-Path $D2Bin 'DriverLoader.exe'
$cheatOut = Join-Path $D2Bin 'CheatLoader.exe'

$bat = @"
call "$VsDevCmd" -arch=amd64
cl $($common -join ' ') "$Tool\DriverLoader.cpp" "$Tool\DriverLoaderGui.cpp" /Fe"$driverOut" /link /SUBSYSTEM:WINDOWS /ENTRY:wmainCRTStartup /MANIFESTUAC:level='requireAdministrator' /LIBPATH:"$Lib" kernel32.lib user32.lib gdi32.lib advapi32.lib shell32.lib shlwapi.lib gdiplus.lib ole32.lib comctl32.lib
if errorlevel 1 exit /b 1
cl $($common -join ' ') "$Tool\CheatLoader.cpp" "$Tool\GamePickerWin.cpp" "$Tool\pf_brand\PfInjectSplash.cpp" /Fe"$cheatOut" /link /SUBSYSTEM:WINDOWS /ENTRY:wmainCRTStartup /LIBPATH:"$Lib" kernel32.lib user32.lib advapi32.lib shell32.lib shlwapi.lib gdiplus.lib ole32.lib
if errorlevel 1 exit /b 1
"@
$tmp = Join-Path $env:TEMP 'build_driver2_loaders.cmd'
Set-Content $tmp $bat -Encoding ASCII
cmd /c $tmp
if ($LASTEXITCODE -ne 0) { throw "loader build failed $LASTEXITCODE" }

if ($MirrorToByovdBin) {
    Copy-Item $driverOut (Join-Path $Shared 'DriverLoader.exe') -Force
    Copy-Item $cheatOut (Join-Path $Shared 'CheatLoader.exe') -Force
}
& (Join-Path $Driver2 'sync-driver2-bin.ps1')
Copy-Item (Join-Path $Tool 'cheats.manifest.json') (Join-Path $D2Bin 'cheats.manifest.json') -Force
Copy-Item (Join-Path $Tool 'games.manifest.json') (Join-Path $D2Bin 'games.manifest.json') -Force
if ($MirrorToByovdBin) {
    Copy-Item (Join-Path $Tool 'games.manifest.json') (Join-Path $Shared 'games.manifest.json') -Force
}
$brandDir = Join-Path $Tool 'pf_brand'
New-Item -ItemType Directory -Force -Path (Join-Path $D2Bin 'brand') | Out-Null
Copy-Item (Join-Path $brandDir 'penis-finger-external.png') (Join-Path $D2Bin 'brand\penis-finger-external.png') -Force -ErrorAction SilentlyContinue
$gamesDir = Join-Path $Tool 'games'
if (-not (Test-Path $gamesDir)) { New-Item -ItemType Directory -Force -Path $gamesDir | Out-Null }
Copy-Item -Path (Join-Path $gamesDir '*') -Destination (Join-Path $D2Bin 'games') -Force -ErrorAction SilentlyContinue
if ($MirrorToByovdBin) {
    $brandOut = Join-Path $Shared 'brand'
    New-Item -ItemType Directory -Force -Path $brandOut, (Join-Path $Shared 'games') | Out-Null
    Copy-Item (Join-Path $brandDir 'penis-finger-external.png') (Join-Path $brandOut 'penis-finger-external.png') -Force -ErrorAction SilentlyContinue
    Copy-Item -Path (Join-Path $gamesDir '*') -Destination (Join-Path $Shared 'games') -Force -ErrorAction SilentlyContinue
}

Write-Host "[+] $driverOut (admin — maps driver)"
Write-Host "[+] $cheatOut (GUI — auto-detects driver, launches Rob Run ESP)"
Write-Host "[+] Canonical dir: $D2Bin"
Write-Host "[*] Tip: .\build-loader.ps1 -WithRobRunEsp"
