# Start kernel + usermode for active 1:1 profile (call after bypass map per docs/15_test_ac_replicas.md).
#Requires -RunAsAdministrator
param(
    [ValidateSet('rust', 'r6', 'deltaforce', 'arenabreakout')]
    [string]$Profile,
    [switch]$StrictKernel
)
$strict = @{}
if ($StrictKernel) { $strict.StrictKernel = $true }
& (Join-Path $PSScriptRoot 'Install-AcReplicaL2.ps1') -Profile $Profile -LoadKernel -StartUsermode @strict
exit $LASTEXITCODE