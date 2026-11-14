#Requires -RunAsAdministrator
param(
    [ValidateSet('rust', 'r6', 'deltaforce', 'arenabreakout', 'arksa')]
    [string]$Profile = 'rust',
    [ValidateSet('prove', 'ioctl', 'bypass')]
    [string]$Mode = 'bypass',
    [string]$Driver = 'rtcore',
    [switch]$ForceMap,
    [switch]$SkipRw
)

$ErrorActionPreference = 'Stop'
$Driver2 = $PSScriptRoot
$Runner = Join-Path $Driver2 'bin\Driver2Runner.exe'
if (-not (Test-Path $Runner)) {
    $shared = Join-Path (Split-Path $Driver2 -Parent) 'bin\Driver2Runner.exe'
    if (Test-Path $shared) { $Runner = $shared }
    else { & (Join-Path $Driver2 'build.ps1') }
}

$args = @('--profile', $Profile, '--mode', $Mode, '--driver', $Driver)
if ($ForceMap) { $args += '--force-map' }
if ($SkipRw) { $args += '--skip-rw' }
& $Runner @args
exit $LASTEXITCODE
