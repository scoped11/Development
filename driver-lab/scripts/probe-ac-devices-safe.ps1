# Safe probe: CreateFile only — no IOCTL fuzz, no driver load, no exploit payloads.
$ErrorActionPreference = 'Continue'

Add-Type @'
using System;
using System.Runtime.InteropServices;
public class K {
  [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
  public static extern IntPtr CreateFileW(string lpFileName, uint dwDesiredAccess,
    uint dwShareMode, IntPtr lpSecurityAttributes, uint dwCreationDisposition,
    uint dwFlagsAndAttributes, IntPtr hTemplateFile);
}
'@

$acc = [uint32]3221225472
$names = @(
  '\\.\EasyAntiCheat_EOS',
  '\\.\EasyAntiCheat',
  '\\.\EAC',
  '\\.\BEDaisy',
  '\\.\BattlEye',
  '\\.\GLOBALROOT\Device\EasyAntiCheat',
  '\\.\GLOBALROOT\Device\BEDaisy'
)

Write-Output "=== Services (EAC/BE) ==="
Get-Service -ErrorAction SilentlyContinue |
  Where-Object { $_.Name -match 'EasyAntiCheat|BattlEye|BEService' } |
  Format-Table Name, Status, StartType -AutoSize

Write-Output "=== Loaded AC drivers (driverquery subset) ==="
driverquery /fo csv /v 2>$null |
  ConvertFrom-Csv |
  Where-Object { $_.'Display Name' -match 'EasyAntiCheat|BattlEye|BEDaisy|ACE' -or $_.'Module Name' -match 'EasyAntiCheat|BEDaisy|ACE' } |
  Select-Object 'Module Name', State, 'Display Name' |
  Format-Table -AutoSize

Write-Output "=== CreateFile matrix ==="
foreach ($d in $names) {
  $h = [K]::CreateFileW($d, $acc, 3, [IntPtr]::Zero, 3, 0, [IntPtr]::Zero)
  if ($h.ToInt64() -eq -1) {
    $e = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
    Write-Output "$d -> err $e"
  }
  else {
    Write-Output "$d -> OPEN (handle $h)"
  }
}

Write-Output "=== OS build ==="
(Get-ItemProperty 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion').CurrentBuildNumber
