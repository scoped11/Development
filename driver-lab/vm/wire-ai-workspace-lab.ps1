# Wire existing AI-Workspace VM as DriverLab (shared folder + NAT + host staging).
# Run from elevated PowerShell on host. Stop VirtualBox GUI lock if VBoxManage reports VM locked.
#Requires -Version 5.1
param(
    [string]$VmName = 'AI_workspace',
    [string]$DevRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path,
    [string]$DumpRoot = 'E:\driver-lab-ac-dumps'
)

$ErrorActionPreference = 'Stop'
$VBox = 'C:\Program Files\Oracle\VirtualBox\VBoxManage.exe'
if (-not (Test-Path $VBox)) { throw 'VirtualBox not found' }

$vm = & $VBox list vms | Select-String "`"$VmName`""
if (-not $vm) { throw "VM not found: $VmName" }

foreach ($dir in @(
        $DumpRoot,
        'E:\lab-staging',
        (Join-Path $DevRoot 'driver-lab\exploit\notes'),
        (Join-Path $DevRoot 'driver-lab\audit\inbox'),
        (Join-Path $DevRoot 'driver-lab\dumps')
    )) {
    New-Item -ItemType Directory -Force -Path $dir | Out-Null
}

& $VBox sharedfolder remove $VmName --name hostdev 2>$null | Out-Null
& $VBox sharedfolder add $VmName --name hostdev --hostpath $DevRoot --automount --auto-mount-point 'D:\hostdev'

& (Join-Path $PSScriptRoot 'enable-vm-access.ps1') -VmName $VmName

$hostBuild = (Get-ItemProperty 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion')
$manifest = @{
    wired_utc     = (Get-Date).ToUniversalTime().ToString('o')
    lab_vm        = $VmName
    canonical_vm  = 'DriverLab-E-26200'
    dev_root      = $DevRoot
    dump_root     = $DumpRoot
    host_build    = "$($hostBuild.CurrentBuildNumber).$($hostBuild.UBR)"
    shared_folder = @{ name = 'hostdev'; guest = 'D:\hostdev' }
    next_guest    = @(
        'Install Guest Additions if D:\hostdev missing (reboot once).',
        'Admin PS: D:\hostdev\driver-lab\vm\guest-hunt-bootstrap.ps1',
        'Admin PS: D:\hostdev\driver-lab\vm\setup-guest-remote.ps1'
    )
    next_host     = @(
        'Copy lab.credentials.example.json -> lab.credentials.json (vm AI-Workspace, guest user/password).',
        '..\byovd\build.ps1',
        '.\host-guest-exec.ps1 -VmName AI-Workspace -Command hostname'
    )
}
$manifestPath = Join-Path $DevRoot 'driver-lab\exploit\notes\host-lab-environment.json'
$manifest | ConvertTo-Json -Depth 5 | Set-Content $manifestPath -Encoding UTF8

Write-Host "[+] Lab wired manifest: $manifestPath"
Write-Host "[+] Shared folder hostdev -> $DevRoot (guest D:\hostdev)"
Write-Host "[+] AC dump staging: $DumpRoot"
Write-Host "[!] If sharedfolder add failed (VM GUI lock), close VM window or run this script with GUI closed."