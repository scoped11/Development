# Run inside DriverLab guest (or via host-guest-exec.ps1). No map unless -Execute.
param(
    [string]$Profile = 'rust',
    [switch]$Execute,
    [switch]$ForceMap
)

$ErrorActionPreference = 'Stop'
$shareRoots = @('D:\hostdev', 'Z:\', 'D:\hostdev\byovd\driver3')
$Driver3 = $null
foreach ($c in @(
        'Z:\byovd\driver3',
        'D:\hostdev\byovd\driver3',
        (Join-Path $PSScriptRoot '.')
    )) {
    if (Test-Path (Join-Path $c 'lane-gate.ps1')) { $Driver3 = $c; break }
}
if (-not $Driver3) { throw 'driver3 not found on hostdev share — mount Development as D:\hostdev' }

Write-Host "=== driver3 VM smoke ==="
Write-Host "Host:$env:COMPUTERNAME Driver3:$Driver3"
Get-PSDrive -PSProvider FileSystem | ForEach-Object { Write-Host "  drive $($_.Name): $($_.Root)" }

& (Join-Path $Driver3 'lane-gate.ps1') -Game $Profile
$gateExit = $LASTEXITCODE
Write-Host "[gate] exit=$gateExit"

& (Join-Path $Driver3 'everything.ps1') -Profile $Profile -WhichHost auto
Write-Host "[everything-plan] exit=$LASTEXITCODE"

$hbExit = 0
if ($Execute) {
    Write-Host '[host-b] -Execute requires elevation — use vm-test-from-host.ps1 -Execute (scheduled task)'
    $hbExit = 77
} else {
    & (Join-Path $Driver3 'host-b.ps1') -Profile $Profile -Execute:$false
    $hbExit = $LASTEXITCODE
}
Write-Host "[host-b] exit=$hbExit"

$report = [ordered]@{
    utc        = (Get-Date).ToUniversalTime().ToString('o')
    guest      = $env:COMPUTERNAME
    driver3    = $Driver3
    profile    = $Profile
    execute    = [bool]$Execute
    gate_exit  = $gateExit
    host_b_exit = $hbExit
}
$out = Join-Path $Driver3 'vm_guest_smoke.json'
($report | ConvertTo-Json) | Set-Content $out -Encoding utf8
Write-Host "[+] $out"

if ($hbExit -ne 0) { exit $hbExit }
exit $gateExit
