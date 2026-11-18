# PFAB-20 follow-up: lab minifilter Instances keys so FltRegisterFilter can proceed.
# Still C:\re\artifacts only. No DSE change, no Program Files, no game.
$ErrorActionPreference = 'Continue'
$outDir = 'Z:\ac-captures-host\2026-11-18'
$txt = Join-Path $outDir 'dyn-lab-minifilter.txt'
$jsonPath = Join-Path $outDir 'dyn-lab-minifilter.json'
function Line([string]$s) {
    Add-Content -LiteralPath $txt -Value $s -Encoding UTF8
    Write-Output $s
}
Set-Content -LiteralPath $txt -Value '' -Encoding UTF8
Line '=== PFAB-20 minifilter instance retry ==='
Line ("UTC=" + (Get-Date).ToUniversalTime().ToString('o'))
Line ("IS_ADMIN=" + ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator))

if (-not ([System.Management.Automation.PSTypeName]'LabCreateFile3').Type) {
    Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public static class LabCreateFile3 {
    [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
    public static extern IntPtr CreateFileW(string name, uint access, uint share, IntPtr sa, uint disp, uint flags, IntPtr template);
    [DllImport("kernel32.dll", SetLastError=true)] public static extern bool CloseHandle(IntPtr h);
    public static readonly IntPtr Invalid = new IntPtr(-1);
}
"@
}
function Probe([string]$tag) {
    $rows = @()
    foreach ($d in @('\\.\EasyAntiCheat','\\.\EasyAntiCheatSys','\\.\EasyAntiCheat_EOS','\\.\EasyAntiCheat_EOSSys','\\.\BattlEye','\\.\BEDaisy')) {
        $h = [LabCreateFile3]::CreateFileW($d, [uint32]0, [uint32]3, [IntPtr]::Zero, [uint32]3, [uint32]0, [IntPtr]::Zero)
        $ok = ($h -ne [IntPtr]::Zero -and $h -ne [LabCreateFile3]::Invalid)
        $err = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
        if ($ok) { [void][LabCreateFile3]::CloseHandle($h) }
        Line ("DEV[$tag] $d opened=$ok win32=$err")
        $rows += [pscustomobject]@{ tag = $tag; name = $d; opened = $ok; win32 = $err }
    }
    return $rows
}

$jobs = @(
    @{ svc = 'EasyAntiCheat_EOS'; file = 'EasyAntiCheat_EOS.sys'; inst = 'EasyAntiCheat_EOS Instance'; alt = '370010' },
    @{ svc = 'EasyAntiCheat';     file = 'EasyAntiCheat.sys';     inst = 'EasyAntiCheat Instance';     alt = '370020' },
    @{ svc = 'BEDaisy';           file = 'BEDaisy.sys';           inst = 'BEDaisy Instance';           alt = '370030' }
)
$results = @()
foreach ($j in $jobs) {
    $path = Join-Path 'C:\re\artifacts' $j.file
    Line ("==== MF " + $j.svc + " ====")
    $binPath = '\??\' + $path
    $cout = & sc.exe create $j.svc type= kernel start= demand binPath= $binPath DisplayName= $j.svc 2>&1 | Out-String
    Line ("CREATE=" + $cout.Trim())
    $svcKey = "HKLM:\SYSTEM\CurrentControlSet\Services\$($j.svc)"
    $instRoot = Join-Path $svcKey 'Instances'
    $instKey = Join-Path $instRoot $j.inst
    New-Item -Path $instRoot -Force | Out-Null
    New-Item -Path $instKey -Force | Out-Null
    New-ItemProperty -Path $instRoot -Name DefaultInstance -Value $j.inst -PropertyType String -Force | Out-Null
    New-ItemProperty -Path $instKey -Name Altitude -Value $j.alt -PropertyType String -Force | Out-Null
    New-ItemProperty -Path $instKey -Name Flags -Value 0 -PropertyType DWord -Force | Out-Null
    Line ("INST DefaultInstance=$($j.inst) Altitude=$($j.alt)")
    $t0 = Get-Date
    $tmpOut = Join-Path $env:TEMP ("labdyn-mf-" + $j.svc + ".out")
    $tmpErr = Join-Path $env:TEMP ("labdyn-mf-" + $j.svc + ".err")
    Remove-Item $tmpOut, $tmpErr -ErrorAction SilentlyContinue
    $proc = Start-Process -FilePath "$env:SystemRoot\System32\sc.exe" -ArgumentList @('start', $j.svc) -PassThru -WindowStyle Hidden -RedirectStandardOutput $tmpOut -RedirectStandardError $tmpErr
    if (-not $proc.WaitForExit(90000)) {
        try { $proc.Kill() } catch {}
        Line 'START_TIMEOUT_90s'
        $startOut = 'START_TIMEOUT_90s'
        $startExit = -1
    } else {
        $startExit = $proc.ExitCode
        $startOut = ((Get-Content $tmpOut -ErrorAction SilentlyContinue) + (Get-Content $tmpErr -ErrorAction SilentlyContinue)) -join "`n"
        Line ("START_EXIT=" + $startExit)
        Line $startOut
    }
    $q = & sc.exe query $j.svc 2>&1 | Out-String
    Line $q.Trim()
    Line '--- events ---'
    try {
        Get-WinEvent -FilterHashtable @{ LogName = 'System'; StartTime = $t0.AddSeconds(-8) } -ErrorAction SilentlyContinue |
            Select-Object -First 15 TimeCreated, Id, ProviderName, Message |
            ForEach-Object {
                $msg = ($_.Message -replace '\s+', ' ')
                if ($msg.Length -gt 240) { $msg = $msg.Substring(0, 240) }
                Line ("SYSEVT id=$($_.Id) src=$($_.ProviderName) $msg")
            }
    } catch {}
    try {
        Get-WinEvent -FilterHashtable @{ LogName = 'Microsoft-Windows-CodeIntegrity/Operational'; StartTime = $t0.AddSeconds(-8) } -ErrorAction SilentlyContinue |
            Select-Object -First 10 TimeCreated, Id, Message |
            ForEach-Object {
                $msg = ($_.Message -replace '\s+', ' ')
                if ($msg.Length -gt 240) { $msg = $msg.Substring(0, 240) }
                Line ("CIEVT id=$($_.Id) $msg")
            }
    } catch {}
    Line ((& fltmc.exe 2>&1 | Out-String).TrimEnd())
    $devs = Probe $j.svc
    $stop = & sc.exe stop $j.svc 2>&1 | Out-String
    Line $stop.Trim()
    Start-Sleep -Seconds 2
    $del = & sc.exe delete $j.svc 2>&1 | Out-String
    Line $del.Trim()
    Remove-Item -Path $instRoot -Recurse -Force -ErrorAction SilentlyContinue
    $results += [pscustomobject]@{
        service = $j.svc; start_exit = $startExit; start_out = $startOut.Trim(); query = $q.Trim(); devices = $devs
    }
}
Line 'MF_SCRIPT_OK'
$results | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $jsonPath -Encoding UTF8
Line ("WROTE=" + $jsonPath)
