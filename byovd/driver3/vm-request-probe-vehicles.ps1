# Guest (or host via GuestExec -File): queue vehicle RwTest probe on elevated worker.
param([string[]]$Drivers = @('ts', 'corsair', 'rtcore', 'siv', 'nal'))

$ErrorActionPreference = 'Stop'
$Driver3 = if ($PSScriptRoot) { $PSScriptRoot } else { 'Z:\byovd\driver3' }
$reqPath = Join-Path $Driver3 'vm_hostb_request.json'
[ordered]@{
    action  = 'probe_vehicles'
    drivers = @($Drivers)
    utc     = (Get-Date).ToUniversalTime().ToString('o')
} | ConvertTo-Json | Set-Content $reqPath -Encoding utf8

$q = schtasks /Query /TN Driver3ElevatedWorker /FO LIST 2>&1
if ($LASTEXITCODE -ne 0) {
    Write-Host '[!] Driver3ElevatedWorker missing — run register-elevated-guest-remote.ps1 elevated once'
    exit 24
}
schtasks /Run /TN Driver3ElevatedWorker | Out-Null
if ($LASTEXITCODE -ne 0) { throw "schtasks /Run failed $LASTEXITCODE" }
Write-Host '[+] Vehicle probe queued — see driver2\vm_vehicle_probe.json'
exit 0
