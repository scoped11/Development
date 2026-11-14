# Host-side: run driver3 VM smoke inside DriverLab-E-26200 via Guest Additions.
param(
    [string]$Profile = 'rust',
    [switch]$Execute,
    [switch]$ForceMap,
    [switch]$SkipProve,
    [string]$VmName = 'DriverLab-E-26200',
    [string]$CredentialsFile = '',
    [int]$TimeoutMs = 600000,
    [int]$PollMaxSec = 600
)

$ErrorActionPreference = 'Stop'
$Repo = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$GuestExec = Join-Path $Repo 'driver-lab\vm\host-guest-exec.ps1'
$Smoke = 'Z:\byovd\driver3\vm-guest-smoke.ps1'
$Request = 'Z:\byovd\driver3\vm-request-hostb.ps1'
if (-not $CredentialsFile) {
    $CredentialsFile = Join-Path $Repo 'driver-lab\vm\lab.credentials.json'
}

$VBox = 'C:\Program Files\Oracle\VirtualBox\VBoxManage.exe'
if (-not (Test-Path $VBox)) { throw 'VirtualBox not found' }
$state = & $VBox showvminfo $VmName --machinereadable 2>$null | Select-String 'VMState="(\w+)"'
if ($state -notmatch 'running') {
    Write-Host "[*] Starting $VmName headless..."
    & $VBox startvm $VmName --type headless
    Start-Sleep -Seconds 15
}

function Invoke-GuestLogTail {
    & $GuestExec -VmName $VmName -TimeoutMs 60000 -Command @(
        'C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe',
        '-NoProfile', '-Command',
        'if (Test-Path Z:\byovd\driver3\vm_hostb_execute.log) { Get-Content Z:\byovd\driver3\vm_hostb_execute.log -Tail 20 } else { ''NO_LOG'' }'
    ) 2>&1 | Out-String
}

if ($Execute) {
    Write-Host '[*] Host B via vm-request-hostb (needs Driver3ElevatedWorker task in guest)'
    $psArgs = @(
        '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $Request,
        '-Profile', $Profile
    )
    if ($ForceMap) { $psArgs += '-ForceMap' }
    if ($SkipProve) { $psArgs += '-SkipProve' }
    $cmd = @('C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe') + $psArgs
    & $GuestExec -VmName $VmName -TimeoutMs 120000 -Command $cmd
    $reqEc = $LASTEXITCODE
    if ($reqEc -eq 24) {
        Write-Host '[!] One-time: elevated guest → register-driver3-elevated-task.ps1'
        Write-Host '    See driver3\SCALE_A.md'
        exit 24
    }
    if ($reqEc -ne 0) { exit $reqEc }

    $deadline = (Get-Date).AddSeconds($PollMaxSec)
    $ec = 98
    while ((Get-Date) -lt $deadline) {
        $out = Invoke-GuestLogTail
        if ($out -match 'host-b exit=(\d+)') {
            $ec = [int]$Matches[1]
            Write-Host $out
            break
        }
        if ($out -notmatch 'NO_LOG') { Write-Host $out }
        Write-Host '[*] Waiting for vm_hostb_execute.log...'
        Start-Sleep -Seconds 15
    }
    if ($ec -eq 98) {
        Write-Host "[!] Timed out after ${PollMaxSec}s"
        exit 98
    }
} else {
    $psArgs = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $Smoke, '-Profile', $Profile)
    Write-Host "[*] Guest smoke: $Smoke Profile=$Profile"
    $cmd = @('C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe') + $psArgs
    & $GuestExec -VmName $VmName -TimeoutMs $TimeoutMs -Command $cmd
    $ec = $LASTEXITCODE
}

& (Join-Path $PSScriptRoot 'build-dashboard.ps1') -Profile $Profile | Out-Null

$hostReport = Join-Path $PSScriptRoot 'vm_guest_smoke.json'
if (Test-Path $hostReport) {
    $j = Get-Content $hostReport -Raw | ConvertFrom-Json
    Write-Host "[+] Guest report: gate=$($j.gate_exit) host_b=$($j.host_b_exit)"
}
exit $ec
