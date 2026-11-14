# Fix "cannot find Microsoft Software License Terms" — DVD-first boot + fresh disk + Win11 ISO only (manual setup OK).
param(
    [string]$VmName = 'DriverLab-E-26200',
    [string]$IsoPath = (Join-Path $PSScriptRoot 'media\Win11.iso')
)

$ErrorActionPreference = 'Stop'
$VBox = 'C:\Program Files\Oracle\VirtualBox\VBoxManage.exe'
$VmRoot = Join-Path $PSScriptRoot "vbox\$VmName"
$DiskPath = Join-Path $VmRoot "$VmName.vdi"

if (-not (Test-Path $VBox)) { throw 'VirtualBox not found' }
if (-not (Test-Path $IsoPath)) { throw "ISO missing: $IsoPath" }

Write-Host '[*] Powering off VM...'
$prevEap = $ErrorActionPreference
$ErrorActionPreference = 'Continue'
& $VBox controlvm $VmName poweroff 2>&1 | Out-Null
$ErrorActionPreference = $prevEap
Start-Sleep -Seconds 3

Write-Host '[*] Boot order: DVD then disk'
& $VBox modifyvm $VmName --boot1 dvd --boot2 disk --boot3 none --boot4 none

Write-Host '[*] Detach port 1 (remove stale unattended/aux ISO if any)'
& $VBox storageattach $VmName --storagectl SATA --port 1 --device 0 --type dvddrive --medium none 2>$null | Out-Null

Write-Host '[*] Recreate empty system disk'
& $VBox storageattach $VmName --storagectl SATA --port 0 --device 0 --type hdd --medium none 2>$null | Out-Null
if (Test-Path $DiskPath) {
    & $VBox closemedium disk $DiskPath --delete 2>$null | Out-Null
}
New-Item -ItemType Directory -Force -Path $VmRoot | Out-Null
& $VBox createmedium disk --filename $DiskPath --size 131072
& $VBox storageattach $VmName --storagectl SATA --port 0 --device 0 --type hdd --medium $DiskPath

Write-Host '[*] Attach Win11 install ISO (DVD first boot)'
& $VBox storageattach $VmName --storagectl SATA --port 1 --device 0 --type dvddrive --medium $IsoPath

Write-Host '[+] Starting VM with GUI — use installer from DVD (not the broken disk)'
& $VBox startvm $VmName --type gui
Write-Host ""
Write-Host "If prompted: choose Windows 11 Pro/Home, skip product key, create user DriverLab / LabVM-ChangeMe!"
Write-Host "After install: Devices -> Insert Guest Additions CD, reboot, then D:\hostdev share."
