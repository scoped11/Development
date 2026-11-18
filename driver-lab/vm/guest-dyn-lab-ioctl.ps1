# PFAB-21: lab IOCTL / IRP_MJ_DEVICE_CONTROL capture on AI_workspace.
# Empty buffers only. No live game. No DSE change. No bypass product.
$ErrorActionPreference = 'Continue'
$outDir = 'Z:\ac-captures-host\2026-11-18'
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
$txt = Join-Path $outDir 'dyn-lab-ioctl.txt'
$jsonPath = Join-Path $outDir 'dyn-lab-ioctl.json'
$cdbLog = Join-Path $outDir 'dyn-lab-ioctl-cdb.txt'
$kdLog = Join-Path $outDir 'dyn-lab-ioctl-kd.txt'
function Line([string]$s) {
    Add-Content -LiteralPath $txt -Value $s -Encoding UTF8
    Write-Output $s
}
Set-Content -LiteralPath $txt -Value '' -Encoding UTF8
Line '=== PFAB-21 IOCTL capture (elevated, no DSE, no game) ==='
Line ("UTC=" + (Get-Date).ToUniversalTime().ToString('o'))
Line ("COMPUTERNAME=" + $env:COMPUTERNAME)
Line ("WHOAMI=" + (whoami))
$admin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
Line ("IS_ADMIN=" + $admin)
Line 'DSE_POLICY: will not set testsigning/nointegritychecks. BEDaisy product-name 577 is documented only.'

