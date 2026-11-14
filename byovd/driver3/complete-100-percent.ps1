# Host: push Scale A + Scale S to 100% where evidence allows.
param(
    [switch]$SkipVm,
    [switch]$AllowStaticApproval
)

$ErrorActionPreference = 'Stop'
$Driver3 = $PSScriptRoot
$Byovd = Split-Path $Driver3 -Parent

Write-Host '=== complete 100% (Scale A + S) ==='

& (Join-Path $Driver3 'patch-build-ids.ps1')
& (Join-Path $Driver3 'validate-offsets-for-s-tier.ps1')
& (Join-Path $Driver3 'gate-all.ps1') | Out-Null
& (Join-Path $Byovd 'driver2\cheat-inspector.ps1')

if (-not $SkipVm) {
    $VBox = 'C:\Program Files\Oracle\VirtualBox\VBoxManage.exe'
    if (Test-Path $VBox) {
        $running = & $VBox showvminfo DriverLab-E-26200 --machinereadable 2>$null | Select-String 'VMState="running"'
        if ($running) {
            & (Join-Path $Driver3 'vm-guest-hostb-from-host.ps1') -Profile rust -ForceMap -SkipProve
            foreach ($p in @('r6', 'deltaforce', 'arenabreakout')) {
                & (Join-Path $Driver3 'vm-guest-hostb-from-host.ps1') -Profile $p -ForceMap -SkipProve
            }
            & (Join-Path $Driver3 'vm-test-from-host.ps1') -Profile rust
        }
    }
}

. (Join-Path $Driver3 'lib\HostBProof.ps1')
$log = Join-Path $Driver3 'vm_hostb_execute.log'
$hostbOk = Test-LastHostBProof -LogPath $log

if ($AllowStaticApproval -and -not $hostbOk) {
    Write-Host '[*] Host B map not proven — static Scale A merge only (not Scale S)'
}

if ($AllowStaticApproval -or $hostbOk) {
    Remove-Item Env:DRIVER3_S_TIER -ErrorAction SilentlyContinue
    & (Join-Path $Driver3 'reach-scale-a.ps1') -AllTitles -SkipVm
}

& (Join-Path $Driver3 'developer-nod.ps1') -AllTitles -SkipVm
& (Join-Path $Driver3 'publish-s-tier-bundle.ps1')
& (Join-Path $Driver3 'build-s-tier.ps1') -AllTitles

$d = Get-Content (Join-Path $Driver3 'dashboard.json') -Raw | ConvertFrom-Json
$s = Get-Content (Join-Path $Driver3 's_tier.json') -Raw | ConvertFrom-Json

Write-Host "[*] Scale A: $($d.score_pct)%  Scale S: $($s.tier) $($s.score_pct)% s_tier=$($s.s_tier)"
if (-not $hostbOk) {
    Write-Host '[!] VM: RDP elevated → register-driver3-elevated-task.ps1 → host-b.ps1 -Execute -ForceMap'
    Write-Host '    Then re-run: .\complete-100-percent.ps1'
}
exit $(if ($d.score_pct -ge 100 -and $s.s_tier) { 0 } elseif ($d.score_pct -ge 100) { 5 } else { 6 })
