#Requires -RunAsAdministrator
# Rob Run (EasyAntiCheat) — lab session map. Not ranked-safe; EAC-shaped replica + session IOCTLs.
$ErrorActionPreference = 'Stop'
$Driver2 = $PSScriptRoot
$Byovd = Split-Path $Driver2 -Parent
$runner = Join-Path $Byovd 'bin\Driver2Runner.exe'
if (-not (Test-Path $runner)) { $runner = Join-Path $Driver2 'bin\Driver2Runner.exe' }
if (-not (Test-Path $runner)) { throw 'Build first: .\driver2\build.ps1' }
Write-Host '=== Rob Run lab map (EAC / profile=robrun, mode=session) ==='
& $runner --profile robrun --mode session --driver rtcore --skip-rw
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
$client = Join-Path $Byovd 'bin\Driver2Client.exe'
if (Test-Path $client) {
    & $client probe
    exit $LASTEXITCODE
}
