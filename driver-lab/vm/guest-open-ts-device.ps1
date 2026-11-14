$src = @'
using System;
using System.Runtime.InteropServices;
public class Native {
  [DllImport("kernel32", CharSet=CharSet.Unicode, SetLastError=true)]
  public static extern IntPtr CreateFileW(string n, uint a, uint s, IntPtr p, uint d, uint f, IntPtr t);
  [DllImport("kernel32")] public static extern bool CloseHandle(IntPtr h);
}
'@
Add-Type -TypeDefinition $src
$names = @(
    '\\.\ThrottleStop', '\\.\TsSvc', '\\.\ThrottleStop0',
    '\\.\GLOBALROOT\Device\ThrottleStop', '\\.\GLOBALROOT\Device\TsSvc',
    '\\.\GLOBALROOT\DosDevices\ThrottleStop', '\\.\GLOBALROOT\DosDevices\TsSvc'
)
foreach ($n in $names) {
    $access = [Convert]::ToUInt32('C0000000', 16)
    $h = [Native]::CreateFileW($n, $access, 3, [IntPtr]::Zero, 3, 0, [IntPtr]::Zero)
    $err = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
    $ok = ($h -ne [IntPtr]::new(-1))
    "{0}  ok={1} handle={2} err={3}" -f $n, $ok, $h, $err
    if ($ok) { [Native]::CloseHandle($h) | Out-Null }
}
