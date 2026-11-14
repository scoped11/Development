#Requires -RunAsAdministrator
<#
  One host pipeline: RTCore only, no TRIXX, no mapper vehicle fallback.
  1) Quit Afterburner UI (keeps RTCore .sys on disk)
  2) Tier-0 AC preflight (optional -ForceMap)
  3) RwTest prove (--rw-only, --no-fallback)
  4) Driver2 session map
  5) Driver2Client probe
  6) Confirm RtCoreSvc stopped (mapper post-cleanup)

  Used by DriverLoader GUI. TRIXX and random peripherals are not used.

.EXAMPLE
  .\driver2\map-host-lab.ps1 -Profile robrun
  .\driver2\map-host-lab.ps1 -Profile universal -SkipProve
#>
param(
    [ValidateSet('universal', 'robrun', 'rust', 'r6')]
    [string]$Profile = 'universal',
    [switch]$ProveFirst,
    [switch]$ForceMap
)

$ErrorActionPreference = 'Stop'
$Driver2 = (Resolve-Path $PSScriptRoot).Path
$Byovd = Split-Path $Driver2 -Parent
$D2Bin = Join-Path $Driver2 'bin'
& (Join-Path $Driver2 'sync-driver2-bin.ps1') | Out-Null
Set-Location $D2Bin

$Runner = Join-Path $D2Bin 'Driver2Runner.exe'
$Client = Join-Path $D2Bin 'Driver2Client.exe'
$Mapper = Join-Path $D2Bin 'mapper_nal.exe'
if (-not (Test-Path $Mapper)) { $Mapper = Join-Path $Byovd 'bin\mapper_nal.exe' }
$RwTest = Join-Path $D2Bin 'RwTest.sys'
if (-not (Test-Path $RwTest)) { $RwTest = Join-Path $Byovd 'bin\RwTest.sys' }
$ProcRw = Join-Path $D2Bin 'process_rw.exe'

function Test-Driver2Ready {
    if (-not (Test-Path $Client)) { return $false }
    & $Client probe 2>$null | Out-Null
    return $LASTEXITCODE -eq 0
}

if (Test-Driver2Ready) {
    Write-Host '[+] Driver2 already mapped — skipping BYOVD' -ForegroundColor Green
    exit 0
}

foreach ($t in @($Runner, $Mapper, $RwTest)) {
    if (-not (Test-Path $t)) { throw "Missing $t — run byovd\build.ps1 and driver2\build.ps1" }
}

$vehScript = Join-Path $Byovd 'vehicles\fetch-vehicle.ps1'
$rtVeh = Join-Path $Byovd 'vehicles\RTCore64.sys'
if (-not (Test-Path $rtVeh)) { $rtVeh = Join-Path $D2Bin 'RTCore64.sys' }
if (-not (Test-Path $rtVeh) -and (Test-Path $vehScript)) {
    Write-Host '[*] Copying RTCore from MSI Afterburner...'
    & $vehScript 1
}

Write-Host '[*] Stopping MSI Afterburner UI (not your saved OC profile)...'
Get-Process -Name 'MSIAfterburner','MSIAfterburnerSetup' -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 400

$ac = Join-Path $Byovd 'scripts\ac-preflight.ps1'
if (Test-Path $ac) {
    & $ac -Driver rtcore
    if ($LASTEXITCODE -eq 2 -and -not $ForceMap) {
        Write-Warning 'AC preflight Tier 0 — close Rob Run / EAC games, or re-run with -ForceMap'
        exit 2
    }
}

if ($ProveFirst) {
    Write-Host '[*] Step 1/2: RwTest prove (RTCore, rw-only, no vehicle fallback)...'
    $binRt = Join-Path $D2Bin 'RTCore64.sys'
    if ((Test-Path $rtVeh) -and (-not (Test-Path $binRt))) {
        Copy-Item $rtVeh $binRt -Force -ErrorAction SilentlyContinue
    }
    & $Mapper $RwTest --driver rtcore --no-auth --ac-safe --rw-only --no-fallback
    if ($LASTEXITCODE -ne 0) {
        Write-Warning "RwTest prove failed ($LASTEXITCODE). Fix RTCore before full Driver2 map."
        Write-Host '    Quit game AC, reboot, run vehicles\fetch-vehicle.ps1, retry.'
        exit 1
    }
    if (Test-Path $ProcRw) {
        & $ProcRw
        if ($LASTEXITCODE -ne 0) {
            Write-Warning "process_rw failed ($LASTEXITCODE)"
            exit 1
        }
    }
    Write-Host '[+] RwTest prove OK — Driver2 map next' -ForegroundColor Green
    Start-Sleep -Seconds 1
}

Write-Host "[*] Step 2/2: Driver2 session map (profile=$Profile, driver=rtcore)..."
& $Runner --profile $Profile --mode session --driver rtcore --skip-rw
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Start-Sleep -Seconds 1
& $Client probe
if ($LASTEXITCODE -ne 0) {
    Write-Warning 'Driver2 probe failed after map'
    exit 1
}

$rtSvc = Get-Service -Name 'RtCoreSvc' -ErrorAction SilentlyContinue
if ($rtSvc -and $rtSvc.Status -eq 'Running') {
    Write-Warning 'RtCoreSvc still running — mapper cleanup may have failed; stop Afterburner and: sc stop RtCoreSvc'
} else {
    Write-Host '[+] RtCoreSvc not running (expected after map)' -ForegroundColor Green
}

Write-Host '[+] Host lab map complete — open CheatLoader' -ForegroundColor Green
exit 0
