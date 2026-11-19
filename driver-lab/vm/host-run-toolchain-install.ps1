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

$src = Join-Path $here 'install-vm-artex-toolchain.ps1'
$ps = 'C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe'

& $VBox guestcontrol $VmName mkdir --username=$($cred.username) --password=$($cred.password) --parents 'C:\lab\bootstrap'
$guestScript = 'C:\lab\bootstrap\install-vm-artex-toolchain.ps1'
& $VBox guestcontrol $VmName copyto --username=$($cred.username) --password=$($cred.password) --target-directory $guestScript $src

$args = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', 'C:\lab\bootstrap\install-vm-artex-toolchain.ps1')
if ($SkipHeavyWinget) { $args += '-SkipHeavyWinget' }

Write-Host "[*] Running guest toolchain install (timeout ${TimeoutMs}ms) ..."
& $VBox guestcontrol $VmName run --username=$($cred.username) --password=$($cred.password) `
    --exe=$ps --wait-stdout --wait-stderr --timeout=$TimeoutMs -- `
    @args
exit $LASTEXITCODE