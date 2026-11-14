function Test-SinglePcSTier {
    param([string[]]$Profiles = @('rust', 'r6'))

    $Driver3 = Split-Path $PSScriptRoot -Parent
    $Byovd = Split-Path $Driver3 -Parent

    . (Join-Path $Driver3 'lib\Get-Driver3LiveReadMethod.ps1')
    . (Join-Path $Driver3 'lib\Test-Driver3BuildIdGate.ps1')

    $method = Get-Driver3LiveReadMethod
    $checks = [System.Collections.Generic.List[object]]::new()
    $issues = [System.Collections.Generic.List[string]]::new()

    function Add([string]$Id, [bool]$Pass, [string]$Detail = '') {
        $checks.Add([pscustomobject]@{ id = $Id; pass = $Pass; detail = $Detail })
        if (-not $Pass) { $issues.Add("$Id — $Detail") }
    }

    Add 'method_single_pc' ($method.method -eq 'single_pc_kernel') $method.method

    $log = Join-Path $Driver3 'host_local_map.log'
    $mapOk = $false
    if (Test-Path $log) {
        $last = Get-Content $log | Select-String 'map exit=(\d+)' | Select-Object -Last 1
        if ($last -match 'map exit=0') { $mapOk = $true }
    }
    Add 'host_local_map' $mapOk $log

    $Driver2 = Join-Path $Byovd 'driver2'
    . (Join-Path $Driver2 'lib\Invoke-Driver2RankedVerify.ps1')
    $probeOk = Test-Driver2RankedVerifyFile -Path (Join-Path $Driver2 'driver2_ranked_verify.json')
    Add 'driver2_probe' $probeOk 'driver2_ranked_verify.json'

    $bins = @(
        (Join-Path $Byovd 'bin\Driver2Client.exe'),
        (Join-Path $Byovd 'bin\Driver2Runner.exe')
    )
    Add 'driver2_bins' (@($bins | Where-Object { Test-Path $_ }).Count -eq 2) ''

    foreach ($p in $Profiles) {
        Add "build_id_$p" (Test-Driver3BuildIdGate -Profile $p).ok ''
    }

    $earned = ($checks | Where-Object { $_.pass }).Count
    $max = $checks.Count
    $pct = if ($max) { [math]::Round(100.0 * $earned / $max, 1) } else { 0 }

    $s = ($pct -eq $max -and $mapOk -and $probeOk)

    [pscustomobject]@{
        schema    = 'single_pc_s_v1'
        utc       = (Get-Date).ToUniversalTime().ToString('o')
        tier      = if ($s) { 'S' } elseif ($pct -ge 80) { 'A' } else { 'B' }
        s_tier    = $s
        score_pct = $pct
        checks    = @($checks)
        issues    = @($issues)
        flow      = 'single-pc-play.ps1 -Game rust|r6 -Execute -Live'
        not_claimed = @('undetected', 'tier0_safe', 'D4_marketing')
    }
}
