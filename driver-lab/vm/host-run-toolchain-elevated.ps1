# Run toolchain install elevated via one-shot scheduled task (guestcontrol cannot approve UAC).
param(
    [string]$VmName = '',
    [int]$PollSeconds = 3600
)

$ErrorActionPreference = 'Stop'
$here = $PSScriptRoot
$VBox = 'C:\Program Files\Oracle\VirtualBox\VBoxManage.exe'
$cred = Get-Content (Join-Path $here 'lab.credentials.json') -Raw | ConvertFrom-Json
if (-not $VmName) { $VmName = $cred.vm_name }
$ps = 'C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe'
$shareScript = '\\VBoxSvr\hostdev\driver-lab\vm\install-vm-artex-toolchain.ps1'

$setup = @"
`$log = 'C:\lab\logs\elevated-install.log'
New-Item -ItemType Directory -Force -Path 'C:\lab\logs' | Out-Null
schtasks /Delete /TN PFAB22Toolchain /F 2>`$null | Out-Null
schtasks /Create /TN PFAB22Toolchain /TR "powershell.exe -NoProfile -ExecutionPolicy Bypass -File $shareScript -SkipHeavyWinget" /SC ONCE /ST 23:59 /SD 01/01/2099 /RL HIGHEST /F /RU $($cred.username) /RP $($cred.password) 2>&1 | Out-File `$log -Append
schtasks /Run /TN PFAB22Toolchain 2>&1 | Out-File `$log -Append
"@

$b64 = [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($setup))
& $VBox guestcontrol $VmName run --username=$($cred.username) --password=$($cred.password) --exe=$ps --wait-stdout --wait-stderr --timeout=120000 -- -NoProfile -EncodedCommand $b64

Write-Host "[*] Polling guest install log (max ${PollSeconds}s) ..."
$deadline = (Get-Date).AddSeconds($PollSeconds)
$last = ''
while ((Get-Date) -lt $deadline) {
    $tail = & $VBox guestcontrol $VmName run --username=$($cred.username) --password=$($cred.password) --exe=$ps --wait-stdout --wait-stderr --timeout=60000 -- -NoProfile -Command "if(Test-Path C:\lab\logs\install-toolchain.log){Get-Content C:\lab\logs\install-toolchain.log -Tail 3 -Raw}else{'waiting'}" 2>$null
    if ($tail -and $tail -ne $last) { Write-Host $tail; $last = $tail }
    if ($tail -match 'Done\.') { break }
    Start-Sleep -Seconds 30
}
exit 0