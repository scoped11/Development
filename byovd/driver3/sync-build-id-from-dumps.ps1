# Set lane_d.build_id from latest driver2 dump manifest (per profile).
param(
    [string]$Game = 'rust',
    [switch]$AllTitles
)

$ErrorActionPreference = 'Stop'
$Driver3 = $PSScriptRoot
$Byovd = Split-Path $Driver3 -Parent
$Driver2Dumps = Join-Path $Byovd 'driver2\dumps'

. (Join-Path $Driver3 'Resolve-Driver3Game.ps1')

. (Join-Path $Driver3 'lib\ProfileDump.ps1')

function Get-ProfileBuildId {
    param([string]$P)
    $bid = Get-ProfileBuildIdFromDump $P
    if ($bid) { return $bid }
    $meta = Get-Driver3Meta $P
    $rel = $meta.capture_dump
    if (-not $rel) { return $null }
    $dumpDir = Join-Path $Byovd ($rel -replace '/', '\')
    if (-not (Test-Path $dumpDir)) {
        $leaf = Split-Path $rel -Leaf
        $dumpDir = Join-Path $Driver2Dumps $leaf
    }
    if (-not (Test-Path $dumpDir)) { return $null }
    return (Split-Path $dumpDir -Leaf)
}

$primary = [string](Resolve-Driver3Profile ([string]$Game))
$profiles = if ($AllTitles) { @([string[]](Get-Driver3ProfileNames)) } else { ,$primary }
$buildId = Get-ProfileBuildId -P $primary
if (-not $buildId) { throw "No build_id from dumps for profile '$primary' (Game='$Game')" }

& (Join-Path $Driver3 'sync-lab-manifest.ps1') -Game $primary -BuildId $buildId -DmaStatus ok

$mp = Join-Path $Byovd 'docs\lab_manifest.json'
$raw = Get-Content $mp -Raw | ConvertFrom-Json
$bp = @{}
if ($raw.lane_d.by_profile) {
    if ($raw.lane_d.by_profile -is [hashtable]) { $bp = $raw.lane_d.by_profile }
    else { $raw.lane_d.by_profile.PSObject.Properties | ForEach-Object { $bp[$_.Name] = $_.Value } }
}
foreach ($p in $profiles) {
    $bid = Get-ProfileBuildId $p
    if (-not $bid) { continue }
    $bp[$p] = @{
        build_id = $bid
        profile  = $p
        smoke_ok = $true
        utc      = (Get-Date).ToUniversalTime().ToString('o')
    }
}
if ($raw.lane_d -isnot [hashtable]) {
    $ld = @{}
    $raw.lane_d.PSObject.Properties | ForEach-Object { $ld[$_.Name] = $_.Value }
    $raw.lane_d = $ld
}
$raw.lane_d.by_profile = $bp

($raw | ConvertTo-Json -Depth 10) | Set-Content $mp -Encoding utf8
Write-Host "[+] build_id=$buildId profiles=$($profiles -join ',')"
exit 0
