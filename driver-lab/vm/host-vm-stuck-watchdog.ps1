# Every ~4 min: checks if the elevated VM worker is wedged (open job, stale heartbeat, inbox backlog).
# -Recover: re-run install.cmd via VBox, queue guest-kill-stuck-mapper on the elevated worker.
# Writes human-readable status to vm\bg\stuck-watchdog-status.txt for the host.
param(
    [int]$CheckSec = 240,
    [int]$StallThresholdSec = 270,
    [switch]$Recover,
    [string]$BgRoot = (Join-Path $PSScriptRoot 'bg')
)
$ErrorActionPreference = 'Continue'
Write-Host "[stuck-watchdog] Starting check (stall threshold ${StallThresholdSec}s)."
Write-Host "[stuck-watchdog] Will read worker.log for an open job, worker.heartbeat age, and inbox JSON count."
if ($Recover) {
    Write-Host "[stuck-watchdog] -Recover is ON: if stuck, VBox install.cmd then elevated guest-kill-stuck-mapper."
} else {
    Write-Host "[stuck-watchdog] Read-only pass (no -Recover): report only to stuck-watchdog.json + stuck-watchdog-status.txt."
}
$wl = Join-Path $BgRoot 'worker.log'
$inbox = Join-Path $BgRoot 'inbox'
$admin = Join-Path $BgRoot 'host-bg-admin.ps1'

function Get-ActiveJobFromLog {
    if (-not (Test-Path $wl)) { return $null }
    $lines = Get-Content $wl -Tail 80 -ErrorAction SilentlyContinue
    $open = $null
    foreach ($line in $lines) {
        if ($line -match '^(\S+)\s+job (\w+) action=') {
            $open = @{ id = $Matches[2]; line = $line; at = $Matches[1] }
        }
        if ($open -and $line -match "job $($open.id) exit=") { $open = $null }
    }
    return $open
}

function Test-Stuck {
    param($OpenJob)
    if (-not $OpenJob) { return $false }
    try {
        $t = [datetime]::Parse($OpenJob.at.Trim())
        return ((Get-Date) - $t).TotalSeconds -ge $StallThresholdSec
    } catch { return $false }
}

$open = Get-ActiveJobFromLog
$pending = @(Get-ChildItem $inbox -Filter '*.json' -EA SilentlyContinue)
$hb = Join-Path $BgRoot 'worker.heartbeat'
$hbAge = if (Test-Path $hb) { ((Get-Date) - (Get-Item $hb).LastWriteTime).TotalSeconds } else { 99999 }

$report = [ordered]@{
    utc           = (Get-Date).ToUniversalTime().ToString('o')
    open_job      = $open.id
    open_line     = $open.line
    inbox_pending = $pending.Count
    heartbeat_sec = [int]$hbAge
    stuck         = (Test-Stuck $open) -or ($hbAge -gt $StallThresholdSec -and $pending.Count -gt 0)
}
$report | ConvertTo-Json -Compress | Set-Content (Join-Path $BgRoot 'stuck-watchdog.json')
$msg = @(
    "=== stuck-watchdog $(Get-Date -Format o) ==="
    "Purpose: detect wedged Driver3ElevatedWorker (hung install, deadlock schtasks /Run, no heartbeat)."
    "open_job=$($report.open_job) inbox_pending=$($report.inbox_pending) heartbeat_age_sec=$($report.heartbeat_sec) stuck=$($report.stuck)"
    if ($Recover -and $report.stuck) { 'Recover: VBox install.cmd + elevated guest-kill-stuck-mapper' }
) -join "`n"
Set-Content (Join-Path $BgRoot 'stuck-watchdog-status.txt') $msg
Write-Host $msg

if ($Recover -and $report.stuck) {
    Write-Host '[!] recover: kill + ensure-worker'
    & 'C:\Users\baile\OneDrive\Desktop\Development\driver-lab\vm\host-guest-exec.ps1' -Command @(
        'C:\Windows\System32\cmd.exe', '/c', 'Z:\driver-lab\vm\bg\install.cmd'
    ) -TimeoutMs 120000 | Out-Null
    & $admin -Action exec -PowerShell "& 'Z:\driver-lab\vm\bg\guest-kill-stuck-mapper.ps1'" -TimeoutSec 120 -StallSec 270 -PollSec 60 | Out-Null
}
exit $(if ($report.stuck) { 3 } else { 0 })
