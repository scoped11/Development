# Host: run Host B map inside VM (Guest Additions + env — works when args drop on -File).
param(
    [string]$Profile = 'rust',
    [switch]$ForceMap,
    [switch]$SkipProve,
    [string]$VmName = 'DriverLab-E-26200',
    [int]$TimeoutMs = 600000
)

$ErrorActionPreference = 'Stop'
$Repo = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$GuestExec = Join-Path $Repo 'driver-lab\vm\host-guest-exec.ps1'

$env = @{
    DRIVER3_HOSTB_PROFILE   = $Profile
    DRIVER3_HOSTB_FORCEMAP  = $(if ($ForceMap) { '1' } else { '0' })
    DRIVER3_HOSTB_SKIPPROVE = $(if ($SkipProve) { '1' } else { '0' })
}

& $GuestExec -VmName $VmName -TimeoutMs $TimeoutMs -PutEnv $env -Command @(
    'C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe',
    '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
    'Z:\byovd\driver3\vm-guest-hostb-execute.ps1'
)
$ec = $LASTEXITCODE

& (Join-Path $PSScriptRoot 'sync-guest-artifacts.ps1') -VmName $VmName -AllKernelProofs
& (Join-Path $PSScriptRoot 'sync-kernel-proof-from-artifacts.ps1') -Primary $Profile
exit $ec
