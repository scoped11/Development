#Requires -RunAsAdministrator
# Method TH — Timed AC harness (Rust / R6 test replica with AC running after map, no game)
param(
    [ValidateSet("rust", "r6", "deltaforce", "arenabreakout", "arksa")]
    [string]$Profile = "rust",
    [switch]$SkipRefresh,
    [switch]$Quiet,
    [string]$Driver = "rtcore"
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
Set-Location $Root

$Mapper = Join-Path $Root "bin\mapper_nal.exe"
$Betest = Join-Path $Root "bin\Betest.sys"
$RwExe = Join-Path $Root "bin\process_rw.exe"

if (-not (Test-Path $Mapper) -or -not (Test-Path $Betest)) {
    throw "Build byovd first: .\build.ps1"
}

function Assert-NoAcProcs {
    $p = Get-Process -ErrorAction SilentlyContinue | Where-Object {
        $_.ProcessName -match 'EasyAntiCheat|eac_|BEService|BEClient|bedaisy|SGuard|ACE-Service'
    }
  if ($p) {
        throw "Stop AC replica processes before Phase 2. Running: $($p.ProcessName -join ', ')"
    }
}

Write-Host "[*] Method TH2 — LAB half (docs/16_timed_ac_harness_method.md)"
Write-Host "    Profile: $Profile | Replica VM only — no game"
Write-Host "    LIVE host: keep real EAC/BE running; do NOT run this script there"
Write-Host ""

Write-Host "[Phase 1] AC must be stopped."
Assert-NoAcProcs
if (-not $Quiet) {
    Read-Host "Press Enter when replica is STOPPED and you are ready to map"
}

Write-Host "[Phase 2] Bypass map..."
& (Join-Path $Root "scripts\map-ac-replica-lab.ps1") -Profile $Profile -Mode bypass -Driver $Driver
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host ""
Write-Host "[Phase 3] START your AC replica now (service + driver only — NOT Rust/R6 client)."
if (-not $Quiet) {
    Read-Host "Press Enter when replica is RUNNING"
}

Write-Host "[Phase 4] R/W smoke test..."
& $RwExe
$p4 = $LASTEXITCODE
if ($p4 -ne 0) {
    Write-Warning "Phase 4 failed ($p4). See %TEMP%\pf_diag.tmp"
    exit $p4
}

if ($SkipRefresh) {
    Write-Host "[+] TH complete (refresh skipped). Phase 4 OK."
    exit 0
}

Write-Host "[Phase 5] Refresh IOCTLs (--update)..."
& $Mapper $Betest --driver $Driver --no-auth --update --ac-replica $Profile --ac-replica-bypass
if ($LASTEXITCODE -ne 0) {
    Write-Warning "Phase 5 update exited $LASTEXITCODE"
    exit $LASTEXITCODE
}

Write-Host "[Phase 6] R/W again..."
& $RwExe
$exit = $LASTEXITCODE
if ($exit -eq 0) {
    Write-Host "[+] TH complete: map -> replica on -> R/W OK -> refresh -> R/W OK"
} else {
    Write-Warning "Phase 6 failed ($exit)"
}
exit $exit
