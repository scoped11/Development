#Requires -Version 5.1
<#
.SYNOPSIS
  Locate a BYOVD vehicle .sys already on disk and copy it into vehicles\ + bin\.
  Does NOT download blocklisted drivers from the internet.

  Legitimate sources (install these yourself, then re-run):
    RTCore64.sys        — MSI Afterburner / MSI Center
    CorsairLLAccess64   — Corsair iCUE / LINK
    ThrottleStop.sys    — ThrottleStop (techpowerup)
    TRIXX.sys           — Sapphire TRIXX
    SIVX64.sys          — Gigabyte SIV
    NalDrvIntel.sys     — Intel network/driver packages (often blocklisted on AC hosts)
#>
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$Bin = Join-Path (Split-Path -Parent $Root) "bin"
New-Item -ItemType Directory -Force -Path $Root, $Bin | Out-Null

$catalog = @(
    @{ Name = "RTCore64.sys";           Driver = "rtcore";  Device = "\\.\RTCore64" },
    @{ Name = "CorsairLLAccess64.sys";  Driver = "corsair"; Device = "\\.\CorsairLLAccess" },
    @{ Name = "ThrottleStop.sys";       Driver = "ts";      Device = "ThrottleStop" },
    @{ Name = "TRIXX.sys";              Driver = "trixx";   Device = "TRIXX" },
    @{ Name = "SIVX64.sys";             Driver = "siv";     Device = "SIV" },
    @{ Name = "NalDrvIntel.sys";        Driver = "nal";     Device = "\\.\Nal" },
    @{ Name = "iqvw64e.sys";            Driver = "nal";     Device = "\\.\Nal" },
    @{ Name = "NalDrv.sys";             Driver = "nal";     Device = "\\.\Nal" }
)

$searchRoots = @(
    ${env:ProgramFiles},
    ${env:ProgramFiles(x86)},
    ${env:LOCALAPPDATA},
    ${env:ProgramData},
    "C:\Windows\System32\drivers",
    (Join-Path $env:USERPROFILE "Downloads"),
    (Join-Path $env:USERPROFILE "OneDrive\Desktop")
) | Where-Object { $_ -and (Test-Path $_) }

$found = @{}
foreach ($item in $catalog) {
    $n = $item.Name
    if ($found.ContainsKey($n)) { continue }
    foreach ($root in $searchRoots) {
        $hits = Get-ChildItem -Path $root -Filter $n -Recurse -ErrorAction SilentlyContinue -Depth 12
        foreach ($h in $hits) {
            if ($h.Length -lt 1024) { continue }
            $found[$n] = @{ Path = $h.FullName; Meta = $item }
            break
        }
        if ($found.ContainsKey($n)) { break }
    }
}

if ($found.Count -eq 0) {
    Write-Host "No vehicle .sys found on this machine." -ForegroundColor Yellow
    Write-Host ""
    Write-Host "Install one of these (lab VM recommended), then run this script again:"
    Write-Host "  MSI Afterburner  -> RTCore64.sys   (mapper: --driver rtcore)"
    Write-Host "  Corsair iCUE     -> CorsairLLAccess64.sys  (--driver corsair)"
    Write-Host "  ThrottleStop     -> ThrottleStop.sys (--driver ts)"
    Write-Host ""
    Write-Host "I cannot download Nal/iqvw blocklisted drivers into the repo for you."
    exit 2
}

Write-Host "Found vehicles:"
$i = 0
$keys = @($found.Keys | Sort-Object)
foreach ($k in $keys) {
    $i++
    $m = $found[$k].Meta
    Write-Host ("  [{0}] {1,-28} --driver {2}  ({3})" -f $i, $k, $m.Driver, $found[$k].Path)
}

$choice = if ($args[0]) { [int]$args[0] } else { 1 }
if ($choice -lt 1 -or $choice -gt $keys.Count) { $choice = 1 }
$pick = $keys[$choice - 1]
$src = $found[$pick].Path
$meta = $found[$pick].Meta
$dstVehicle = Join-Path $Root $pick
$dstBin = Join-Path $Bin $pick

Copy-Item -LiteralPath $src -Destination $dstVehicle -Force
$binOk = $true
try {
    Copy-Item -LiteralPath $src -Destination $dstBin -Force -ErrorAction Stop
} catch {
    $binOk = $false
    $tmpDst = Join-Path $Bin ($pick + ".fetched")
    Copy-Item -LiteralPath $src -Destination $tmpDst -Force -ErrorAction SilentlyContinue
    Write-Host "[!] bin\$pick is locked (mapper still uses vehicles\ or copies to %TEMP% on map)" -ForegroundColor Yellow
}

$manifest = @{
    file = $pick
    source_path = $src
    mapper_flag = $meta.Driver
    device = $meta.Device
    fetched_utc = (Get-Date).ToUniversalTime().ToString("o")
} | ConvertTo-Json

Set-Content -Path (Join-Path $Root "vehicle.manifest.json") -Value $manifest -Encoding UTF8

Write-Host ""
Write-Host "Copied to:"
Write-Host "  $dstVehicle"
if ($binOk) { Write-Host "  $dstBin" } else { Write-Host "  (bin skipped — file in use)" }
Write-Host ""
Write-Host "Map RwTest (elevated):"
Write-Host "  cd .."
Write-Host "  .\map-secure-rwtest.ps1"
Write-Host "  # or: bin\mapper_nal.exe bin\RwTest.sys --driver $($meta.Driver) --no-auth --rw-only"
exit 0
