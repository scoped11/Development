param(
  [ValidateSet("Debug", "Build")]
  [string]$Section = "Debug",
  [switch]$IApprove
)

$ErrorActionPreference = "Stop"
$dma = Split-Path -Parent $PSScriptRoot
$dev = Split-Path -Parent $dma
Set-Location $dev

if ($Section -eq "Build") {
  if (-not $IApprove) {
    throw @"
Refusing to write Development\build\.
Debug output is Development\debug\ (run without -Section Build).
To populate build/ you must pass both: -Section Build -IApprove
"@
  }
  $outDir = Join-Path $dev "build"
  $cmakeFlag = "-DROBRUN_I_APPROVE=ON"
  Write-Host "BUILD section — writing approved binaries to $outDir" -ForegroundColor Yellow
} else {
  $outDir = Join-Path $dev "debug"
  $cmakeFlag = "-DROBRUN_I_APPROVE=OFF"
  Write-Host "DEBUG section — $outDir"
}

New-Item -ItemType Directory -Force -Path $outDir | Out-Null

cmake -S $dma -B $outDir $cmakeFlag
if ($LASTEXITCODE -ne 0) {
  if ($Section -eq "Build") {
    Get-ChildItem $outDir -Force | Where-Object { $_.Name -ne "README.md" } | Remove-Item -Recurse -Force
  }
  throw "cmake configure failed"
}
cmake --build $outDir --config Release
if ($LASTEXITCODE -ne 0) { throw "cmake build failed" }

$exe = Join-Path $outDir "RobRunDMA.exe"
if (-not (Test-Path $exe)) {
  throw "RobRunDMA.exe not found in $outDir"
}

$cfg = Join-Path $dma "config.json"
if (Test-Path $cfg) {
  Copy-Item $cfg (Join-Path $outDir "config.json") -Force
}

$kmUsb = Join-Path $dma "kmbox-usb"
if (Test-Path $kmUsb) {
  $kmOut = Join-Path $outDir "kmbox-usb"
  New-Item -ItemType Directory -Force -Path $kmOut | Out-Null
  foreach ($name in @("boot.py", "main.py", "ac_profile.txt", "kmbox_identity.txt", "identity_presets.json")) {
    $src = Join-Path $kmUsb $name
    if (Test-Path $src) {
      Copy-Item $src (Join-Path $kmOut $name) -Force
    }
  }
}

$kitFw = Join-Path $dev "kmbox-second-pc\firmware"
if (Test-Path $kitFw) {
  foreach ($name in @("boot.py", "main.py", "ac_profile.txt", "kmbox_identity.txt", "identity_presets.json")) {
    $src = Join-Path $kitFw $name
    if (Test-Path $src) {
      Copy-Item $src (Join-Path $outDir $name) -Force -ErrorAction SilentlyContinue
    }
  }
}

Write-Host "Built: $exe (section=$Section)"
