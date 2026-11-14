# driver3 Host A — live reads (DMA); all paid-AC titles default to no kernel map here.
param(
    [string]$Profile = 'rust',
    [string]$BuildId = '',
    [switch]$Execute
)

$ErrorActionPreference = 'Stop'
$Driver3 = $PSScriptRoot
$Byovd = Split-Path $Driver3 -Parent
$DevRoot = Split-Path $Byovd -Parent
$DmaRoot = Join-Path $DevRoot 'dma'
$BuildScript = Join-Path $DmaRoot 'scripts\build-windows.ps1'

. (Join-Path $Driver3 'Resolve-Driver3Game.ps1')
$profile = Resolve-Driver3Profile $Profile
$meta = Get-Driver3Meta $profile

Write-Host '=== driver3 Host A (game PC / live reads) ==='
Write-Host "Title: $($meta.title) AC=$($meta.ac_family)"

& (Join-Path $Driver3 'lane-gate.ps1') -Game $profile
if ($LASTEXITCODE -eq 2) {
    Write-Host "[+] Tier 0 / Lane D for $($meta.ac_family) — DMA path (no map on this host)."
}

if (-not (Test-Path $DmaRoot)) {
    Write-Host "[!] DMA tree missing: $DmaRoot"
    if (-not $Execute) { exit 0 }
    throw 'DMA required for Host A'
}

Write-Host "[+] DMA: $DmaRoot"
Write-Host "    Static: $($meta.capture_dump) (driver2/dumps)"
Write-Host '    Manifest: sync-lab-manifest.ps1 -BuildId <id> -DmaStatus ok'

if ($BuildId) {
    & (Join-Path $Driver3 'sync-lab-manifest.ps1') -Game $profile -BuildId $BuildId -DmaStatus ok -LiveHost $env:COMPUTERNAME
}

if (-not $Execute) {
    Write-Host '[*] PLAN — add -Execute to run dma build script'
    $bg = & (Join-Path $Driver3 'Test-Driver3BuildId.ps1') -Profile $profile 2>&1
    if ($LASTEXITCODE -ne 0) { Write-Host '[*] build_id gate would fail on -Execute (set manifest first)' }
    exit 0
}

. (Join-Path $Driver3 'Test-Driver3BuildId.ps1') -Profile $profile
if ($LASTEXITCODE -ne 0) {
    throw 'build_id gate failed — sync-lab-manifest.ps1 -BuildId <id> before Host A -Execute'
}

if (Test-Path $BuildScript) {
    Write-Host '[*] Building DMA (Host A)...'
    & $BuildScript
    if ($LASTEXITCODE -ne 0) { throw "DMA build failed exit $LASTEXITCODE" }
    & (Join-Path $Driver3 'sync-lab-manifest.ps1') -Game $profile -DmaStatus ok -LiveHost $env:COMPUTERNAME
    Write-Host '[+] Host A build complete — overlay per dma/README (same build_id for all titles)'
    exit 0
}

Write-Host '[!] build-windows.ps1 not found — manual build per dma/README.md'
exit 0
