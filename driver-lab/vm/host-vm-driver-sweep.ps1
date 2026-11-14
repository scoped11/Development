# Host: elevated guest harvest + load-test matrix (schtasks HIGHEST).
param([int]$TimeoutMs = 900000)
$elev = Join-Path $PSScriptRoot 'host-guest-run-elevated.ps1'
$guest = Join-Path $PSScriptRoot 'host-guest-exec.ps1'

Write-Host '[*] unstick PawnIO_setup'
& $guest -Command @('cmd.exe', '/c', 'taskkill /F /IM PawnIO_setup.exe 2>nul') -TimeoutMs 60000 | Out-Null

Write-Host '[*] VM harvest PawnIO (7z on guest -> Z:\harvested-vm)'
& $elev -ScriptOnGuest 'Z:\driver-lab\vm\bg\guest-vm-harvest-setup.ps1' -TimeoutMs $TimeoutMs

Write-Host '[*] elevated driver matrix (sc load inbox)'
& $elev -ScriptOnGuest 'Z:\driver-lab\vm\bg\guest-vm-driver-matrix.ps1' -TimeoutMs $TimeoutMs
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

if (Test-Path 'C:\Users\baile\OneDrive\Desktop\Development\driver-lab\vm\bg\vm_driver_matrix.csv') {
    Import-Csv 'C:\Users\baile\OneDrive\Desktop\Development\driver-lab\vm\bg\vm_driver_matrix.csv' | Format-Table -AutoSize
}
