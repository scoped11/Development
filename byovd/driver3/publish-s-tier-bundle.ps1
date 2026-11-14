# Collect S-tier evidence paths into release/s_tier_bundle_manifest.json (no zip — paths only).
param([string]$OutDir = '')

$Driver3 = $PSScriptRoot
$Byovd = Split-Path $Driver3 -Parent
if (-not $OutDir) { $OutDir = Join-Path $Driver3 'release' }
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

$files = @(
    (Join-Path $Driver3 's_tier.json'),
    (Join-Path $Driver3 'dashboard.json'),
    (Join-Path $Driver3 'developer_nod.json'),
    (Join-Path $Driver3 'offsets_validation.json'),
    (Join-Path $Driver3 'vm_hostb_execute.log'),
    (Join-Path $Driver3 'vm_guest_smoke.json'),
    (Join-Path $Driver3 'driver3_gates_all.json'),
    (Join-Path $Driver3 'ranked_session.json'),
    (Join-Path $Byovd 'docs\lab_manifest.json'),
    (Join-Path $Byovd 'driver2\cheat_inspector.json'),
    (Join-Path $Byovd 'driver2\developer_approval.json')
) + @(Get-ChildItem $Driver3 -Filter 'kernel_proof_*.json' | ForEach-Object FullName)

$manifest = [ordered]@{
    schema = 's_tier_bundle_v1'
    utc    = (Get-Date).ToUniversalTime().ToString('o')
    files  = @($files | ForEach-Object { @{ path = $_; present = (Test-Path $_) } })
}

$out = Join-Path $OutDir 's_tier_bundle_manifest.json'
($manifest | ConvertTo-Json -Depth 6) | Set-Content $out -Encoding utf8
Write-Host "[+] $out"
exit 0
