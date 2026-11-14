#Requires -RunAsAdministrator
# driver2 confidence gate — must pass on DriverLab VM for claimed 85–90% realtime (replica) confidence.
param(
    [ValidateSet('rust', 'r6', 'both')]
    [string]$Profile = 'both',
    [string]$Driver = 'rtcore',
    [switch]$ForceMap,
    [string]$OutJson = ''
)

$ErrorActionPreference = 'Stop'
$Driver2 = $PSScriptRoot
$Byovd = Split-Path $Driver2 -Parent
if (-not $OutJson) {
    $OutJson = Join-Path $Driver2 'confidence.json'
}

function Score-Checks {
    param($Checks)
    $n = $Checks.Count
    $ok = ($Checks | Where-Object { $_.pass }).Count
    [math]::Round(100.0 * $ok / [math]::Max(1, $n), 1)
}

& (Join-Path $Byovd 'build.ps1')
& (Join-Path $Driver2 'build.ps1')

$veh = Join-Path $Byovd 'vehicles\RTCore64.sys'
if (Test-Path $veh) { Copy-Item $veh (Join-Path $Byovd 'bin\RTCore64.sys') -Force }

$preflight = Join-Path $Byovd 'scripts\ac-preflight.ps1'
& $preflight -Driver $Driver
$pfExit = $LASTEXITCODE

$profiles = if ($Profile -eq 'both') { @('rust', 'r6') } else { @($Profile) }
$results = @()

foreach ($p in $profiles) {
    $args = @('--profile', $p, '--mode', 'bypass', '--driver', $Driver)
    if ($ForceMap) { $args += '--force-map' }
    $runner = Join-Path $Byovd 'bin\Driver2Runner.exe'
    if (-not (Test-Path $runner)) { $runner = Join-Path $Driver2 'bin\Driver2Runner.exe' }
    & $runner @args
    $mapExit = $LASTEXITCODE

    $diag = & (Join-Path $Byovd 'scripts\verify-pf-diag.ps1') -Profile $p
    $rwExe = Join-Path $Byovd 'bin\process_rw.exe'
    $rwExit = $null
    if (Test-Path $rwExe) {
        & $rwExe
        $rwExit = $LASTEXITCODE
    }

    $hygieneLine = $false
    $pfPath = Join-Path $env:TEMP 'pf_diag.tmp'
    if (Test-Path $pfPath) {
        $t = Get-Content $pfPath -Raw
        $hygieneLine = $t -match 'Hygiene:|PiDDB|WdFilter|hash bucket'
    }

    $checks = @(
        @{ name = 'map_exit_0'; pass = ($mapExit -eq 0) }
        @{ name = 'pf_diag_bypass'; pass = $diag.ok }
        @{ name = 'strip'; pass = $diag.strip }
        @{ name = 'max869'; pass = $diag.max869 }
        @{ name = 'process_rw'; pass = ($rwExit -eq 0) }
        @{ name = 'preflight_not_tier0'; pass = ($pfExit -ne 2) }
        @{ name = 'hygiene_log'; pass = $hygieneLine }
    )
    $score = Score-Checks $checks
    $pass = ($score -ge 85.0) -and ($mapExit -eq 0) -and ($rwExit -eq 0) -and $diag.ok

    $results += [pscustomobject]@{
        profile     = $p
        map_exit    = $mapExit
        rw_exit     = $rwExit
        pf_diag_ok  = $diag.ok
        checks      = $checks
        score_pct   = $score
        pass_85     = $pass
    }
}

$allPass = ($results | Where-Object { -not $_.pass_85 }).Count -eq 0
$avg = if ($results.Count) { [math]::Round(($results | Measure-Object score_pct -Average).Average, 1) } else { 0 }

$report = @{
    schema           = 'driver2_realtime_confidence_v1'
    utc              = (Get-Date).ToUniversalTime().ToString('o')
    hostname         = $env:COMPUTERNAME
    preflight_exit   = $pfExit
    avg_score_pct    = $avg
    target_pct       = 85
    all_pass_85      = $allPass
    claimed_band     = if ($allPass) { '85-90' } else { 'below-85' }
    profiles         = $results
    note             = 'VM replica realtime only — not live ranked Rust/R6'
}

$report | ConvertTo-Json -Depth 6 | Set-Content $OutJson -Encoding UTF8
Write-Host "[*] Wrote $OutJson avg=$avg% all_pass_85=$allPass"

if (-not $allPass) { exit 1 }
exit 0
