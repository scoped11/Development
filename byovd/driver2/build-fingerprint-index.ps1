# Emit dumps/ac_fingerprints_index.json from bundled manifests + ac_host_captures.
$ErrorActionPreference = 'Stop'
$Driver2 = $PSScriptRoot
$DevRoot = Split-Path (Split-Path $Driver2 -Parent) -Parent
$Dumps = Join-Path $Driver2 'dumps'
$Out = Join-Path $Dumps 'ac_fingerprints_index.json'
$Captures = Join-Path $DevRoot 'ac-captures-host\config\ac_host_captures.json'
$Policy = Join-Path $DevRoot 'driver-lab\data\ac_eac_be_vehicle_policy.json'

function Read-Manifest($dir) {
    $m = Join-Path $dir 'manifest.json'
    if (-not (Test-Path $m)) { return $null }
    $j = Get-Content $m -Raw | ConvertFrom-Json
    $slim = [ordered]@{}
    foreach ($k in @(
            'game', 'driver_sha256', 'host_os_build', 'captured_utc', 'note',
            'eos_driver_path', 'platform', 'bedaisy_sha256', 'beservice_r6s_sha256', 'gameid'
        )) {
        if ($j.PSObject.Properties.Name -contains $k -and $null -ne $j.$k) { $slim[$k] = $j.$k }
    }
    if ($j.settings_json) { $slim['eos_settings'] = $j.settings_json }
    if ($slim.Count -eq 0) { return @{ capture_error = 'manifest_unreadable_or_huge' } }
    return $slim
}

$bundles = @(
    @{ key = 'eac_rust'; folder = 'rust-eac-eos_2026-11-13_0204' }
    @{ key = 'be_r6'; folder = 'r6-battleye_2026-11-13_0214' }
    @{ key = 'ace_deltaforce'; folder = 'deltaforce-ace_2026-11-13_0215' }
    @{ key = 'ace_arenabreakout'; folder = 'arenabreakout-ace_2026-11-13_0215' }
)

$profiles = @{}
foreach ($b in $bundles) {
    $root = Join-Path $Dumps $b.folder
  $profiles[$b.key] = @{
        capture_dir = "dumps/$($b.folder)"
        manifest    = Read-Manifest $root
    }
}

$eos = Join-Path $Dumps 'rust-eac-eos_2026-11-13_0204\eac\EasyAntiCheat\Settings.json'
$eosIds = $null
if (Test-Path $eos) {
    $eosIds = Get-Content $eos -Raw | ConvertFrom-Json
}

@{
    schema            = 'driver2_ac_fingerprints_v1'
    generated_utc     = (Get-Date).ToUniversalTime().ToString('o')
    doc               = 'AC_FINGERPRINTS.md'
    host_captures_ref = $(if (Test-Path $Captures) { $Captures } else { $null })
    vehicle_policy_ref = $(if (Test-Path $Policy) { $Policy } else { $null })
    eos_settings_ids  = $eosIds
    profiles          = $profiles
    driver_detection_surfaces_ref = '../../byovd/docs/04_detection_surfaces.md'
} | ConvertTo-Json -Depth 12 | Set-Content $Out -Encoding UTF8

Write-Host "[+] $Out"
