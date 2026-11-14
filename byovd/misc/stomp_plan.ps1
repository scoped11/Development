# Host-side usermode stomp placement preview (no vehicle / no guest).
param(
    [string]$Victim = 'luafv',
    [string]$Payload,
    [string]$ByovdRoot = (Join-Path (Split-Path $PSScriptRoot -Parent) '')
)

$ErrorActionPreference = 'Stop'
if (-not $Payload) {
    $Payload = Join-Path $ByovdRoot 'driver2\bin\Driver2Stomp.sys'
}
$plan = Join-Path $ByovdRoot 'bin\stomp_plan.exe'
if (-not (Test-Path $plan)) {
    Write-Error "Build stomp_plan.exe first: cd $ByovdRoot; .\build.ps1"
}
$build = Join-Path $ByovdRoot 'bin\BUILD.txt'
if (Test-Path $build) { Write-Host "#BUILD $(Get-Content $build -Raw)" }
& $plan $Victim $Payload
exit $LASTEXITCODE