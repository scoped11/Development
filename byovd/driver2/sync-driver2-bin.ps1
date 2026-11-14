# Mirror shared BYOVD stack into driver2\bin (canonical launcher directory).
$ErrorActionPreference = 'Stop'
$Driver2 = $PSScriptRoot
$Byovd = Split-Path $Driver2 -Parent
$D2Bin = Join-Path $Driver2 'bin'
$ByovdBin = Join-Path $Byovd 'bin'
$Vehicles = Join-Path $Byovd 'vehicles'
New-Item -ItemType Directory -Force -Path $D2Bin, (Join-Path $D2Bin 'games'), (Join-Path $D2Bin 'brand') | Out-Null

$fromByovd = @(
    'mapper_nal.exe', 'RwTest.sys', 'Betest.sys', 'process_rw.exe',
    'BUILD.txt', 'BUILT.json', 'stomp_plan.exe'
)
foreach ($f in $fromByovd) {
    $src = Join-Path $ByovdBin $f
    if (Test-Path $src) {
        Copy-Item $src (Join-Path $D2Bin $f) -Force
    }
}

foreach ($v in 'RTCore64.sys', 'CorsairLLAccess64.sys', 'ThrottleStop.sys', 'TRIXX.sys', 'SIVX64.sys') {
    $src = Join-Path $Vehicles $v
    if (-not (Test-Path $src)) { $src = Join-Path $ByovdBin $v }
    if (Test-Path $src) {
        try { Copy-Item $src (Join-Path $D2Bin $v) -Force -ErrorAction Stop } catch {}
    }
}

Write-Host "[+] driver2\bin synced from byovd (mapper + vehicles mirror)"
