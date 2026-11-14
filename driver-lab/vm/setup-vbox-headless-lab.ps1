#Requires -Version 5.1
<#
.SYNOPSIS
  Headless VirtualBox lab VM for Lane E (no visible VM window when started with -Headless).

  Host: Win11 Home (no Hyper-V). Matches BUILD 26200 family via install ISO you provide.

.EXAMPLE
  .\setup-vbox-headless-lab.ps1
  .\setup-vbox-headless-lab.ps1 -IsoPath D:\ISOs\Win11_24H2.iso -Start
#>
param(
    [string]$VmName = 'DriverLab-E-26200',
    [string]$IsoPath = '',
    [int]$MemoryMB = 8192,
    [int]$Cpus = 4,
    [int]$DiskGB = 128,
    [switch]$Start,
    [switch]$ForceRecreate
)

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
$VBox = 'C:\Program Files\Oracle\VirtualBox\VBoxManage.exe'
if (-not (Test-Path $VBox)) { throw "Install VirtualBox first (winget install Oracle.VirtualBox)" }

$VmRoot = Join-Path $PSScriptRoot "vbox\$VmName"
New-Item -ItemType Directory -Force -Path $VmRoot | Out-Null
$DiskPath = Join-Path $VmRoot "$VmName.vdi"
$DefaultIso = Join-Path $PSScriptRoot "media\Win11.iso"

function Invoke-VBox {
    param([Parameter(ValueFromRemainingArguments)][string[]]$VBoxArgs)
    $oldEap = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        & $VBox @VBoxArgs 2>&1 | Out-Null
        if ($LASTEXITCODE -ne 0) { return $null }
        return $true
    } finally {
        $ErrorActionPreference = $oldEap
    }
}

$existing = Invoke-VBox @('showvminfo', $VmName)
if ($null -ne $existing -and $ForceRecreate) {
    Invoke-VBox controlvm $VmName poweroff 2>$null | Out-Null
    Start-Sleep -Seconds 2
    Invoke-VBox unregistervm $VmName --delete
}

$existing = Invoke-VBox @('showvminfo', $VmName)
if ($null -eq $existing) {
    Write-Host "[+] Creating VM $VmName"
    Invoke-VBox createvm --name $VmName --ostype Windows11_64 --register --basefolder $VmRoot
    Invoke-VBox modifyvm $VmName --memory $MemoryMB --cpus $Cpus --vram 128 `
        --graphicscontroller vboxsvga --nic1 nat --audio-driver none --audio-enabled off `
        --boot1 dvd --boot2 disk --boot3 none --boot4 none `
        --ioapic on --pae on --longmode on --largepages on --triplefaultreset on `
        --accelerate3d off --clipboard-mode disabled
    if (-not (Test-Path $DiskPath)) {
        Invoke-VBox createmedium disk --filename $DiskPath --size ($DiskGB * 1024)
    }
    Invoke-VBox storagectl $VmName --name SATA --add sata --controller IntelAhci --portcount 2 --bootable on
    Invoke-VBox storageattach $VmName --storagectl SATA --port 0 --device 0 --type hdd --medium $DiskPath
} else {
    Write-Host "[=] VM already exists: $VmName"
}

# Shared folder: host Development -> guest D:\host
$DevRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
Invoke-VBox sharedfolder remove $VmName --name hostdev 2>$null | Out-Null
Invoke-VBox sharedfolder add $VmName --name hostdev --hostpath $DevRoot --automount --auto-mount-point D:\hostdev

# ISO attach
if (-not $IsoPath -and (Test-Path $DefaultIso)) { $IsoPath = $DefaultIso }
if ($IsoPath -and (Test-Path $IsoPath)) {
    Invoke-VBox storageattach $VmName --storagectl SATA --port 1 --device 0 --type dvddrive --medium $IsoPath
    Write-Host "[+] Attached ISO: $IsoPath"
} else {
    Write-Warning "No ISO at '$IsoPath' or '$DefaultIso'. Place Win11 ISO in driver-lab\vm\media\Win11.iso then re-run or use -IsoPath"
    Write-Host "Get closest build to host $(Get-ItemProperty 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion' | ForEach-Object { $_.CurrentBuildNumber + '.' + $_.UBR })"
    Write-Host "Insider 26200.x may need same-channel ISO; retail 24H2 (26100) is acceptable for CLFS/E-VM primitive work."
}

# Host metadata for guest manifest
$manifest = @{
    host_build = (Get-ItemProperty 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion' | ForEach-Object { "$($_.CurrentBuildNumber).$($_.UBR)" })
    vm_name    = $VmName
    created_utc = (Get-Date).ToUniversalTime().ToString('o')
    iso        = $IsoPath
}
$manifest | ConvertTo-Json | Set-Content (Join-Path $VmRoot 'host_build.json') -Encoding UTF8

if ($Start) {
    Write-Host "[+] Starting headless (no VM window): $VmName"
    Invoke-VBox startvm $VmName --type headless
    Write-Host "Install Windows from attached ISO (first boot). After install, run guest script:"
    Write-Host "  D:\hostdev\driver-lab\vm\guest-hunt-bootstrap.ps1"
}

Write-Host "[+] VM root: $VmRoot"
Write-Host "Headless start: & '$VBox' startvm $VmName --type headless"
Write-Host "Stop:           & '$VBox' controlvm $VmName poweroff"
