# PFAB-20: elevated lab-only load of C:\re\artifacts copies on AI_workspace.
# Do not change DSE / testsigning / HVCI. Do not touch Program Files or start a game.
$ErrorActionPreference = 'Continue'
$outDir = 'Z:\ac-captures-host\2026-11-18'
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
$txt = Join-Path $outDir 'dyn-lab-load.txt'
$jsonPath = Join-Path $outDir 'dyn-lab-load.json'
function Line([string]$s) {
    Add-Content -LiteralPath $txt -Value $s -Encoding UTF8
    Write-Output $s
}
Set-Content -LiteralPath $txt -Value '' -Encoding UTF8

Line '=== PFAB-20 dyn lab load (elevated, no DSE change) ==='
Line ("UTC=" + (Get-Date).ToUniversalTime().ToString('o'))
Line ("COMPUTERNAME=" + $env:COMPUTERNAME)
Line ("USER=" + $env:USERNAME)
Line ("WHOAMI=" + (whoami))
$admin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
Line ("IS_ADMIN=" + $admin)
Line '--- whoami /groups (filtered) ---'
& whoami /groups 2>&1 | Select-String -Pattern 'Administrators|Mandatory Label' | ForEach-Object { Line $_.Line }
Line '--- whoami /priv ---'
& whoami /priv 2>&1 | Select-String -Pattern 'SeLoadDriver|SeDebug|SeTcb' | ForEach-Object { Line $_.Line }

Line '--- bcdedit {current} ---'
$bcd = cmd /c 'bcdedit /enum {current}' 2>&1 | Out-String
Line $bcd.TrimEnd()
$testsigning = $null; $nointegrity = $null; $debug = $null
if ($bcd -match 'testsigning\s+(\S+)') { $testsigning = $Matches[1] }
if ($bcd -match 'nointegritychecks\s+(\S+)') { $nointegrity = $Matches[1] }
if ($bcd -match '(?m)^\s*debug\s+(\S+)') { $debug = $Matches[1] }
Line ("TESTSIGNING=" + $testsigning)
Line ("NOINTEGRITYCHECKS=" + $nointegrity)
Line ("KERNEL_DEBUG=" + $debug)
Line 'DSE_POLICY: will not set testsigning/nointegritychecks; documenting CI outcome only.'

$ciPolicy = $null
try {
    $ciPolicy = Get-CimInstance -ClassName Win32_DeviceGuard -Namespace 'root\Microsoft\Windows\DeviceGuard' -ErrorAction Stop
    Line ("DG.VBS=" + $ciPolicy.VirtualizationBasedSecurityStatus)
    Line ("DG.CI_ENFORCE=" + $ciPolicy.CodeIntegrityPolicyEnforcementStatus)
    Line ("DG.UMCI=" + $ciPolicy.UsermodeCodeIntegrityPolicyEnforcementStatus)
} catch { Line ("DG_ERR=" + $_.Exception.Message) }
$vdb = (Get-ItemProperty -Path 'HKLM:\SYSTEM\CurrentControlSet\Control\CI\Config' -ErrorAction SilentlyContinue).VulnerableDriverBlocklistEnable
Line ("VulnerableDriverBlocklistEnable=" + $vdb)

$expect = @{
    'EasyAntiCheat_EOS.sys' = '26797A1122781CAF12F1A686E33A0883FA1E2895B40C2249932E319B338841C7'
    'EasyAntiCheat.sys'     = 'DD60A8CD0771B4FC548FDCEA68EA0FC78FBAAC58A77099731A62E70B03EB1C2A'
    'BEDaisy.sys'           = 'E7BCF344EEFA7A4EBEA9B048E376958A5AD711489249033FA480A7EC8B6C018E'
}

