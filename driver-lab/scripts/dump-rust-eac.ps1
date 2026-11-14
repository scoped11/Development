<#
.SYNOPSIS
  Archive Rust (Steam) EasyAntiCheat EOS binaries + metadata into driver-lab/dumps.

  Read-only: copies from disk; does not start/stop EAC or Rust.
#>
param(
    [string]$RustRoot = 'C:\Program Files (x86)\Steam\steamapps\common\Rust',
    [string]$EosDriver = 'C:\Program Files (x86)\EasyAntiCheat_EOS\EasyAntiCheat_EOS.sys',
    [string]$OutRoot = (Join-Path $PSScriptRoot '..\dumps')
)

$ErrorActionPreference = 'Stop'
$stamp = Get-Date -Format 'yyyy-MM-dd_HHmm'
$out = Join-Path $OutRoot "rust-eac-eos_$stamp"
New-Item -ItemType Directory -Force -Path $out, (Join-Path $out 'eac'), (Join-Path $out 'kernel') | Out-Null

if (-not (Test-Path $EosDriver)) {
    throw "EOS driver missing: $EosDriver"
}
if (-not (Test-Path $RustRoot)) {
    throw "Rust install missing: $RustRoot"
}

$eacDir = Join-Path $RustRoot 'EasyAntiCheat'
$settings = Join-Path $eacDir 'Settings.json'
if (-not (Test-Path $settings)) {
    throw "Not Rust EAC layout — missing $settings"
}

# Kernel driver (shared EOS install — this is what Rust loads via EasyAntiCheat_EOSSys)
$kernelDest = Join-Path $out 'kernel\EasyAntiCheat_EOS.sys'
Copy-Item -LiteralPath $EosDriver -Destination $kernelDest -Force

# Rust-side EAC bundle (version channel / certs — not the .sys, but ties dump to Rust)
Copy-Item -LiteralPath $eacDir -Destination (Join-Path $out 'eac\EasyAntiCheat') -Recurse -Force

$build = Get-ItemProperty 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion'
$vi = (Get-Item -LiteralPath $kernelDest).VersionInfo
$hash = (Get-FileHash -LiteralPath $kernelDest -Algorithm SHA256).Hash

$inbox = Join-Path $PSScriptRoot '..\audit\inbox'
New-Item -ItemType Directory -Force -Path $inbox | Out-Null
$inboxCopy = Join-Path $inbox 'EasyAntiCheat_EOS.sys'
Copy-Item -LiteralPath $kernelDest -Destination $inboxCopy -Force

$manifest = @{
    game              = 'rust'
    platform          = 'steam'
    rust_path         = $RustRoot
    eos_driver_path   = $EosDriver
    driver_sha256     = $hash
    driver_bytes      = (Get-Item $kernelDest).Length
    product_version   = $vi.ProductVersion
    file_version      = $vi.FileVersion
    host_os_build     = "$($build.CurrentBuildNumber).$($build.UBR)"
    captured_utc      = (Get-Date).ToUniversalTime().ToString('o')
    settings_json     = (Get-Content $settings -Raw | ConvertFrom-Json)
    note              = 'Rust uses EOS shared driver EasyAntiCheat_EOS.sys; game identity in Settings.json / certificates'
}
$manifestPath = Join-Path $out 'manifest.json'
$manifest | ConvertTo-Json -Depth 6 | Set-Content $manifestPath -Encoding UTF8

& (Join-Path $PSScriptRoot '..\reverse\clone-driver.ps1') -SourcePath $kernelDest

Write-Host "[+] Rust EAC dump: $out"
Write-Host "    Driver SHA256: $hash"
Write-Host "    Inbox: $inboxCopy"
Write-Host "    IDA clone: driver-lab\reverse\EasyAntiCheat_EOS\bin\"
