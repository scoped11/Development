<#
.SYNOPSIS
  Method 18 (BFD): AC process snapshot + vehicle device open matrix + CI .sys events.
  Does NOT install, start, or copy kernel drivers.

.EXAMPLE
  .\block-fingerprint-delta.ps1
  .\block-fingerprint-delta.ps1 -WaitForAcSec 120   # poll until AC process or timeout
#>
param(
    [string]$OutJson = "",
    [int]$WaitForAcSec = 0,
    [string[]]$Devices = @(
        "\\.\RTCore64", "\\.\Nal", "\\.\CorsairLLAccess",
        "\\.\TrixxSvc", "\\.\ThrottleStop", "\\.\SIVDRIVER",
        "\\.\GIO", "\\.\ASUSGIO", "\\.\GLOBALROOT\Device\Nal"
    ),
    [int]$CiDays = 365
)

$ErrorActionPreference = "Stop"
$OutDir = Join-Path $PSScriptRoot "..\audit"
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
if (-not $OutJson) {
    $OutJson = Join-Path $OutDir ("bfd_{0:yyyy-MM-dd_HHmmss}.json" -f (Get-Date))
}

$acPatterns = 'EasyAntiCheat|EasyAntiCheat_EOS|eac_|BEService|BEClient|bedaisy|ACE|AceService|acesvc|tpwinace|vgtray|vgc'

function Get-AcProcesses {
    Get-Process -ErrorAction SilentlyContinue |
        Where-Object { $_.ProcessName -match $acPatterns } |
        Select-Object ProcessName, Id
}

function Wait-AcPresence([int]$maxSec) {
    $deadline = (Get-Date).AddSeconds($maxSec)
    while ((Get-Date) -lt $deadline) {
        $p = @(Get-AcProcesses)
        if ($p.Count -gt 0) { return $p }
        Start-Sleep -Seconds 2
    }
    return @()
}

if (-not ("BfdDevIo" -as [type])) {
    Add-Type @"
using System;
using System.Runtime.InteropServices;
using Microsoft.Win32.SafeHandles;
public static class BfdDevIo {
  [DllImport("kernel32.dll", SetLastError=true, CharSet=CharSet.Auto)]
  public static extern SafeFileHandle CreateFile(string lpFileName, uint dwDesiredAccess,
    uint dwShareMode, IntPtr lpSecurityAttributes, uint dwCreationDisposition,
    uint dwFlagsAndAttributes, IntPtr hTemplateFile);
  [DllImport("kernel32.dll")]
  public static extern bool CloseHandle(IntPtr h);
}
"@
}

function Test-DeviceOpen([string]$path) {
    $access = [uint32]3221225472   # GENERIC_READ | GENERIC_WRITE
    $h = [BfdDevIo]::CreateFile($path, $access, 3, [IntPtr]::Zero, 3, 0, [IntPtr]::Zero)
    $err = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
  $ok = $h -and -not $h.IsInvalid
    if ($ok) {
        [BfdDevIo]::CloseHandle($h.DangerousGetHandle()) | Out-Null
        $h.Dispose()
    }
    [pscustomobject]@{
        Device     = $path
        Opened     = $ok
        Win32Error = if ($ok) { 0 } else { $err }
    }
}

$acProcs = @(Get-AcProcesses)
if ($WaitForAcSec -gt 0 -and $acProcs.Count -eq 0) {
    Write-Host "[*] Waiting up to $WaitForAcSec s for AC process..."
    $acProcs = @(Wait-AcPresence $WaitForAcSec)
}

$deviceRows = foreach ($d in $Devices) { Test-DeviceOpen $d }

$ciEvents = @()
try {
    $start = (Get-Date).AddDays(-1 * $CiDays)
    $raw = Get-WinEvent -FilterHashtable @{
        LogName   = 'Microsoft-Windows-CodeIntegrity/Operational'
        StartTime = $start
    } -MaxEvents 2000 -ErrorAction Stop
    foreach ($ev in $raw) {
        if ($ev.Message -notmatch '\.sys') { continue }
        if ($ev.Id -notin 3087, 3033, 3004, 3099, 3100) { continue }
        $ciEvents += [pscustomobject]@{
            TimeCreated = $ev.TimeCreated.ToString("o")
            Id          = $ev.Id
            Message     = if ($ev.Message.Length -gt 500) { $ev.Message.Substring(0, 500) } else { $ev.Message }
        }
    }
} catch {
    Write-Warning "CI log read failed: $($_.Exception.Message)"
}

$hvci = $null
$bl = $null
try { $hvci = (Get-ItemProperty "HKLM:\SYSTEM\CurrentControlSet\Control\DeviceGuard\Scenarios\HypervisorEnforcedCodeIntegrity").Enabled } catch {}
try { $bl = (Get-ItemProperty "HKLM:\SYSTEM\CurrentControlSet\Control\CI\Config").VulnerableDriverBlocklistEnable } catch {}
$ver = Get-ItemProperty "HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion"

$tier0 = @()
foreach ($row in $deviceRows) {
    if ($row.Win32Error -eq 5 -and $acProcs.Count -gt 0) {
        $tier0 += $row.Device
    }
}

$doc = [ordered]@{
    schema       = "bfd_v1"
    captured_utc = (Get-Date).ToUniversalTime().ToString("o")
    host         = [ordered]@{
        product   = $ver.ProductName
        build     = "$($ver.CurrentBuild).$($ver.UBR)"
        hvci      = $hvci
        vuln_bl   = $bl
    }
    ac_seen      = ($acProcs.Count -gt 0)
    ac_processes = @($acProcs | ForEach-Object { "$($_.ProcessName):$($_.Id)" })
    device_probe = $deviceRows
    tier0_blocked_devices = $tier0
    ci_sys_events = $ciEvents
    method_doc    = "byovd/docs/18_block_fingerprint_delta.md"
}

$json = $doc | ConvertTo-Json -Depth 6
Set-Content -LiteralPath $OutJson -Value $json -Encoding utf8

Write-Host "[+] BFD written: $OutJson"
Write-Host ("    AC seen: {0} ({1} procs)" -f $doc.ac_seen, $acProcs.Count)
if ($tier0.Count -gt 0) {
    Write-Host "    Tier-0 fingerprint (AC + access denied):" -ForegroundColor Yellow
    $tier0 | ForEach-Object { Write-Host "      $_" }
} else {
    Write-Host "    No Tier-0 device fingerprint this capture."
}
Write-Host ("    CI .sys events (filtered): {0}" -f $ciEvents.Count)
