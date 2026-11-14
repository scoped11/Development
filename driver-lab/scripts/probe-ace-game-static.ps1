# Static probe — no sc start
$ErrorActionPreference = 'Continue'
$sys = 'C:\WINDOWS\system32\drivers\ACE-GAME.sys'
if (Test-Path $sys) {
    $h = (Get-FileHash $sys -Algorithm SHA256).Hash
    $sig = Get-AuthenticodeSignature $sys
    Write-Output "OK $sys"
    Write-Output "  SHA256=$h"
    Write-Output "  Auth=$($sig.SignerCertificate.Subject)"
}

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
foreach ($d in @(
    '\\.\ACE-GAME', '\\.\ACEGAME', '\\.\AceGame',
    '\\.\AntiCheatExpert', '\\.\ACE', '\\.\SGuard'
)) {
    $h = [K]::CreateFileW($d, $acc, 3, [IntPtr]::Zero, 3, 0, [IntPtr]::Zero)
    if ($h.ToInt64() -eq -1) {
        $e = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
        Write-Output "CreateFile $d -> err $e"
    }
    else {
        Write-Output "CreateFile $d -> OPEN handle=$h"
    }
}

Write-Output "Service KSystemInformer skip; ACE-GAME service:"
Test-Path 'HKLM:\SYSTEM\CurrentControlSet\Services\ACE-GAME'
if (Test-Path 'HKLM:\SYSTEM\CurrentControlSet\Services\ACE-GAME') {
    Get-ItemProperty 'HKLM:\SYSTEM\CurrentControlSet\Services\ACE-GAME' |
        Select-Object ImagePath, Start, Type, DisplayName | Format-List
}
