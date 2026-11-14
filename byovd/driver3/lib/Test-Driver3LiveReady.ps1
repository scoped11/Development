function Test-Driver3LiveReady {
    param(
        [string]$Profile = 'rust',
        [switch]$StrictDump
    )
    $Driver3 = Split-Path $PSScriptRoot -Parent
    $Byovd = Split-Path $Driver3 -Parent
    $DevRoot = Split-Path $Byovd -Parent

    . (Join-Path $Driver3 'Resolve-Driver3Game.ps1')
    . (Join-Path $Driver3 'lib\Test-Driver3BuildIdGate.ps1')
    . (Join-Path $Driver3 'lib\ProfileDump.ps1')
    . (Join-Path $Driver3 'lib\Test-DumpFreshness.ps1')
    . (Join-Path $Driver3 'lib\Get-Driver3LiveReadMethod.ps1')
    . (Join-Path $Driver3 'lib\Test-Driver3KernelLabReady.ps1')

    $readMethod = (Get-Driver3LiveReadMethod).method
    if ($readMethod -eq 'kernel_lab_s') {
        return Test-Driver3KernelLabReady -Profile $Profile
    }

    $p = Resolve-Driver3Profile $Profile
    $meta = Get-Driver3Meta $p
    $issues = [System.Collections.Generic.List[string]]::new()
    $checks = [System.Collections.Generic.List[object]]::new()

    function Add([string]$Name, [bool]$Pass, [string]$Detail = '') {
        $checks.Add([pscustomobject]@{ name = $Name; pass = $Pass; detail = $Detail })
        if (-not $Pass) { $issues.Add("$Name — $Detail") }
    }

    $bg = Test-Driver3BuildIdGate -Profile $p
    Add 'build_id_manifest' $bg.ok ($bg.issues -join '; ')

    $fresh = Test-Driver3DumpFreshness -Profile $p
    $dumpOk = $fresh.ok -and ($fresh.level -ne 'missing')
    if ($StrictDump) { $dumpOk = $dumpOk -and ($fresh.level -eq 'ok') }
    Add 'ac_dump_fresh' $dumpOk ($fresh.issues -join '; ')

    $dmaRoot = Join-Path $DevRoot 'dma'
    Add 'dma_tree' (Test-Path $dmaRoot) $dmaRoot

    $dmaExe = @(
        (Join-Path $DevRoot 'debug\RobRunDMA.exe'),
        (Join-Path $DevRoot 'build\RobRunDMA.exe')
    ) | Where-Object { Test-Path $_ } | Select-Object -First 1
    $buildPs1 = Join-Path $dmaRoot 'scripts\build-windows.ps1'
    Add 'dma_binary_or_build' ($dmaExe -or (Test-Path $buildPs1)) 'build dma/scripts/build-windows.ps1'

    $mp = Join-Path $Byovd 'docs\lab_manifest.json'
    $splitOk = $false
    if (Test-Path $mp) {
        $m = Get-Content $mp -Raw | ConvertFrom-Json
        $splitOk = ($m.lane_d.method -eq 'split-trust-calibration-bridge') -and [bool]$m.lane_d.build_id
    }
    Add 'lane_d_manifest' $splitOk 'docs/lab_manifest.json lane_d'

    $route = Join-Path $Driver3 $meta.doc
    Add 'route_card' (Test-Path $route) $meta.doc

    Add 'ranked_fail_closed' (Test-Path (Join-Path $Driver3 'ranked.ps1')) 'no kernel default on ranked'

    $nod = [pscustomobject]@{
        ok              = ($issues.Count -eq 0)
        profile         = $p
        title           = $meta.title
        ac_family       = $meta.ac_family
        checks          = @($checks)
        issues          = @($issues)
        requires_note   = 'Hardware DMA required for live Host A reads — not auto-detected'
        not_claimed     = @('undetected', 'D4', 'no_dma_card_shortcut')
    }
    return $nod
}
