# Host-only: build, optional offline-sign, stage for guest (no network).
$ErrorActionPreference = 'Stop'
$VmRoot = Split-Path -Parent $PSScriptRoot
$Staged = Join-Path $VmRoot 'vault\staged'
New-Item -ItemType Directory -Force -Path $Staged | Out-Null

$build = Join-Path $VmRoot 'build-vm-lab-driver.ps1'
if (-not (Test-Path (Join-Path $VmRoot 'bin\VmLabDrv.sys'))) {
    & $build
}

$sys = Join-Path $VmRoot 'bin\VmLabDrv.sys'
$cli = Join-Path $VmRoot 'bin\VmLabClient.exe'
$dstSys = Join-Path $Staged 'VmLabDrv.signed.sys'
Copy-Item $sys $dstSys -Force
try {
    & (Join-Path $PSScriptRoot 'Sign-LabDriver.ps1') -SysPath $dstSys -SkipIfNoTool
    if (Test-Path (Join-Path $PSScriptRoot 'Ensure-OfflineLabPublisher.ps1')) {
        & (Join-Path $PSScriptRoot 'Ensure-OfflineLabPublisher.ps1') -ErrorAction Stop
        & (Join-Path $PSScriptRoot 'Sign-LabDriver.ps1') -SysPath $dstSys
    }
} catch {
    Write-Warning "Sign skipped (run elevated once for LocalMachine publisher): $_"
}
Copy-Item $cli (Join-Path $Staged 'VmLabClient.exe') -Force

@{
    schema = 'offline_staged_v1'
    utc    = (Get-Date).ToUniversalTime().ToString('o')
    sys    = $dstSys
    host   = $env:COMPUTERNAME
} | ConvertTo-Json | Set-Content (Join-Path $Staged 'prepare.json') -Encoding UTF8

Write-Host "[+] Staged: $dstSys"