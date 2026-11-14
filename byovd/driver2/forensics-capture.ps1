# Snap Lab I / pf_diag artifacts after a driver2 map (no mapper call).
param(
    [string]$Profile = 'rust',
    [string]$OutDir = ''
)

$ErrorActionPreference = 'Stop'
$Driver2 = $PSScriptRoot
if (-not $OutDir) {
    $OutDir = Join-Path $Driver2 "forensics\$(Get-Date -Format 'yyyy-MM-dd_HHmmss')_$Profile"
}
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

$pf = Join-Path $env:TEMP 'pf_diag.tmp'
if (Test-Path $pf) { Copy-Item $pf (Join-Path $OutDir 'pf_diag.tmp') -Force }

$diag = $null
$verify = Join-Path (Split-Path $Driver2 -Parent) 'scripts\verify-pf-diag.ps1'
if (Test-Path $verify) {
    $diag = & $verify -Profile $Profile
}

$hvci = $false
try {
    $dg = Get-CimInstance -ClassName Win32_DeviceGuard -ErrorAction SilentlyContinue
    if ($dg -and $dg.SecurityServicesRunning) { $hvci = $dg.SecurityServicesRunning -contains 1 }
} catch {}

$bl = $false
try {
    $ci = Get-ItemProperty 'HKLM:\SYSTEM\CurrentControlSet\Control\CI\Config' -ErrorAction SilentlyContinue
    if ($ci.VulnerableDriverBlocklistEnable -eq 1) { $bl = $true }
} catch {}

@{
    schema       = 'driver2_forensics_v1'
    captured_utc = (Get-Date).ToUniversalTime().ToString('o')
    hostname     = $env:COMPUTERNAME
    profile      = $Profile
    hvci_on      = $hvci
    blocklist_on = $bl
    pf_diag      = $diag
    pf_path      = $(if (Test-Path $pf) { $pf } else { $null })
    checklist    = @{
        hygiene_in_log = [bool]((Test-Path $pf) -and ((Get-Content $pf -Raw) -match 'Hygiene:|PiDDB|WdFilter'))
        strip          = $diag.strip
        max869         = $diag.max869
        mode           = $diag.mode
    }
} | ConvertTo-Json -Depth 6 | Set-Content (Join-Path $OutDir 'snapshot.json') -Encoding UTF8

Write-Host "[+] Forensics -> $OutDir"
return $OutDir
