# Host-side: run guest bootstrap + doctor snapshot when Guest Additions + login are ready.
#   .\host-guest-bootstrap.ps1
#   .\host-guest-bootstrap.ps1 -ProbeOnly
param(
    [string]$VmName = 'AI_workspace',
    [switch]$ProbeOnly,
    [switch]$SkipBootstrap
)

$ErrorActionPreference = 'Stop'
$here = $PSScriptRoot
if (-not $here) { $here = Split-Path -Parent $PSCommandPath }

if ($ProbeOnly) {
    & (Join-Path $here 'host-guest-exec.ps1') -VmName $VmName -Command @(
        'powershell', '-NoProfile', '-ExecutionPolicy', 'Bypass',
        '-File', 'D:\hostdev\driver-lab\vm\guest-probe-rea-env.ps1'
    )
    exit $LASTEXITCODE
}

if (-not $SkipBootstrap) {
    & (Join-Path $here 'host-guest-exec.ps1') -VmName $VmName -TimeoutMs 3600000 -Command @(
        'powershell', '-NoProfile', '-ExecutionPolicy', 'Bypass',
        '-File', 'D:\hostdev\driver-lab\vm\bootstrap-vm-tools.ps1'
    )
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

& (Join-Path $here 'host-guest-exec.ps1') -VmName $VmName -TimeoutMs 600000 -Command @(
    'powershell', '-NoProfile', '-ExecutionPolicy', 'Bypass',
    '-Command',
    'Set-Location D:\hostdev; npx.cmd -y rea-agents@latest doctor 2>&1 | Tee-Object -FilePath D:\hostdev\docs\rea-doctor-guest-snapshot.txt'
)