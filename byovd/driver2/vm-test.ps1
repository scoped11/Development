# VM live-map test — PLAN ONLY until you pass -Execute.
#Requires -RunAsAdministrator
param(
    [ValidateSet('rust', 'r6', 'deltaforce', 'arenabreakout', 'arksa', 'both')]
    [string]$Profile = 'rust',
    [string]$Driver = 'rtcore',
    [switch]$ForceMap,
    [switch]$Execute,
    [switch]$GateOnly
)

$Driver2 = $PSScriptRoot
if ($Profile -eq 'both') {
    if (-not $Execute) {
        Write-Host '[*] VM test (plan): run rust then r6 with -Execute'
        & (Join-Path $Driver2 'everything.ps1') -Profile rust -Target vm -ForceMap:$ForceMap
        & (Join-Path $Driver2 'everything.ps1') -Profile r6 -Target vm -ForceMap:$ForceMap
        exit 0
    }
    foreach ($p in @('rust', 'r6')) {
        & (Join-Path $Driver2 'everything.ps1') -Profile $p -Target vm -Execute -ForceMap:$ForceMap
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    }
    if ($GateOnly -or $Execute) {
        & (Join-Path $Driver2 'realtime-gate.ps1') -Profile both -ForceMap:$ForceMap
    }
    exit $LASTEXITCODE
}

if (-not $Execute) {
    & (Join-Path $Driver2 'everything.ps1') -Profile $Profile -Target vm -ForceMap:$ForceMap
    Write-Host '[*] Optional after map: .\realtime-gate.ps1 -Profile' $Profile
    exit 0
}

& (Join-Path $Driver2 'everything.ps1') -Profile $Profile -Target vm -Execute -ForceMap:$ForceMap
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& (Join-Path $Driver2 'realtime-gate.ps1') -Profile $Profile -ForceMap:$ForceMap
exit $LASTEXITCODE
