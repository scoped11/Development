# Headless: make DriverLab Guest Control a full admin token (no RDP, no UAC prompt).
# 1) If guest is already admin, register workers and exit 0.
# 2) Else clean-shutdown VM, patch SOFTWARE hive in the VDI (user-mode DiscUtils), start headless.
param(
    [string]$VmName = 'DriverLab-E-26200',
    [int]$ShutdownWaitSec = 180
)

$ErrorActionPreference = 'Stop'
$VmDir = $PSScriptRoot
$VBox = 'C:\Program Files\Oracle\VirtualBox\VBoxManage.exe'
$GuestExec = Join-Path $VmDir 'host-guest-exec.ps1'
$ToolProj = Join-Path $VmDir 'tools\VdiGuestElevate\VdiGuestElevate.csproj'
$ToolExe = Join-Path $VmDir 'tools\VdiGuestElevate\bin\Release\net8.0\VdiGuestElevate.exe'
$Vdi = Join-Path $VmDir 'vbox\DriverLab-E-26200\DriverLab-E-26200.vdi'

function Get-VmState {
    $prev = $ErrorActionPreference
    $ErrorActionPreference = 'SilentlyContinue'
    $raw = & $VBox showvminfo $VmName --machinereadable 2>$null | Out-String
    $ErrorActionPreference = $prev
    if ($raw -match 'VMState="(\w+)"') { return $Matches[1] }
    return 'unknown'
}

function Test-GuestAdmin {
    & $GuestExec -VmName $VmName -TimeoutMs 90000 -Command @(
        'C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe',
        '-NoProfile', '-Command',
        'if (([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) { "ADMIN_YES" } else { "ADMIN_NO" }'
    ) 2>&1 | Out-String
}

function Wait-GuestControl {
    param([int]$Sec = 240)
    $deadline = (Get-Date).AddSeconds($Sec)
    while ((Get-Date) -lt $deadline) {
        $null = & $GuestExec -VmName $VmName -TimeoutMs 45000 -Command @(
            'C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe',
            '-NoProfile', '-Command', 'hostname'
        ) 2>$null
        if ($LASTEXITCODE -eq 0) { return $true }
        Start-Sleep -Seconds 8
    }
    return $false
}

function Register-GuestWorkers {
    & $GuestExec -VmName $VmName -TimeoutMs 120000 -Command @(
        'C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe',
        '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
        'Z:\byovd\driver3\register-driver3-elevated-task.ps1'
    )
    & $GuestExec -VmName $VmName -TimeoutMs 120000 -PutEnv @{
        DRIVER2_VM_LAB_GUEST = '1'
        DRIVER2_VM_REGISTER_TASK = '1'
    } -Command @(
        'C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe',
        '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
        'Z:\byovd\driver2\vm-lab.ps1'
    )
}

Write-Host "=== ensure-guest-admin ($VmName) ==="
$st = Get-VmState
Write-Host "    state=$st"

if ($st -eq 'running') {
    $q = & $GuestExec -VmName $VmName -TimeoutMs 60000 -Command @(
        'C:\Windows\System32\schtasks.exe', '/Query', '/TN', 'Driver3ElevatedWorker'
    ) 2>&1 | Out-String
    if ($q -match 'Driver3ElevatedWorker') {
        Write-Host '[+] Driver3ElevatedWorker already registered'
        exit 0
    }
    $probe = Test-GuestAdmin
    if ($probe -match 'ADMIN_YES') {
        Write-Host '[+] Guest Control already admin'
        Register-GuestWorkers
        exit 0
    }
    Write-Host '[*] Console inject Win+R UAC (headless, no host UI)'
    & (Join-Path $VmDir 'host-keyboard-elevate.ps1') -VmName $VmName
    Start-Sleep -Seconds 10
    $q = & $GuestExec -VmName $VmName -TimeoutMs 60000 -Command @(
        'C:\Windows\System32\schtasks.exe', '/Query', '/TN', 'Driver3ElevatedWorker'
    ) 2>&1 | Out-String
    if ($q -match 'Driver3ElevatedWorker') {
        Write-Host '[+] Worker registered via console inject'
        exit 0
    }
    Write-Host '[*] Guest is filtered/non-admin — clean shutdown then offline UAC patch'
    & $GuestExec -VmName $VmName -TimeoutMs 60000 -Command @(
        'C:\Windows\System32\shutdown.exe', '/s', '/t', '0', '/f'
    ) | Out-Null
}

$deadline = (Get-Date).AddSeconds($ShutdownWaitSec)
do {
    $st = Get-VmState
    if ($st -eq 'poweroff' -or $st -eq 'aborted') { break }
    Start-Sleep -Seconds 5
} while ((Get-Date) -lt $deadline)

$st = Get-VmState
if ($st -ne 'poweroff' -and $st -ne 'aborted') {
    Write-Host "[!] ACPI/shutdown still $st — poweroff"
    & $VBox controlvm $VmName poweroff | Out-Null
    Start-Sleep -Seconds 8
}

if (-not (Test-Path $Vdi)) { throw "Missing VDI $Vdi" }
Write-Host '[*] Patching VDI SOFTWARE hive (DiscUtils, host user-mode)'
if (-not (Test-Path $ToolExe)) {
    dotnet build $ToolProj -c Release
}
& $ToolExe $Vdi
if ($LASTEXITCODE -ne 0) { throw "VdiGuestElevate failed $LASTEXITCODE" }

Write-Host '[*] Starting VM headless'
& $VBox startvm $VmName --type headless
if ($LASTEXITCODE -ne 0) { throw 'startvm failed' }

if (-not (Wait-GuestControl)) { throw 'Guest Control did not come back' }

$probe = Test-GuestAdmin
Write-Host $probe
if ($probe -notmatch 'ADMIN_YES') {
    Write-Host '[!] Still not admin after hive patch — LocalAccountTokenFilterPolicy may need a second reboot'
    # one extra reboot via shutdown
    & $GuestExec -VmName $VmName -TimeoutMs 60000 -Command @(
        'C:\Windows\System32\shutdown.exe', '/r', '/t', '0', '/f'
    ) | Out-Null
    $deadline = (Get-Date).AddSeconds($ShutdownWaitSec)
    do {
        Start-Sleep -Seconds 10
        if (Wait-GuestControl -Sec 20) { break }
    } while ((Get-Date) -lt $deadline)
    $probe = Test-GuestAdmin
    Write-Host $probe
    if ($probe -notmatch 'ADMIN_YES') { throw 'Guest still not admin' }
}

Register-GuestWorkers
Write-Host '[+] Guest admin + workers registered'
exit 0
