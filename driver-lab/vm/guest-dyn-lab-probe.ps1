# PFAB-20: read-only lab probe on AI_workspace. No driver load. No DSE change.
$ErrorActionPreference = 'Continue'
$outDir = 'Z:\ac-captures-host\2026-11-18'
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
$txt = Join-Path $outDir 'dyn-lab-probe.txt'
$jsonPath = Join-Path $outDir 'dyn-lab-probe.json'

function Line([string]$s) {
    $s | Tee-Object -FilePath $txt -Append
}

'' | Set-Content -LiteralPath $txt -Encoding UTF8
Line '=== PFAB-20 dyn lab probe (no load) ==='
Line ("UTC=" + (Get-Date).ToUniversalTime().ToString('o'))
Line ("COMPUTERNAME=" + $env:COMPUTERNAME)
Line ("USER=" + $env:USERNAME)
Line ("WHOAMI=" + (whoami))

$who = & whoami /priv 2>&1 | Out-String
Line '--- whoami /priv (SeLoadDriverPrivilege) ---'
($who -split "`n" | Select-String -Pattern 'SeLoadDriver|SeDebug|SeTcb' | ForEach-Object { $_.Line }) | ForEach-Object { Line $_ }

$admin = $false
try {
    $admin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
} catch {}
Line ("IS_ADMIN=" + $admin)

$expect = @{
    'EasyAntiCheat_EOS.sys' = '26797A1122781CAF12F1A686E33A0883FA1E2895B40C2249932E319B338841C7'
    'EasyAntiCheat.sys'     = 'DD60A8CD0771B4FC548FDCEA68EA0FC78FBAAC58A77099731A62E70B03EB1C2A'
    'BEDaisy.sys'           = 'E7BCF344EEFA7A4EBEA9B048E376958A5AD711489249033FA480A7EC8B6C018E'
}

