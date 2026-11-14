# driver3 — split-trust for all titles (see games.json).
param(
    [string]$Profile = 'rust',
    [ValidateSet('auto', 'A', 'B', 'both')]
    [string]$WhichHost = 'auto',
    [string]$BuildId = '',
    [switch]$Execute,
    [switch]$ForceMap,
    [switch]$Inspector,
    [switch]$FullAcMatrix,
    [switch]$SkipProve,
    [switch]$AllTitles
)

$ErrorActionPreference = 'Stop'
$Driver3 = $PSScriptRoot
. (Join-Path $Driver3 'Resolve-Driver3Game.ps1')

if ($AllTitles) {
    $worst = 0
    foreach ($p in (Get-Driver3ProfileNames)) {
        Write-Host "=== everything: $p ==="
        & $PSCommandPath -Profile $p -WhichHost $WhichHost -BuildId $BuildId -Execute:$Execute -ForceMap:$ForceMap `
            -Inspector:$Inspector -FullAcMatrix:$FullAcMatrix -SkipProve:$SkipProve
        if ($LASTEXITCODE -gt $worst) { $worst = $LASTEXITCODE }
    }
    exit $worst
}

$profile = Resolve-Driver3Profile $Profile
$meta = Get-Driver3Meta $profile

Write-Host '=== driver3 EVERYTHING (Lane D split-trust) ==='
Write-Host "Title: $($meta.title)"
Write-Host "Profile=$profile AC=$($meta.ac_family) WhichHost=$WhichHost Execute=$Execute"
Write-Host ''
Write-Host '  Host A: dma live reads, no map on Tier 0'
Write-Host "  Host B: driver2 ($($meta.driver2_profile_file))"
Write-Host '  Sync:   docs/lab_manifest.json lane_d.* + by_profile'

& (Join-Path $Driver3 'lane-gate.ps1') -Game $profile
$gateExit = $LASTEXITCODE
$gateJson = Join-Path $Driver3 'driver3_gate.json'
$recommend = 'both'
if (Test-Path $gateJson) {
    $g = Get-Content $gateJson -Raw | ConvertFrom-Json
    if ($WhichHost -eq 'auto') {
        $recommend = switch ($g.recommend_host) {
            'A' { 'A' }
            'B' { 'B' }
            default { 'both' }
        }
    } else {
        $recommend = $WhichHost
    }
} elseif ($WhichHost -ne 'auto') {
    $recommend = $WhichHost
}

if (-not $Execute) {
    Write-Host "[*] PLAN — recommend workload host: $recommend (gate exit=$gateExit)"
    Write-Host "    Host A: .\host-a.ps1 -Profile $profile -Execute"
    Write-Host "    Host B: .\host-b.ps1 -Profile $profile -Execute -ForceMap"
    Write-Host '    All titles: .\everything.ps1 -AllTitles'
    exit 0
}

$fail = 0
if ($recommend -in @('A', 'both')) {
    $a = @('-Profile', $profile, '-Execute')
    if ($BuildId) { $a += '-BuildId'; $a += $BuildId }
    & (Join-Path $Driver3 'host-a.ps1') @a
    if ($LASTEXITCODE -ne 0) { $fail = $LASTEXITCODE }
}

if ($recommend -in @('B', 'both')) {
    $b = @('-Profile', $profile, '-Execute')
    if ($ForceMap) { $b += '-ForceMap' }
    if ($Inspector) { $b += '-Inspector' }
    if ($FullAcMatrix) { $b += '-FullAcMatrix' }
    if ($SkipProve) { $b += '-SkipProve' }
    & (Join-Path $Driver3 'host-b.ps1') @b
    if ($LASTEXITCODE -ne 0) { $fail = $LASTEXITCODE }
}

if ($BuildId -and $recommend -eq 'both') {
    & (Join-Path $Driver3 'sync-lab-manifest.ps1') -Game $profile -BuildId $BuildId -DmaStatus ok
}

if ($fail -ne 0) { exit $fail }
Write-Host '[+] driver3 everything complete'
exit 0
