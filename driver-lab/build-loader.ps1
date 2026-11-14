#Requires -Version 5.1
$ErrorActionPreference = "Stop"
$Root = $PSScriptRoot
$Bin = Join-Path $Root "bin"
$Loader = Join-Path $Root "loader"
$Assets = Join-Path $Root "assets"
New-Item -ItemType Directory -Force -Path $Bin, $Assets | Out-Null

$dmaGif = Join-Path (Split-Path $Root -Parent) "dma\assets\loading_animation.gif"
$dmaMp4 = Join-Path (Split-Path $Root -Parent) "dma\assets\loading_animation.mp4"
if (Test-Path $dmaGif) { Copy-Item $dmaGif (Join-Path $Assets "loading_animation.gif") -Force }
if (Test-Path $dmaMp4) { Copy-Item $dmaMp4 (Join-Path $Assets "loading_animation.mp4") -Force }

Push-Location $Loader
try {
    dotnet publish -c Release -r win-x64 --self-contained false -o $Bin
} finally {
    Pop-Location
}

foreach ($name in @("loading_animation.gif", "loading_animation.mp4")) {
    $src = Join-Path $Assets $name
    if (Test-Path $src) { Copy-Item $src (Join-Path $Bin $name) -Force }
}

Write-Host "OK: $Bin\DriverLabLoader.exe"
Write-Host "Optional: drop your Discord loading_animation.mp4 into $Assets and re-run build-loader.ps1"
