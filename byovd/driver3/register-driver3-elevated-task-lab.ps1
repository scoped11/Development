# Guest: register Driver3ElevatedWorker (no #Requires — use from VBox guestcontrol as DriverLab).
$ErrorActionPreference = 'Stop'
$prevEap = $ErrorActionPreference
$ErrorActionPreference = 'SilentlyContinue'
$Driver3 = if ($PSScriptRoot) { $PSScriptRoot } else { 'Z:\byovd\driver3' }
$worker = Join-Path $Driver3 'vm-elevated-worker.ps1'
if (-not (Test-Path $worker)) { throw "Missing $worker" }

$taskName = 'Driver3ElevatedWorker'
cmd /c "schtasks /Delete /TN $taskName /F" 2>$null | Out-Null
$tr = "powershell.exe -NoProfile -ExecutionPolicy Bypass -File `"$worker`""
$ec = 0
schtasks /Create /TN $taskName /TR $tr /SC ONLOGON /RL HIGHEST /F
if ($LASTEXITCODE -ne 0) {
    schtasks /Create /TN $taskName /TR $tr /SC ONLOGON /F
    if ($LASTEXITCODE -ne 0) { throw "schtasks failed — RDP elevated once: register-driver3-elevated-task.ps1" }
    $ec = 1
}
schtasks /Change /TN $taskName /ENABLE | Out-Null
$ErrorActionPreference = $prevEap
Write-Host "[+] Registered $taskName (HIGHEST=$(if ($ec -eq 0) { 'yes' } else { 'limited' }))"
exit 0
