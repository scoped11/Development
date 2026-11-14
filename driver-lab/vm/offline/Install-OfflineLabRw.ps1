# Lane E (offline): staged vault copy + ntdll NtLoadDriver — no sc.exe, no System32\drivers.
# Lab VM only. Requires signed VmLabDrv.sys (Sign-LabDriver.ps1).
#Requires -RunAsAdministrator
param(
    [switch]$Uninstall,
    [string]$ShareRoot = 'Z:\driver-lab\vm'
)

$ErrorActionPreference = 'Stop'
$VmRoot = if (Test-Path $ShareRoot) { $ShareRoot } else { Split-Path -Parent $PSScriptRoot }
$Vault = 'C:\DriverLab\vault\rw'
$Log = Join-Path $Vault 'install-offline-lab-rw.log'
New-Item -ItemType Directory -Force -Path $Vault | Out-Null

function L([string]$m) {
    $line = "$(Get-Date -Format o) $m"
    Add-Content -Path $Log -Value $line
    Write-Host $line
}

. (Join-Path $VmRoot 'bg\guest-lab-privilege.ps1')

$SvcLeaf = 'DlvRw' + ([BitConverter]::ToString([guid]::NewGuid().ToByteArray()).Replace('-', '').Substring(0, 8))
$RegSvc = "HKLM:\SYSTEM\CurrentControlSet\Services\$SvcLeaf"

if ($Uninstall) {
    if (Test-Path $RegSvc) {
        Enable-LabPrivilege -Name 'SeLoadDriverPrivilege' | Out-Null
        $uPath = "\Registry\Machine\System\CurrentControlSet\Services\$SvcLeaf"
        if (-not ('NtDrv' -as [type])) {
            Add-Type @'
using System; using System.Runtime.InteropServices;
[StructLayout(LayoutKind.Sequential, CharSet=CharSet.Unicode)]
public struct UnicodeStr {
  public ushort Length; public ushort Max; public IntPtr Buffer;
}
public static class NtDrv {
  [DllImport("ntdll.dll", CharSet=CharSet.Unicode)]
  public static extern int NtUnloadDriver(ref UnicodeStr name);
  public static int Unload(string p) {
    var u = Marshal.StringToHGlobalUni(p);
    var s = new UnicodeStr { Buffer = u, Length = (ushort)(p.Length*2), Max = (ushort)(p.Length*2+2) };
    int st = NtUnloadDriver(ref s);
    Marshal.FreeHGlobal(u);
    return st;
  }
}
'@
        }
        $st = [NtDrv]::Unload($uPath)
        L "NtUnloadDriver $SvcLeaf st=0x$($st.ToString('x'))"
        Remove-Item -Path $RegSvc -Recurse -Force
    }
    exit 0
}

$staged = Join-Path $VmRoot 'vault\staged\VmLabDrv.signed.sys'
$srcSys = if (Test-Path $staged) { $staged } else { Join-Path $VmRoot 'bin\VmLabDrv.sys' }
if (-not (Test-Path $srcSys)) { throw "Missing driver — host runs offline\host-prepare-offline-lab.ps1 first" }

$dstSys = Join-Path $Vault 'VmLabDrv.sys'
Copy-Item $srcSys $dstSys -Force
L 'using staged signed image (guest does not sign)'

$imp = Join-Path $PSScriptRoot 'Import-LabPublisher.ps1'
if (Test-Path $imp) { & $imp }

Enable-LabPrivilegeSet | Out-Null

New-Item -Path $RegSvc -Force | Out-Null
New-ItemProperty -Path $RegSvc -Name 'ImagePath' -Value "\??\$dstSys" -PropertyType String -Force | Out-Null
New-ItemProperty -Path $RegSvc -Name 'Type' -Value 1 -PropertyType DWord -Force | Out-Null
New-ItemProperty -Path $RegSvc -Name 'ErrorControl' -Value 1 -PropertyType DWord -Force | Out-Null
New-ItemProperty -Path $RegSvc -Name 'Start' -Value 3 -PropertyType DWord -Force | Out-Null

