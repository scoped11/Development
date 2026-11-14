<#
.SYNOPSIS
  VM-B safe shortlist for dispatch-hijack hosts (no IOCTL fuzz, no kernel patch).

  Run inside DriverLab-E-26200 (or any lab guest). Writes CSV under driver-lab\audit\.

.EXAMPLE
  # Guest (admin): D:\hostdev\driver-lab\scripts\probe-hijack-candidates.ps1
  .\probe-hijack-candidates.ps1 -OpenDeviceProbe
#>
param(
    [string]$OutDir = (Join-Path $PSScriptRoot '..\audit'),
    [switch]$OpenDeviceProbe
)

$ErrorActionPreference = 'Stop'
$OutDir = [System.IO.Path]::GetFullPath($OutDir)
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

$stamp = Get-Date -Format 'yyyy-MM-dd_HHmm'
$outCsv = Join-Path $OutDir "hijack_candidates_vm_$stamp.csv"
$outJson = Join-Path $OutDir "hijack_candidates_vm_$stamp.json"

$AcBlock = '(EasyAntiCheat|BattlEye|BEDaisy|BEService|ACE-GAME|ACE_GAME|EAC|iqvw|Nal|RTCore|DBK|PROCMON|dbk64|KProcessHacker|KPH)'
$xfgNeedles = @(
    '__guard_dispatch_icall_fptr',
    '_guard_xfg_dispatch_icall_nop',
    '_guard_dispatch_icall'
)

$deviceTry = @(
    '\\.\Beep', '\\.\GLOBALROOT\Device\Beep',
    '\\.\DR0', '\\.\PhysicalDrive0'
)

if ($OpenDeviceProbe) {
    Add-Type @'
using System;
using System.Runtime.InteropServices;
public class HjProbe {
  [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
  public static extern IntPtr CreateFileW(string n, uint a, uint sh, IntPtr sa, uint dis, uint f, IntPtr t);
}
'@
    $acc = [uint32]0xC0000000
    $share = [uint32]3
    $disp = [uint32]3
}

function Get-DriverPathLiteral([string]$raw) {
    if ([string]::IsNullOrWhiteSpace($raw)) { return $null }
    $p = $raw -replace '^\\?\?\?\\', ''
    if (Test-Path -LiteralPath $p) { return $p }
    return $null
}

function Test-XfgStrings([string]$path) {
    if (-not $path) { return @{ HasFptr = $false; HasNop = $false; HasAny = $false } }
    try {
        $fs = [System.IO.File]::OpenRead($path)
        try {
            $buf = New-Object byte[] 4194304
            $total = [Math]::Min($fs.Length, 64MB)
            $read = 0
            $hay = [System.Text.StringBuilder]::new()
            while ($read -lt $total) {
                $n = $fs.Read($buf, 0, [Math]::Min($buf.Length, $total - $read))
                if ($n -le 0) { break }
                $read += $n
                [void]$hay.Append([Text.Encoding]::ASCII.GetString($buf, 0, $n))
                if ($hay.Length -gt 48MB) { break }
            }
            $s = $hay.ToString()
        } finally { $fs.Dispose() }
    } catch {
        return @{ HasFptr = $false; HasNop = $false; HasAny = $false }
    }
    @{
        HasFptr = $s.Contains('__guard_dispatch_icall_fptr')
        HasNop  = $s.Contains('_guard_xfg_dispatch_icall_nop')
        HasAny  = ($xfgNeedles | Where-Object { $s.Contains($_) }).Count -gt 0
    }
}

$build = (Get-ItemProperty 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion')
$guestBuild = "$($build.CurrentBuildNumber).$($build.UBR)"

$csvTmp = Join-Path $env:TEMP "driverquery_hijack.csv"
& driverquery.exe /v /fo csv | Out-File -FilePath $csvTmp -Encoding utf8
$rows = Import-Csv $csvTmp

$openedDevices = @{}
if ($OpenDeviceProbe) {
    foreach ($d in $deviceTry) {
        $h = [HjProbe]::CreateFileW($d, $acc, $share, [IntPtr]::Zero, $disp, 0, [IntPtr]::Zero)
        if ($h.ToInt64() -ne -1) {
            $openedDevices[$d] = 'open'
        }
    }
}

$candidates = foreach ($d in $rows) {
    $path = Get-DriverPathLiteral $d.Path
    $fn = if ($path) { [System.IO.Path]::GetFileName($path) } else { $d.'Display Name' }
    if ($fn -match $AcBlock -or $d.'Display Name' -match $AcBlock) { continue }
    if ($d.State -ne 'Running') { continue }

    $signer = 'unknown'
    if ($path) {
        try {
            $sig = Get-AuthenticodeSignature -LiteralPath $path
            if ($sig.SignerCertificate) { $signer = $sig.SignerCertificate.Subject }
        } catch { }
    }

    $xfg = if ($path) { Test-XfgStrings $path } else { @{ HasFptr = $false; HasNop = $false; HasAny = $false } }

    $score = 0
    if ($d.State -eq 'Running') { $score += 10 }
    if ($signer -match 'Microsoft Corporation') { $score += 5 }
    if ($xfg.HasFptr -and $xfg.HasNop) { $score += 15 }
    elseif ($xfg.HasAny) { $score += 5 }
    if ($path -and (Get-Item -LiteralPath $path).Length -gt 200KB) { $score += 3 }

    [pscustomobject]@{
        Module       = $fn
        DisplayName  = $d.'Display Name'
        StartMode    = $d.'Start Mode'
        Path         = $path
        Signer       = $signer
        XfgFptr      = $xfg.HasFptr
        XfgNop       = $xfg.HasNop
        Score        = $score
        VmProfile    = 'VM-B'
        GuestBuild   = $guestBuild
        Note         = 'IDA: RVAs for fptr/nop + openable \\.\ name required for H2'
    }
}

$candidates | Sort-Object Score, Module -Descending | Export-Csv $outCsv -NoTypeInformation

$payload = @{
    guest_build     = $guestBuild
    captured_utc    = (Get-Date).ToUniversalTime().ToString('o')
    open_device_try = $openedDevices
    top5            = @($candidates | Sort-Object Score -Descending | Select-Object -First 5)
    csv             = $outCsv
    doc             = 'driver-lab/docs/25_hijack_host_vm.md'
}
$payload | ConvertTo-Json -Depth 5 | Set-Content $outJson -Encoding UTF8

Write-Host "[+] Guest build: $guestBuild"
Write-Host "[+] Candidates: $($candidates.Count) -> $outCsv"
if ($OpenDeviceProbe) {
    Write-Host "[+] Device probe: $($openedDevices.Count) opens (expand list after first pass)"
}
Write-Host "Next: IDA on top modules via shared folder; H2 patch only inside this VM."
