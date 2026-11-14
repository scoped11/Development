# Copy driver3 artifacts from VM guest (Z:) to host repo via guestcontrol.
param(
    [string]$VmName = 'DriverLab-E-26200',
    [switch]$AllKernelProofs,
    [switch]$IncludeInspector
)

$Driver3 = $PSScriptRoot
$GuestExec = Join-Path (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path 'driver-lab\vm\host-guest-exec.ps1'
$names = @('vm_hostb_execute.log', 'vm_guest_smoke.json', 'kernel_proof_rust.json')
if ($AllKernelProofs) {
    $names += @(
        'kernel_proof_r6.json', 'kernel_proof_arksa.json',
        'kernel_proof_deltaforce.json', 'kernel_proof_arenabreakout.json'
    )
}
if ($IncludeInspector) {
    $names += 'cheat_inspector_guest.json'
    $destInsp = Join-Path (Split-Path $Driver3 -Parent) 'driver2\cheat_inspector.json'
} else { $destInsp = $null }

foreach ($f in $names) {
    $out = & $GuestExec -VmName $VmName -TimeoutMs 120000 -Command @(
        'C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe',
        '-NoProfile', '-Command',
        "if (Test-Path 'Z:\byovd\driver3\$f') { Get-Content 'Z:\byovd\driver3\$f' -Raw } else { '' }"
    ) 2>&1 | Out-String
    if ($out.Trim()) {
        $target = if ($f -eq 'cheat_inspector_guest.json' -and $destInsp) { $destInsp } else { Join-Path $Driver3 $f }
        $out.Trim() | Set-Content $target -Encoding utf8
        Write-Host "[+] synced $f -> $target"
    }
}
