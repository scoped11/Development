# CI / pre-session: titles + manifest + live ready (no undetected claim).
param(
    [switch]$AllTitles,
    [string]$Game = 'rust'
)

$Driver3 = $PSScriptRoot
$profiles = if ($AllTitles) { 'rust', 'r6', 'arksa', 'deltaforce', 'arenabreakout' } else { ,$Game }

& (Join-Path $Driver3 'developer-nod.ps1') -AllTitles:$AllTitles -Game $Game -SkipVm
exit $LASTEXITCODE
