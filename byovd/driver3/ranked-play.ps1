# Legacy alias — use ranked.ps1
param(
    [string]$Profile = 'rust',
    [switch]$Execute,
    [string]$BuildId = '',
    [switch]$ForceMap,
    [switch]$SyncBuildId
)

$Driver3 = $PSScriptRoot
if (-not $ForceMap) {
    $a = @('-Game', $Profile)
    if ($Execute) { $a += '-Execute' }
    if ($BuildId) { $a += '-BuildId'; $a += $BuildId }
    if ($SyncBuildId) { $a += '-SyncBuildId' }
    & (Join-Path $Driver3 'ranked.ps1') @a
    exit $LASTEXITCODE
}

. (Join-Path $Driver3 'Resolve-Driver3Game.ps1')
$profile = Resolve-Driver3Profile $Profile
$meta = Get-Driver3Meta $profile
$hb = Get-Driver3HostBDeveloperFlags $profile

if ($ForceMap) {
    Write-Host "[!] ForceMap on ranked — Tier 0 risk for $($meta.ac_family). Use driver2 only if intentional."
    $a = @('-Profile', $hb.Profile, '-Execute', '-ForceMap')
    & (Join-Path (Split-Path $Driver3 -Parent) 'driver2\ranked-play.ps1') @a
    exit $LASTEXITCODE
}

Write-Host "[*] Ranked: $($meta.title)"
& (Join-Path $Driver3 'lane-gate.ps1') -Game $profile
& (Join-Path $Driver3 'host-a.ps1') -Profile $profile -BuildId $BuildId -Execute:$Execute
exit $LASTEXITCODE
