# Live read method: dma (ranked Host A) | kernel_lab_s (VM Driver2, no FPGA).
function Get-Driver3LiveReadMethod {
    $Driver3 = Split-Path $PSScriptRoot -Parent
    $Byovd = Split-Path $Driver3 -Parent
    $cfg = Join-Path $Driver3 'live_read_method.json'
    $fromFile = $null
    if (Test-Path $cfg) {
        $fromFile = (Get-Content $cfg -Raw | ConvertFrom-Json).method
    }
    $mp = Join-Path $Byovd 'docs\lab_manifest.json'
    $fromManifest = $null
    if (Test-Path $mp) {
        $m = Get-Content $mp -Raw | ConvertFrom-Json
        if ($m.lane_d.live_read_method) { $fromManifest = [string]$m.lane_d.live_read_method }
    }
    $method = if ($fromManifest) { $fromManifest } elseif ($fromFile) { $fromFile } else { 'dma' }
    if ($method -notin @('dma', 'kernel_lab_s', 'single_pc_kernel')) { $method = 'dma' }
    [pscustomobject]@{
        method = $method
        dma_ranked_live = ($method -eq 'dma')
        kernel_lab_no_fpga = ($method -eq 'kernel_lab_s')
        single_pc_kernel = ($method -eq 'single_pc_kernel')
        not_for_live_ac_same_pc = if ($method -eq 'kernel_lab_s') {
            'Rust/R6/ABI with AC on game PC — use VM map + Driver2Client; not undetected'
        } else { $null }
    }
}
