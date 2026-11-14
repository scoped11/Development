# Build HijackDriver.exe → driver-lab\vm\bin
$ErrorActionPreference = 'Stop'
$Here = $PSScriptRoot
$VmBin = Join-Path (Split-Path $Here -Parent) 'bin'
$ByovdInc = Resolve-Path (Join-Path $Here '..\..\..\byovd\include')
New-Item -ItemType Directory -Force -Path $VmBin | Out-Null

$VsDevCmd = Get-ChildItem 'C:\Program Files\Microsoft Visual Studio' -Recurse -Filter 'VsDevCmd.bat' -ErrorAction SilentlyContinue |
    Sort-Object FullName -Descending | Select-Object -First 1 -ExpandProperty FullName
if (-not $VsDevCmd) { throw 'Visual Studio not found' }

$KitRoot = 'C:\Program Files (x86)\Windows Kits\10'
$KitVersion = Get-ChildItem (Join-Path $KitRoot 'Include') -Directory |
    Where-Object { Test-Path (Join-Path $_.FullName 'um\Windows.h') } |
    Sort-Object Name -Descending | Select-Object -First 1 -ExpandProperty Name

$Out = Join-Path $VmBin 'HijackDriver.exe'
$Src = Join-Path $Here 'HijackDriver.cpp'

$cmd = @"
call "$VsDevCmd" -arch=amd64 -host_arch=amd64
cl /nologo /EHsc /std:c++17 /O2 /MT /W3 /D_WIN32_WINNT=0x0A00 ^
  /I"$ByovdInc" /I"$KitRoot\Include\$KitVersion\um" /I"$KitRoot\Include\$KitVersion\shared" ^
  "$Src" /Fe"$Out" /link /MANIFESTUAC:level='requireAdministrator' ^
  /LIBPATH:"$KitRoot\Lib\$KitVersion\um\x64" kernel32.lib user32.lib advapi32.lib shlwapi.lib
if errorlevel 1 exit /b 1
"@
$bat = Join-Path $Here 'build-hijack-driver.cmd'
Set-Content -LiteralPath $bat -Value $cmd -Encoding ASCII
cmd.exe /d /c "`"$bat`""
if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
Remove-Item (Join-Path $VmBin 'LabAcRunner.exe') -Force -ErrorAction SilentlyContinue
Write-Host "[+] $Out"
Write-Host '    Guest: D:\hostdev\driver-lab\vm\bin\HijackDriver.exe --profile rust --map'
