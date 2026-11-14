# Driver2 on DriverLab VM — one entry file (host orchestration + guest body via env).
# Host (repo machine):
#   .\driver2\vm-lab.ps1 -Profile rust                    # smoke / plan
#   .\driver2\vm-lab.ps1 -Profile rust -Execute -ForceMap  # map + probe (needs elevated task once)
# Guest (elevated, one-time): set DRIVER2_VM_LAB_GUEST=1 or RDP:
#   powershell -File Z:\byovd\driver2\vm-lab.ps1 -RegisterElevatedTask
#
param(
    [ValidateSet('rust', 'r6', 'deltaforce', 'arenabreakout', 'arksa')]
    [string]$Profile = 'rust',
    [switch]$Execute,
    [switch]$ForceMap,
    [switch]$SkipProve,
    [switch]$Inspector,
    [string]$VmName = 'DriverLab-E-26200',
    [switch]$RegisterElevatedTask,
    [int]$PollMaxSec = 900
)

$ErrorActionPreference = 'Stop'

# ---------------------------------------------------------------------------
# Guest / worker (same file — Guest Additions sets env before -File)
# ---------------------------------------------------------------------------
function Invoke-Driver2VmLabGuest {
    $Driver2 = if ($PSScriptRoot) { $PSScriptRoot } else { 'Z:\byovd\driver2' }
    $Byovd = Split-Path $Driver2 -Parent
    $Driver3 = Join-Path $Byovd 'driver3'
    $log = Join-Path $Driver2 'vm_lab.log'
    function L($m) { $t = "$(Get-Date -Format o) $m"; Add-Content $log $t; Write-Host $t }

    $profile = $env:DRIVER2_VM_PROFILE
    if (-not $profile) { $profile = 'rust' }
    $doExec = ($env:DRIVER2_VM_EXECUTE -eq '1')
    $force = ($env:DRIVER2_VM_FORCEMAP -eq '1')
    $skipProve = ($env:DRIVER2_VM_SKIPPROVE -eq '1')
    $insp = ($env:DRIVER2_VM_INSPECTOR -eq '1')
    $regTask = ($env:DRIVER2_VM_REGISTER_TASK -eq '1')

    L "=== vm-lab GUEST computer=$env:COMPUTERNAME driver2=$Driver2 profile=$profile execute=$doExec admin=$((([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)))"

    if ($regTask) {
        $worker = Join-Path $Driver2 'vm-lab-worker.ps1'
        if (-not (Test-Path $worker)) { throw "missing $worker" }
        $cmd = Join-Path $Driver2 'run-vm-lab-worker.cmd'
        "@echo off`r`npowershell.exe -NoProfile -ExecutionPolicy Bypass -File `"$worker`"" | Set-Content $cmd -Encoding ASCII
        $tn = 'Driver2VmLabWorker'
        $prev = $ErrorActionPreference
        $ErrorActionPreference = 'SilentlyContinue'
        schtasks /Delete /TN $tn /F 2>&1 | Out-Null
        $ErrorActionPreference = $prev
        schtasks /Create /TN $tn /TR "`"$cmd`"" /SC ONLOGON /RL HIGHEST /F 2>&1 | ForEach-Object { L $_ }
        if ($LASTEXITCODE -ne 0) {
            schtasks /Create /TN $tn /TR "`"$cmd`"" /SC ONLOGON /F 2>&1 | ForEach-Object { L $_ }
        }
        if ($LASTEXITCODE -ne 0) { throw 'schtasks failed — RDP elevated once: see driver2\VM_LAB_SETUP.md' }
        L "[+] registered $tn"
        exit 0
    }

    if (-not (Test-Path (Join-Path $Byovd 'bin\Driver2Runner.exe'))) {
        L '[!] build on share: .\build.ps1 ; .\driver2\build.ps1'
    }

    if (Test-Path (Join-Path $Driver3 'lane-gate.ps1')) {
        & (Join-Path $Driver3 'lane-gate.ps1') -Game $profile | Out-Null
        L "lane-gate exit=$LASTEXITCODE"
    }

    if (-not $doExec) {
        L 'PLAN — host: vm-lab.ps1 -RegisterElevatedTask then -Execute -ForceMap'
        L "  map: everything.ps1 -Profile $profile -Target vm -Execute -ForceMap"
        L '  probe: Driver2Client.exe probe'
        $plan = [ordered]@{
            schema  = 'driver2_vm_lab_v1'
            mode    = 'plan'
            profile = $profile
            guest   = $env:COMPUTERNAME
            utc     = (Get-Date).ToUniversalTime().ToString('o')
        }
        ($plan | ConvertTo-Json) | Set-Content (Join-Path $Driver2 'vm_lab_result.json') -Encoding utf8
        exit 0
    }

    $adm = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
        [Security.Principal.WindowsBuiltInRole]::Administrator)
    if (-not $adm) {
        L 'BLOCKED: map requires Administrator (register Driver2VmLabWorker + host -Execute)'
        exit 3
    }

    foreach ($vf in Get-ChildItem (Join-Path $Byovd 'vehicles\*.sys') -ErrorAction SilentlyContinue) {
        Copy-Item $vf.FullName (Join-Path $Byovd "bin\$($vf.Name)") -Force -ErrorAction SilentlyContinue
    }

    if ($insp) {
        $dg = @{
            Profile   = $profile
            Execute   = $true
            ForceMap  = $true
            Inspector = $true
        }
        & (Join-Path $Driver2 'developer-approval-gate.ps1') @dg
        $mapEc = $LASTEXITCODE
    } else {
        $a = @('-Profile', $profile, '-Target', 'vm', '-Execute')
        if ($force) { $a += '-ForceMap' }
        if ($skipProve) { $a += '-SkipProve' }
        & (Join-Path $Driver2 'everything.ps1') @a
        $mapEc = $LASTEXITCODE
    }
    L "everything exit=$mapEc"

    $probeEc = 99
    $client = Join-Path $Byovd 'bin\Driver2Client.exe'
    if (Test-Path $client) {
        & $client probe 2>&1 | ForEach-Object { L $_ }
        $probeEc = $LASTEXITCODE
        L "probe exit=$probeEc"
    }

    $diag = $null
    $verify = Join-Path $Byovd 'scripts\verify-pf-diag.ps1'
    if (Test-Path $verify) {
        $diag = & $verify -Profile $profile -MapMode $(if ($insp) { 'bypass' } else { 'any' })
        L "pf_diag ok=$($diag.ok)"
    }

    $ver = Get-ItemProperty 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion'
    $result = [ordered]@{
        schema     = 'driver2_vm_lab_v1'
        mode       = 'execute'
        profile    = $profile
        guest      = $env:COMPUTERNAME
        map_exit   = $mapEc
        probe_exit = $probeEc
        pf_diag    = $diag
        ok         = ($mapEc -eq 0 -and $probeEc -eq 0)
        os_build   = ('{0}.{1}' -f $ver.CurrentBuild, $ver.UBR)
        utc        = (Get-Date).ToUniversalTime().ToString('o')
    }
    $out = Join-Path $Driver2 'vm_lab_result.json'
    ($result | ConvertTo-Json -Depth 6) | Set-Content $out -Encoding utf8

    if (Test-Path (Join-Path $Driver3 'sync-lab-manifest.ps1')) {
        $proof = [ordered]@{
            marker_ok = ($probeEc -eq 0)
            profile   = $profile
            host      = 'driver2_vm_lab'
            utc       = $result.utc
        } | ConvertTo-Json -Depth 4
        $pf = Join-Path $Driver3 "kernel_proof_$profile.json"
        $proof | Set-Content $pf -Encoding utf8
        & (Join-Path $Driver3 'sync-lab-manifest.ps1') -Game $profile -LabHost $env:COMPUTERNAME -KernelProofJson $pf | Out-Null
    }

    exit $(if ($result.ok) { 0 } else { $mapEc })
}

function Invoke-Driver2VmLabWorker {
    $Driver2 = if ($PSScriptRoot) { $PSScriptRoot } else { 'Z:\byovd\driver2' }
    $req = Join-Path $Driver2 'vm_lab_request.json'
    $lock = Join-Path $Driver2 'vm_lab_request.lock'
    if (-not (Test-Path $req)) { exit 0 }
    if (Test-Path $lock) { exit 0 }
    New-Item -ItemType File -Path $lock -Force | Out-Null
    try {
        $j = Get-Content $req -Raw | ConvertFrom-Json
        Remove-Item $req -Force -ErrorAction SilentlyContinue
        $env:DRIVER2_VM_LAB_GUEST = '1'
        $env:DRIVER2_VM_PROFILE = $j.profile
        $env:DRIVER2_VM_EXECUTE = '1'
        $env:DRIVER2_VM_FORCEMAP = $(if ($j.forceMap) { '1' } else { '0' })
        $env:DRIVER2_VM_SKIPPROVE = $(if ($j.skipProve) { '1' } else { '0' })
        $env:DRIVER2_VM_INSPECTOR = $(if ($j.inspector) { '1' } else { '0' })
        Invoke-Driver2VmLabGuest
        exit $LASTEXITCODE
    } finally {
        Remove-Item $lock -Force -ErrorAction SilentlyContinue
    }
}

if ($env:DRIVER2_VM_LAB_WORKER -eq '1') {
    Invoke-Driver2VmLabWorker
    exit $LASTEXITCODE
}

if ($env:DRIVER2_VM_LAB_GUEST -eq '1') {
    if ($RegisterElevatedTask) { $env:DRIVER2_VM_REGISTER_TASK = '1' }
    Invoke-Driver2VmLabGuest
    exit $LASTEXITCODE
}

# ---------------------------------------------------------------------------
# Host
# ---------------------------------------------------------------------------
$Driver2 = $PSScriptRoot
$Byovd = Split-Path $Driver2 -Parent
$Repo = Split-Path $Byovd -Parent
$GuestExec = Join-Path $Repo 'driver-lab\vm\host-guest-exec.ps1'
$VBox = 'C:\Program Files\Oracle\VirtualBox\VBoxManage.exe'

if (-not (Test-Path $VBox)) { throw 'Install VirtualBox / DriverLab VM' }
if (-not (Test-Path $GuestExec)) { throw "Missing $GuestExec" }

$st = & $VBox showvminfo $VmName --machinereadable 2>$null | Select-String 'VMState="(\w+)"'
if ($st -notmatch 'running') {
    Write-Host "[*] Starting $VmName..."
    & $VBox startvm $VmName --type headless
    Start-Sleep -Seconds 20
}

function Sync-Driver2VmLabArtifacts {
    foreach ($f in @('vm_lab.log', 'vm_lab_result.json', 'vm_hostb_execute.log')) {
        $out = & $GuestExec -VmName $VmName -TimeoutMs 120000 -Command @(
            'C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe',
            '-NoProfile', '-Command',
            "if (Test-Path 'Z:\byovd\driver2\$f') { Get-Content 'Z:\byovd\driver2\$f' -Raw }"
        ) 2>&1 | Out-String
        if ($out.Trim()) {
            $out.Trim() | Set-Content (Join-Path $Driver2 $f) -Encoding utf8
            Write-Host "[+] synced $f"
        }
    }
    foreach ($g in @('rust', 'r6')) {
        $pf = "kernel_proof_$g.json"
        $out = & $GuestExec -VmName $VmName -TimeoutMs 60000 -Command @(
            'C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe',
            '-NoProfile', '-Command',
            "if (Test-Path 'Z:\byovd\driver3\$pf') { Get-Content 'Z:\byovd\driver3\$pf' -Raw }"
        ) 2>&1 | Out-String
        if ($out.Trim()) {
            $out.Trim() | Set-Content (Join-Path $Byovd "driver3\$pf") -Encoding utf8
        }
    }
}

function Invoke-GuestVmLab {
    param([hashtable]$EnvExtra = @{})
    $envBlock = @{
        DRIVER2_VM_LAB_GUEST = '1'
        DRIVER2_VM_PROFILE = $Profile
        DRIVER2_VM_EXECUTE   = $(if ($Execute) { '1' } else { '0' })
        DRIVER2_VM_FORCEMAP  = $(if ($ForceMap) { '1' } else { '0' })
        DRIVER2_VM_SKIPPROVE = $(if ($SkipProve) { '1' } else { '0' })
        DRIVER2_VM_INSPECTOR = $(if ($Inspector) { '1' } else { '0' })
    }
    foreach ($k in $EnvExtra.Keys) { $envBlock[$k] = $EnvExtra[$k] }
    $script = 'Z:\byovd\driver2\vm-lab.ps1'
    & $GuestExec -VmName $VmName -TimeoutMs 900000 -PutEnv $envBlock -Command @(
        'C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe',
        '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $script
    )
}

Write-Host '=== driver2 VM LAB (DriverLab) ==='
Write-Host "    Profile=$Profile Execute=$Execute ForceMap=$ForceMap"

if ($RegisterElevatedTask) {
    Write-Host '[*] Guest: register Driver2VmLabWorker (needs elevation in VM — use RDP if this fails)'
    Invoke-GuestVmLab -EnvExtra @{ DRIVER2_VM_REGISTER_TASK = '1' }
    exit $LASTEXITCODE
}

if (-not $Execute) {
    Invoke-GuestVmLab
    Sync-Driver2VmLabArtifacts
    exit $LASTEXITCODE
}

# Execute: queue request + elevated worker
$reqPath = Join-Path $Driver2 'vm_lab_request.json'
[ordered]@{
    profile   = $Profile
    forceMap  = [bool]$ForceMap
    skipProve = [bool]$SkipProve
    inspector = [bool]$Inspector
    utc       = (Get-Date).ToUniversalTime().ToString('o')
} | ConvertTo-Json | Set-Content $reqPath -Encoding utf8

$prevErr = $ErrorActionPreference
$ErrorActionPreference = 'SilentlyContinue'
$null = & $GuestExec -VmName $VmName -TimeoutMs 120000 -PutEnv @{ DRIVER2_VM_LAB_GUEST = '1' } -Command @(
    'C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe',
    '-NoProfile', '-Command',
    '$e=schtasks /Query /TN Driver2VmLabWorker 2>&1; if ($LASTEXITCODE -eq 0) { exit 0 } else { exit 1 }'
)
$taskOk = ($LASTEXITCODE -eq 0)
$ErrorActionPreference = $prevErr
if (-not $taskOk) {
    Write-Host '[!] Driver2VmLabWorker missing. One-time in elevated VM:'
    Write-Host '    powershell -File Z:\byovd\driver2\vm-lab.ps1 -RegisterElevatedTask'
    Write-Host '    (or RDP → elevated → same command with env DRIVER2_VM_LAB_GUEST=1)'
    exit 24
}

& $GuestExec -VmName $VmName -TimeoutMs 60000 -Command @(
    'C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe',
    '-NoProfile', '-Command',
    'schtasks /Run /TN Driver2VmLabWorker'
)
if ($LASTEXITCODE -ne 0) { throw 'schtasks /Run failed' }

Write-Host '[*] Worker running — polling vm_lab.log...'
$deadline = (Get-Date).AddSeconds($PollMaxSec)
$ec = 98
while ((Get-Date) -lt $deadline) {
    $tail = & $GuestExec -VmName $VmName -TimeoutMs 120000 -Command @(
        'C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe',
        '-NoProfile', '-Command',
        'if (Test-Path Z:\byovd\driver2\vm_lab.log) { Get-Content Z:\byovd\driver2\vm_lab.log -Tail 25 }'
    ) 2>&1 | Out-String
    if ($tail -match 'everything exit=(\d+)') {
        $ec = [int]$Matches[1]
        if ($tail -match 'probe exit=(\d+)') {
            $pe = [int]$Matches[1]
            if ($pe -ne 0) { $ec = $pe }
        }
        Write-Host $tail
        break
    }
    Write-Host '[.] waiting...'
    Start-Sleep -Seconds 15
}

Sync-Driver2VmLabArtifacts
if (Test-Path (Join-Path $Driver2 'vm_lab_result.json')) {
    $r = Get-Content (Join-Path $Driver2 'vm_lab_result.json') -Raw | ConvertFrom-Json
    Write-Host "[+] vm_lab ok=$($r.ok) map=$($r.map_exit) probe=$($r.probe_exit)"
}
exit $(if ($ec -ne 98) { $ec } else { 98 })
