#Requires -RunAsAdministrator
# VM lab must match **live EAC/BE ioctl + map flags**, not a stripped teaching driver.
# Run in guest: D:\hostdev\driver-lab\vm\setup-vm-lab-betest.ps1
param(
    [ValidateSet('prove', 'ioctl', 'bypass')]
    [string]$Mode = 'bypass',
    [ValidateSet('rust', 'r6', 'deltaforce', 'arenabreakout', 'arksa')]
    [string]$Profile = 'rust',
    [string]$Driver = 'rtcore',
    [switch]$ForceMap,
    [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'
$Repo = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$Byovd = Join-Path $Repo 'byovd'

if (-not $SkipBuild) {
    $need = @(
        (Join-Path $Byovd 'bin\mapper_nal.exe'),
        (Join-Path $Byovd 'bin\Betest.sys'),
        (Join-Path $Byovd 'bin\process_rw.exe')
    )
    if (@($need | Where-Object { -not (Test-Path $_) }).Count -gt 0) {
        Write-Host '[*] Building byovd (Betest + mapper)…'
        & (Join-Path $Byovd 'build.ps1')
    }
}

$map = Join-Path $Byovd 'scripts\map-ac-replica-lab.ps1'
if (-not (Test-Path $map)) { throw "Missing $map" }

Write-Host "[*] VM lab parity: profile=$Profile mode=$Mode (live ioctl surface via Betest)"
Write-Host "    Ref: byovd\docs\15_test_ac_replicas.md, 05_existing_bypass_catalog.md"

$args = @{
    Profile  = $Profile
    Mode     = $Mode
    Driver   = $Driver
    ForceMap = $ForceMap
}
& $map @args
$ex = $LASTEXITCODE

$note = Join-Path $Repo 'driver-lab\exploit\notes\vm-betest-lab.json'
@{
    profile      = $Profile
    mode         = $Mode
    driver       = $Driver
    exit_code    = $ex
    captured_utc = (Get-Date).ToUniversalTime().ToString('o')
    parity       = 'Betest + ac-replica flags (not VmLab-only)'
} | ConvertTo-Json | Set-Content $note -Encoding UTF8

Write-Host "[+] Logged $note"

if ($Mode -eq 'bypass' -and $env:VM_LAB_FULL_GATE -eq '1') {
    & (Join-Path $PSScriptRoot 'vm-lab-parity-gate.ps1') -Profile $Profile -ForceMap:$ForceMap
    exit $LASTEXITCODE
}
exit $ex
