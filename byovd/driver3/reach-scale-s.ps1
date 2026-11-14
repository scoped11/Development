# Reach Scale S: orchestrate evidence (Scale A + Host B + dynamic + offsets).
param(
    [string]$Profile = 'rust',
    [switch]$AllTitles,
    [switch]$VmMap,
    [switch]$SkipVm,
    [switch]$BuildDma,
    [switch]$RequireDynamic
)

$ErrorActionPreference = 'Stop'
$Driver3 = $PSScriptRoot
$Byovd = Split-Path $Driver3 -Parent

Write-Host '=== reach Scale S (evidence-backed) ==='
Write-Host '    Not claimed: undetected, D4, live ranked BYOVD'

$env:DRIVER3_S_TIER = '1'

# 1) Scale A base
& (Join-Path $Driver3 'reach-scale-a.ps1') -Profile $Profile -AllTitles:$AllTitles -VmMap:$VmMap -SkipVm:$SkipVm
$scaleExit = $LASTEXITCODE

# 2) Offset cross-check
& (Join-Path $Driver3 'validate-offsets-for-s-tier.ps1')

# 3) Developer nod (all titles)
& (Join-Path $Driver3 'developer-nod.ps1') -Game $Profile -AllTitles:$AllTitles -SkipVm:$SkipVm

# 4) Extra VM map per title when requested
if ($VmMap -and -not $SkipVm) {
    foreach ($p in @('rust', 'r6', 'deltaforce', 'arenabreakout')) {
        if ($AllTitles -or $p -eq $Profile) {
            & (Join-Path $Driver3 'vm-test-from-host.ps1') -Profile $p -Execute -ForceMap -SkipProve -PollMaxSec 600
        }
    }
    & (Join-Path $Driver3 'sync-guest-artifacts.ps1') -AllKernelProofs
    & (Join-Path $Driver3 'sync-kernel-proof-from-artifacts.ps1') -Primary $Profile
}

# 5) Dynamic gate on VM (no static merge for S)
if ($VmMap -and -not $SkipVm) {
    try {
        & (Join-Path $Driver3 'invoke-vm-elevated.ps1') -ScriptBlock {
            & powershell -NoProfile -ExecutionPolicy Bypass -File Z:\byovd\driver2\cheat-inspector.ps1 -Execute -ForceMap -IncludeAce -IncludeBe
        }
    } catch { Write-Warning "VM cheat-inspector -Execute: $_" }
    & (Join-Path $Driver3 'sync-guest-artifacts.ps1') -IncludeInspector
}

# 6) Host-only dynamic (admin) when -RequireDynamic
if ($RequireDynamic -and -not $SkipVm) {
    $isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
        [Security.Principal.WindowsBuiltInRole]::Administrator)
    if ($isAdmin) {
        & (Join-Path $Byovd 'driver2\cheat-inspector.ps1') -Execute -ForceMap -IncludeAce -IncludeBe
    } else {
        Write-Host '[!] -RequireDynamic: re-run elevated on host or use -VmMap'
    }
}

# 7) Optional DMA build
if ($BuildDma) {
    $bs = Join-Path (Split-Path $Byovd -Parent) 'dma\scripts\build-windows.ps1'
    if (Test-Path $bs) { & $bs }
}

# 8) DMA status in manifest when no exe
$devRoot = Split-Path $Byovd -Parent
$hasExe = @(
    (Join-Path $devRoot 'debug\RobRunDMA.exe'),
    (Join-Path $devRoot 'build\RobRunDMA.exe')
) | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $hasExe) {
    & (Join-Path $Driver3 'sync-lab-manifest.ps1') -Game $Profile -DmaStatus build_only
}

# 9) Release bundle manifest
& (Join-Path $Driver3 'publish-s-tier-bundle.ps1')

# 10) S-tier score
& (Join-Path $Driver3 'build-s-tier.ps1') -Profile $Profile -AllTitles:$AllTitles
$s = Get-Content (Join-Path $Driver3 's_tier.json') -Raw | ConvertFrom-Json

Write-Host "[*] Scale S: grade=$($s.tier) score=$($s.score_pct)% s_tier=$($s.s_tier)"
if (-not $s.s_tier) {
    Write-Host '[*] Gaps:' ($s.issues -join '; ')
    Write-Host '    Next: reach-scale-s.ps1 -AllTitles -VmMap (guest elevated task registered)'
}

exit $(if ($s.s_tier) { 0 } else { [Math]::Max($scaleExit, 7) })
