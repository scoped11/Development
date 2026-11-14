# Host: smoke + optional full driver2 map on DriverLab VM.
param(
    [ValidateSet('rust', 'r6')]
    [string]$Profile = 'rust',
    [switch]$Execute,
    [switch]$ForceMap,
    [switch]$SkipProve
)

$ErrorActionPreference = 'Stop'
$Driver3 = $PSScriptRoot

Write-Host '=== VM driver2 test (DriverLab-E-26200) ==='

& (Join-Path $Driver3 'vm-test-from-host.ps1') -Profile $Profile
$smokeEc = $LASTEXITCODE
if ($smokeEc -ne 0 -and $smokeEc -ne 2) {
    Write-Host "[!] Smoke failed exit=$smokeEc"
    exit $smokeEc
}

if (-not $Execute) {
    Write-Host ''
    Write-Host '[*] Smoke OK. Full map (Host B):'
    Write-Host "    .\driver3\vm-test-driver2.ps1 -Profile $Profile -Execute -ForceMap -SkipProve"
    Write-Host '    Requires one-time elevated guest: register-driver3-elevated-task.ps1 (RDP)'
    exit 0
}

& (Join-Path $Driver3 'vm-test-from-host.ps1') -Profile $Profile -Execute -ForceMap:$ForceMap -SkipProve:$SkipProve
exit $LASTEXITCODE
