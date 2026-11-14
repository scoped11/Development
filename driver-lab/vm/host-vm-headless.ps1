# Host control — headless VM (no VirtualBox GUI required after install)
param(
    [ValidateSet('start','stop','status','snapshot')]
    [string]$Action = 'status',
    [string]$VmName = 'DriverLab-E-26200',
    [string]$SnapshotName = 'clean'
)

$VBox = 'C:\Program Files\Oracle\VirtualBox\VBoxManage.exe'
if (-not (Test-Path $VBox)) { throw 'VirtualBox not installed' }

switch ($Action) {
    'start'  { & $VBox startvm $VmName --type headless; break }
    'stop'   { & $VBox controlvm $VmName poweroff; break }
    'status' { & $VBox showvminfo $VmName --machinereadable | Select-String 'VMState'; break }
    'snapshot' {
        & $VBox snapshot $VmName take $SnapshotName
        Write-Host "Snapshot: $SnapshotName"
    }
}
