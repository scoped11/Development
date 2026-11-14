# Host: copy stripped .sys from VM share into inbox x64 for IDA (no installers).
param(
    [string]$FromVm = (Join-Path $PSScriptRoot '..\audit\inbox\oem-sys\from-vm'),
    [string]$InboxX64 = (Join-Path $PSScriptRoot '..\audit\inbox\oem-sys\x64')
)

$ErrorActionPreference = 'Stop'
if (-not (Test-Path $FromVm)) {
    Write-Host "No from-vm folder yet: $FromVm"
    exit 0
}

Get-ChildItem $FromVm -Directory | ForEach-Object {
    $manifest = Join-Path $_.FullName 'manifest.json'
    if (-not (Test-Path $manifest)) { return }
    $m = Get-Content $manifest -Raw | ConvertFrom-Json
    $src = Get-ChildItem $_.FullName -Filter '*.sys' | Select-Object -First 1
    if (-not $src) { return }
    $dest = Join-Path $InboxX64 $m.file
    if ((Test-Path $dest) -and ((Get-FileHash $dest -Algorithm SHA256).Hash -eq $m.sha256)) {
        Write-Host "[=] $($m.file) already in inbox"
        return
    }
    Copy-Item $src.FullName $dest -Force
    Write-Host "[+] $($m.file) -> $dest (sha $($m.sha256.Substring(0,16))...)"
}
Write-Host "IDA: idb_open input_path under $InboxX64"
