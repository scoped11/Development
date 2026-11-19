# Host: copy toolchain installer to guest and run (long-running).
param(
    [string]$VmName = '',
    [int]$TimeoutMs = 7200000,
    [switch]$SkipHeavyWinget
)

$ErrorActionPreference = 'Stop'
$here = $PSScriptRoot
$VBox = 'C:\Program Files\Oracle\VirtualBox\VBoxManage.exe'
$credFile = Join-Path $here 'lab.credentials.json'
$cred = Get-Content $credFile -Raw | ConvertFrom-Json
if (-not $VmName) { $VmName = if ($cred.vm_name) { $cred.vm_name } else { 'AI_Workspave_v3' } }

$ps = 'C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe'
# Guestcontrol sessions see UNC, not mapped Z:
$guestScript = '\\VBoxSvr\hostdev\driver-lab\vm\install-vm-artex-toolchain.ps1'

$args = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $guestScript)
if ($SkipHeavyWinget) { $args += '-SkipHeavyWinget' }

Write-Host "[*] Running guest toolchain install (timeout ${TimeoutMs}ms) ..."
& $VBox guestcontrol $VmName run --username=$($cred.username) --password=$($cred.password) `
    --exe=$ps --wait-stdout --wait-stderr --timeout=$TimeoutMs -- `
    @args
exit $LASTEXITCODE