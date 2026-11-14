function Test-Driver3KernelLabReady {
    param([string]$Profile = 'rust')
    $Driver3 = Split-Path $PSScriptRoot -Parent
    $Byovd = Split-Path $Driver3 -Parent

    . (Join-Path $Driver3 'Resolve-Driver3Game.ps1')
    . (Join-Path $Driver3 'lib\Test-Driver3BuildIdGate.ps1')
    . (Join-Path $Driver3 'lib\Test-DumpFreshness.ps1')
    . (Join-Path $Driver3 'lib\HostBProof.ps1')

    $p = Resolve-Driver3Profile $Profile
    $meta = Get-Driver3Meta $p
    $issues = [System.Collections.Generic.List[string]]::new()
    $checks = [System.Collections.Generic.List[object]]::new()

    function Add([string]$Name, [bool]$Pass, [string]$Detail = '') {
        $checks.Add([pscustomobject]@{ name = $Name; pass = $Pass; detail = $Detail })
        if (-not $Pass) { $issues.Add("$Name — $Detail") }
    }

    Add 'build_id_manifest' (Test-Driver3BuildIdGate -Profile $p).ok ''
    $fresh = Test-Driver3DumpFreshness -Profile $p
    Add 'ac_dump_fresh' ($fresh.level -ne 'missing') ($fresh.issues -join '; ')

    $bins = @(
        (Join-Path $Byovd 'bin\Driver2Client.exe'),
        (Join-Path $Byovd 'bin\Driver2Runner.exe'),
        (Join-Path $Byovd 'bin\mapper_nal.exe')
    )
    Add 'driver2_binaries' (@($bins | Where-Object { Test-Path $_ }).Count -ge 2) ($bins -join '; ')

    $sdk = Join-Path $Byovd 'driver2\sdk\Driver2Session.hpp'
    Add 'driver2_sdk' (Test-Path $sdk) $sdk

    $hostb = Join-Path $Driver3 'vm_hostb_execute.log'
    $hb = Test-LastHostBProof -LogPath $hostb
    Add 'vm_host_b_proof' $hb $(if ($hb) { 'ok' } else { 'elevated VM map pending' })

    $route = Join-Path $Driver3 'drivers\route-kernel-lab.md'
    Add 'kernel_lab_route' (Test-Path $route) 'drivers/route-kernel-lab.md'

    [pscustomobject]@{
        ok        = ($issues.Count -eq 0)
        profile   = $p
        title     = $meta.title
        mode      = 'kernel_lab_s'
        checks    = @($checks)
        issues    = @($issues)
        use_how   = 'Map in VM → Driver2Client pread / Driver2Session in your EXE — hooks in your process, not in game on AC host'
        not_claimed = @('undetected', 'live_ac_same_pc_no_dma', 'D4')
    }
}
