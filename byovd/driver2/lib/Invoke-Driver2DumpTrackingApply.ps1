function Invoke-Driver2DumpTrackingApply {
    param(
        [ValidateSet('rust', 'r6')]
        [string]$Profile = 'rust',
        [ValidateSet('session', 'bypass', 'any')]
        [string]$MapMode = 'session',
        [switch]$SkipClientImproves
    )

    $Driver2 = Split-Path $PSScriptRoot -Parent
    $Byovd = Split-Path $Driver2 -Parent
    $Out = Join-Path $Driver2 'driver2_dump_tracking.json'

    . (Join-Path $PSScriptRoot 'Get-Driver2DumpTracking.ps1')
    $model = Get-Driver2DumpTrackingModel $Profile
    $live = Test-LiveAcDriverMatchesDump $model

    $verify = Join-Path $Byovd 'scripts\verify-pf-diag.ps1'
    $diag = & $verify -Profile $Profile -MapMode $MapMode

    $client = Join-Path $Byovd 'bin\Driver2Client.exe'
    $improveResults = [System.Collections.Generic.List[object]]::new()
    if (-not $SkipClientImproves -and (Test-Path $client)) {
        foreach ($c in $model.session_client_cmds) {
            $args = @($c.cmd)
            if ($c.args) { $args += $c.args }
            & $client @args
            $improveResults.Add([pscustomobject]@{ cmd = $c.cmd; args = $c.args; exit = $LASTEXITCODE })
        }
    }

    $improveOk = ($improveResults.Count -eq 0) -or (@($improveResults | Where-Object { $_.exit -ne 0 }).Count -eq 0)
    $noBypassSweep = -not $diag.max869
    if ($MapMode -eq 'session' -and $diag.strip -and $diag.max869) {
        $noBypassSweep = $false
    }

    $issues = [System.Collections.Generic.List[string]]::new()
    if (-not $diag.ok) { $issues.Add("pf_diag session mismatch (see $($diag.path))") }
    if (-not $noBypassSweep) { $issues.Add('bypass sweep IOCTLs detected (0x869/strip) — not ranked-safe') }
    if (-not $improveOk) { $issues.Add('title improve IOCTL via Driver2Client failed') }
    $live.warnings | ForEach-Object { if ($_ -match 'drift') { $issues.Add($_) } }

    $report = [ordered]@{
        schema              = 'driver2_dump_tracking_v1'
        utc                 = (Get-Date).ToUniversalTime().ToString('o')
        profile             = $Profile
        map_mode            = $MapMode
        policy_tier         = $model.policy_tier
        dump_manifest       = $model.manifest
        kernel_hunt_high    = $model.kernel_hunt_high
        pf_diag             = $diag
        live_ac_hash        = $live
        client_improves     = @($improveResults)
        ranked_safe_no_bypass = $noBypassSweep
        ok                  = ($diag.ok -and $noBypassSweep -and $improveOk -and ($issues.Count -eq 0))
        issues              = @($issues)
        doc                 = 'AC_FINGERPRINTS.md'
    }

    ($report | ConvertTo-Json -Depth 10) | Set-Content $Out -Encoding utf8
    return [pscustomobject]$report
}

function Test-Driver2DumpTrackingFile {
    param([string]$Path = '')
    $Driver2 = Split-Path $PSScriptRoot -Parent
    if (-not $Path) { $Path = Join-Path $Driver2 'driver2_dump_tracking.json' }
    if (-not (Test-Path $Path)) { return $false }
    try {
        $j = Get-Content $Path -Raw | ConvertFrom-Json
        return ($j.ok -eq $true)
    } catch { return $false }
}
