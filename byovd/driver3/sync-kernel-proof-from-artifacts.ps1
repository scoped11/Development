# Merge kernel_proof_*.json + host-b log into docs/lab_manifest.json (Lane D D2).
param(
    [string]$Primary = 'rust'
)

$ErrorActionPreference = 'Stop'
$Driver3 = $PSScriptRoot
$Byovd = Split-Path $Driver3 -Parent
$manifestPath = Join-Path $Byovd 'docs\lab_manifest.json'

. (Join-Path $Driver3 'Resolve-Driver3Game.ps1')

$proofs = Get-ChildItem -Path $Driver3 -Filter 'kernel_proof_*.json' -ErrorAction SilentlyContinue
if (-not $proofs.Count) {
    Write-Host '[!] No kernel_proof_*.json — run host-b.ps1 -Execute on VM'
    exit 2
}

$raw = Get-Content $manifestPath -Raw | ConvertFrom-Json
if (-not $raw.lane_d) {
    throw 'lane_d missing in lab_manifest.json'
}

$bp = @{}
if ($raw.lane_d.by_profile) {
    $raw.lane_d.by_profile.PSObject.Properties | ForEach-Object { $bp[$_.Name] = $_.Value }
}

$primaryProof = $null
foreach ($f in $proofs) {
    $profile = ($f.BaseName -replace '^kernel_proof_', '')
    $proof = Get-Content $f.FullName -Raw | ConvertFrom-Json
    if (-not $bp.ContainsKey($profile)) {
        $bp[$profile] = @{ profile = $profile }
    }
    $entry = $bp[$profile]
    if ($entry -is [pscustomobject]) {
        $ht = @{}
        $entry.PSObject.Properties | ForEach-Object { $ht[$_.Name] = $_.Value }
        $entry = $ht
        $bp[$profile] = $entry
    }
    $entry['kernel_proof'] = $proof
    $entry['kernel_proof_utc'] = $proof.utc
    if ($profile -eq (Resolve-Driver3Profile $Primary)) { $primaryProof = $proof }
}

. (Join-Path $Driver3 'lib\HostBProof.ps1')
$hostb = Join-Path $Driver3 'vm_hostb_execute.log'
if (Test-Path $hostb) {
    if (Test-LastHostBProof -LogPath $hostb) {
        $raw.lane_d | Add-Member -NotePropertyName vm_host_b_ok -NotePropertyValue $true -Force
    }
}

if ($primaryProof) {
    $raw.lane_d.kernel_proof = $primaryProof
}

$raw.lane_d.by_profile = $bp
($raw | ConvertTo-Json -Depth 12) | Set-Content $manifestPath -Encoding utf8
Write-Host "[+] lane_d.kernel_proof synced profiles=$($proofs.Count)"
exit 0
