#Requires -RunAsAdministrator
# VM-only gate: prove → ioctl → bypass + pf_diag + process_rw. Records pass/fail for EAC/BE parity.
param(
    [ValidateSet('rust', 'r6', 'deltaforce', 'arenabreakout', 'arksa', 'both', 'ace', 'all')]
    [string]$Profile = 'both',
    [string]$Driver = 'rtcore',
    [switch]$SkipProve,
    [switch]$ForceMap,
    [switch]$WithTimedHarness,
    [string]$OutJson = ''
)

$ErrorActionPreference = 'Stop'
$Repo = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$Byovd = Join-Path $Repo 'byovd'
if (-not $OutJson) {
    $OutJson = Join-Path $Repo 'driver-lab\exploit\notes\vm-parity-gate.json'
}

function Ensure-ByovdBin {
    & (Join-Path $Byovd 'build.ps1')
    $fetch = Join-Path $Byovd 'vehicles\fetch-vehicle.ps1'
    if (Test-Path $fetch) { & $fetch -Driver $Driver -ErrorAction SilentlyContinue }
    foreach ($name in @('RTCore64.sys', 'CorsairLLAccess64.sys', 'NalDrvIntel.sys')) {
        $src = Join-Path $Byovd "vehicles\$name"
        if (Test-Path $src) {
            Copy-Item $src (Join-Path $Byovd "bin\$name") -Force
            break
        }
    }
}

function Run-Mode {
    param([string]$P, [string]$Mode)
    $map = Join-Path $Byovd 'scripts\map-ac-replica-lab.ps1'
    $args = @{ Profile = $P; Mode = $Mode; Driver = $Driver }
    if ($ForceMap) { $args.ForceMap = $true }
    & $map @args
    $exit = $LASTEXITCODE
    $diag = & (Join-Path $Byovd 'scripts\verify-pf-diag.ps1') -Profile $P
    $rw = $null
    $rwExe = Join-Path $Byovd 'bin\process_rw.exe'
    if (Test-Path $rwExe) {
        & $rwExe --emit-manifest
        $rw = $LASTEXITCODE
    }
    [pscustomobject]@{
        profile   = $P
        mode      = $Mode
        map_exit  = $exit
        pf_diag   = $diag
        rw_exit   = $rw
        pass      = ($exit -eq 0) -and $diag.ok -and (($Mode -ne 'prove') -or ($rw -eq 0))
    }
}

$profiles = switch ($Profile) {
    'both' { @('rust', 'r6') }
    'ace'  { @('deltaforce', 'arenabreakout') }
    'all'  { @('rust', 'r6', 'deltaforce', 'arenabreakout') }
    default { @($Profile) }
}

Write-Host '[*] VM parity gate — VM only (not Host A ranked)'

$acProcs = Get-Process -ErrorAction SilentlyContinue | Where-Object {
    $_.ProcessName -match 'EasyAntiCheat|eac_|BEService|BEClient|RustClient|RainbowSix|SGuard|ACE-Service|DeltaForce|ABInfinite'
}
if ($acProcs) {
    throw "AC/game processes running — run this only inside DriverLab VM (revert snapshot). Found: $($acProcs.ProcessName -join ', ')"
}
if ($env:COMPUTERNAME -notmatch 'driverlab|DRIVERLAB') {
    Write-Warning "Hostname '$($env:COMPUTERNAME)' is not DriverLab — continuing only if you are on the lab VM"
}

Ensure-ByovdBin

$results = @()
foreach ($p in $profiles) {
    if (-not $SkipProve) {
        $results += Run-Mode -P $p -Mode 'prove'
    }
    $results += Run-Mode -P $p -Mode 'ioctl'
    $results += Run-Mode -P $p -Mode 'bypass'
}

$allPass = -not @($results | Where-Object { -not $_.pass }).Count

if ($WithTimedHarness -and $allPass) {
    foreach ($p in $profiles) {
        Write-Host "[*] Timed harness (TH) profile=$p — manual replica start between prompts"
        & (Join-Path $Byovd 'scripts\run-timed-harness.ps1') -Profile $p -Driver $Driver
        $results += [pscustomobject]@{
            profile = $p
            mode    = 'timed_harness_TH'
            pass    = ($LASTEXITCODE -eq 0)
            map_exit = $LASTEXITCODE
        }
    }
}

$report = @{
    captured_utc = (Get-Date).ToUniversalTime().ToString('o')
    host         = $env:COMPUTERNAME
    vm_gate      = 'lab_L2_not_live_L3'
    disclaimer   = 'Pass = ioctl chain + pf_diag on snapshot VM. Production Rust/R6 not guaranteed.'
    all_pass     = $allPass
    results      = $results
}
$dir = Split-Path $OutJson
if ($dir) { New-Item -ItemType Directory -Force -Path $dir | Out-Null }
$report | ConvertTo-Json -Depth 6 | Set-Content $OutJson -Encoding UTF8

# Merge into lab_manifest if present
$manifest = Join-Path $Repo 'byovd\docs\lab_manifest.json'
if (Test-Path $manifest) {
    $m = Get-Content $manifest -Raw | ConvertFrom-Json
    if (-not $m.PSObject.Properties['vm_bypass_gate']) {
        $m | Add-Member -NotePropertyName 'vm_bypass_gate' -NotePropertyValue @{}
    }
    $m.vm_bypass_gate = @{
        last_utc = $report.captured_utc
        all_pass = $allPass
        json     = $OutJson
    }
    $m | ConvertTo-Json -Depth 8 | Set-Content $manifest -Encoding UTF8
}

foreach ($r in $results) {
    $st = if ($r.pass) { 'PASS' } else { 'FAIL' }
    Write-Host "[$st] $($r.profile) $($r.mode) map=$($r.map_exit) rw=$($r.rw_exit)"
}

if (-not $allPass) {
    Write-Warning 'Gate failed — revert VM snapshot, read pf_diag.tmp, fix vehicle/HVCI/blocklist'
    exit 1
}

Write-Host "[+] VM parity gate OK -> $OutJson"
Write-Host '[+] For replica-running confidence: re-run with -WithTimedHarness after EAC/BE test service in VM'
exit 0
