function Test-Driver3STier {
    param(
        [string]$Profile = 'rust',
        [switch]$AllTitles
    )

    $Driver3 = Split-Path $PSScriptRoot -Parent
    $Byovd = Split-Path $Driver3 -Parent
    $DevRoot = Split-Path $Byovd -Parent

    . (Join-Path $Driver3 'Resolve-Driver3Game.ps1')
    . (Join-Path $Driver3 'lib\Test-Driver3LiveReady.ps1')
    . (Join-Path $Driver3 'lib\Get-Driver3LiveReadMethod.ps1')

    $liveMethod = (Get-Driver3LiveReadMethod).method
    $checks = [System.Collections.Generic.List[object]]::new()
    $issues = [System.Collections.Generic.List[string]]::new()

    function Add-SCheck {
        param([string]$Id, [int]$Weight, [bool]$Pass, [string]$Detail = '')
        $checks.Add([pscustomobject]@{ id = $Id; weight = $Weight; pass = $Pass; detail = $Detail })
        if (-not $Pass) { $issues.Add("$Id — $Detail") }
    }

    $dashPath = Join-Path $Driver3 'dashboard.json'
    $scaleA = $false
    $scaleScore = 'n/a'
    if (Test-Path $dashPath) {
        $dash = Get-Content $dashPath -Raw | ConvertFrom-Json
        $scaleScore = [string]$dash.score_pct
        $scaleA = ($dash.score_pct -ge 90)
    }
    Add-SCheck 'scale_a_dashboard' 15 $scaleA "score=$scaleScore need≥90"

    . (Join-Path $Driver3 'lib\HostBProof.ps1')
    if ($liveMethod -eq 'single_pc_kernel') {
        $localLog = Join-Path $Driver3 'host_local_map.log'
        $hostbOk = $false
        if (Test-Path $localLog) {
            $last = Get-Content $localLog | Select-String 'map exit=(\d+)' | Select-Object -Last 1
            $hostbOk = $last -match 'map exit=0'
        }
        Add-SCheck 'host_local_map_proof' 20 $hostbOk $localLog
    } else {
        $hostbLog = Join-Path $Driver3 'vm_hostb_execute.log'
        $hostbOk = Test-LastHostBProof -LogPath $hostbLog
        Add-SCheck 'vm_host_b_proof' 20 $hostbOk $hostbLog
    }

    $mp = Join-Path $Byovd 'docs\lab_manifest.json'
    $kpOk = $false
    $kpDetail = 'lane_d.kernel_proof empty'
    if (Test-Path $mp) {
        $m = Get-Content $mp -Raw | ConvertFrom-Json
        $kp = $m.lane_d.kernel_proof
        if ($kp -and ($kp.PSObject.Properties | Where-Object { $_.Name -match 'marker|exit|gate|proof|ioctl' })) {
            $kpOk = $true
            $kpDetail = 'kernel_proof populated'
        }
        if (-not $kpOk -and $m.lane_d.by_profile) {
            foreach ($p in $m.lane_d.by_profile.PSObject.Properties) {
                if ($p.Value.kernel_proof -or $p.Value.marker_ok) {
                    $kpOk = $true
                    $kpDetail = "by_profile.$($p.Name)"
                    break
                }
            }
        }
    }
    Add-SCheck 'manifest_kernel_proof' 15 $kpOk $kpDetail

    $insp = Join-Path $Byovd 'driver2\cheat_inspector.json'
    $devAp = Join-Path $Byovd 'driver2\developer_approval.json'
    $dynamicOk = $false
    $dynDetail = 'no dynamic gate'
    if (Test-Path $insp) {
        $ci = Get-Content $insp -Raw | ConvertFrom-Json
        if ($ci.dynamic -and $ci.dynamic.gate_exit -eq 0) {
            $dynamicOk = $true
            $dynDetail = 'cheat_inspector dynamic gate_exit=0'
        }
        if ($ci.execute -eq $true -and $ci.approved -eq $true) {
            $dynamicOk = $true
            $dynDetail = 'cheat_inspector -Execute approved'
        }
    }
    if (-not $dynamicOk -and (Test-Path $devAp)) {
        $da = Get-Content $devAp -Raw | ConvertFrom-Json
        if ($da.approved -eq $true -and $da.scale_a_static_merge -ne $true) {
            $dynamicOk = $true
            $dynDetail = 'developer_approval dynamic (no static merge)'
        }
        if ($da.approved -eq $true -and $da.execute -eq $true) {
            $dynamicOk = $true
            $dynDetail = 'developer_approval execute'
        }
    }
    Add-SCheck 'dynamic_sdk_gate' 15 $dynamicOk $dynDetail

    $offVal = Join-Path $Driver3 'offsets_validation.json'
    $offOk = $false
    if (Test-Path $offVal) {
        $ov = Get-Content $offVal -Raw | ConvertFrom-Json
        $offOk = ($ov.ok -eq $true) -or ($ov.exit_code -eq 0)
        if (-not $offOk -and $ov.mismatches -eq 0) { $offOk = $true }
    }
    Add-SCheck 'offsets_vs_dump' 10 $offOk $offVal

    $profiles = if ($AllTitles) { @(Get-Driver3ProfileNames) } else { ,@(Resolve-Driver3Profile $Profile) }
    $liveFails = @()
    foreach ($p in $profiles) {
        $lr = Test-Driver3LiveReady -Profile $p
        if (-not $lr.ok) { $liveFails += $p }
    }
    Add-SCheck 'all_profiles_live_ready' 10 ($liveFails.Count -eq 0) ($liveFails -join ',')

    if ($liveMethod -in @('kernel_lab_s', 'single_pc_kernel')) {
        $sdkOk = (Test-Path (Join-Path $Byovd 'driver2\sdk\Driver2Session.hpp')) -and
            (Test-Path (Join-Path $Byovd 'bin\Driver2Client.exe'))
        Add-SCheck 'driver2_sdk_ship' 10 $sdkOk 'kernel_lab_s — no FPGA'
        Add-SCheck 'live_read_method' 5 $true 'kernel_lab_s'
    } else {
        $dmaExe = @(
            (Join-Path $DevRoot 'debug\RobRunDMA.exe'),
            (Join-Path $DevRoot 'build\RobRunDMA.exe')
        ) | Where-Object { Test-Path $_ } | Select-Object -First 1
        $dmaBuild = Join-Path $DevRoot 'dma\scripts\build-windows.ps1'
        $manifestDmaOk = $false
        if (Test-Path $mp) {
            $manifestDmaOk = (Get-Content $mp -Raw | ConvertFrom-Json).lane_d.dma_status -in 'ok', 'build_only'
        }
        $dmaOk = [bool]$dmaExe -or ($manifestDmaOk -and (Test-Path $dmaBuild))
        Add-SCheck 'dma_ship_path' 10 $dmaOk ($(if ($dmaExe) { $dmaExe } else { 'exe missing; need build or dma_status' }))
        Add-SCheck 'live_read_method' 5 $true 'dma'
    }

    $nodPath = Join-Path $Driver3 'developer_nod.json'
    $nodOk = $false
    if (Test-Path $nodPath) {
        $nod = Get-Content $nodPath -Raw | ConvertFrom-Json
        $nodOk = ($nod.nod_pct -ge 90) -and ($nod.checks | Where-Object { $_.check -eq 'vm_host_b_proof' }).pass
    }
    Add-SCheck 'developer_nod_integrated' 5 $nodOk 'nod≥90 + host_b in nod'

    $earned = ($checks | Where-Object { $_.pass } | ForEach-Object { $_.weight } | Measure-Object -Sum).Sum
    $max = ($checks | ForEach-Object { $_.weight } | Measure-Object -Sum).Sum
    $pct = if ($max -gt 0) { [Math]::Round(100.0 * $earned / $max, 1) } else { 0 }

    $letter = if ($pct -ge 95 -and $hostbOk -and $dynamicOk -and $kpOk) { 'S' }
        elseif ($pct -ge 85) { 'A+' }
        elseif ($pct -ge 70) { 'A' }
        else { 'B' }

    [pscustomobject]@{
        schema    = 'driver3_s_tier_v1'
        utc       = (Get-Date).ToUniversalTime().ToString('o')
        tier      = $letter
        score_pct = $pct
        earned    = $earned
        max       = $max
        s_tier    = ($letter -eq 'S')
        checks    = @($checks)
        issues    = @($issues)
        not_claimed = @('undetected', 'D4', 'live_ranked_kernel')
        live_read_method = $liveMethod
        profiles  = $profiles
    }
}
