function Test-Driver2RankedReady {
    param(
        [string[]]$Profiles = @('rust', 'r6'),
        [switch]$RequireMapProof
    )

    $Driver2 = Split-Path $PSScriptRoot -Parent
    $Byovd = Split-Path $Driver2 -Parent
    $Driver3 = Join-Path $Byovd 'driver3'

    . (Join-Path $Driver3 'lib\Get-Driver3LiveReadMethod.ps1')
    . (Join-Path $Driver3 'lib\Test-Driver3BuildIdGate.ps1')
    . (Join-Path $Driver2 'lib\Invoke-Driver2RankedVerify.ps1')
    . (Join-Path $Driver2 'lib\Invoke-Driver2RankedSafeGate.ps1')
    . (Join-Path $Driver2 'lib\Invoke-Driver2DumpTrackingApply.ps1')

    $checks = [System.Collections.Generic.List[object]]::new()
    $issues = [System.Collections.Generic.List[string]]::new()

    function Add([string]$Id, [bool]$Pass, [string]$Detail = '') {
        $checks.Add([pscustomobject]@{ id = $Id; pass = $Pass; detail = $Detail })
        if (-not $Pass) { $issues.Add("$Id — $Detail") }
    }

    $method = Get-Driver3LiveReadMethod
    Add 'single_pc_ranked_mode' ($method.method -eq 'single_pc_kernel') $method.method

    $mapOk = $false
    $log = Join-Path $Driver3 'host_local_map.log'
    if (Test-Path $log) {
        $last = Get-Content $log | Select-String 'map exit=(\d+)' | Select-Object -Last 1
        $mapOk = $last -match 'map exit=0'
    }
    Add 'host_map_proof' $mapOk $(if ($mapOk) { 'host_local_map.log exit=0' } else { 'run confirm-ranked-100.ps1 (admin)' })

    $verifyPath = Join-Path $Driver2 'driver2_ranked_verify.json'
    $probeOk = Test-Driver2RankedVerifyFile -Path $verifyPath
    Add 'driver2_probe' $probeOk $verifyPath

    $safeOk = Test-Driver2RankedSafeFile
    Add 'ranked_safe_policy' $safeOk (Join-Path $Driver2 'ranked_safe.json')

    $dumpTrackOk = Test-Driver2DumpTrackingFile
    Add 'dump_tracking_session' $dumpTrackOk 'driver2_dump_tracking.json'

    $bins = @(
        (Join-Path $Byovd 'bin\Driver2.sys'),
        (Join-Path $Byovd 'bin\Driver2Client.exe'),
        (Join-Path $Byovd 'bin\Driver2Runner.exe'),
        (Join-Path $Byovd 'bin\mapper_nal.exe')
    )
    Add 'driver2_stack' (@($bins | Where-Object { Test-Path $_ }).Count -ge 3) ''

    $insp = Join-Path $Driver2 'cheat_inspector.json'
    $inspOk = $false
    if (Test-Path $insp) {
        $ci = Get-Content $insp -Raw | ConvertFrom-Json
        $inspOk = $ci.approved -eq $true -and $ci.final_score_pct -ge 90
    }
    Add 'cheat_inspector' $inspOk ''

    foreach ($p in $Profiles) {
        Add "build_id_$p" (Test-Driver3BuildIdGate -Profile $p).ok ''
        $pf = switch ($p) {
            'rust' { 'profiles\eac-rust.json' }
            'r6' { 'profiles\be-r6.json' }
            default { "profiles\$p.json" }
        }
        Add "profile_$p" (Test-Path (Join-Path $Driver2 $pf)) $pf
    }

    $passed = @($checks | Where-Object { $_.pass }).Count
    $max = $checks.Count
    $pct = if ($max) { [math]::Round(100.0 * $passed / $max, 1) } else { 0 }

    $stackOk = @($bins | Where-Object { Test-Path $_ }).Count -ge 3
    $rustId = (Test-Driver3BuildIdGate -Profile 'rust').ok
    $r6Id = (Test-Driver3BuildIdGate -Profile 'r6').ok

    $rankedReady = ($method.method -eq 'single_pc_kernel') -and $inspOk -and $stackOk -and $rustId -and $r6Id

    $sTier = $rankedReady -and $mapOk -and $probeOk -and $safeOk -and $dumpTrackOk -and ($passed -eq $max)
    if ($RequireMapProof -and (-not $mapOk -or -not $probeOk)) { $sTier = $false }

    [pscustomobject]@{
        schema        = 'driver2_ranked_ready_v1'
        utc           = (Get-Date).ToUniversalTime().ToString('o')
        tier          = if ($sTier) { 'S' } elseif ($rankedReady) { 'PREPARED' } else { 'SETUP' }
        s_tier        = $sTier
        verified_100  = $sTier
        ranked_ready  = $rankedReady
        score_pct     = $pct
        titles        = @{ rust = 'Rust EAC'; r6 = 'Rainbow Six BattlEye' }
        profiles      = $Profiles
        checks        = @($checks)
        issues        = @($issues)
        play          = @{
            rust = 'driver3\single-pc-play.ps1 -Game rust -Execute -Live -Build'
            r6   = 'driver3\single-pc-play.ps1 -Game r6 -Execute -Live -Build'
        }
        read          = 'Driver2Client.exe pread <pid> <va> <n>'
        disclaimer    = 'Ranked operational — not undetected; kernel on session with live AC is high risk'
    }
}
