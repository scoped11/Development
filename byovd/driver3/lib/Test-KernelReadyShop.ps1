# Rust + R6 kernel shop bar (no FPGA, VM Driver2).
function Test-KernelReadyShop {
    param([string[]]$Profiles = @('rust', 'r6'))

    $Driver3 = Split-Path $PSScriptRoot -Parent
    $Byovd = Split-Path $Driver3 -Parent

    . (Join-Path $Driver3 'lib\Get-Driver3LiveReadMethod.ps1')
    . (Join-Path $Driver3 'lib\Test-Driver3BuildIdGate.ps1')
    . (Join-Path $Driver3 'lib\Test-Driver3KernelLabReady.ps1')
    . (Join-Path $Driver3 'lib\HostBProof.ps1')

    $method = Get-Driver3LiveReadMethod
    $checks = [System.Collections.Generic.List[object]]::new()
    $issues = [System.Collections.Generic.List[string]]::new()

    function Add([string]$Id, [bool]$Pass, [string]$Detail = '') {
        $checks.Add([pscustomobject]@{ id = $Id; pass = $Pass; detail = $Detail })
        if (-not $Pass) { $issues.Add("$Id — $Detail") }
    }

    Add 'live_read_kernel_lab_s' ($method.method -eq 'kernel_lab_s') $method.method

    $inspOk = $false
    $insp = Join-Path $Byovd 'driver2\cheat_inspector.json'
    if (Test-Path $insp) {
        $ci = Get-Content $insp -Raw | ConvertFrom-Json
        $inspOk = $ci.approved -eq $true -and $ci.final_score_pct -ge 90
    }
    Add 'cheat_inspector' $inspOk $insp

    $bins = @(
        (Join-Path $Byovd 'bin\Driver2Client.exe'),
        (Join-Path $Byovd 'bin\Driver2Runner.exe'),
        (Join-Path $Byovd 'bin\mapper_nal.exe')
    )
    Add 'driver2_stack' (@($bins | Where-Object { Test-Path $_ }).Count -eq 3) 'bin/'

    foreach ($p in $Profiles) {
        Add "build_id_$p" (Test-Driver3BuildIdGate -Profile $p).ok ''
        $lr = Test-Driver3KernelLabReady -Profile $p
        Add "profile_$p" $lr.ok ($lr.issues -join '; ')
    }

    $hostb = Join-Path $Driver3 'vm_hostb_execute.log'
    $hb = Test-LastHostBProof -LogPath $hostb
    Add 'vm_host_b_map' $hb $hostb

    $mp = Join-Path $Byovd 'docs\lab_manifest.json'
    $kp = $false
    if (Test-Path $mp) {
        $m = Get-Content $mp -Raw | ConvertFrom-Json
        foreach ($p in $Profiles) {
            if ($m.lane_d.by_profile.$p.kernel_proof -or $m.lane_d.by_profile.$p.marker_ok) { $kp = $true; break }
        }
        if (-not $kp -and $m.lane_d.kernel_proof) { $kp = $true }
    }
    Add 'kernel_proof_manifest' $kp 'lane_d'

    $passed = @($checks | Where-Object { $_.pass }).Count
    $max = $checks.Count
    $pct = if ($max) { [math]::Round(100.0 * $passed / $max, 1) } else { 0 }

    $grade = 'BLOCKED'
    $shopReady = $false
    if ($method.method -eq 'kernel_lab_s' -and (Test-Driver3BuildIdGate -Profile 'rust').ok -and (Test-Driver3BuildIdGate -Profile 'r6').ok -and $inspOk) {
        $shopReady = @($bins | Where-Object { Test-Path $_ }).Count -eq 3
    }
    if ($shopReady -and $hb -and $kp) { $grade = 'S_KERNEL' }
    elseif ($shopReady) { $grade = 'KERNEL_READY' }
    elseif ($pct -ge 70) { $grade = 'KERNEL_PARTIAL' }

    [pscustomobject]@{
        schema      = 'kernel_ready_shop_v1'
        utc         = (Get-Date).ToUniversalTime().ToString('o')
        grade       = $grade
        shop_ready  = $shopReady
        score_pct   = $pct
        profiles    = $Profiles
        titles      = @{ rust = 'Rust EAC'; r6 = 'Rainbow Six BattlEye' }
        checks      = @($checks)
        issues      = @($issues)
        vm_commands = @{
            rust = 'Z:\byovd\bin\Driver2Runner.exe --profile rust --mode bypass --force-map'
            r6   = 'Z:\byovd\bin\Driver2Runner.exe --profile r6 --mode bypass --force-map'
        }
        cheat_read  = 'Z:\byovd\bin\Driver2Client.exe pread <pid> <va_hex> <n>'
        not_claimed = @('undetected', 'live_ac_host_no_dma', 'D4')
    }
}
