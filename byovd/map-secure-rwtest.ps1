#Requires -RunAsAdministrator
# BYOVD map only — no test-signing / service load.
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $Root

$Mapper = Join-Path $Root "bin\mapper_nal.exe"
$Payload = Join-Path $Root "bin\RwTest.sys"
$DriverFlag = "rtcore"
$manifestPath = Join-Path $Root "vehicles\vehicle.manifest.json"
if (Test-Path $manifestPath) {
    try {
        $manifest = Get-Content $manifestPath -Raw | ConvertFrom-Json
        if ($manifest.mapper_flag) { $DriverFlag = $manifest.mapper_flag }
    } catch { }
}

$VehicleNames = @("RTCore64.sys", "CorsairLLAccess64.sys", "NalDrvIntel.sys", "NalDrv.sys", "iqvw64e.sys")
if ($manifest -and $manifest.file) {
    $VehicleNames = @([string]$manifest.file) + $VehicleNames
}

if (-not (Test-Path $Mapper)) { throw "Run .\build.ps1 first." }
if (-not (Test-Path $Payload)) { throw "Missing bin\RwTest.sys" }

$Vehicle = $null
foreach ($name in $VehicleNames) {
    $candidates = @(
        (Join-Path $Root "vehicles\$name"),
        (Join-Path $Root "bin\$name"),
        (Join-Path $env:TEMP $name)
    )
    foreach ($p in $candidates) {
        if (Test-Path $p) { $Vehicle = $p; break }
    }
    if ($Vehicle) { break }
}
if (-not $Vehicle) {
    throw "No vehicle .sys in vehicles\ — run vehicles\fetch-vehicle.ps1 or install MSI Afterburner / Corsair iCUE"
}

$leaf = Split-Path $Vehicle -Leaf
if ($leaf -match "RTCore") { $DriverFlag = "rtcore" }
elseif ($leaf -match "Corsair") { $DriverFlag = "corsair" }
elseif ($leaf -match "ThrottleStop") { $DriverFlag = "ts" }
elseif ($leaf -match "TRIXX") { $DriverFlag = "trixx" }
elseif ($leaf -match "SIV") { $DriverFlag = "siv" }
elseif ($leaf -match "Nal|iqvw") { $DriverFlag = "nal" }

$binVehicle = Join-Path $Root "bin" (Split-Path $Vehicle -Leaf)
if ($Vehicle -ne $binVehicle) {
    Copy-Item $Vehicle $binVehicle -Force
}

Write-Host "[*] Preflight (Tier 0/1)..."
& (Join-Path $Root "scripts\ac-preflight.ps1") -Driver $DriverFlag
if ($LASTEXITCODE -eq 2) {
    Write-Warning "Tier 0: AC + blocked vehicle — see docs/11_ac_block_vs_hide_plan.md"
    exit 2
}

Write-Host "[*] Secure BYOVD map: RwTest.sys vehicle=$leaf --driver $DriverFlag --ac-safe"
& $Mapper $Payload --driver $DriverFlag --no-auth --ac-safe
$mapExit = $LASTEXITCODE

Write-Host ""
if ($mapExit -eq 0) {
    $rw = Join-Path $Root "bin\process_rw.exe"
    if (Test-Path $rw) {
        Write-Host "[*] IOCTL smoke test..."
        & $rw
        $rwExit = $LASTEXITCODE
        if ($rwExit -ne 0) {
            Write-Warning "process_rw failed ($rwExit). Check $env:TEMP\pf_diag*.tmp before it is deleted."
        }
    }
} else {
    Write-Warning "Mapper exited $mapExit — see $env:TEMP\pf_diag*.tmp"
}

exit $mapExit
