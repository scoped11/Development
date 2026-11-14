# Guest: register Driver3ElevatedWorker without interactive UAC.
# Uses lab.credentials.json on Z: (same share). No #Requires — caller must be able to spawn admin token.
param([switch]$Direct)
$ErrorActionPreference = 'Stop'
$Driver3 = if ($PSScriptRoot) { $PSScriptRoot } else { 'Z:\byovd\driver3' }
$credPath = 'Z:\driver-lab\vm\lab.credentials.json'
if (-not (Test-Path $credPath)) { throw "Missing $credPath" }
$c = Get-Content $credPath -Raw | ConvertFrom-Json
$worker = Join-Path $Driver3 'vm-elevated-worker.ps1'
if (-not (Test-Path $worker)) { throw "Missing $worker" }

function Register-Worker {
    $tn = 'Driver3ElevatedWorker'
    schtasks /Delete /TN $tn /F 2>$null | Out-Null
    $tr = "powershell.exe -NoProfile -ExecutionPolicy Bypass -File `"$worker`""
    schtasks /Create /TN $tn /TR $tr /SC ONLOGON /RL HIGHEST /F | Out-String | Write-Host
    if ($LASTEXITCODE -ne 0) { throw "schtasks /Create failed $LASTEXITCODE" }
    schtasks /Change /TN $tn /ENABLE | Out-Null
    Write-Host "[+] Registered $tn"
}

$adm = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
    [Security.Principal.WindowsBuiltInRole]::Administrator)
if ($adm -or $Direct) {
    Register-Worker
    exit 0
}

Write-Host '[!] Not elevated — one-time elevated PowerShell in VM:'
Write-Host "    Z:\byovd\driver3\register-driver3-elevated-task.ps1"
Write-Host '    (or vm-enable-lab-uac-off.ps1 → reboot → register)'
exit 24
