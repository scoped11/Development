# Print path to per-title route card (method / route / R/W / tiers).
param([string]$Profile = 'rust')

$Driver3 = $PSScriptRoot
. (Join-Path $Driver3 'Resolve-Driver3Game.ps1')
$p = Resolve-Driver3Profile $Profile
$path = Show-Driver3RouteDoc $p
if ($path) {
    Write-Output $path
    Get-Content $path -TotalCount 8
    exit 0
}
exit 1
