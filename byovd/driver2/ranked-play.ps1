# Ranked session entry: driver2 EVERYTHING on the machine you are on.
# Default PLAN ONLY — pass -Execute when you explicitly approve live map.
#Requires -RunAsAdministrator
param(
    [ValidateSet('rust', 'r6', 'deltaforce', 'arenabreakout', 'arksa')]
    [string]$Profile = 'rust',
    [string]$Driver = 'rtcore',
    [switch]$SkipProve,
    [switch]$ForceMap,
    [switch]$Execute,
    [switch]$DmaOnly
)

if ($DmaOnly) {
    Write-Host '[*] Ranked reads: driver3 Host A — dma/ overlay, no kernel map.'
    $d3 = Join-Path $PSScriptRoot '..\driver3\ranked-play.ps1'
    if (Test-Path $d3) { & $d3 -Profile $Profile -Execute }
    else { Write-Host '    (driver3 not found — use dma/ manually)' }
    exit 0
}

if (-not $ForceMap) {
    $d3 = Join-Path $PSScriptRoot '..\driver3\ranked-play.ps1'
    if (Test-Path $d3) { & $d3 -Profile $Profile -Execute:$Execute; exit $LASTEXITCODE }
}
& (Join-Path $PSScriptRoot 'everything.ps1') `
    -Profile $Profile `
    -Target ranked `
    -Driver $Driver `
    -SkipProve:$SkipProve `
    -ForceMap:$ForceMap `
    -Execute:$Execute
exit $LASTEXITCODE
