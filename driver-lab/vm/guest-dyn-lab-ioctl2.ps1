# PFAB-21 pass3: inline empty-buffer IOCTL probe. Copy to C:\re before elevated run.
$ErrorActionPreference = 'Continue'
$outDir = 'Z:\ac-captures-host\2026-11-18'
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
$txt = Join-Path $outDir 'dyn-lab-ioctl.txt'
$jsonPath = Join-Path $outDir 'dyn-lab-ioctl.json'
$kdLog = Join-Path $outDir 'dyn-lab-ioctl-kd.txt'
$cdbLog = Join-Path $outDir 'dyn-lab-ioctl-cdb.txt'
function Line([string]$s) {
    Add-Content -LiteralPath $txt -Value $s -Encoding UTF8
    Write-Output $s
}
Set-Content -LiteralPath $txt -Value '' -Encoding UTF8
Line '=== PFAB-21 IOCTL capture pass3 (inline, no DSE, no game) ==='
Line ("UTC=" + (Get-Date).ToUniversalTime().ToString('o'))
Line ("WHOAMI=" + (whoami))
$admin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
Line ("IS_ADMIN=" + $admin)

Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
[StructLayout(LayoutKind.Sequential)]
public struct LabIoStatus3 {
    public IntPtr Status;
    public IntPtr Information;
}
public static class LabIoctl3 {
    public static readonly IntPtr Invalid = new IntPtr(-1);
    [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
    public static extern IntPtr CreateFileW(string name, uint access, uint share, IntPtr sa, uint disp, uint flags, IntPtr template);
    [DllImport("kernel32.dll", SetLastError=true)] public static extern bool CloseHandle(IntPtr h);
    [DllImport("kernel32.dll", SetLastError=true)]
    public static extern bool DeviceIoControl(IntPtr h, uint code, IntPtr inBuf, uint inLen, IntPtr outBuf, uint outLen, out uint returned, IntPtr overlapped);
    [DllImport("ntdll.dll")]
    public static extern int NtDeviceIoControlFile(IntPtr FileHandle, IntPtr Event, IntPtr ApcRoutine, IntPtr ApcContext, ref LabIoStatus3 IoStatusBlock, uint IoControlCode, IntPtr InputBuffer, uint InputBufferLength, IntPtr OutputBuffer, uint OutputBufferLength);
}
"@
Line 'ADDTYPE_OK'

$cdb = 'C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\cdb.exe'
$kd = 'C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\kd.exe'
Line ("CDB_EXISTS=" + (Test-Path $cdb))
Line ("KD_EXISTS=" + (Test-Path $kd))

$bcd = cmd /c 'bcdedit /enum {current}' 2>&1 | Out-String
Line $bcd.TrimEnd()
Line 'DID_NOT_SET testsigning/nointegritychecks/debug this pass'

$ioctlRows = New-Object System.Collections.Generic.List[object]
$codes = @(0, 0x00220000, 0x00220003)
$accs = @(
    @{ n = 'GENERIC_RW'; a = [uint32]2147483648 },
    @{ n = 'NONE'; a = [uint32]0 }
)
# 2147483648 = 0x80000000 GENERIC_READ; use 0xC0000000 below as uint
$accs = @(
    @{ n = 'GENERIC_RW'; a = [Convert]::ToUInt32('C0000000', 16) },
    @{ n = 'NONE'; a = [uint32]0 }
)

function Start-Filt([string]$svc, [string]$file, [string]$inst, [string]$alt) {
    $binPath = '\??\C:\re\artifacts\' + $file
    Line ("CREATE " + ((& sc.exe create $svc type= kernel start= demand binPath= $binPath DisplayName= $svc 2>&1 | Out-String).Trim()))
    $svcKey = "HKLM:\SYSTEM\CurrentControlSet\Services\$svc"
    $instRoot = Join-Path $svcKey 'Instances'
    $instKey = Join-Path $instRoot $inst
    New-Item -Path $instRoot -Force | Out-Null
    New-Item -Path $instKey -Force | Out-Null
    New-ItemProperty -Path $instRoot -Name DefaultInstance -Value $inst -PropertyType String -Force | Out-Null
    New-ItemProperty -Path $instKey -Name Altitude -Value $alt -PropertyType String -Force | Out-Null
    New-ItemProperty -Path $instKey -Name Flags -Value 0 -PropertyType DWord -Force | Out-Null
    Line ("INST $svc $inst alt=$alt")
    $t0 = Get-Date
    Line ((& sc.exe start $svc 2>&1 | Out-String).Trim())
    Line ((& sc.exe query $svc 2>&1 | Out-String).Trim())
    try {
        Get-WinEvent -FilterHashtable @{ LogName = 'Microsoft-Windows-CodeIntegrity/Operational'; StartTime = $t0.AddSeconds(-8) } -ErrorAction SilentlyContinue |
            Select-Object -First 6 TimeCreated, Id, Message |
            ForEach-Object { Line ("CIEVT id=$($_.Id) " + (($_.Message -replace '\s+',' ').Substring(0, [Math]::Min(200, ($_.Message -replace '\s+',' ').Length)))) }
    } catch {}
    Line ((& fltmc.exe 2>&1 | Out-String).TrimEnd())
}

function Stop-Filt([string]$svc) {
    Line (("STOP " + $svc + " " + ((& sc.exe stop $svc 2>&1 | Out-String).Trim())))
    Start-Sleep -Seconds 2
    Line (("DELETE " + $svc + " " + ((& sc.exe delete $svc 2>&1 | Out-String).Trim())))
    Remove-Item -Path "HKLM:\SYSTEM\CurrentControlSet\Services\$svc\Instances" -Recurse -Force -ErrorAction SilentlyContinue
}

function Probe-Dev([string]$dev) {
    Line ("PROBE_BEGIN $dev")
    foreach ($acc in $accs) {
        $an = [uint32]$acc.a
        Line ("TRY_OPEN $dev $($acc.n) acc=0x$($an.ToString('X8'))")
        try {
            $h = [LabIoctl3]::CreateFileW($dev, $an, [uint32]3, [IntPtr]::Zero, [uint32]3, [uint32]0, [IntPtr]::Zero)
            $ok = ($h -ne [IntPtr]::Zero -and $h -ne [LabIoctl3]::Invalid)
            $err = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
            Line ("OPEN $dev $($acc.n) opened=$ok win32=$err handle=$h")
            if ($ok) {
                foreach ($code in $codes) {
                    $returned = [uint32]0
                    $dio = [LabIoctl3]::DeviceIoControl($h, [uint32]$code, [IntPtr]::Zero, [uint32]0, [IntPtr]::Zero, [uint32]0, [ref]$returned, [IntPtr]::Zero)
                    $dw = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
                    $iosb = New-Object LabIoStatus3
                    $nt = [LabIoctl3]::NtDeviceIoControlFile($h, [IntPtr]::Zero, [IntPtr]::Zero, [IntPtr]::Zero, [ref]$iosb, [uint32]$code, [IntPtr]::Zero, [uint32]0, [IntPtr]::Zero, [uint32]0)
                    Line ("IOCTL $dev $($acc.n) code=0x$($code.ToString('X8')) dio_ok=$dio dio_win32=$dw ret=$returned ntstatus=0x$($nt.ToString('X8'))")
                    $ioctlRows.Add([pscustomobject]@{
                        device = $dev; access = $acc.n; code = ('0x{0:X8}' -f $code)
                        opened = $true; open_win32 = $err; dio_ok = $dio; dio_win32 = $dw
                        dio_returned = $returned; ntstatus = ('0x{0:X8}' -f $nt)
                    })
                }
                [void][LabIoctl3]::CloseHandle($h)
            } else {
                $ioctlRows.Add([pscustomobject]@{
                    device = $dev; access = $acc.n; code = $null
                    opened = $false; open_win32 = $err
                })
            }
        } catch {
            Line ("OPEN_EX $dev $($acc.n) " + $_.Exception.Message)
        }
    }
    Line ("PROBE_END $dev")
}

Line '==== EOS solo ===='
Start-Filt 'EasyAntiCheat_EOS' 'EasyAntiCheat_EOS.sys' 'EasyAntiCheat_EOS Instance' '370010'
Probe-Dev '\\.\EasyAntiCheat_EOS'
Probe-Dev '\\.\EasyAntiCheat_EOSSys'
Probe-Dev '\\.\EasyAntiCheat'
Probe-Dev '\\.\BattlEye'
Probe-Dev '\\.\BEDaisy'

if (Test-Path $kd) {
    Line '--- kd -kl ---'
    $kdTmp = 'C:\re\labioctl-kd.out'
    Remove-Item $kdTmp -ErrorAction SilentlyContinue
    Push-Location 'C:\Program Files (x86)\Windows Kits\10\Debuggers\x64'
    try {
        & .\kd.exe -kl -c '!drvobj EasyAntiCheat_EOS 2; q' 2>&1 | Out-File -FilePath $kdTmp -Encoding utf8
        Line ("KD_EXIT=" + $LASTEXITCODE)
    } catch {
        Line ("KD_EX " + $_.Exception.Message)
    }
    Pop-Location
    $kdOut = Get-Content $kdTmp -Raw -ErrorAction SilentlyContinue
    if (-not $kdOut) { $kdOut = '(empty)' }
    Set-Content $kdLog $kdOut -Encoding UTF8
    Line $kdOut.Substring(0, [Math]::Min(2500, $kdOut.Length))
}

if (Test-Path $cdb) {
    Line '--- cdb DeviceIoControl ---'
    $probeSrc = 'C:\re\LabIoctlProbe.cs'
    $probeExe = 'C:\re\LabIoctlProbe.exe'
    @'
using System;
using System.Runtime.InteropServices;
class P {
    [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
    static extern IntPtr CreateFileW(string n, uint a, uint s, IntPtr sa, uint d, uint f, IntPtr t);
    [DllImport("kernel32.dll", SetLastError=true)]
    static extern bool DeviceIoControl(IntPtr h, uint c, IntPtr i, uint il, IntPtr o, uint ol, out uint r, IntPtr ov);
    [DllImport("kernel32.dll", SetLastError=true)] static extern bool CloseHandle(IntPtr h);
    static int Main(string[] args) {
        IntPtr h = CreateFileW(args[0], 0xC0000000, 3, IntPtr.Zero, 3, 0, IntPtr.Zero);
        if (h == IntPtr.Zero || h == new IntPtr(-1)) { Console.WriteLine("open_fail "+Marshal.GetLastWin32Error()); return 2; }
        uint ret;
        bool ok = DeviceIoControl(h, 0, IntPtr.Zero, 0, IntPtr.Zero, 0, out ret, IntPtr.Zero);
        Console.WriteLine("dio_ok="+ok+" win32="+Marshal.GetLastWin32Error()+" ret="+ret);
        CloseHandle(h);
        return 0;
    }
}
'@ | Set-Content $probeSrc -Encoding ASCII
    $csc = Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
    Line ("CSC=" + $csc + " exists=" + (Test-Path $csc))
    if (Test-Path $csc) {
        Line ((& $csc /nologo /out:$probeExe $probeSrc 2>&1 | Out-String).Trim())
        Line ("PROBE_EXE=" + (Test-Path $probeExe))
        if (Test-Path $probeExe) {
            $cdbScript = 'C:\re\labioctl-cdb.wds'
            "bp kernel32!DeviceIoControl`ng`n.echo IOCTL_HIT`nr rdx`nk 6`ng`nq`n" | Set-Content $cdbScript -Encoding ASCII
            $cdbTmp = 'C:\re\labioctl-cdb.out'
            Push-Location 'C:\Program Files (x86)\Windows Kits\10\Debuggers\x64'
            try {
                & .\cdb.exe -g -G -cf $cdbScript $probeExe '\\.\EasyAntiCheat_EOS' 2>&1 | Out-File $cdbTmp -Encoding utf8
                Line ("CDB_EXIT=" + $LASTEXITCODE)
            } catch { Line ("CDB_EX " + $_.Exception.Message) }
            Pop-Location
            $cdbOut = Get-Content $cdbTmp -Raw -ErrorAction SilentlyContinue
            if (-not $cdbOut) { $cdbOut = '(empty)' }
            Set-Content $cdbLog $cdbOut -Encoding UTF8
            Line $cdbOut.Substring(0, [Math]::Min(2500, $cdbOut.Length))
        }
    }
}

Stop-Filt 'EasyAntiCheat_EOS'
Start-Sleep -Seconds 2

Line '==== legacy solo ===='
Start-Filt 'EasyAntiCheat' 'EasyAntiCheat.sys' 'EasyAntiCheat Instance' '370020'
Probe-Dev '\\.\EasyAntiCheat'
Probe-Dev '\\.\EasyAntiCheat_EOS'
Stop-Filt 'EasyAntiCheat'
Start-Sleep -Seconds 1

Line '==== BEDaisy product-name CI 577 only ===='
Start-Filt 'BEDaisy' 'BEDaisy.sys' 'BEDaisy Instance' '370030'
Probe-Dev '\\.\BattlEye'
Probe-Dev '\\.\BEDaisy'
Stop-Filt 'BEDaisy'

Line '==== leftover ===='
foreach ($s in @('EasyAntiCheat_EOS','EasyAntiCheat','BEDaisy')) {
    Line ("LEFTOVER $s " + ((& sc.exe query $s 2>&1 | Out-String) -replace '\s+',' ').Trim())
}
Line ((& fltmc.exe 2>&1 | Out-String).TrimEnd())
Line 'IOCTL_SCRIPT_OK'
$ioctlRows | ConvertTo-Json -Depth 6 | Set-Content $jsonPath -Encoding UTF8
Line ("WROTE=" + $jsonPath + " rows=" + $ioctlRows.Count)
