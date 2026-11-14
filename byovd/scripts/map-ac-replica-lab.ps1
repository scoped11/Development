#Requires -RunAsAdministrator
param(
    [ValidateSet("rust", "r6", "deltaforce", "arenabreakout", "arksa")]
    [string]$Profile = "rust",
    [ValidateSet("prove", "ioctl", "bypass")]
    [string]$Mode = "prove",
    [switch]$ForceMap,
    [string]$Driver = "rtcore",
    [switch]$UseDriver2
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
Set-Location $Root

$Mapper = Join-Path $Root "bin\mapper_nal.exe"
$RwTest = Join-Path $Root "bin\RwTest.sys"
$Betest = Join-Path $Root "bin\Betest.sys"
$Driver2 = Join-Path $Root "bin\Driver2.sys"
if (-not (Test-Path $Driver2)) { $Driver2 = Join-Path $Root "driver2\bin\Driver2.sys" }
if ($UseDriver2 -or $Mode -ne 'prove') { $UseDriver2 = $true }
$RwExe = Join-Path $Root "bin\process_rw.exe"

if (-not (Test-Path $Mapper)) { throw "Run .\build.ps1 first." }

$manifest = Join-Path $Root "docs\ac_replica_profiles.json"
if (Test-Path $manifest) {
    Write-Host "[*] Profile spec: $manifest ($Profile)"
}

Write-Host "[*] AC replica lab — profile=$Profile mode=$Mode (docs/15_test_ac_replicas.md)"
Write-Host "[!] Test env only — not a claim of live Rust/R6 bypass"
if ($Mode -eq "bypass") {
    Write-Host "[*] Live-test order: map NOW -> start AC REPLICA only (no game) -> process_rw.exe"
}

$vehicleNames = @("RTCore64.sys", "CorsairLLAccess64.sys", "NalDrvIntel.sys")
foreach ($name in $vehicleNames) {
    $src = Join-Path $Root "vehicles\$name"
    if (Test-Path $src) {
        Copy-Item $src (Join-Path $Root "bin\$name") -Force
        break
    }
}

& (Join-Path $Root "scripts\ac-preflight.ps1") -Driver $Driver
if ($LASTEXITCODE -eq 2) {
    if ($Mode -eq "bypass" -and $ForceMap) {
        Write-Warning "Tier 0 ignored: -ForceMap (live-test order: map may still fail)"
    } elseif ($Mode -eq "bypass") {
        Write-Warning "For bypass: map BEFORE starting replica, or re-run with -ForceMap"
        Write-Warning "Recommended: start replica AFTER this script completes"
        exit 2
    } else {
        Write-Warning "Tier 0: replica blocks vehicle with AC-like processes up."
        Write-Warning "Map on snapshot without replica, or pause replica for prove mode."
        exit 2
    }
}

if ($Mode -eq "prove") {
    if (-not (Test-Path $RwTest)) { throw "Missing RwTest.sys" }
    & $Mapper $RwTest --driver $Driver --no-auth --ac-safe --ac-replica $Profile
    $ex = $LASTEXITCODE
    if ($ex -ne 0) { exit $ex }
    if (Test-Path $RwExe) {
        & $RwExe --emit-manifest
        exit $LASTEXITCODE
    }
    exit 0
}

if ($Mode -eq "bypass") {
    $payload = if ($UseDriver2 -and (Test-Path $Driver2)) { $Driver2 } else { $Betest }
    if (-not (Test-Path $payload)) { throw "Missing payload (Driver2.sys / Betest.sys)" }
    $mapArgs = @($payload, "--driver", $Driver, "--no-auth")
    if ($UseDriver2 -and $payload -like '*Driver2*') { $mapArgs += "--driver2" }
    $mapArgs += @("--ac-replica", $Profile, "--ac-replica-bypass")
    if ($ForceMap) { $mapArgs += "--force-map" }
    & $Mapper @mapArgs
    $ex = $LASTEXITCODE
    if ($ex -ne 0) { exit $ex }
    Write-Host ""
    Write-Host "[+] Bypass map complete. Do NOT start Rust/R6 game clients."
    Write-Host "    1) Start your AC replica / test harness for profile '$Profile'"
    Write-Host "    2) Run: bin\process_rw.exe  (must read marker 0x1122334455667788)"
    Write-Host "    Log: $env:TEMP\pf_diag.tmp"
    $v = & (Join-Path $Root "scripts\verify-pf-diag.ps1") -Profile $Profile
    if (-not $v.ok) { Write-Warning "pf_diag verify: mode=$($v.mode) strip=$($v.strip) 869=$($v.max869)" }
    else { Write-Host "[+] pf_diag verify OK (mode=$($v.mode))" }
    if ($env:AC_REPLICA_AUTO_RW -eq "1" -and (Test-Path $RwExe)) {
        Write-Host "[*] AC_REPLICA_AUTO_RW=1 — running process_rw now"
        & $RwExe
        exit $LASTEXITCODE
    }
    exit 0
}

$payload = if ($UseDriver2 -and (Test-Path $Driver2)) { $Driver2 } else { $Betest }
if (-not (Test-Path $payload)) { throw "Missing payload (Driver2.sys / Betest.sys)" }
$mapArgs = @($payload, "--driver", $Driver, "--no-auth", "--stealth", "3f", "--ac-replica", $Profile, "--ac-replica-ioctl")
if ($UseDriver2 -and $payload -like '*Driver2*') { $mapArgs = @($payload, "--driver", $Driver, "--no-auth", "--driver2", "--ac-replica", $Profile, "--ac-replica-ioctl") }
& $Mapper @mapArgs
exit $LASTEXITCODE
