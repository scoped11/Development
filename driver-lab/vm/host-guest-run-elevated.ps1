# Run a PowerShell script on the guest at HIGHEST (one-shot scheduled task). Uses lab password.
param(
    [Parameter(Mandatory)][string]$ScriptOnGuest,  # e.g. Z:\driver-lab\vm\bg\guest-vm-driver-matrix.ps1
    [string]$VmName = 'DriverLab-E-26200',
    [int]$TimeoutMs = 900000,
    [string]$WaitMarkerPath,  # optional guest file; poll until exists or TimeoutMs
    [int]$PollMs = 2000
)
$ErrorActionPreference = 'Stop'
$guestExec = Join-Path $PSScriptRoot 'host-guest-exec.ps1'
$ps = 'C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe'
$tn = 'DriverLabOneShot_' + [guid]::NewGuid().ToString('N').Substring(0, 8)
$tr = "$ps -NoProfile -ExecutionPolicy Bypass -File `"$ScriptOnGuest`""
$st = (Get-Date).AddMinutes(1).ToString('HH:mm')
$sd = (Get-Date).ToString('MM/dd/yyyy')
# Create as the interactive guest user (Guest Control session); HIGHEST = full admin token, no UAC prompt.
& $guestExec -VmName $VmName -TimeoutMs 120000 -Command @(
    'C:\Windows\System32\schtasks.exe', '/Create', '/TN', $tn,
    '/TR', $tr, '/SC', 'ONCE', '/ST', $st, '/SD', $sd, '/RL', 'HIGHEST', '/F'
)
if ($LASTEXITCODE -ne 0) { throw "schtasks create failed $LASTEXITCODE" }
& $guestExec -VmName $VmName -TimeoutMs 120000 -Command @(
    'C:\Windows\System32\schtasks.exe', '/Run', '/TN', $tn
)
if ($LASTEXITCODE -ne 0) { throw "schtasks run failed $LASTEXITCODE" }

if ($WaitMarkerPath) {
    $deadline = (Get-Date).AddMilliseconds($TimeoutMs)
    $probe = @(
        "if (Test-Path -LiteralPath '$WaitMarkerPath') { exit 0 } else { exit 1 }"
    )
    while ((Get-Date) -lt $deadline) {
        & $guestExec -VmName $VmName -TimeoutMs 60000 -Command @(
            $ps, '-NoProfile', '-Command', $probe
        ) | Out-Null
        if ($LASTEXITCODE -eq 0) { break }
        Start-Sleep -Milliseconds $PollMs
    }
    if ($LASTEXITCODE -ne 0) { throw "Timeout waiting for marker $WaitMarkerPath" }
}

& $guestExec -VmName $VmName -TimeoutMs 60000 -Command @(
    'C:\Windows\System32\schtasks.exe', '/Delete', '/TN', $tn, '/F'
) | Out-Null