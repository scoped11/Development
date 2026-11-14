# Host orchestration: queue vm-probe-vehicles (VM must have Driver3ElevatedWorker registered once).
param(
    [string]$VmName = 'DriverLab-E-26200',
    [string[]]$Drivers = @('ts', 'corsair', 'rtcore', 'siv', 'nal'),
    [int]$PollSec = 300
)

$ErrorActionPreference = 'Stop'
$Repo = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$GuestExec = Join-Path $Repo 'driver-lab\vm\host-guest-exec.ps1'
$VBox = 'C:\Program Files\Oracle\VirtualBox\VBoxManage.exe'
$state = & $VBox showvminfo $VmName --machinereadable 2>$null | Select-String 'VMState="(\w+)"'
if ($state -notmatch 'running') {
    & $VBox startvm $VmName --type headless
    Start-Sleep -Seconds 20
}

$drvArg = ($Drivers | ForEach-Object { "'$_'" }) -join ','
$ps = @(
    'C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe',
    '-NoProfile', '-ExecutionPolicy', 'Bypass', '-Command',
    "& 'Z:\byovd\driver3\vm-request-probe-vehicles.ps1' -Drivers @($drvArg)"
)
& $GuestExec -VmName $VmName -TimeoutMs 120000 -Command $ps
if ($LASTEXITCODE -eq 24) {
    Write-Host '[!] One-time in VM (elevated PowerShell):'
    Write-Host '    Z:\byovd\driver3\register-driver3-elevated-task.ps1'
    Write-Host '    Z:\byovd\driver3\vm-request-probe-vehicles.ps1'
    exit 24
}
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$deadline = (Get-Date).AddSeconds($PollSec)
$probeHost = Join-Path $Repo 'byovd\driver2\vm_vehicle_probe.json'
while ((Get-Date) -lt $deadline) {
    & $GuestExec -VmName $VmName -TimeoutMs 60000 -Command @(
        'C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe',
        '-NoProfile', '-Command',
        "if (Test-Path 'Z:\byovd\driver2\vm_vehicle_probe.json') { Get-Content 'Z:\byovd\driver2\vm_vehicle_probe.json' -Raw }"
    ) 2>&1 | Out-String | ForEach-Object {
        if ($_ -match 'vm_vehicle_probe_v1') {
            $_.Trim() | Set-Content $probeHost -Encoding utf8
            $_ | Write-Host
            $j = $_ | ConvertFrom-Json
            Write-Host "[+] winner=$($j.winner)"
            exit $(if ($j.winner) { 0 } else { 1 })
        }
    }
    Start-Sleep -Seconds 10
}
Write-Host '[!] probe timed out'
exit 98
