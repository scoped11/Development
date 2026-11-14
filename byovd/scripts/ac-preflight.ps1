#Requires -RunAsAdministrator
# Tier 0/1 gate: AC running + vehicle device not openable => blocked (exit 2)
param(
    [string]$Driver = "rtcore"
)

$ErrorActionPreference = "SilentlyContinue"
$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)

$devices = @{
    rtcore  = "\\.\RTCore64"
    corsair = "\\.\CorsairLLAccess"
    nal     = "\\.\Nal"
    trixx   = "\\.\TrixxSvc"
    ts      = "\\.\ThrottleStop"
    siv     = "\\.\SIVDRIVER"
}
$dev = $devices[$Driver.ToLower()]
if (-not $dev) {
    Write-Error "Unknown -Driver '$Driver' (rtcore, corsair, nal, trixx, ts, siv)"
    exit 1
}

function Test-AcProcess {
    $patterns = 'vgc|vgtray|EasyAntiCheat|eac_|BEService|BEClient|bedaisy|ACE|AceService|acesvc|tpwinace'
    Get-Process -ErrorAction SilentlyContinue | Where-Object { $_.ProcessName -match $patterns }
}

Write-Host "[*] AC block/hide preflight (docs/11_ac_block_vs_hide_plan.md)"
$ver = Get-ItemProperty "HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion"
Write-Host ("    OS: {0} build {1}.{2}" -f $ver.ProductName, $ver.CurrentBuild, $ver.UBR)

$hvci = Get-ItemProperty "HKLM:\SYSTEM\CurrentControlSet\Control\DeviceGuard\Scenarios\HypervisorEnforcedCodeIntegrity" -ErrorAction SilentlyContinue
$bl = Get-ItemProperty "HKLM:\SYSTEM\CurrentControlSet\Control\CI\Config" -ErrorAction SilentlyContinue
Write-Host ("    HVCI Enabled: {0}" -f $(if ($hvci) { $hvci.Enabled } else { "?" }))
Write-Host ("    VulnerableDriverBlocklistEnable: {0}" -f $(if ($bl) { $bl.VulnerableDriverBlocklistEnable } else { "?" }))

$acProcs = @(Test-AcProcess)
if ($acProcs.Count -gt 0) {
    Write-Host ("[!] AC-related processes: {0}" -f ($acProcs.ProcessName -join ", "))
} else {
    Write-Host "[+] No known AC usermode processes"
}

$open = $false
if (-not ("DevIo" -as [type])) {
Add-Type @"
using System;
using System.Runtime.InteropServices;
using Microsoft.Win32.SafeHandles;
public class DevIo {
  [DllImport("kernel32.dll", SetLastError=true, CharSet=CharSet.Auto)]
  public static extern SafeFileHandle CreateFile(string lpFileName, uint dwDesiredAccess,
    uint dwShareMode, IntPtr lpSecurityAttributes, uint dwCreationDisposition,
    uint dwFlagsAndAttributes, IntPtr hTemplateFile);
}
"@
}
$path = $dev
$handle = [DevIo]::CreateFile($path, 0xC0000000, 3, [IntPtr]::Zero, 3, 0, [IntPtr]::Zero)
if ($handle -and -not $handle.IsInvalid) {
    $open = $true
    $handle.Close()
    Write-Host "[+] Vehicle device open OK: $dev"
} else {
    Write-Host "[!] Vehicle device NOT open: $dev (err=$([Runtime.InteropServices.Marshal]::GetLastWin32Error()))"
}

if (-not $open -and $acProcs.Count -gt 0) {
    Write-Host ""
    Write-Host "[!] Tier 0: vehicle blocked or unavailable while AC is running."
    Write-Host "    Map on a VM snapshot without game AC, or stop AC client (lab only)."
    Write-Host "    See docs/11_ac_block_vs_hide_plan.md"
    exit 2
}
if (-not $open) {
    Write-Host "[*] Tier 1 prep: start vehicle service / copy .sys to bin, then retry."
    exit 1
}
Write-Host "[+] Tier 1: safe to attempt --ac-safe map (still not live-game undetected)."
exit 0