if (-not ('NtDrv' -as [type])) {
    Add-Type @'
using System; using System.Runtime.InteropServices;
[StructLayout(LayoutKind.Sequential, CharSet=CharSet.Unicode)]
public struct UnicodeStr {
  public ushort Length; public ushort Max; public IntPtr Buffer;
}
public static class NtDrv {
  [DllImport("ntdll.dll", CharSet=CharSet.Unicode)]
  public static extern int NtLoadDriver(ref UnicodeStr name);
  [DllImport("ntdll.dll", CharSet=CharSet.Unicode)]
  public static extern int NtUnloadDriver(ref UnicodeStr name);
  public static int Load(string p) {
    var u = Marshal.StringToHGlobalUni(p);
    var s = new UnicodeStr { Buffer = u, Length = (ushort)(p.Length*2), Max = (ushort)(p.Length*2+2) };
    int st = NtLoadDriver(ref s);
    Marshal.FreeHGlobal(u);
    return st;
  }
}
'@
}

$ntPath = "\Registry\Machine\System\CurrentControlSet\Services\$SvcLeaf"
. (Join-Path $PSScriptRoot 'Set-LabProductionBcd.ps1')
Set-LabProductionBcd -Log $Log

$manifestMode = 'offline_ntload_vault'
$st = [NtDrv]::Load($ntPath)
L "NtLoadDriver $SvcLeaf st=0x$($st.ToString('x'))"
if ($st -eq -0x3ffffdbb -or $st -eq 0xC0000428 -or $st -eq -1073740760) {
    L 'NtLoad hash/policy fail — try sc start (trusted publisher, testsigning off)'
    $sysDst = Join-Path $env:SystemRoot 'System32\drivers\VmLabDrv.sys'
    Copy-Item $dstSys $sysDst -Force
    $sc = 'VmLabDrv'
    sc.exe query $sc 2>$null | Out-Null
    if ($LASTEXITCODE -eq 0) {
        sc.exe stop $sc 2>$null | Out-Null
        sc.exe delete $sc 2>$null | Out-Null
        Start-Sleep -Seconds 1
    }
    sc.exe create $sc type= kernel start= demand binPath= 'System32\drivers\VmLabDrv.sys' | Out-Null
    sc.exe start $sc 2>&1 | ForEach-Object { L $_ }
    if ($LASTEXITCODE -eq 0) {
        L 'sc start VmLabDrv OK (production trust path)'
        $st = 0
        $manifestMode = 'offline_sc_trusted_v1'
    } else {
        L "sc start failed ec=$LASTEXITCODE"
        Start-Sleep -Seconds 1
        $st = [NtDrv]::Load($ntPath)
        L "NtLoadDriver retry st=0x$($st.ToString('x'))"
    }
} else {
    $manifestMode = 'offline_ntload_vault'
}
if (-not $manifestMode) { $manifestMode = 'offline_ntload_vault' }

$cliStaged = Join-Path $VmRoot 'vault\staged\VmLabClient.exe'
$cli = if (Test-Path $cliStaged) { $cliStaged } else { Join-Path $VmRoot 'bin\VmLabClient.exe' }
$rwEc = $null
if ($st -eq 0 -and (Test-Path $cli)) {
    & $cli
    $rwEc = $LASTEXITCODE
    L "VmLabClient exit=$rwEc"
}

$manifest = [ordered]@{
    schema     = 'offline_vault_ntload_v1'
    utc        = (Get-Date).ToUniversalTime().ToString('o')
    service    = $SvcLeaf
    image      = $dstSys
    ntload_st  = "0x$($st.ToString('x'))"
    client_ec  = $rwEc
    method     = $manifestMode
    testsigning = ((bcdedit /enum '{current}' 2>$null | Select-String 'testsigning') -match 'Yes')
}
$out = Join-Path $Vault 'offline-lab-rw.json'
($manifest | ConvertTo-Json -Depth 5) | Set-Content $out -Encoding UTF8
Copy-Item $out (Join-Path $VmRoot 'bg\offline-lab-rw.json') -Force -ErrorAction SilentlyContinue

if ($st -ne 0) {
    if ($st -eq -0x3ffffdbb) { L 'STATUS_INVALID_IMAGE_HASH — re-run host-prepare-offline-lab.ps1; enable lab BCD policy if required' }
    exit $st
}
if ($rwEc -ne 0 -and $null -ne $rwEc) { exit $rwEc }
exit 0