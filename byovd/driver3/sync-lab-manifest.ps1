# Merge Lane D fields into byovd/docs/lab_manifest.json (per-game).
param(
    [string]$BuildId = '',
    [string]$Game = 'rust',
    [string]$LiveHost = '',
    [string]$LabHost = 'lab-vm',
    [ValidateSet('ok', 'missing', 'unknown', 'not_used', 'build_only')]
    [string]$DmaStatus = 'unknown',
    [string]$KernelProofJson = ''
)

$ErrorActionPreference = 'Stop'
$Driver3 = $PSScriptRoot
$Byovd = Split-Path $Driver3 -Parent
$manifestPath = Join-Path $Byovd 'docs\lab_manifest.json'

. (Join-Path $Driver3 'Resolve-Driver3Game.ps1')
$profile = Resolve-Driver3Profile $Game
$meta = Get-Driver3Meta $profile

if (-not (Test-Path $manifestPath)) {
    throw "Missing $manifestPath"
}

$raw = Get-Content $manifestPath -Raw | ConvertFrom-Json

if (-not $raw.lane_d) {
    $raw | Add-Member -NotePropertyName lane_d -NotePropertyValue ([pscustomobject]@{})
}

$ld = @{}
if ($raw.lane_d) {
    $raw.lane_d.PSObject.Properties | ForEach-Object { $ld[$_.Name] = $_.Value }
}
$ld.method = 'split-trust-calibration-bridge'
$ld.game = $profile
$ld.ac_family = $meta.ac_family
$ld.title = $meta.title
if ($BuildId) { $ld.build_id = $BuildId }
if ($LiveHost) { $ld.live_read_host = $LiveHost }
$ld.kernel_proof_host = $LabHost
$ld.dma_status = $DmaStatus
if ($ld.by_profile) { } else { $ld.by_profile = @{} }
$raw.lane_d = $ld
$raw.date_utc = (Get-Date).ToUniversalTime().ToString('yyyy-MM-dd')

if ($KernelProofJson -and (Test-Path $KernelProofJson)) {
    $proof = Get-Content $KernelProofJson -Raw | ConvertFrom-Json
    $raw.lane_d.kernel_proof = $proof
    if ($raw.lane_d.by_profile -isnot [hashtable]) {
        $bp = @{}
        if ($raw.lane_d.by_profile) {
            $raw.lane_d.by_profile.PSObject.Properties | ForEach-Object { $bp[$_.Name] = $_.Value }
        }
        $raw.lane_d.by_profile = $bp
    }
    $raw.lane_d.by_profile[$profile] = $proof
}

$ver = Get-ItemProperty 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion'
$raw.os_build = ('{0}.{1}' -f $ver.CurrentBuild, $ver.UBR)
$raw.host_id = $env:COMPUTERNAME

($raw | ConvertTo-Json -Depth 10) | Set-Content $manifestPath -Encoding utf8
Write-Host "[+] Updated $manifestPath (game=$profile ac=$($meta.ac_family) build_id=$($raw.lane_d.build_id))"
return $manifestPath
