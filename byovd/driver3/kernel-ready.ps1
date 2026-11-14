# Rust + R6 kernel shop — no FPGA, VM Driver2 (kernel_lab_s).
param(
    [switch]$MapVm,
    [switch]$ReportOnly,
    [switch]$BuildDriver2
)

$ErrorActionPreference = 'Stop'
$Driver3 = $PSScriptRoot
$Byovd = Split-Path $Driver3 -Parent

Write-Host ''
Write-Host '=== KERNEL READY (Rust + R6) ==='
Write-Host '    No FPGA — map Driver2 in VM, cheat reads via IOCTL'
Write-Host '    NOT live EAC/BE on game PC without dma mode'
Write-Host ''

& (Join-Path $Driver3 'set-live-read-method.ps1') -Method kernel_lab_s | Out-Null
& (Join-Path $Driver3 'sync-build-id-from-dumps.ps1') -Game rust -AllTitles | Out-Null

if ($BuildDriver2) {
    if (Test-Path (Join-Path $Byovd 'build.ps1')) { & (Join-Path $Byovd 'build.ps1') }
    if (Test-Path (Join-Path $Byovd 'driver2\build.ps1')) { & (Join-Path $Byovd 'driver2\build.ps1') }
}

& (Join-Path $Byovd 'driver2\cheat-inspector.ps1') | Out-Null
foreach ($g in @('rust', 'r6')) {
    & (Join-Path $Driver3 'lane-gate.ps1') -Game $g | Out-Null
}

if ($MapVm -and -not $ReportOnly) {
    $VBox = 'C:\Program Files\Oracle\VirtualBox\VBoxManage.exe'
    if (Test-Path $VBox) {
        $run = & $VBox showvminfo DriverLab-E-26200 --machinereadable 2>$null | Select-String 'VMState="running"'
        if ($run) {
            foreach ($p in @('rust', 'r6')) {
                Write-Host "[*] VM Host B $p..."
                & (Join-Path $Driver3 'vm-guest-hostb-from-host.ps1') -Profile $p -ForceMap -SkipProve
            }
            & (Join-Path $Driver3 'sync-kernel-proof-from-artifacts.ps1') -Primary rust
        } else {
            Write-Host '[!] Start DriverLab-E-26200 or use open-vm-rdp.ps1'
        }
    }
}

. (Join-Path $Driver3 'lib\Test-KernelReadyShop.ps1')
$kr = Test-KernelReadyShop

& (Join-Path $Driver3 'build-dashboard.ps1') -AllTitles | Out-Null
& (Join-Path $Driver3 'build-s-tier.ps1') -AllTitles | Out-Null

$out = Join-Path $Driver3 'kernel_ready.json'
($kr | ConvertTo-Json -Depth 8) | Set-Content $out -Encoding utf8

Write-Host "[+] grade=$($kr.grade) shop_ready=$($kr.shop_ready) score=$($kr.score_pct)% -> $out"

if ($kr.grade -eq 'S_KERNEL') {
    Write-Host '[+] S_KERNEL — Rust/R6 kernel path proved. VM map + manifest OK.'
} elseif ($kr.grade -eq 'KERNEL_READY') {
    Write-Host '[+] KERNEL_READY — bins + dumps + gates. Map in VM (see KERNEL_READY.md) for S_KERNEL.'
    Write-Host '    open-vm-rdp.ps1 → elevated register + host-b rust/r6'
} else {
    $kr.issues | ForEach-Object { Write-Host "    [!] $_" }
}

Write-Host ''
Write-Host 'VM Rust: ' $kr.vm_commands.rust
Write-Host 'VM R6:   ' $kr.vm_commands.r6
Write-Host 'Read:    ' $kr.cheat_read
Write-Host ''

exit $(switch ($kr.grade) { 'S_KERNEL' { 0 } 'KERNEL_READY' { 0 } 'KERNEL_PARTIAL' { 5 } default { 6 } })
