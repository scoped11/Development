# Host: build HijackDriver, ensure byovd bin on share, run inside guest (Guest Additions + login).
param(
    [string]$VmName = 'DriverLab-E-26200',
    [ValidateSet('rust', 'r6', 'deltaforce', 'arenabreakout', 'arksa')]
    [string]$Profile = 'rust',
    [switch]$SkipBuild,
    [switch]$SkipByovdBuild,
    [int]$TimeoutMs = 600000
)

$ErrorActionPreference = 'Stop'
$VmDir = $PSScriptRoot
$Repo = Resolve-Path (Join-Path $VmDir '..\..')
$Byovd = Join-Path $Repo 'byovd'

if (-not $SkipBuild) {
    & (Join-Path $VmDir 'tools\build-hijack-driver.ps1')
}

if (-not $SkipByovdBuild) {
    foreach ($f in @('mapper_nal.exe', 'RwTest.sys')) {
        if (-not (Test-Path (Join-Path $Byovd "bin\$f"))) {
            Write-Host '[*] Building byovd...'
            & (Join-Path $Byovd 'build.ps1')
            break
        }
    }
    $veh = Join-Path $Byovd 'vehicles\RTCore64.sys'
    if (Test-Path $veh) {
        Copy-Item $veh (Join-Path $Byovd 'bin\RTCore64.sys') -Force
    }
}

$runnerHost = Join-Path $VmDir 'bin\HijackDriver.exe'
if (-not (Test-Path $runnerHost)) { throw "Missing $runnerHost" }

Write-Host "[*] VM=$VmName profile=$Profile"
Write-Host "[*] Guest: D:\hostdev\driver-lab\vm\bin\HijackDriver.exe --profile $Profile --map"

$guestExe = 'D:\hostdev\driver-lab\vm\bin\HijackDriver.exe'
$exec = Join-Path $VmDir 'host-guest-exec.ps1'
& $exec -VmName $VmName -TimeoutMs $TimeoutMs -Cwd 'D:\hostdev\driver-lab\vm\bin' -Command @(
    $guestExe, '--profile', $Profile, '--map'
)
exit $LASTEXITCODE