$artifacts = @()
Line '--- C:\re\artifacts ---'
$artDir = 'C:\re\artifacts'
Line ("ART_DIR_EXISTS=" + (Test-Path -LiteralPath $artDir))
if (Test-Path -LiteralPath $artDir) {
    Get-ChildItem -LiteralPath $artDir -File | ForEach-Object {
        Line ("FILE=" + $_.Name + " LEN=" + $_.Length)
    }
}
foreach ($name in @('EasyAntiCheat_EOS.sys','EasyAntiCheat.sys','BEDaisy.sys')) {
    $p = Join-Path $artDir $name
    $row = [ordered]@{
        name = $name
        path = $p
        exists = (Test-Path -LiteralPath $p)
        length = $null
        sha256 = $null
        hash_match = $false
        authenticode_status = $null
        authenticode_signer = $null
    }
    if ($row.exists) {
        $item = Get-Item -LiteralPath $p
        $row.length = $item.Length
        $row.sha256 = (Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash
        $row.hash_match = ($row.sha256 -eq $expect[$name])
        try {
            $sig = Get-AuthenticodeSignature -LiteralPath $p
            $row.authenticode_status = [string]$sig.Status
            $row.authenticode_signer = [string]$sig.SignerCertificate.Subject
        } catch {
            $row.authenticode_status = $_.Exception.Message
        }
    }
    Line ("ART name=" + $row.name + " exists=" + $row.exists + " len=" + $row.length + " match=" + $row.hash_match + " sha=" + $row.sha256 + " sig=" + $row.authenticode_status)
    $artifacts += [pscustomobject]$row
}

Line '--- live Program Files (must remain unused) ---'
$live = @(
    'C:\Program Files (x86)\EasyAntiCheat_EOS\EasyAntiCheat_EOS.sys',
    'C:\Program Files (x86)\EasyAntiCheat\EasyAntiCheat.sys',
    'C:\Program Files (x86)\Common Files\BattlEye\BEDaisy.sys',
    'C:\Windows\System32\drivers\BEDaisy.sys',
    'C:\Windows\System32\drivers\EasyAntiCheat.sys',
    'C:\Windows\System32\drivers\EasyAntiCheat_EOS.sys'
)
$liveHits = @()
foreach ($p in $live) {
    $hit = Test-Path -LiteralPath $p
    Line ("LIVE exists=" + $hit + " path=" + $p)
    $liveHits += [pscustomobject]@{ path = $p; exists = $hit }
}

Line '--- DSE / CI / DeviceGuard (read-only) ---'
$bcd = & bcdedit /enum '{current}' 2>&1 | Out-String
Line $bcd.TrimEnd()
$testsigning = $null
$nointegrity = $null
if ($bcd -match 'testsigning\s+(\S+)') { $testsigning = $Matches[1] }
if ($bcd -match 'nointegritychecks\s+(\S+)') { $nointegrity = $Matches[1] }
Line ("TESTSIGNING=" + $testsigning)
Line ("NOINTEGRITYCHECKS=" + $nointegrity)

$ci = $null
try {
    $ci = Get-ItemProperty -Path 'HKLM:\SYSTEM\CurrentControlSet\Control\CI' -ErrorAction SilentlyContinue
    Line ("CI.CIDll=" + $ci.CIDll)
    Line ("CI.VulnerableDriverBlocklistEnable=" + (Get-ItemProperty -Path 'HKLM:\SYSTEM\CurrentControlSet\Control\CI\Config' -ErrorAction SilentlyContinue).VulnerableDriverBlocklistEnable)
} catch {
    Line ("CI_ERR=" + $_.Exception.Message)
}

$dg = $null
try {
    $dg = Get-CimInstance -ClassName Win32_DeviceGuard -Namespace 'root\Microsoft\Windows\DeviceGuard' -ErrorAction Stop
    Line ("DG.VirtualizationBasedSecurityStatus=" + $dg.VirtualizationBasedSecurityStatus)
    Line ("DG.CodeIntegrityPolicyEnforcementStatus=" + $dg.CodeIntegrityPolicyEnforcementStatus)
    Line ("DG.UsermodeCodeIntegrityPolicyEnforcementStatus=" + $dg.UsermodeCodeIntegrityPolicyEnforcementStatus)
    Line ("DG.SecurityServicesRunning=" + ($dg.SecurityServicesRunning -join ','))
    Line ("DG.SecurityServicesConfigured=" + ($dg.SecurityServicesConfigured -join ','))
} catch {
    Line ("DEVICEGUARD_ERR=" + $_.Exception.Message)
}

$hvci = $null
try {
    $hvci = Get-ItemProperty -Path 'HKLM:\SYSTEM\CurrentControlSet\Control\DeviceGuard\Scenarios\HypervisorEnforcedCodeIntegrity' -ErrorAction SilentlyContinue
    Line ("HVCI.Enabled=" + $hvci.Enabled)
    Line ("HVCI.Locked=" + $hvci.Locked)
} catch {}

Line '--- existing services (query only) ---'
$svcNames = @('EasyAntiCheat','EasyAntiCheat_EOS','EasyAntiCheatSys','EasyAntiCheat_EOSSys','BEDaisy','BEService','BEService_unturned')
$services = @()
foreach ($n in $svcNames) {
    $q = & sc.exe query $n 2>&1 | Out-String
    Line ("SVC " + $n + ":")
    Line $q.TrimEnd()
    $qc = & sc.exe qc $n 2>&1 | Out-String
    Line $qc.TrimEnd()
    $services += [pscustomobject]@{ name = $n; query = $q; qc = $qc }
}

Line '--- driverquery (EAC/BE filter) ---'
$dq = @()
try {
    $dq = @(driverquery /v /fo csv | ConvertFrom-Csv | Where-Object {
        $_.ModuleName -match 'EasyAntiCheat|BEDaisy|BattlEye|EAC' -or $_.'Display Name' -match 'EasyAntiCheat|BattlEye|BEDaisy'
    })
    if ($dq.Count -eq 0) { Line 'DRIVERQUERY_MATCHES=0' }
    $dq | ForEach-Object { Line ("DRV " + $_.ModuleName + " state=" + $_.State + " path=" + $_.'Path') }
} catch {
    Line ("DRIVERQUERY_ERR=" + $_.Exception.Message)
}

Line '--- fltmc (minifilters) ---'
$flt = & fltmc.exe 2>&1 | Out-String
Line $flt.TrimEnd()

Line '--- WinDbg / kd presence ---'
$dbgPaths = @(
    'C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\windbg.exe',
    'C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\kd.exe',
    'C:\Program Files\Windows Kits\10\Debuggers\x64\windbg.exe',
    'C:\Program Files\Windows Kits\10\Debuggers\x64\kd.exe',
    "$env:LOCALAPPDATA\Microsoft\WindowsApps\WinDbgX.exe"
)
$dbg = @()
foreach ($p in $dbgPaths) {
    $hit = Test-Path -LiteralPath $p
    Line ("DBG exists=" + $hit + " path=" + $p)
    $dbg += [pscustomobject]@{ path = $p; exists = $hit }
}
$dbgCmd = Get-Command windbg, kd, WinDbgX -ErrorAction SilentlyContinue
if ($dbgCmd) { $dbgCmd | ForEach-Object { Line ("DBGCMD=" + $_.Source) } } else { Line 'DBGCMD=none' }

Line '--- hypothesized device CreateFile (no IOCTL) ---'
$devNames = @(
    '\\.\EasyAntiCheat',
    '\\.\EasyAntiCheatSys',
    '\\.\EasyAntiCheat_EOS',
    '\\.\EasyAntiCheat_EOSSys',
    '\\.\EasyAntiCheatEOS',
    '\\.\BattlEye',
    '\\.\BEDaisy',
    '\\.\BEDaisySys'
)
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public static class LabCreateFile {
    [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
    public static extern IntPtr CreateFileW(string name, uint access, uint share, IntPtr sa, uint disp, uint flags, IntPtr template);
    [DllImport("kernel32.dll", SetLastError=true)] public static extern bool CloseHandle(IntPtr h);
    public static readonly IntPtr Invalid = new IntPtr(-1);
}
"@ -ErrorAction SilentlyContinue

$devices = @()
foreach ($d in $devNames) {
    # Access 0 = existence probe only (no GENERIC_READ uint32 wrap, no IOCTL).
    $h = [LabCreateFile]::CreateFileW($d, [uint32]0, [uint32]3, [IntPtr]::Zero, [uint32]3, [uint32]0, [IntPtr]::Zero)
    $ok = ($h -ne [IntPtr]::Zero -and $h -ne [LabCreateFile]::Invalid)
    $err = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
    if ($ok) { [void][LabCreateFile]::CloseHandle($h) }
    Line ("DEV " + $d + " ok=" + $ok + " win32=" + $err)
    $devices += [pscustomobject]@{ name = $d; opened = $ok; win32 = $err }
}

Line '--- Code Integrity events (last 24h, sample) ---'
$ciEvents = @()
try {
    $ciEvents = @(Get-WinEvent -FilterHashtable @{ LogName = 'Microsoft-Windows-CodeIntegrity/Operational'; StartTime = (Get-Date).AddDays(-1) } -ErrorAction SilentlyContinue |
        Select-Object -First 25 TimeCreated, Id, LevelDisplayName, Message)
    Line ("CI_EVENT_SAMPLE=" + $ciEvents.Count)
    foreach ($e in $ciEvents) {
        $msg = ($e.Message -replace '\s+', ' ')
        if ($msg.Length -gt 240) { $msg = $msg.Substring(0, 240) }
        Line ("CIEVT t=" + $e.TimeCreated + " id=" + $e.Id + " " + $msg)
    }
} catch {
    Line ("CI_EVENTS_ERR=" + $_.Exception.Message)
}

$payload = [ordered]@{
    utc = (Get-Date).ToUniversalTime().ToString('o')
    computer = $env:COMPUTERNAME
    user = $env:USERNAME
    is_admin = $admin
    artifacts = $artifacts
    live_program_files = $liveHits
    testsigning = $testsigning
    nointegritychecks = $nointegrity
    hvci_enabled = $hvci.Enabled
    device_guard = @{
        vbs = $dg.VirtualizationBasedSecurityStatus
        ci_enforcement = $dg.CodeIntegrityPolicyEnforcementStatus
        umci = $dg.UsermodeCodeIntegrityPolicyEnforcementStatus
        services_running = @($dg.SecurityServicesRunning)
    }
    devices = $devices
    dbg = $dbg
    load_attempted = $false
    dse_changed = $false
    scope = 'lab copies under C:\re\artifacts only; no live game; no DSE disable'
}
$payload | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $jsonPath -Encoding UTF8
Line ("WROTE=" + $jsonPath)
Line 'PROBE_OK'
