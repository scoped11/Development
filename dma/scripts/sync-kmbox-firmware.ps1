# Canonical onboard scripts live in dma/kmbox-usb/
$ErrorActionPreference = "Stop"
$dma = Split-Path -Parent $PSScriptRoot
$dev = Split-Path -Parent $dma
$src = Join-Path $dma "kmbox-usb"
$dst = Join-Path $dev "kmbox-second-pc\firmware"
if (-not (Test-Path $src)) { throw "Missing $src" }
New-Item -ItemType Directory -Force -Path $dst | Out-Null
foreach ($name in @("boot.py", "main.py", "ac_profile.txt", "kmbox_identity.txt", "identity_presets.json")) {
    $f = Join-Path $src $name
    if (Test-Path $f) {
        Copy-Item $f (Join-Path $dst $name) -Force
        Write-Host "synced $name"
    }
}
Write-Host "[+] $dst"
