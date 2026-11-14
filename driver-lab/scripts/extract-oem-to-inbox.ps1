# Pull .sys from official packages under audit/inbox/oem-raw into oem-sys/x64 + heuristics CSV.
param(
    [switch]$RefreshHeuristics
)

$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$raw = Join-Path $root 'audit\inbox\oem-raw'
$extract = Join-Path $root 'audit\inbox\oem-extract'
$x64 = Join-Path $root 'audit\inbox\oem-sys\x64'
$7z = @('C:\Program Files\7-Zip\7z.exe', 'C:\Program Files (x86)\7-Zip\7z.exe') | Where-Object { Test-Path $_ } | Select-Object -First 1

New-Item -ItemType Directory -Force -Path $x64, $extract | Out-Null

function Copy-SysToX64([string]$src, [string]$note) {
    $name = Split-Path $src -Leaf
    $dst = Join-Path $x64 $name
    Copy-Item -LiteralPath $src -Destination $dst -Force
    $hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $dst).Hash.ToLower()
    [pscustomobject]@{ File = $name; SHA256 = $hash; Note = $note }
}

$manifest = [System.Collections.Generic.List[object]]::new()

# Already unpacked trees (iCUE 3, NZXT, SteelSeries, PawnIO)
$known = @(
    @{ Glob = 'oem-extract\icue3\**\CorsairLLAccess64.sys'; Note = 'Corsair iCUE 3 MSI' }
    @{ Glob = 'oem-extract\nzxt\**\cam_driver_mb.sys'; Note = 'NZXT CAM' }
    @{ Glob = 'oem-extract\pawnio-all\PawnIO.sys'; Note = 'PawnIO setup' }
)
foreach ($k in $known) {
    $g = Join-Path (Join-Path $root 'audit\inbox') ($k.Glob -replace '^oem-extract\\', 'oem-extract\')
    Get-ChildItem -Path (Split-Path $g -Parent) -Recurse -Filter (Split-Path $g -Leaf) -ErrorAction SilentlyContinue |
        ForEach-Object { $manifest.Add((Copy-SysToX64 $_.FullName $k.Note)) }
}

# x64 folder already populated — re-hash everything present
Get-ChildItem $x64 -Filter *.sys -ErrorAction SilentlyContinue | ForEach-Object {
    if (-not ($manifest.File -contains $_.Name)) {
        $manifest.Add([pscustomobject]@{
                File   = $_.Name
                SHA256 = (Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLower()
                Note   = 'inbox/x64 (pre-staged)'
            })
    }
}

if ($7z) {
    # ViGEm — nested NSIS → CAB → ViGEmBus.sys
    $vigemOut = Join-Path $extract 'vigem-full'
    New-Item -ItemType Directory -Force -Path $vigemOut | Out-Null
  $vigemExe = Join-Path $raw 'ViGEmBus_1.22.0.exe'
    if (Test-Path $vigemExe) {
        & $7z x -y -o$vigemOut $vigemExe | Out-Null
        Get-ChildItem $vigemOut -Recurse -Filter *.sys -ErrorAction SilentlyContinue | ForEach-Object {
            $manifest.Add((Copy-SysToX64 $_.FullName 'ViGEmBus installer'))
        }
    }
    # Voicemeeter outer zip → setup → inner
    $vmZip = Join-Path $raw 'VoicemeeterSetup_v1122.zip'
    if (Test-Path $vmZip) {
        $vmOut = Join-Path $extract 'voicemeeter-full'
        Expand-Archive -Path $vmZip -DestinationPath $vmOut -Force
        Get-ChildItem $vmOut -Recurse -Filter *.exe | ForEach-Object {
            $inner = Join-Path $vmOut ("inner-" + $_.BaseName)
            New-Item -ItemType Directory -Force -Path $inner | Out-Null
            & $7z x -y -o$inner $_.FullName | Out-Null
            Get-ChildItem $inner -Recurse -Filter *.sys -ErrorAction SilentlyContinue | ForEach-Object {
                $manifest.Add((Copy-SysToX64 $_.FullName 'Voicemeeter setup'))
            }
        }
    }
    # SteelSeries / ROCcat NSIS if present
    foreach ($pair in @(
            @{ Exe = 'SteelSeriesEngine3.22.1Setup.exe'; Note = 'SteelSeries Engine 3' }
            @{ Exe = 'PawnIO_setup.exe'; Note = 'PawnIO setup' }
            @{ Exe = 'RazerSynapse4Installer.exe'; Note = 'Razer Synapse 4' }
            @{ Exe = 'NZXT_CAM_Setup_4.76.6.exe'; Note = 'NZXT CAM' }
            @{ Exe = 'ViperRGB_setup.exe'; Note = 'Patriot Viper RGB' }
        )) {
        $p = Join-Path $raw $pair.Exe
        if (-not (Test-Path $p)) { continue }
        $out = Join-Path $extract ("unpack-" + [IO.Path]::GetFileNameWithoutExtension($pair.Exe))
        New-Item -ItemType Directory -Force -Path $out | Out-Null
        & $7z x -y -o$out $p | Out-Null
        Get-ChildItem $out -Recurse -Filter *.sys -ErrorAction SilentlyContinue | ForEach-Object {
            $manifest.Add((Copy-SysToX64 $_.FullName $pair.Note))
        }
    }
    # Outputs of scripts\deep-extract-oem.ps1 / innounp-* folders
    Get-ChildItem $extract -Directory -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -match '^(deep-|poly208-innounp|mbu-innounp|fusion-|tt-innounp)' } |
        ForEach-Object {
            $dir = $_
            Get-ChildItem $dir.FullName -Recurse -Filter *.sys -ErrorAction SilentlyContinue | ForEach-Object {
                $manifest.Add((Copy-SysToX64 $_.FullName ('innounp/deep ' + $dir.Name)))
            }
        }
    # Gigabyte / ASRock outer zips (single nested setup exe)
    foreach ($pair in @(
            @{ Zip = 'PolychromeRGB_v2.0.208.zip'; Note = 'ASRock Polychrome 2.0.208' }
            @{ Zip = 'PolychromeRGB_v2.0.190.zip'; Note = 'ASRock Polychrome 2.0.190' }
            @{ Zip = 'RGB_Fusion_B26.0923.1.zip'; Note = 'Gigabyte RGB Fusion' }
            @{ Zip = 'MotherboardUtility_v4.1.11.zip'; Note = 'ASRock A-Tuning 4.1.11' }
            @{ Zip = 'F-Stream_v2.0.188.zip'; Note = 'ASRock F-Stream' }
            @{ Zip = 'TT_RGBPlus_v2.9.95.zip'; Note = 'Thermaltake RGB Plus' }
            @{ Zip = 'MSIAfterburnerSetup466.zip'; Note = 'MSI Afterburner' }
            @{ Zip = 'FanControl.zip'; Note = 'FanControl' }
        )) {
        $z = Join-Path $raw $pair.Zip
        if (-not (Test-Path $z)) { continue }
        $out = Join-Path $extract ('unpack-' + ($pair.Zip -replace '\.zip$', ''))
        New-Item -ItemType Directory -Force -Path $out | Out-Null
        & $7z x -y -o$out $z | Out-Null
        Get-ChildItem $out -Recurse -Filter *.sys -ErrorAction SilentlyContinue | ForEach-Object {
            $manifest.Add((Copy-SysToX64 $_.FullName $pair.Note))
        }
    }
}

$date = Get-Date -Format 'yyyy-MM-dd'
$csv = Join-Path $root "audit\oem_sys_manifest_$date.csv"
$manifest | Sort-Object File -Unique | Export-Csv -Path $csv -NoTypeInformation
Write-Host "[+] $($manifest.Count) entries -> $csv"

if ($RefreshHeuristics) {
    $dumpbin = Get-ChildItem "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\*\VC\Tools\MSVC\*\bin\Hostx64\x64\dumpbin.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $dumpbin) { Write-Warning 'dumpbin missing'; return }
    $danger = @('MmMapIoSpace', 'MmMapIoSpaceEx', 'MmMapLockedPages', 'HalGetBusData', 'HalSetBusData')
    $rows = foreach ($f in Get-ChildItem $x64 -Filter *.sys) {
        $imp = & $dumpbin.FullName /imports $f.FullName 2>$null | Out-String
        $tier = 0
        $hits = @($danger | Where-Object { $imp -match $_ })
        if ($hits.Count -ge 2) { $tier = 3 } elseif ($hits.Count -eq 1) { $tier = 2 }
        [pscustomobject]@{ File = $f.Name; Tier = $tier; Imports = ($hits -join ';') }
    }
    $hcsv = Join-Path $root "audit\driver_byovd_heuristic_oem_$date.csv"
    $rows | Sort-Object { -$_.Tier }, File | Export-Csv $hcsv -NoTypeInformation
    Write-Host "[+] heuristics -> $hcsv"
    $rows | Where-Object Tier -ge 2 | Format-Table -AutoSize
}
