# Run a PowerShell script on the guest at HIGHEST (one-shot scheduled task). Uses lab password.
param(
    [Parameter(Mandatory)][string]$ScriptOnGuest,  # e.g. Z:\driver-lab\vm\bg\guest-vm-driver-matrix.ps1
    [string]$VmName = 'DriverLab-E-26200',
    [int]$TimeoutMs = 900000
)
$ErrorActionPreference = 'Stop'
$credFile = Join-Path $PSScriptRoot 'lab.credentials.json'
$c = Get-Content $credFile -Raw | ConvertFrom-Json
$guestExec = Join-Path $PSScriptRoot 'host-guest-exec.ps1'
$ps = 'C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe'
$tn = 'DriverLabOneShot_' + [guid]::NewGuid().ToString('N').Substring(0, 8)
$tr = "$ps -NoProfile -ExecutionPolicy Bypass -File `"$ScriptOnGuest`""
$st = (Get-Date).AddMinutes(1).ToString('HH:mm')
$sd = Get-Date -Format 'MM/dd/yyyy'
& $guestExec -VmName $VmName -TimeoutMs 120000 -Command @(
    'C:\Windows\System32\schtasks.exe', '/Create', '/TN', $tn,
    '/TR', $tr, '/SC', 'ONCE', '/ST', $st, '/SD', $sd,
    '/RU', $c.username, '/RP', $c.password, '/RL', 'HIGHEST', '/F'
)
if ($LASTEXITCODE -ne 0) { throw "schtasks create failed $LASTEXITCODE" }
& $guestExec -VmName $VmName -TimeoutMs $TimeoutMs -Command @(
    'C:\Windows\System32\schtasks.exe', '/Run', '/TN', $tn
)
& $guestExec -VmName $VmName -TimeoutMs 60000 -Command @(
    'C:\Windows\System32\schtasks.exe', '/Delete', '/TN', $tn, '/F'
) | Out-Null
