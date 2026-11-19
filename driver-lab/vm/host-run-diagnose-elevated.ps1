param([string]$VmName = '')
$here = $PSScriptRoot
$cred = Get-Content (Join-Path $here 'lab.credentials.json') -Raw | ConvertFrom-Json
if (-not $VmName) { $VmName = $cred.vm_name }
$VBox = 'C:\Program Files\Oracle\VirtualBox\VBoxManage.exe'
$ps = 'C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe'
$script = '\\VBoxSvr\hostdev\P_Finger_ByPass\scripts\Vm-Lab-DiagnoseDriverLoad.ps1'
$out = '\\VBoxSvr\hostdev\P_Finger_ByPass\artifacts\vm-lab-driver-diagnosis.txt'

$setup = @"
schtasks /Delete /TN PFBypassDiag /F 2>`$null | Out-Null
schtasks /Create /TN PFBypassDiag /TR "powershell.exe -NoProfile -ExecutionPolicy Bypass -File $script" /SC ONCE /ST 23:59 /SD 01/01/2099 /RL HIGHEST /F /RU $($cred.username) /RP $($cred.password)
schtasks /Run /TN PFBypassDiag
Start-Sleep -Seconds 25
if (Test-Path '$out') { Get-Content '$out' }
"@

$b64 = [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($setup))
& $VBox guestcontrol $VmName run --username=$($cred.username) --password=$($cred.password) --exe=$ps --wait-stdout --wait-stderr --timeout=180000 -- -NoProfile -EncodedCommand $b64