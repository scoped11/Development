# Ranked-safe map: Tier 1 hygiene, ioctl-only (--ac-safe), no bypass sweep. Fail-closed Tier 0.
#Requires -RunAsAdministrator
param(
    [ValidateSet('rust', 'r6', 'deltaforce', 'arenabreakout', 'arksa')]
    [string]$Profile = 'rust',
    [string]$Driver = 'rtcore',
    [switch]$Live,
    [switch]$AllowTier0Override,
    [switch]$SkipProbe
)

$ErrorActionPreference = 'Stop'
$Driver2 = $PSScriptRoot
$Byovd = Split-Path $Driver2 -Parent

. (Join-Path $Driver2 'lib\Invoke-Driver2RankedSafeGate.ps1')
$gate = Invoke-Driver2RankedSafeGate -Profile $Profile -Driver $Driver -Live:$Live `
    -AllowTier0Override:$AllowTier0Override

Write-Host "=== driver2 RANKED SAFE map profile=$Profile tier=$($gate.tier) ==="
Write-Host "    Policy: session ioctl + --ac-safe (no bypass sweep)"
Write-Host "    NOT ban-proof — fewer lab traces, same live AC risk if -Live"

if ($gate.blocked) {
    $gate.reasons | ForEach-Object { Write-Host "[!] $_" }
    Write-Host '[!] Ranked-safe blocked. Options:'
    Write-Host '    • Close game AC client and map before launch (lab order)'
    Write-Host '    • VM snapshot without AC (docs/11_ac_block_vs_hide_plan.md)'
    Write-Host '    • Unsafe: ranked-play.ps1 -ForceMap (not ranked-safe)'
    exit 2
}

$env:DRIVER2_RANKED_OVERRIDE = '1'
$env:DRIVER2_RANKED_SAFE = '1'

& (Join-Path $Byovd 'build.ps1') | Out-Null
& (Join-Path $Driver2 'build.ps1') | Out-Null

$veh = Join-Path $Byovd 'vehicles\RTCore64.sys'
if (Test-Path $veh) { Copy-Item $veh (Join-Path $Byovd 'bin\RTCore64.sys') -Force }

$runner = Join-Path $Byovd 'bin\Driver2Runner.exe'
if (-not (Test-Path $runner)) { $runner = Join-Path $Driver2 'bin\Driver2Runner.exe' }

& $runner --profile $Profile --mode session --driver $Driver --skip-rw
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

. (Join-Path $Driver2 'lib\Invoke-Driver2RankedVerify.ps1')
if (-not $SkipProbe) {
    $v = Invoke-Driver2RankedVerify -Profile $Profile
    if (-not $v.ok) {
        Write-Host '[!] Driver2Client probe failed after ranked-safe map'
        exit $v.probe_exit
    }
    Write-Host '[+] ranked-safe map + probe OK'
}

. (Join-Path $Driver2 'lib\Invoke-Driver2DumpTrackingApply.ps1')
$track = Invoke-Driver2DumpTrackingApply -Profile $Profile -MapMode session
Write-Host "[*] dump tracking ok=$($track.ok) tier=$($track.policy_tier)"
if (-not $track.ok) {
    $track.issues | ForEach-Object { Write-Host "    [!] $_" }
    Write-Host '    See driver2_dump_tracking.json + AC_FINGERPRINTS.md'
    exit 8
}

exit 0
