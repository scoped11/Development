# Host: queue Vm-Lab-InstallAndStartSigned.ps1 on AI_Workspave_v3 (elevated worker or print Sam command).
param(
    [string]$VmName = '',
    [int]$TimeoutMs = 600000
)

$ErrorActionPreference = 'Stop'
$here = $PSScriptRoot
$cred = Get-Content (Join-Path $here 'lab.credentials.json') -Raw | ConvertFrom-Json
if (-not $VmName) { $VmName = $cred.vm_name }
$VBox = 'C:\Program Files\Oracle\VirtualBox\VBoxManage.exe'
$ps = 'C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe'
$guestScript = '\\VBoxSvr\hostdev\P_Finger_ByPass\scripts\Vm-Lab-InstallAndStartSigned.ps1'
$outGuest = '\\VBoxSvr\hostdev\P_Finger_ByPass\artifacts\vm-lab-signed-start.txt'

$probe = 'schtasks /Query /TN Driver3ElevatedWorker 2>$null; exit $LASTEXITCODE'
$probeB64 = [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($probe))
& $VBox guestcontrol $VmName run --username=$($cred.username) --password=$($cred.password) --exe=$ps `
    --wait-stdout --wait-stderr --timeout=60000 -- -NoProfile -EncodedCommand $probeB64 | Out-Null

if ($LASTEXITCODE -ne 0) {
    Write-Host ''
    Write-Host '=== Guest Admin (one-time on AI_Workspave_v3) ==='
    Write-Host 'Right-click PowerShell → Run as administrator, then:'
    Write-Host "  powershell -NoProfile -ExecutionPolicy Bypass -File Z:\P_Finger_ByPass\scripts\Vm-Lab-RegisterElevatedWorker.ps1"
    Write-Host "  powershell -NoProfile -ExecutionPolicy Bypass -File Z:\P_Finger_ByPass\scripts\Vm-Lab-InstallAndStartSigned.ps1"
    Write-Host ''
    Write-Host 'Root cause: VBox guestcontrol runs vboxuser without elevation; schtasks /RL HIGHEST fails with Access denied.'
    exit 24
}

$reqHost = 'C:\Users\bj284\Downloads\TraeTesting\cursor\Development\byovd\driver3\vm_hostb_request.json'
@{ script = $guestScript; args = ''; utc = (Get-Date).ToUniversalTime().ToString('o') } | ConvertTo-Json |
    Set-Content -LiteralPath $reqHost -Encoding UTF8

$run = 'schtasks /End /TN Driver3ElevatedWorker 2>$null | Out-Null; schtasks /Run /TN Driver3ElevatedWorker; exit $LASTEXITCODE'
$runB64 = [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($run))
& $VBox guestcontrol $VmName run --username=$($cred.username) --password=$($cred.password) --exe=$ps `
    --wait-stdout --wait-stderr --timeout=120000 -- -NoProfile -EncodedCommand $runB64 | Out-Null

Write-Host '[*] Waiting for vm-lab-signed-start.txt ...'
$deadline = (Get-Date).AddMilliseconds($TimeoutMs)
while ((Get-Date) -lt $deadline) {
    Start-Sleep -Seconds 8
    $tail = & $VBox guestcontrol $VmName run --username=$($cred.username) --password=$($cred.password) --exe=$ps `
        --wait-stdout --wait-stderr --timeout=120000 -- -NoProfile -Command "if (Test-Path '$outGuest') { Get-Content '$outGuest' -Tail 25 -Raw }"
    if ($tail -match 'sc start MsIo rc=0') { Write-Host $tail; exit 0 }
    if ($tail -match 'FAIL:') { Write-Host $tail; exit 1 }
}
Write-Host '[!] Timed out — read guest artifact or re-run guest InstallAndStartSigned elevated.'
exit 98