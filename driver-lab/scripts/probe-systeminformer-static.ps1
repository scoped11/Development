# Static probe — no sc start, no FltConnect
$ErrorActionPreference = 'Continue'
$root = 'C:\Program Files\SystemInformer'
@(
    'SystemInformer.exe','SystemInformer.sys','ksi.sys','ksid.sys'
) | ForEach-Object {
    $p = Join-Path $root $_
    if (Test-Path $p) {
        $h = (Get-FileHash $p -Algorithm SHA256).Hash
        $sig = Get-AuthenticodeSignature $p
        $subj = if ($sig.SignerCertificate) { $sig.SignerCertificate.Subject } else { $sig.Status }
        Write-Output "OK $p"
        Write-Output "  SHA256=$h"
        Write-Output "  Auth=$subj"
    }
    else { Write-Output "MISSING $p" }
}

Add-Type @'
using System;
using System.Runtime.InteropServices;
public class K {
  [DllImport("kernel32.dll", CharSet = CharSet.Unicode)]
  public static extern IntPtr CreateFileW(string lpFileName, uint dwDesiredAccess,
    uint dwShareMode, IntPtr lpSecurityAttributes, uint dwCreationDisposition,
    uint dwFlagsAndAttributes, IntPtr hTemplateFile);
}
'@

$acc = [uint32]3221225472
foreach ($d in @('\\.\KSystemInformer', '\\.\SystemInformer', '\\.\KProcessHacker3')) {
    $h = [K]::CreateFileW($d, $acc, 3, [IntPtr]::Zero, 3, 0, [IntPtr]::Zero)
    if ($h.ToInt64() -eq -1) {
        $e = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
        Write-Output "CreateFile $d -> err $e"
    }
    else {
        Write-Output "CreateFile $d -> open handle $h"
        [void][K]::CreateFileW($d, 0, 0, [IntPtr]::Zero, 0, 0, [IntPtr]::Zero) # no CloseHandle export in type — skip
    }
}

$sys = Join-Path $root 'SystemInformer.sys'
if (Test-Path $sys) {
    $dump = & dumpbin /imports $sys 2>$null | Select-String -Pattern 'IoCreateDevice|MmMapIoSpace|DeviceIoControl'
    if ($dump) { $dump } else { Write-Output 'dumpbin: no IoCreateDevice/MmMapIoSpace in import table (expected)' }
}
