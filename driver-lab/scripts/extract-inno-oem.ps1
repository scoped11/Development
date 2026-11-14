# Unpack Inno-based OEM installers (ASRock / some RGB) into oem-extract/*-innounp
param(
    [string[]]$Only = @()
)

$innounp = @(
    "${env:LOCALAPPDATA}\Microsoft\WinGet\Packages\JurgenRathlev.innounp_Microsoft.Winget.Source_8wekyb3d8bbwe\innounp.exe",
    (Get-Command innounp -ErrorAction SilentlyContinue).Source
) | Where-Object { $_ -and (Test-Path $_) } | Select-Object -First 1

if (-not $innounp) {
    Write-Warning 'innounp missing — winget install JurgenRathlev.innounp'
    exit 2
}

$7z = @('C:\Program Files\7-Zip\7z.exe', 'C:\Program Files (x86)\7-Zip\7z.exe') | Where-Object { Test-Path $_ } | Select-Object -First 1
$raw = Join-Path $PSScriptRoot '..\audit\inbox\oem-raw'
$extract = Join-Path $PSScriptRoot '..\audit\inbox\oem-extract'

$targets = @(
    @{ Zip = 'PolychromeRGB_v2.0.208.zip'; Tag = 'poly208-innounp' }
    @{ Zip = 'PolychromeRGB_v2.0.190.zip'; Tag = 'poly190-innounp' }
    @{ Zip = 'MotherboardUtility_v4.1.11.zip'; Tag = 'mbu-innounp' }
    @{ Zip = 'F-Stream_v2.0.188.zip'; Tag = 'fstream-innounp' }
)

foreach ($t in $targets) {
    if ($Only.Count -gt 0 -and ($Only -notcontains $t.Zip)) { continue }
    $zip = Join-Path $raw $t.Zip
    if (-not (Test-Path $zip)) {
        Write-Warning "missing $zip"
        continue
    }
    $flat = Join-Path $extract ("flat-" + ($t.Zip -replace '\.zip$', ''))
    $out = Join-Path $extract $t.Tag
    New-Item -ItemType Directory -Force -Path $flat, $out | Out-Null
    if ($7z) { & $7z x -y "-o$flat" $zip | Out-Null }
    $setup = Get-ChildItem $flat -Recurse -Filter *.exe -ErrorAction SilentlyContinue |
        Sort-Object Length -Descending | Select-Object -First 1
    if (-not $setup) {
        Write-Warning "no setup exe in $($t.Zip)"
        continue
    }
    Write-Host "[*] innounp $($t.Tag) <- $($setup.Name)"
    & $innounp -x -d"$out" $setup.FullName -y | Out-Null
    Get-ChildItem $out -Recurse -Filter *.sys -ErrorAction SilentlyContinue | ForEach-Object {
        Write-Host "    [sys] $($_.Name)"
    }
}

Write-Host "[+] done — run extract-oem-to-inbox.ps1 -RefreshHeuristics"
