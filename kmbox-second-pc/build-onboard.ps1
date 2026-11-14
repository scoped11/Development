$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw "Visual Studio C++ tools not found" }
$tools = Get-ChildItem (Join-Path $vs "VC\Tools\MSVC") | Sort-Object Name -Descending | Select-Object -First 1
$cl = Join-Path $tools.FullName "bin\Hostx64\x64\cl.exe"
$winsdk = "${env:ProgramFiles(x86)}\Windows Kits\10"
$includeUcrt = Join-Path $winsdk "Include"
$ver = Get-ChildItem (Join-Path $includeUcrt "*") -Directory | Where-Object { Test-Path (Join-Path $_.FullName "um\Windows.h") } | Sort-Object Name -Descending | Select-Object -First 1
$inc = @(
  (Join-Path $tools.FullName "include"),
  (Join-Path $ver.FullName "ucrt"),
  (Join-Path $ver.FullName "um"),
  (Join-Path $ver.FullName "shared")
)
$libDirs = @(
  (Join-Path $tools.FullName "lib\x64"),
  (Join-Path $winsdk "Lib\$($ver.Name)\ucrt\x64"),
  (Join-Path $winsdk "Lib\$($ver.Name)\um\x64")
)
$env:INCLUDE = ($inc -join ";")
$env:LIB = ($libDirs -join ";")
$src = Join-Path $here "src\kmbox_onboard.cpp"
$out = Join-Path $here "KmboxOnboard.exe"
Push-Location $here
& $cl /nologo /O2 /EHsc /std:c++17 /Fe:$out $src setupapi.lib
if ($LASTEXITCODE -ne 0) { throw "cl failed" }
Remove-Item (Join-Path $here "kmbox_onboard.obj") -ErrorAction SilentlyContinue
Pop-Location
Write-Host "Built $out"
