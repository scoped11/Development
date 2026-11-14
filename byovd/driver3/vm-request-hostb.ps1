# Drop Host B request + trigger Driver3ElevatedWorker (non-admin OK if task registered).
param(
    [string]$Profile = 'rust',
    [switch]$ForceMap,
    [switch]$SkipProve,
    [switch]$Inspector
)

$ErrorActionPreference = 'Stop'
$Driver3 = $PSScriptRoot
. (Join-Path $Driver3 'Resolve-Driver3Game.ps1')
$reqPath = Join-Path $Driver3 'vm_hostb_request.json'
$body = [ordered]@{
    profile   = (Resolve-Driver3Profile $Profile)
    forceMap  = [bool]$ForceMap
    skipProve = [bool]$SkipProve
    inspector = [bool]$Inspector
    utc       = (Get-Date).ToUniversalTime().ToString('o')
}
($body | ConvertTo-Json) | Set-Content $reqPath -Encoding utf8

$q = schtasks /Query /TN Driver3ElevatedWorker /FO LIST 2>&1
if ($LASTEXITCODE -ne 0) {
    Write-Host '[!] Driver3ElevatedWorker not installed. In elevated guest run once:'
    Write-Host "    powershell -ExecutionPolicy Bypass -File $Driver3\register-driver3-elevated-task.ps1"
    exit 24
}

schtasks /Run /TN Driver3ElevatedWorker | Out-Null
if ($LASTEXITCODE -ne 0) { throw "schtasks /Run failed $LASTEXITCODE" }
Write-Host '[+] Request queued; worker running (see vm_hostb_execute.log)'
exit 0