if (-not ([System.Management.Automation.PSTypeName]'LabCreateFile2').Type) {
    Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public static class LabCreateFile2 {
    [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
    public static extern IntPtr CreateFileW(string name, uint access, uint share, IntPtr sa, uint disp, uint flags, IntPtr template);
    [DllImport("kernel32.dll", SetLastError=true)] public static extern bool CloseHandle(IntPtr h);
    public static readonly IntPtr Invalid = new IntPtr(-1);
}
"@
}

function Probe-Devices([string]$tag) {
    $names = @(
        '\\.\EasyAntiCheat','\\.\EasyAntiCheatSys','\\.\EasyAntiCheat_EOS','\\.\EasyAntiCheat_EOSSys',
        '\\.\EasyAntiCheatEOS','\\.\BattlEye','\\.\BEDaisy','\\.\BEDaisySys'
    )
    $rows = @()
    foreach ($d in $names) {
        $h = [LabCreateFile2]::CreateFileW($d, [uint32]0, [uint32]3, [IntPtr]::Zero, [uint32]3, [uint32]0, [IntPtr]::Zero)
        $ok = ($h -ne [IntPtr]::Zero -and $h -ne [LabCreateFile2]::Invalid)
        $err = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
        if ($ok) { [void][LabCreateFile2]::CloseHandle($h) }
        Line ("DEV[$tag] " + $d + " opened=" + $ok + " win32=" + $err)
        $rows += [pscustomobject]@{ tag = $tag; name = $d; opened = $ok; win32 = $err }
    }
    return $rows
}

$devicesBefore = Probe-Devices 'before'

$jobs = @(
    @{ svc = 'LabDynEACEOS'; file = 'EasyAntiCheat_EOS.sys' },
    @{ svc = 'LabDynEAC';    file = 'EasyAntiCheat.sys' },
    @{ svc = 'LabDynBEDaisy'; file = 'BEDaisy.sys' }
)

$results = @()
foreach ($j in $jobs) {
    $path = Join-Path 'C:\re\artifacts' $j.file
    $row = [ordered]@{
        service = $j.svc; image = $path; sha256 = $null; hash_match = $false; authenticode = $null
        create_exit = $null; create_out = $null; start_exit = $null; start_out = $null
        query_out = $null; stop_out = $null; delete_out = $null
        dse_or_ci_blocker = $false; win32_guess = $null
    }
    Line ("==== LOAD " + $j.svc + " " + $path + " ====")
    if (-not (Test-Path -LiteralPath $path)) {
        Line 'MISSING_IMAGE'; $row.create_out = 'MISSING_IMAGE'; $results += [pscustomobject]$row; continue
    }
    $row.sha256 = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
    $row.hash_match = ($row.sha256 -eq $expect[$j.file])
    $sig = Get-AuthenticodeSignature -LiteralPath $path
    $row.authenticode = [string]$sig.Status
    Line ("HASH=" + $row.sha256 + " MATCH=" + $row.hash_match + " SIG=" + $row.authenticode)

    $binPath = '\??\' + $path
    $cout = & sc.exe create $j.svc type= kernel start= demand binPath= $binPath DisplayName= $j.svc 2>&1 | Out-String
    $row.create_exit = $LASTEXITCODE
    $row.create_out = $cout.Trim()
    Line ("CREATE_EXIT=" + $row.create_exit)
    Line $row.create_out

    $t0 = Get-Date
    $tmpOut = Join-Path $env:TEMP ("labdyn-start-" + $j.svc + ".out")
    $tmpErr = Join-Path $env:TEMP ("labdyn-start-" + $j.svc + ".err")
    Remove-Item $tmpOut, $tmpErr -ErrorAction SilentlyContinue
    $proc = Start-Process -FilePath "$env:SystemRoot\System32\sc.exe" -ArgumentList @('start', $j.svc) -PassThru -WindowStyle Hidden -RedirectStandardOutput $tmpOut -RedirectStandardError $tmpErr
    if (-not $proc.WaitForExit(90000)) {
        try { $proc.Kill() } catch {}
        $row.start_exit = -1
        $row.start_out = 'START_TIMEOUT_90s'
        Line 'START_TIMEOUT_90s'
    } else {
        $row.start_exit = $proc.ExitCode
        $sout = @()
        if (Test-Path $tmpOut) { $sout += Get-Content -LiteralPath $tmpOut -ErrorAction SilentlyContinue }
        if (Test-Path $tmpErr) { $sout += Get-Content -LiteralPath $tmpErr -ErrorAction SilentlyContinue }
        $row.start_out = ($sout -join "`n").Trim()
    }
    Line ("START_EXIT=" + $row.start_exit)
    Line $row.start_out
    $qout = & sc.exe query $j.svc 2>&1 | Out-String
    $row.query_out = $qout.Trim()
    Line $row.query_out

    $blob = ($row.start_out + ' ' + $row.query_out)
    if ($blob -match '577|1275|0xC0000428|0xC0000603|cannot verify the digital signature|Code Integrity|Windows cannot verify|blocked from loading|unsigned') {
        $row.dse_or_ci_blocker = $true
    }
    if ($row.start_exit -eq 577 -or $row.start_exit -eq 1275) { $row.dse_or_ci_blocker = $true }
    $row.win32_guess = $row.start_exit

    Line '--- System/CI events since start ---'
    try {
        Get-WinEvent -FilterHashtable @{ LogName = 'System'; StartTime = $t0.AddSeconds(-5) } -ErrorAction SilentlyContinue |
            Select-Object -First 12 TimeCreated, Id, ProviderName, Message |
            ForEach-Object {
                $msg = ($_.Message -replace '\s+', ' ')
                if ($msg.Length -gt 220) { $msg = $msg.Substring(0, 220) }
                Line ("SYSEVT t=" + $_.TimeCreated + " id=" + $_.Id + " src=" + $_.ProviderName + " " + $msg)
            }
    } catch { Line ("SYSEVT_ERR=" + $_.Exception.Message) }
    try {
        Get-WinEvent -FilterHashtable @{ LogName = 'Microsoft-Windows-CodeIntegrity/Operational'; StartTime = $t0.AddSeconds(-5) } -ErrorAction SilentlyContinue |
            Select-Object -First 12 TimeCreated, Id, Message |
            ForEach-Object {
                $msg = ($_.Message -replace '\s+', ' ')
                if ($msg.Length -gt 220) { $msg = $msg.Substring(0, 220) }
                Line ("CIEVT t=" + $_.TimeCreated + " id=" + $_.Id + " " + $msg)
            }
    } catch { Line ("CIEVT_ERR=" + $_.Exception.Message) }

    Line '--- fltmc after this start ---'
    Line ((& fltmc.exe 2>&1 | Out-String).TrimEnd())
    Line '--- driverquery match ---'
    try {
        driverquery /v /fo csv | ConvertFrom-Csv | Where-Object {
            $_.ModuleName -match 'EasyAntiCheat|BEDaisy|LabDyn' -or $_.'Path' -match 're\\artifacts'
        } | ForEach-Object { Line ("DRV " + $_.ModuleName + " state=" + $_.State + " path=" + $_.'Path') }
    } catch { Line ("DRV_ERR=" + $_.Exception.Message) }

    $null = Probe-Devices $j.svc

    $stop = & sc.exe stop $j.svc 2>&1 | Out-String
    $row.stop_out = $stop.Trim()
    Line $row.stop_out
    Start-Sleep -Seconds 2
    $del = & sc.exe delete $j.svc 2>&1 | Out-String
    $row.delete_out = $del.Trim()
    Line $row.delete_out
    $results += [pscustomobject]$row
}

$devicesAfter = Probe-Devices 'after'
Line '--- leftover LabDyn services ---'
foreach ($n in @('LabDynEACEOS','LabDynEAC','LabDynBEDaisy')) {
    $q = & sc.exe query $n 2>&1 | Out-String
    Line ("LEFT " + $n + " " + (($q.Trim() -replace '\s+', ' ')))
}

$payload = [ordered]@{
    utc = (Get-Date).ToUniversalTime().ToString('o')
    computer = $env:COMPUTERNAME
    is_admin = $admin
    testsigning = $testsigning
    nointegritychecks = $nointegrity
    kernel_debug = $debug
    ci_enforcement = $ciPolicy.CodeIntegrityPolicyEnforcementStatus
    vbs = $ciPolicy.VirtualizationBasedSecurityStatus
    vulnerable_driver_blocklist = $vdb
    dse_changed = $false
    live_game = $false
    image_root = 'C:\re\artifacts'
    results = $results
    devices_before = $devicesBefore
    devices_after = $devicesAfter
}
$payload | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $jsonPath -Encoding UTF8
Line ("WROTE=" + $jsonPath)
Line 'LOAD_SCRIPT_OK'