if (-not ([System.Management.Automation.PSTypeName]'LabIoctl21').Type) {
    Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
[StructLayout(LayoutKind.Sequential)]
public struct LabIoStatus {
    public IntPtr Status;
    public IntPtr Information;
}
public static class LabIoctl21 {
    public static readonly IntPtr Invalid = new IntPtr(-1);
    [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
    public static extern IntPtr CreateFileW(string name, uint access, uint share, IntPtr sa, uint disp, uint flags, IntPtr template);
    [DllImport("kernel32.dll", SetLastError=true)] public static extern bool CloseHandle(IntPtr h);
    [DllImport("kernel32.dll", SetLastError=true)]
    public static extern bool DeviceIoControl(IntPtr h, uint code, IntPtr inBuf, uint inLen, IntPtr outBuf, uint outLen, out uint returned, IntPtr overlapped);
    [DllImport("ntdll.dll")]
    public static extern int NtDeviceIoControlFile(IntPtr FileHandle, IntPtr Event, IntPtr ApcRoutine, IntPtr ApcContext, ref LabIoStatus IoStatusBlock, uint IoControlCode, IntPtr InputBuffer, uint InputBufferLength, IntPtr OutputBuffer, uint OutputBufferLength);
}
"@
}

function Find-DbgTools {
    $hits = New-Object System.Collections.Generic.List[string]
    $names = @('cdb.exe','kd.exe','windbg.exe','WinDbgX.exe','DbgX.Shell.exe')
    $roots = @(
        "${env:ProgramFiles(x86)}\Windows Kits\10\Debuggers\x64",
        "$env:ProgramFiles\Windows Kits\10\Debuggers\x64",
        'C:\Dbg',
        'C:\Tools\Debuggers\x64',
        "$env:LOCALAPPDATA\Microsoft\WindowsApps"
    )
    foreach ($r in $roots) {
        if (-not (Test-Path $r)) { continue }
        foreach ($n in $names) {
            Get-ChildItem -LiteralPath $r -Filter $n -ErrorAction SilentlyContinue | ForEach-Object { $hits.Add($_.FullName) }
        }
    }
    $apps = Join-Path $env:ProgramFiles 'WindowsApps'
    if (Test-Path $apps) {
        Get-ChildItem $apps -Filter 'Microsoft.WinDbg*' -Directory -ErrorAction SilentlyContinue | ForEach-Object {
            Get-ChildItem $_.FullName -Include $names -Recurse -ErrorAction SilentlyContinue |
                Select-Object -First 8 | ForEach-Object { $hits.Add($_.FullName) }
        }
    }
    Get-Command cdb,kd,windbg,WinDbgX -ErrorAction SilentlyContinue | ForEach-Object { $hits.Add($_.Source) }
    return @($hits | Select-Object -Unique)
}

$bcd = cmd /c 'bcdedit /enum {current}' 2>&1 | Out-String
Line '--- bcdedit {current} ---'
Line $bcd.TrimEnd()
$testsigning = $null; $nointegrity = $null; $debug = $null
if ($bcd -match 'testsigning\s+(\S+)') { $testsigning = $Matches[1] }
if ($bcd -match 'nointegritychecks\s+(\S+)') { $nointegrity = $Matches[1] }
if ($bcd -match '(?m)^\s*debug\s+(\S+)') { $debug = $Matches[1] }
Line ("TESTSIGNING=" + $testsigning)
Line ("NOINTEGRITYCHECKS=" + $nointegrity)
Line ("KERNEL_DEBUG=" + $debug)
try {
    $ciPolicy = Get-CimInstance -ClassName Win32_DeviceGuard -Namespace 'root\Microsoft\Windows\DeviceGuard' -ErrorAction Stop
    Line ("DG.VBS=" + $ciPolicy.VirtualizationBasedSecurityStatus)
    Line ("DG.CI_ENFORCE=" + $ciPolicy.CodeIntegrityPolicyEnforcementStatus)
} catch { Line ("DG_ERR=" + $_.Exception.Message) }

$expect = @{
    'EasyAntiCheat_EOS.sys' = '26797A1122781CAF12F1A686E33A0883FA1E2895B40C2249932E319B338841C7'
    'EasyAntiCheat.sys'     = 'DD60A8CD0771B4FC548FDCEA68EA0FC78FBAAC58A77099731A62E70B03EB1C2A'
    'BEDaisy.sys'           = 'E7BCF344EEFA7A4EBEA9B048E376958A5AD711489249033FA480A7EC8B6C018E'
}
foreach ($f in $expect.Keys) {
    $p = Join-Path 'C:\re\artifacts' $f
    $h = (Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash
    Line ("HASH $f match=" + ($h -eq $expect[$f]) + " sha=" + $h)
}

$dbg = Find-DbgTools
Line ('DBG_TOOLS=' + ($dbg -join ';'))
$cdb = $dbg | Where-Object { $_ -match '\\cdb\.exe$' } | Select-Object -First 1
$kd = $dbg | Where-Object { $_ -match '\\kd\.exe$' } | Select-Object -First 1
$windbgx = $dbg | Where-Object { $_ -match 'WinDbgX\.exe$|DbgX\.Shell\.exe$' } | Select-Object -First 1
Line ("CDB=" + $cdb)
Line ("KD=" + $kd)
Line ("WINDGBX=" + $windbgx)

# Sentinel IOCTL codes only: empty in/out, reject-status documentation. Not a code sweep.
$ioctlCodes = @(
    @{ name = 'zero'; code = [uint32]0 },
    @{ name = 'FILE_DEVICE_UNKNOWN_fn0_METHOD_BUFFERED'; code = [uint32]0x00220000 },
    @{ name = 'FILE_DEVICE_UNKNOWN_fn0_METHOD_NEITHER'; code = [uint32]0x00220003 }
)
# Hashtable key cannot be 'value' — PowerShell maps .value to Hashtable.Values.
$accesses = @(
    @{ name = 'GENERIC_RW'; acc = [uint32]0xC0000000 },
    @{ name = 'NONE'; acc = [uint32]0 }
)

function Start-LabFilter([string]$svc, [string]$file, [string]$inst, [string]$alt) {
    $path = Join-Path 'C:\re\artifacts' $file
    $binPath = '\??\' + $path
    $cout = & sc.exe create $svc type= kernel start= demand binPath= $binPath DisplayName= $svc 2>&1 | Out-String
    Line ("CREATE $svc " + $cout.Trim())
    $svcKey = "HKLM:\SYSTEM\CurrentControlSet\Services\$svc"
    $instRoot = Join-Path $svcKey 'Instances'
    $instKey = Join-Path $instRoot $inst
    New-Item -Path $instRoot -Force | Out-Null
    New-Item -Path $instKey -Force | Out-Null
    New-ItemProperty -Path $instRoot -Name DefaultInstance -Value $inst -PropertyType String -Force | Out-Null
    New-ItemProperty -Path $instKey -Name Altitude -Value $alt -PropertyType String -Force | Out-Null
    New-ItemProperty -Path $instKey -Name Flags -Value 0 -PropertyType DWord -Force | Out-Null
    Line ("INST $svc DefaultInstance=$inst Altitude=$alt")
    $t0 = Get-Date
    $tmpOut = Join-Path $env:TEMP ("labioctl-" + $svc + ".out")
    $tmpErr = Join-Path $env:TEMP ("labioctl-" + $svc + ".err")
    Remove-Item $tmpOut, $tmpErr -ErrorAction SilentlyContinue
    $proc = Start-Process -FilePath "$env:SystemRoot\System32\sc.exe" -ArgumentList @('start', $svc) -PassThru -WindowStyle Hidden -RedirectStandardOutput $tmpOut -RedirectStandardError $tmpErr
    if (-not $proc.WaitForExit(90000)) {
        try { $proc.Kill() } catch {}
        Line ("START_TIMEOUT $svc")
        return @{ running = $false; start_out = 'TIMEOUT'; win32_exit = -1 }
    }
    $startOut = ((Get-Content $tmpOut -ErrorAction SilentlyContinue) + (Get-Content $tmpErr -ErrorAction SilentlyContinue)) -join "`n"
    Line ("START $svc exit=" + $proc.ExitCode)
    Line $startOut
    $q = & sc.exe query $svc 2>&1 | Out-String
    Line $q.Trim()
    $running = $q -match 'STATE\s+:\s+4\s+RUNNING'
    try {
        Get-WinEvent -FilterHashtable @{ LogName = 'System'; StartTime = $t0.AddSeconds(-8) } -ErrorAction SilentlyContinue |
            Select-Object -First 8 TimeCreated, Id, ProviderName, Message |
            ForEach-Object {
                $msg = ($_.Message -replace '\s+', ' ')
                if ($msg.Length -gt 220) { $msg = $msg.Substring(0, 220) }
                Line ("SYSEVT $svc id=$($_.Id) src=$($_.ProviderName) $msg")
            }
    } catch {}
    try {
        Get-WinEvent -FilterHashtable @{ LogName = 'Microsoft-Windows-CodeIntegrity/Operational'; StartTime = $t0.AddSeconds(-8) } -ErrorAction SilentlyContinue |
            Select-Object -First 8 TimeCreated, Id, Message |
            ForEach-Object {
                $msg = ($_.Message -replace '\s+', ' ')
                if ($msg.Length -gt 220) { $msg = $msg.Substring(0, 220) }
                Line ("CIEVT $svc id=$($_.Id) $msg")
            }
    } catch {}
    return @{ running = $running; start_out = $startOut; query = $q }
}

function Stop-LabFilter([string]$svc) {
    $stop = & sc.exe stop $svc 2>&1 | Out-String
    Line ("STOP $svc " + $stop.Trim())
    Start-Sleep -Seconds 2
    $del = & sc.exe delete $svc 2>&1 | Out-String
    Line ("DELETE $svc " + $del.Trim())
    Remove-Item -Path "HKLM:\SYSTEM\CurrentControlSet\Services\$svc\Instances" -Recurse -Force -ErrorAction SilentlyContinue
}

function Invoke-EmptyIoctl([string]$device, [uint32]$access, [uint32]$code) {
    $row = [ordered]@{
        device = $device; access = ('0x{0:X8}' -f $access); code = ('0x{0:X8}' -f $code)
        opened = $false; open_win32 = $null
        dio_ok = $null; dio_win32 = $null; dio_returned = $null
        ntstatus = $null; iosb_info = $null; error = $null
    }
    try {
        $h = [LabIoctl21]::CreateFileW($device, $access, [uint32]3, [IntPtr]::Zero, [uint32]3, [uint32]0, [IntPtr]::Zero)
        $openOk = ($h -ne [IntPtr]::Zero -and $h -ne [LabIoctl21]::Invalid)
        $row.opened = $openOk
        $row.open_win32 = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
        if (-not $openOk) { return [pscustomobject]$row }
        $returned = [uint32]0
        $dio = [LabIoctl21]::DeviceIoControl($h, $code, [IntPtr]::Zero, [uint32]0, [IntPtr]::Zero, [uint32]0, [ref]$returned, [IntPtr]::Zero)
        $row.dio_ok = $dio
        $row.dio_win32 = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
        $row.dio_returned = $returned
        $iosb = New-Object LabIoStatus
        $nt = [LabIoctl21]::NtDeviceIoControlFile($h, [IntPtr]::Zero, [IntPtr]::Zero, [IntPtr]::Zero, [ref]$iosb, $code, [IntPtr]::Zero, [uint32]0, [IntPtr]::Zero, [uint32]0)
        $row.ntstatus = ('0x{0:X8}' -f $nt)
        $row.iosb_info = [int64]$iosb.Information
        [void][LabIoctl21]::CloseHandle($h)
    } catch {
        $row.error = $_.Exception.Message
        Line ("IOCTL_EX $device " + $_.Exception.Message)
    }
    return [pscustomobject]$row
}

function Invoke-DevicePass([string]$dev) {
    foreach ($a in $accesses) {
        try {
            $h = [LabIoctl21]::CreateFileW($dev, [uint32]$a.acc, [uint32]3, [IntPtr]::Zero, [uint32]3, [uint32]0, [IntPtr]::Zero)
            $ok = ($h -ne [IntPtr]::Zero -and $h -ne [LabIoctl21]::Invalid)
            $err = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
            if ($ok) { [void][LabIoctl21]::CloseHandle($h) }
            Line ("OPEN $dev access=$($a.name) opened=$ok win32=$err")
        } catch {
            Line ("OPEN_EX $dev access=$($a.name) " + $_.Exception.Message)
        }
        foreach ($c in $ioctlCodes) {
            $row = Invoke-EmptyIoctl $dev ([uint32]$a.acc) ([uint32]$c.code)
            Line ("IOCTL dev=$($row.device) access=$($a.name) code=$($c.name) $($row.code) opened=$($row.opened) open_win32=$($row.open_win32) dio_ok=$($row.dio_ok) dio_win32=$($row.dio_win32) ret=$($row.dio_returned) ntstatus=$($row.ntstatus) err=$($row.error)")
            $script:ioctlRows += $row
        }
    }
}

$script:ioctlRows = @()
$kdOut = $null
$cdbOut = $null

Line '==== LOAD EasyAntiCheat_EOS (solo; avoid altitude clash) ===='
$eos = Start-LabFilter 'EasyAntiCheat_EOS' 'EasyAntiCheat_EOS.sys' 'EasyAntiCheat_EOS Instance' '370010'
Line ((& fltmc.exe 2>&1 | Out-String).TrimEnd())
foreach ($dev in @('\\.\EasyAntiCheat_EOS','\\.\EasyAntiCheat_EOSSys','\\.\EasyAntiCheat','\\.\BattlEye','\\.\BEDaisy')) {
    Invoke-DevicePass $dev
}
if ($kd) {
    Line '--- kd -kl while EOS loaded ---'
    $kdTmp = Join-Path $env:TEMP 'labioctl-kd.out'
    $kdCmd = " `"$kd`" -kl -c `"!drvobj EasyAntiCheat_EOS 2; !devobj \\Device\\EasyAntiCheat_EOS; q`" > `"$kdTmp`" 2>&1"
    $kp = Start-Process -FilePath "$env:SystemRoot\System32\cmd.exe" -ArgumentList @('/c', $kdCmd) -PassThru -WindowStyle Hidden
    if (-not $kp.WaitForExit(45000)) {
        try { $kp.Kill() } catch {}
        Line 'KD_TIMEOUT_45s'
    } else {
        Line ("KD_EXIT=" + $kp.ExitCode)
    }
    $kdOut = Get-Content $kdTmp -Raw -ErrorAction SilentlyContinue
    if (-not $kdOut) { $kdOut = '(empty)' }
    Set-Content -LiteralPath $kdLog -Value $kdOut -Encoding UTF8
    $preview = if ($kdOut.Length -gt 3000) { $kdOut.Substring(0, 3000) } else { $kdOut }
    Line $preview
    Line ("WROTE_KD=" + $kdLog)
} else {
    Line 'KD_ABSENT'
}

if ($cdb) {
    Line '--- cdb bp kernel32!DeviceIoControl (EOS loaded, empty IOCTL 0) ---'
    $probeSrc = Join-Path $env:TEMP 'LabIoctlProbe.cs'
    $probeExe = Join-Path $env:TEMP 'LabIoctlProbe.exe'
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
'@ | Set-Content -LiteralPath $probeSrc -Encoding UTF8
    $csc = Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
    if (Test-Path $csc) {
        & $csc /nologo /out:$probeExe $probeSrc 2>&1 | ForEach-Object { Line $_ }
        if (Test-Path $probeExe) {
            $cdbScript = Join-Path $env:TEMP 'labioctl-cdb.wds'
            @(
                'bp kernel32!DeviceIoControl',
                'g',
                '.echo IOCTL_HIT',
                'r rdx',
                'k 6',
                'g',
                'q'
            ) | Set-Content -LiteralPath $cdbScript -Encoding ASCII
            $cdbTmp = Join-Path $env:TEMP 'labioctl-cdb.out'
            Remove-Item $cdbTmp -ErrorAction SilentlyContinue
            $cdbCmd = " `"$cdb`" -g -G -cf `"$cdbScript`" `"$probeExe`" \\.\EasyAntiCheat_EOS > `"$cdbTmp`" 2>&1"
            $cp = Start-Process -FilePath "$env:SystemRoot\System32\cmd.exe" -ArgumentList @('/c', $cdbCmd) -PassThru -WindowStyle Hidden
            if (-not $cp.WaitForExit(60000)) {
                try { $cp.Kill() } catch {}
                Line 'CDB_TIMEOUT_60s'
            } else {
                Line ("CDB_EXIT=" + $cp.ExitCode)
            }
            $cdbOut = Get-Content $cdbTmp -Raw -ErrorAction SilentlyContinue
            if (-not $cdbOut) { $cdbOut = '(empty)' }
            Set-Content -LiteralPath $cdbLog -Value $cdbOut -Encoding UTF8
            $preview = if ($cdbOut.Length -gt 2500) { $cdbOut.Substring(0, 2500) } else { $cdbOut }
            Line $preview
            Line ("WROTE_CDB=" + $cdbLog)
        } else { Line 'PROBE_EXE_MISSING' }
    } else { Line 'CSC_MISSING' }
} else {
    Line 'CDB_ABSENT'
}

Stop-LabFilter 'EasyAntiCheat_EOS'
Start-Sleep -Seconds 2

Line '==== LOAD EasyAntiCheat legacy (solo) ===='
$eac = Start-LabFilter 'EasyAntiCheat' 'EasyAntiCheat.sys' 'EasyAntiCheat Instance' '370020'
Line ((& fltmc.exe 2>&1 | Out-String).TrimEnd())
foreach ($dev in @('\\.\EasyAntiCheat','\\.\EasyAntiCheat_EOS','\\.\BattlEye','\\.\BEDaisy')) {
    Invoke-DevicePass $dev
}
Stop-LabFilter 'EasyAntiCheat'
Start-Sleep -Seconds 2

Line '==== BEDaisy product-name start (document CI 577 only, no DSE change) ===='
$be = Start-LabFilter 'BEDaisy' 'BEDaisy.sys' 'BEDaisy Instance' '370030'
Line ("BEDAISY_RUNNING=" + $be.running)
Stop-LabFilter 'BEDaisy'

Line '==== cleanup EAC ===='
Stop-LabFilter 'EasyAntiCheat_EOS'
Stop-LabFilter 'EasyAntiCheat'
Start-Sleep -Seconds 1
foreach ($s in @('EasyAntiCheat_EOS','EasyAntiCheat','BEDaisy')) {
    $q = & sc.exe query $s 2>&1 | Out-String
    Line ("LEFTOVER $s " + (($q -replace '\s+', ' ').Trim()))
}
Line ((& fltmc.exe 2>&1 | Out-String).TrimEnd())
Line 'IOCTL_SCRIPT_OK'

@{
    utc = (Get-Date).ToUniversalTime().ToString('o')
    is_admin = $admin
    testsigning = $testsigning
    nointegritychecks = $nointegrity
    kernel_debug = $debug
    dse_changed = $false
    live_game = $false
    dbg_tools = $dbg
    eos_running = [bool]$eos.running
    eac_running = [bool]$eac.running
    bedaisy_running = [bool]$be.running
    ioctl = $script:ioctlRows
} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $jsonPath -Encoding UTF8
Line ("WROTE=" + $jsonPath)
