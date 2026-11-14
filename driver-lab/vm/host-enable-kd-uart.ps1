# One-time host setup: VirtualBox COM1 pipe for kernel debug.
# VM must be powered off. Does not start or stop the VM.
param(
    [string]$VmName = 'DriverLab-E-26200',
    [string]$Pipe = '\\.\pipe\DriverLabKD'
)

$ErrorActionPreference = 'Stop'
$VBox = 'C:\Program Files\Oracle\VirtualBox\VBoxManage.exe'
if (-not (Test-Path $VBox)) { throw 'VBoxManage missing' }

$info = & $VBox showvminfo $VmName --machinereadable
$state = ($info | Select-String '^VMState=').ToString().Split('=')[1].Trim('"')
if ($state -ne 'poweroff') {
    Write-Host "VM state is $state. Power off (not save-state) before changing UART."
    exit 2
}

& $VBox modifyvm $VmName --uart1 0x3F8 4 --uartmode1 server $Pipe
if ($LASTEXITCODE -ne 0) { throw "modifyvm uart failed: $LASTEXITCODE" }
Write-Host "UART1 server pipe $Pipe (COM1, IRQ4)."
Write-Host "Next, elevated in the guest: Z:\driver-lab\vm\bg\guest-enable-kd-serial.ps1"
Write-Host "Reboot the guest, then: python driver-lab\reversal\kd_dump.py dump <disk.sys>"
exit 0
