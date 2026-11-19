param([string]$VmName = '')
$here = $PSScriptRoot
$cred = Get-Content (Join-Path $here 'lab.credentials.json') -Raw | ConvertFrom-Json
if (-not $VmName) { $VmName = $cred.vm_name }
$VBox = 'C:\Program Files\Oracle\VirtualBox\VBoxManage.exe'
$ps = 'C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe'
$script = '\\VBoxSvr\hostdev\P_Finger_ByPass\scripts\Vm-Lab-DiagnoseDriverLoad.ps1'
$out = '\\VBoxSvr\hostdev\P_Finger_ByPass\artifacts\vm-lab-driver-diagnosis.txt'

$setup = @"
`$log = 'C:\lab\logs\elevated-diag.log'
New-Item -ItemType Directory -Force -Path 'C:\lab\logs' | Out-Null
try { schtasks /Query /TN Driver3ElevatedWorker | Out-Null; if (`$LASTEXITCODE -eq 0) {
  `$req = @{ script = '$script'; args = ''; utc = (Get-Date).ToUniversalTime().ToString('o') } | ConvertTo-Json
  Set-Content -Path 'Z:\byovd\driver3\vm_hostb_request.json' -Value `$req -Encoding UTF8
  schtasks /Run /TN Driver3ElevatedWorker | Out-File `$log -Append
  Start-Sleep -Seconds 30
} else { throw 'no worker' } } catch {
  schtasks /Create /TN PFBypassDiag /TR "powershell.exe -NoProfile -ExecutionPolicy Bypass -File $script" /SC ONCE /ST 23:59 /SD 01/01/2099 /RL HIGHEST /F /RU $($cred.username) /RP $($cred.password) 2>&1 | Out-File `$log -Append
  if (`$LASTEXITCODE -ne 0) { 'ELEVATION_BLOCKED: guest Admin once — Z:\P_Finger_ByPass\scripts\Vm-Lab-RegisterElevatedWorker.ps1' | Out-File `$log; exit 24 }
  schtasks /Run /TN PFBypassDiag | Out-File `$log -Append
  Start-Sleep -Seconds 25
}
if (Test-Path '$out') { Get-Content '$out' }
Get-Content `$log -ErrorAction SilentlyContinue
"@

$b64 = [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($setup))
& $VBox guestcontrol $VmName run --username=$($cred.username) --password=$($cred.password) --exe=$ps --wait-stdout --wait-stderr --timeout=180000 -- -NoProfile -EncodedCommand $b64