#Requires -RunAsAdministrator
# Lab VM: load RwTest.sys as a normal kernel service (test-signing required).
param(
    [switch]$Uninstall,
    [string]$ServiceName = "RwTestDrv"
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$SysSrc = Join-Path $Root "bin\RwTest.sys"
$SysDst = Join-Path $env:SystemRoot "System32\drivers\RwTest.sys"

if (-not (Test-Path $SysSrc)) {
    throw "Build first: .\build.ps1 → bin\RwTest.sys"
}

if ($Uninstall) {
    sc.exe stop $ServiceName 2>$null | Out-Null
    Start-Sleep -Seconds 1
    sc.exe delete $ServiceName 2>$null | Out-Null
    if (Test-Path $SysDst) { Remove-Item $SysDst -Force -ErrorAction SilentlyContinue }
    Write-Host "Uninstalled $ServiceName"
    exit 0
}

$ts = bcdedit /enum "{current}" 2>$null | Select-String "testsigning"
if ($ts -notmatch "Yes") {
    Write-Warning "testsigning is not Yes. On a lab VM run: bcdedit /set testsigning on  (reboot)"
    Write-Warning "Unsigned drivers may fail with STATUS_INVALID_IMAGE_HASH (577)"
}

Copy-Item $SysSrc $SysDst -Force
sc.exe create $ServiceName type= kernel start= demand binPath= "System32\drivers\RwTest.sys"
if ($LASTEXITCODE -ne 0) {
    throw "sc create failed — service may already exist (use -Uninstall)"
}
sc.exe start $ServiceName
if ($LASTEXITCODE -ne 0) {
    Write-Host "sc start failed. Use BYOVD map instead: .\run-test-rwtest.cmd"
    exit $LASTEXITCODE
}
Write-Host "RwTest loaded. Usermode: \\.\RwTest0"
Write-Host "Test: bin\process_rw.exe"
