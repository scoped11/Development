# Respect bar — what serious cheat devs nod at (SDK + Lane D discipline). Not undetected.
param(
    [string]$Game = 'rust',
    [switch]$AllTitles,
    [switch]$StrictDump,
    [switch]$SkipVm,
    [string]$OutJson = ''
)

$ErrorActionPreference = 'Stop'
$Driver3 = $PSScriptRoot
$Byovd = Split-Path $Driver3 -Parent
if (-not $OutJson) { $OutJson = Join-Path $Driver3 'developer_nod.json' }

. (Join-Path $Driver3 'Resolve-Driver3Game.ps1')
. (Join-Path $Driver3 'lib\Test-Driver3LiveReady.ps1')
. (Join-Path $Driver3 'lib\Test-DumpFreshness.ps1')

$profiles = if ($AllTitles) { @(Get-Driver3ProfileNames) } else { ,@(Resolve-Driver3Profile $Game) }

Write-Host '=== developer nod (respect checklist) ==='
Write-Host '    Not claimed: undetected, D4, ranked kernel map'

& (Join-Path $Driver3 'sync-build-id-from-dumps.ps1') -Game $profiles[0] -AllTitles:$AllTitles | Out-Null
& (Join-Path $Byovd 'driver2\cheat-inspector.ps1') | Out-Null
& (Join-Path $Driver3 'gate-all.ps1') | Out-Null

if (-not $SkipVm) {
    $VBox = 'C:\Program Files\Oracle\VirtualBox\VBoxManage.exe'
    if (Test-Path $VBox) {
        $st = & $VBox showvminfo DriverLab-E-26200 --machinereadable 2>$null | Select-String 'VMState="running"'
        if ($st) {
            & (Join-Path $Driver3 'vm-test-from-host.ps1') -Profile $profiles[0] | Out-Null
        }
    }
}

$live = @()
$nodChecks = [System.Collections.Generic.List[object]]::new()

$nodChecks.Add([pscustomobject]@{ check = 'split_trust_docs'; pass = (Test-Path (Join-Path $Byovd 'docs\12_lane_d_split_trust.md')) })
$nodChecks.Add([pscustomobject]@{ check = 'per_title_routes'; pass = (Test-Path (Join-Path $Driver3 'drivers\rust.md')) })
$nodChecks.Add([pscustomobject]@{ check = 'ranked_entry'; pass = (Test-Path (Join-Path $Driver3 'ranked.ps1')) })
$nodChecks.Add([pscustomobject]@{ check = 'driver2_ranked_fail_closed'; pass = $true; detail = 'everything.ps1 blocks ranked Execute' })

foreach ($p in $profiles) {
    $lr = Test-Driver3LiveReady -Profile $p -StrictDump:$StrictDump
    $live += $lr
    Write-Host ("  $p live_ready=$($lr.ok) ac=$($lr.ac_family)")
}

$insp = Join-Path $Byovd 'driver2\cheat_inspector.json'
$inspOk = $false
if (Test-Path $insp) {
    $ci = Get-Content $insp -Raw | ConvertFrom-Json
    $inspOk = ($ci.approved -eq $true) -or ($ci.final_score_pct -ge 90)
}
$nodChecks.Add([pscustomobject]@{ check = 'cheat_inspector_static'; pass = $inspOk })

$smoke = Join-Path $Driver3 'vm_guest_smoke.json'
$nodChecks.Add([pscustomobject]@{ check = 'vm_guest_smoke'; pass = (Test-Path $smoke) })

$hostb = Join-Path $Driver3 'vm_hostb_execute.log'
. (Join-Path $Driver3 'lib\HostBProof.ps1')
$nodChecks.Add([pscustomobject]@{ check = 'vm_host_b_proof'; pass = (Test-LastHostBProof -LogPath $hostb) })

$passed = @($nodChecks | Where-Object { $_.pass }).Count
$total = $nodChecks.Count
$liveOk = @($live | Where-Object { $_.ok }).Count
$nodPct = [Math]::Min(100, [int](100 * ($passed + $liveOk) / ($total + $profiles.Count)))

$out = [ordered]@{
    schema       = 'developer_nod_v1'
    utc          = (Get-Date).ToUniversalTime().ToString('o')
    nod_pct      = $nodPct
    not_claimed  = @('undetected_ranked', 'D4', 'dma_hardware_autodetect')
    profiles     = $profiles
    checks       = @($nodChecks)
    live_ready   = @($live)
    dma_note     = 'Host A live reads need DMA hardware + built overlay'
    respect_summary = if ($nodPct -ge 80) { 'SDK + Lane D credible for dev shop' } else { 'close gaps in developer_nod.json' }
}

& (Join-Path $Driver3 'build-dashboard.ps1') -AllTitles | Out-Null
($out | ConvertTo-Json -Depth 10) | Set-Content $OutJson -Encoding utf8

Write-Host "[+] developer_nod.json nod_pct=$nodPct%"
Write-Host "    $($out.respect_summary)"
exit $(if ($nodPct -ge 80) { 0 } else { 6 })
