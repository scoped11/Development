#Requires -RunAsAdministrator
# Live-test replica bypass (Betest) — does NOT launch Rust or R6.
param(
    [ValidateSet("rust", "r6")]
    [string]$Profile = "rust",
    [switch]$ForceMap,
    [string]$Driver = "rtcore"
)

$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
& (Join-Path $Root "scripts\map-ac-replica-lab.ps1") -Profile $Profile -Mode bypass -Driver $Driver -ForceMap:$ForceMap
