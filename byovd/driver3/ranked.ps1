# driver3 RANKED — live session on game PC (Host A / DMA only). Default entry for paid-AC play.
param(
    [string]$Game = 'rust',
    [switch]$Execute,
    [string]$BuildId = '',
    [switch]$SyncBuildId,
    [switch]$PreflightOnly,
    [switch]$ForceMap
)

$ErrorActionPreference = 'Stop'
$Driver3 = $PSScriptRoot
$Byovd = Split-Path $Driver3 -Parent
$DevRoot = Split-Path $Byovd -Parent

. (Join-Path $Driver3 'Resolve-Driver3Game.ps1')
. (Join-Path $Driver3 'lib\Get-Driver3LiveReadMethod.ps1')
$readMethod = Get-Driver3LiveReadMethod
$profile = Resolve-Driver3Profile $Game
$meta = Get-Driver3Meta $profile

if ($readMethod.method -eq 'single_pc_kernel') {
    if ($ForceMap) { $env:DRIVER2_RANKED_OVERRIDE = '1' }
    if ($Execute) {
        & (Join-Path $Driver3 'single-pc-play.ps1') -Game $profile -Execute -Live -Build -SyncBuildId:$SyncBuildId
        exit $LASTEXITCODE
    }
    if ($PreflightOnly) {
        & (Join-Path $Byovd 'driver2\ranked-ready.ps1') -Title $profile -PreflightOnly
        exit $LASTEXITCODE
    }
    & (Join-Path $Byovd 'driver2\ranked-ready.ps1') -Title $profile -PreflightOnly
    exit $LASTEXITCODE
}

Write-Host '=========================================='
Write-Host ' driver3 RANKED (Host A — Lane D live reads)'
Write-Host " Title: $($meta.title)"
Write-Host ' Kernel on this PC: blocked (Tier 0) — use VM for driver2'
Write-Host '    Or: set-live-read-method.ps1 -Method single_pc_kernel → driver2 ranked'
Write-Host '=========================================='

if ($SyncBuildId -or (-not $BuildId)) {
    & (Join-Path $Driver3 'sync-build-id-from-dumps.ps1') -Game $profile
    $manifest = Get-Content (Join-Path $Byovd 'docs\lab_manifest.json') -Raw | ConvertFrom-Json
    if ($manifest.lane_d.build_id) { $BuildId = $manifest.lane_d.build_id }
}

& (Join-Path $Driver3 'lane-gate.ps1') -Game $profile
$gateExit = $LASTEXITCODE

. (Join-Path $Driver3 'lib\Test-Driver3BuildIdGate.ps1')
$buildGate = Test-Driver3BuildIdGate -Profile $profile

$session = [ordered]@{
    schema      = 'driver3_ranked_session_v1'
    utc         = (Get-Date).ToUniversalTime().ToString('o')
    hostname    = $env:COMPUTERNAME
    profile     = $profile
    title       = $meta.title
    ac_family   = $meta.ac_family
    mode        = 'ranked_host_a'
    lane_d      = ($gateExit -eq 2)
    gate_exit   = $gateExit
    build_id    = $BuildId
    build_gate  = $buildGate.ok
    dma_root    = (Test-Path (Join-Path $DevRoot 'dma'))
    execute     = [bool]$Execute
    dma_exe     = $null
}

if ($gateExit -eq 2) {
    Write-Host '[+] Ranked path: Tier 0 — DMA/overlay on THIS machine; driver2 on DriverLab VM.'
} elseif ($gateExit -eq 0) {
    Write-Host '[*] Vehicle heuristic open — ranked still uses DMA (Scale A); do not map here.'
}

if ($PreflightOnly) {
    $out = Join-Path $Driver3 'ranked_session.json'
    ($session | ConvertTo-Json -Depth 5) | Set-Content $out -Encoding utf8
    Write-Host "[+] Preflight only → $out"
    exit $(if ($buildGate.ok) { 0 } else { 3 })
}

if ($ForceMap) {
    Write-Host '[!] Ranked ForceMap is OFF by default. Set DRIVER2_RANKED_OVERRIDE=1 to opt into driver2 kernel (not Scale A).'
    if ($env:DRIVER2_RANKED_OVERRIDE -ne '1') {
        Write-Host '    Aborting. Use: driver3\ranked.ps1 -Execute'
        exit 41
    }
    & (Join-Path $Driver3 'ranked-play.ps1') -Profile $profile -ForceMap -Execute
    exit $LASTEXITCODE
}

if (-not $buildGate.ok) {
    Write-Host '[!] build_id gate failed — run with -SyncBuildId or sync-lab-manifest.ps1'
    $buildGate.issues | ForEach-Object { Write-Host "    $_" }
    if (-not $Execute) {
        ($session | ConvertTo-Json -Depth 5) | Set-Content (Join-Path $Driver3 'ranked_session.json') -Encoding utf8
        exit 3
    }
    throw 'build_id required before ranked -Execute'
}

if ($Execute) {
    . (Join-Path $Driver3 'lib\Test-Driver3LiveReady.ps1')
    $live = Test-Driver3LiveReady -Profile $profile
    $session.live_ready = $live.checks
    if (-not $live.ok) {
        $live.issues | ForEach-Object { Write-Host "[!] live ready: $_" }
        throw 'Test-LiveReady.ps1 failed — fix before ranked -Execute (not undetected)'
    }
    Write-Host '[+] live ready gate passed (Hardware DMA still required at runtime)'
}

& (Join-Path $Driver3 'host-a.ps1') -Profile $profile -BuildId $BuildId -Execute:$Execute
$hostAExit = $LASTEXITCODE

$dmaExe = @(
    (Join-Path $DevRoot 'debug\RobRunDMA.exe'),
    (Join-Path $DevRoot 'build\RobRunDMA.exe')
) | Where-Object { Test-Path $_ } | Select-Object -First 1
$session.dma_exe = if ($dmaExe) { $dmaExe } else { 'build: dma\scripts\build-windows.ps1' }
$session.host_a_exit = $hostAExit

$out = Join-Path $Driver3 'ranked_session.json'
($session | ConvertTo-Json -Depth 6) | Set-Content $out -Encoding utf8
Write-Host "[+] Ranked session → $out"

if ($Execute -and $dmaExe) {
    Write-Host "[*] DMA binary: $dmaExe — launch overlay when ready (same build_id as manifest)."
}

& (Join-Path $Driver3 'sync-lab-manifest.ps1') -Game $profile -BuildId $BuildId -DmaStatus ok -LiveHost $env:COMPUTERNAME

exit $hostAExit
