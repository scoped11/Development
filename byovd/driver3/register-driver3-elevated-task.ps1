#Requires -RunAsAdministrator
# Run ONCE in DriverLab guest (elevated). Enables host/MCP vm-test -Execute.
$ErrorActionPreference = 'Stop'
$Driver3 = if ($PSScriptRoot) { $PSScriptRoot } else { 'Z:\byovd\driver3' }
$worker = Join-Path $Driver3 'vm-elevated-worker.ps1'
if (-not (Test-Path $worker)) { throw "Missing $worker — mount Development share as Z:" }

$taskName = 'Driver3ElevatedWorker'
$prev = $ErrorActionPreference
$ErrorActionPreference = 'SilentlyContinue'
schtasks /Delete /TN $taskName /F 2>$null | Out-Null
$ErrorActionPreference = $prev

$tr = "powershell.exe -NoProfile -ExecutionPolicy Bypass -File `"$worker`""
$credFile = 'Z:\driver-lab\vm\lab.credentials.json'
$ru = 'DriverLab'
$rp = 'LabVM-ChangeMe!'
if (Test-Path $credFile) {
    $c = Get-Content $credFile -Raw | ConvertFrom-Json
    if ($c.username) { $ru = $c.username }
    if ($c.password) { $rp = $c.password }
}
# Stored creds so Guest Control (non-interactive) can schtasks /Run this later.
$null = schtasks /Create /TN $taskName /TR $tr /SC ONLOGON /RL HIGHEST /RU $ru /RP $rp /F
if ($LASTEXITCODE -ne 0) {
    $null = schtasks /Create /TN $taskName /TR $tr /SC ONLOGON /RL HIGHEST /RU SYSTEM /F
}
if ($LASTEXITCODE -ne 0) { throw "schtasks /Create failed $LASTEXITCODE" }

# Allow standard user to trigger the task (lab guest DriverLab)
$null = schtasks /Change /TN $taskName /ENABLE

Write-Host "[+] Registered $taskName — host uses vm-request-hostb.ps1 or vm-test-from-host -Execute"
Write-Host "    Test: schtasks /Run /TN $taskName"
exit 0
