$ErrorActionPreference = 'Stop'
$Tool = $PSScriptRoot
$Driver3 = Split-Path $Tool -Parent
$Out = Join-Path $Driver3 'bin\Driver3Play.exe'
$Src = Join-Path $Tool 'Driver3Play.cpp'
$VsDevCmd = Get-ChildItem 'C:\Program Files\Microsoft Visual Studio' -Recurse -Filter 'VsDevCmd.bat' -ErrorAction SilentlyContinue |
    Sort-Object FullName -Descending | Select-Object -First 1 -ExpandProperty FullName
if (-not $VsDevCmd) { throw 'VS not found' }
$KitRoot = 'C:\Program Files (x86)\Windows Kits\10'
$KitVer = (Get-ChildItem (Join-Path $KitRoot 'Include') -Directory | Sort-Object Name -Descending | Select-Object -First 1).Name
$bat = @"
call "$VsDevCmd" -arch=amd64
cl /nologo /EHsc /std:c++17 /O2 /MT /DUNICODE /D_UNICODE ^
  "$Src" /Fe"$Out" /link /SUBSYSTEM:CONSOLE /MANIFESTUAC:level='requireAdministrator' Shell32.lib
"@
$tmp = Join-Path $env:TEMP 'build_driver3play.cmd'
$bat | Set-Content $tmp -Encoding ASCII
cmd /c $tmp
Write-Host "[+] $Out (admin manifest)"
