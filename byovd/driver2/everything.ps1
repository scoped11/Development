# driver2 full stack: preflight → (prove) → ioctl → bypass → forensics.
# Does NOT map unless -Execute (wait for your go-ahead).
#Requires -RunAsAdministrator
param(
    [string]$Profile = 'rust',
    [ValidateSet('vm', 'ranked')]
    [string]$Target = 'vm',
    [string]$Driver = 'rtcore',
    [switch]$SkipProve,
    [switch]$ForceMap,
    [switch]$Execute
)

$ErrorActionPreference = 'Stop'
$Driver2 = $PSScriptRoot
$Byovd = Split-Path $Driver2 -Parent
. (Join-Path $Driver2 'Resolve-Driver2Profile.ps1')
$Profile = Resolve-Driver2Profile $Profile

$plan = @(
    'build: byovd\build.ps1 + driver2\build.ps1'
    'preflight: scripts\ac-preflight.ps1'
    $(if (-not $SkipProve) { 'map: Driver2Runner --mode prove' })
    'map: Driver2Runner --mode ioctl  (--driver2 full IOCTL matrix)'
    'map: Driver2Runner --mode bypass (--driver2 ioctl+bypass sweep, 0x808)'
    'forensics: driver2\forensics-capture.ps1'
)

Write-Host '=== driver2 EVERYTHING (bypass + ioctl + forensics) ==='
Write-Host "Target=$Target Profile=$Profile Driver=$Driver Execute=$Execute"
Write-Host ''
Write-Host 'Planned steps:'
$plan | ForEach-Object { Write-Host "  - $_" }

if ($Target -eq 'ranked') {
    Write-Host ''
    Write-Host '[!] RANKED: Tier 0 likely if AC is up. Use DMA for live reads unless you accept map risk.'
    Write-Host '    lane-d-preflight will run before map when -Execute.'
}

if (-not $Execute) {
    Write-Host ''
    Write-Host '[*] PLAN ONLY — no map. When ready, re-run with -Execute'
    Write-Host "    Example VM:    .\everything.ps1 -Profile rust -Target vm -Execute -ForceMap"
    Write-Host '    Ranked live:   driver3\ranked-play.ps1 (Host A DMA only — no driver2 map)'
    exit 0
}

if ($Target -eq 'ranked') {
    $driver3 = Join-Path $Byovd 'driver3\ranked-play.ps1'
    if ($env:DRIVER2_RANKED_OVERRIDE -ne '1') {
        Write-Host '[!] FAIL-CLOSED: driver2 ranked map blocked. Use driver3 Host A (DMA).'
        Write-Host "    .\driver3\ranked-play.ps1 -Profile $Profile -Execute"
        if (Test-Path $driver3) { & $driver3 -Profile $Profile -Execute; exit $LASTEXITCODE }
        exit 40
    }
    Write-Warning 'DRIVER2_RANKED_OVERRIDE=1 — lab exception only; not Scale A'
}

& (Join-Path $Byovd 'build.ps1')
& (Join-Path $Driver2 'build.ps1')

$veh = Join-Path $Byovd 'vehicles\RTCore64.sys'
if (Test-Path $veh) { Copy-Item $veh (Join-Path $Byovd 'bin\RTCore64.sys') -Force }

& (Join-Path $Byovd 'scripts\ac-preflight.ps1') -Driver $Driver
if ($LASTEXITCODE -eq 2 -and $Target -eq 'ranked') {
    Write-Warning 'Tier 0 (AC + no vehicle). Continue only if intentional (-ForceMap does not remove block).'
}
if ($LASTEXITCODE -eq 2 -and $Target -eq 'vm' -and -not $ForceMap) {
    throw 'Tier 0 on VM — stop replica AC or use -ForceMap for lab'
}

if ($Target -eq 'ranked') {
  if (Test-Path (Join-Path $Byovd 'scripts\lane-d-preflight.ps1')) {
    & (Join-Path $Byovd 'scripts\lane-d-preflight.ps1') -Game $Profile
    Write-Host "[*] lane-d preflight exit=$LASTEXITCODE (see 12_lane_d_split_trust.md)"
  }
}

$runner = Join-Path $Byovd 'bin\Driver2Runner.exe'
if (-not (Test-Path $runner)) { $runner = Join-Path $Driver2 'bin\Driver2Runner.exe' }

function Invoke-D2 {
    param([string]$Mode, [switch]$SkipRw)
    $a = @('--profile', $Profile, '--mode', $Mode, '--driver', $Driver)
    if ($ForceMap) { $a += '--force-map' }
    if ($SkipRw) { $a += '--skip-rw' }
    & $runner @a
    if ($LASTEXITCODE -ne 0) { throw "Driver2Runner $Mode failed exit $LASTEXITCODE" }
}

if (-not $SkipProve) {
    Write-Host '[*] Step prove (RwTest hygiene baseline)'
    Invoke-D2 -Mode prove
}

Write-Host '[*] Step ioctl (Driver2 + --driver2 title matrix)'
Invoke-D2 -Mode ioctl -SkipRw

Write-Host '[*] Step bypass (Driver2 everything sweep — revert snapshot after)'
Invoke-D2 -Mode bypass

$client = Join-Path $Byovd 'bin\Driver2Client.exe'
if (Test-Path $client) {
    & $client probe
    if ($LASTEXITCODE -ne 0) { throw 'Driver2Client probe failed after everything sequence' }
}

$out = & (Join-Path $Driver2 'forensics-capture.ps1') -Profile $Profile
Write-Host "[+] Everything complete. Forensics: $out"
exit 0
