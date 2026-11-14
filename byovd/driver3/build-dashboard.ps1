# Scale A dashboard: gate + manifest + optional inspector/smoke/vm proof.
param(
    [string]$Profile = 'rust',
    [switch]$AllTitles,
    [string]$OutJson = ''
)

$ErrorActionPreference = 'Stop'
$Driver3 = $PSScriptRoot
$Byovd = Split-Path $Driver3 -Parent
if (-not $OutJson) { $OutJson = Join-Path $Driver3 'dashboard.json' }

. (Join-Path $Driver3 'Resolve-Driver3Game.ps1')

$manifest = $null
$mp = Join-Path $Byovd 'docs\lab_manifest.json'
if (Test-Path $mp) { $manifest = Get-Content $mp -Raw | ConvertFrom-Json }

$gates = @{}
if ($AllTitles) {
    & (Join-Path $Driver3 'gate-all.ps1') | Out-Null
    $gall = Join-Path $Driver3 'driver3_gates_all.json'
    if (Test-Path $gall) { $gates.all = Get-Content $gall -Raw | ConvertFrom-Json }
} else {
    & (Join-Path $Driver3 'lane-gate.ps1') -Game $Profile | Out-Null
    $g1 = Join-Path $Driver3 'driver3_gate.json'
    if (Test-Path $g1) { $gates.single = Get-Content $g1 -Raw | ConvertFrom-Json }
}

. (Join-Path $Driver3 'lib\Test-Driver3BuildIdGate.ps1')
$buildGate = Test-Driver3BuildIdGate -Profile $Profile

$insp = Join-Path $Byovd 'driver2\cheat_inspector.json'
$dev = Join-Path $Byovd 'driver2\developer_approval.json'
$smoke = Join-Path $Driver3 'vm_guest_smoke.json'
$hostb = Join-Path $Driver3 'vm_hostb_execute.log'
. (Join-Path $Driver3 'lib\HostBProof.ps1')

function Score-ScaleA {
    $pts = 0
    $max = 100
    if ($gates.all -or $gates.single) { $pts += 15 }
    if ($buildGate.ok) { $pts += 20 }
    if ($manifest -and $manifest.lane_d.build_id -and -not ($manifest.lane_d.build_id -match '^\s*$|replace|TODO')) { $pts += 15 }
    if ($manifest -and $manifest.lane_d.by_profile) { $pts += 10 }
    if (Test-Path $smoke) { $pts += 15 }
    if (Test-Path $insp) {
        $ci = Get-Content $insp -Raw | ConvertFrom-Json
        $inspOk = ($ci.tier -ge 1) -or ($ci.approved -eq $true) -or ($ci.final_score_pct -ge 90)
        if ($inspOk) { $pts += 10 }
    }
    if (Test-Path $dev) {
        $da = Get-Content $dev -Raw | ConvertFrom-Json
        if ($da.approved -eq $true) { $pts += 15 }
        elseif ($da.target_score_pct -ge 90) { $pts += 8 }
    }
    if (Test-LastHostBProof -LogPath $hostb) { $pts += 10 }
    [Math]::Min($pts, $max)
}

. (Join-Path $Driver3 'lib\Test-Driver3LiveReady.ps1')
$liveReady = Test-Driver3LiveReady -Profile $Profile
$nodPath = Join-Path $Driver3 'developer_nod.json'
$nodPct = $null
if (Test-Path $nodPath) {
    $nodPct = (Get-Content $nodPath -Raw | ConvertFrom-Json).nod_pct
}

$dash = [ordered]@{
    schema     = 'driver3_scale_a_v1'
    utc        = (Get-Date).ToUniversalTime().ToString('o')
    scale      = 'A'
    not_claimed = @('undetected_live_ranked', 'D4')
    score_pct  = (Score-ScaleA)
    live_ready = $liveReady.ok
    live_ready_issues = @($liveReady.issues)
    developer_nod_pct = $nodPct
    target_pct = '80-90'
    profile    = (Resolve-Driver3Profile $Profile)
    build_id_gate = $buildGate
    gates      = $gates
    manifest_lane_d = if ($manifest) { $manifest.lane_d } else { $null }
    artifacts  = @{
        cheat_inspector = (Test-Path $insp)
        developer_approval = (Test-Path $dev)
        developer_nod_json = (Test-Path $nodPath)
        vm_guest_smoke = (Test-Path $smoke)
        vm_hostb_log = (Test-Path $hostb)
    }
    gaps       = @()
}
if ($dash.score_pct -lt 80) {
    if (-not $buildGate.ok) { $dash.gaps += 'build_id: sync-build-id-from-dumps.ps1' }
    if (-not (Test-Path $hostb)) { $dash.gaps += 'vm_hostb: reach-scale-a.ps1 -VmMap' }
    if (-not (Test-Path $dev)) { $dash.gaps += 'developer_approval: VM -Execute gate' }
    elseif (-not ((Get-Content $dev -Raw | ConvertFrom-Json).approved)) { $dash.gaps += 'developer_approval: approved=false' }
}
if (-not $dash.gaps.Count) {
    $dash.next = @('Maintain build_id on title updates', 'Re-run gate-all after AC dump refresh')
} else {
    $dash.next = @($dash.gaps)
}

($dash | ConvertTo-Json -Depth 12) | Set-Content $OutJson -Encoding utf8
Write-Host "[+] Scale A dashboard: $OutJson score=$($dash.score_pct)%"
exit 0
