param(
    [ValidateSet('dma', 'kernel_lab_s', 'single_pc_kernel')]
    [string]$Method = 'kernel_lab_s',
    [string]$Game = 'rust'
)

$Driver3 = $PSScriptRoot
$Byovd = Split-Path $Driver3 -Parent
$cfg = Join-Path $Driver3 'live_read_method.json'

@{
    schema = 'driver3_live_read_method_v1'
    utc    = (Get-Date).ToUniversalTime().ToString('o')
    method = $Method
    note   = switch ($Method) {
        'kernel_lab_s' { 'VM Driver2, no FPGA' }
        'single_pc_kernel' { 'One PC map + cheat EXE — NOT undetected on Tier 0' }
        default { 'Lane D ranked: FPGA dma, no kernel on Tier 0 host' }
    }
} | ConvertTo-Json | Set-Content $cfg -Encoding utf8

$dma = if ($Method -eq 'dma') { 'ok' } else { 'not_used' }
& (Join-Path $Driver3 'sync-lab-manifest.ps1') -Game $Game -DmaStatus $dma
$mp = Join-Path $Byovd 'docs\lab_manifest.json'
$j = Get-Content $mp -Raw | ConvertFrom-Json
if (-not $j.lane_d) { $j | Add-Member lane_d @{} }
$j.lane_d | Add-Member -NotePropertyName live_read_method -NotePropertyValue $Method -Force
($j | ConvertTo-Json -Depth 12) | Set-Content $mp -Encoding utf8

Write-Host "[+] live_read_method=$Method → $cfg + lab_manifest.lane_d"
if ($Method -eq 'kernel_lab_s') {
    Write-Host '    Play path: VM only — kernel-lab.ps1, Driver2Runner, Driver2Client'
    Write-Host '    See SCALE_S_KERNEL_LAB.md'
}
exit 0
