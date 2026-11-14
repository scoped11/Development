#Requires -RunAsAdministrator
# Optional IOCTL hello driver only. For EAC/BE parity use setup-vm-lab-betest.ps1 (Betest + mapper).
param([switch]$Uninstall, [string]$ServiceName = 'VmLabDrv')

$ErrorActionPreference = 'Stop'
$VmRoot = $PSScriptRoot
$SysSrc = Join-Path $VmRoot 'bin\VmLabDrv.sys'
$Cli = Join-Path $VmRoot 'bin\VmLabClient.exe'
$SysDst = Join-Path $env:SystemRoot 'System32\drivers\VmLabDrv.sys'

if (-not (Test-Path $SysSrc)) {
    & (Join-Path $VmRoot 'build-vm-lab-driver.ps1')
}

if ($Uninstall) {
    sc.exe stop $ServiceName 2>$null | Out-Null
    Start-Sleep -Seconds 1
    sc.exe delete $ServiceName 2>$null | Out-Null
    if (Test-Path $SysDst) { Remove-Item $SysDst -Force -ErrorAction SilentlyContinue }
    Write-Host "[+] Uninstalled $ServiceName"
    exit 0
}

$offline = Join-Path $VmRoot 'offline'
if (Test-Path (Join-Path $offline 'Import-LabPublisher.ps1')) {
    & (Join-Path $offline 'Import-LabPublisher.ps1')
    . (Join-Path $offline 'Set-LabProductionBcd.ps1')
    Set-LabProductionBcd
} else {
    Write-Warning 'Import-LabPublisher missing — run offline vault setup first'
}

Copy-Item $SysSrc $SysDst -Force
sc.exe query $ServiceName 2>$null | Out-Null
if ($LASTEXITCODE -eq 0) {
    sc.exe stop $ServiceName 2>$null | Out-Null
    sc.exe delete $ServiceName 2>$null | Out-Null
    Start-Sleep -Seconds 1
}

sc.exe create $ServiceName type= kernel start= demand binPath= 'System32\drivers\VmLabDrv.sys'
if ($LASTEXITCODE -ne 0) { throw 'sc create failed' }

sc.exe start $ServiceName
if ($LASTEXITCODE -ne 0) {
    Write-Warning 'sc start failed — reboot guest after testsigning, then re-run this script'
    exit $LASTEXITCODE
}

Write-Host "[+] VmLab loaded. Device: \\.\VmLab0"
if (Test-Path $Cli) {
    & $Cli
    if ($LASTEXITCODE -eq 0) {
        $note = Join-Path (Join-Path $VmRoot '..\exploit\notes') 'vm-vmlab-smoke.json'
        @{ smoke = 'vmlab_ioctl_rw'; ok = $true; utc = (Get-Date).ToUniversalTime().ToString('o') } |
            ConvertTo-Json | Set-Content $note -Encoding UTF8
        Write-Host "[+] Smoke OK -> $note"
    }
}
