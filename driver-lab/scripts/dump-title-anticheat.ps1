<#
.SYNOPSIS
  Archive per-title anti-cheat binaries + metadata (read-only disk copies).

.PARAMETER Title
  arksa | r6 | deltaforce | arenabreakout | rust

.EXAMPLE
  .\dump-title-anticheat.ps1 -Title r6,deltaforce
  .\dump-title-anticheat.ps1 -Title all
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string[]]$Title,

    [string]$OutRoot = 'D:\driver-lab-ac-dumps',
    [string]$RustRoot = 'C:\Program Files (x86)\Steam\steamapps\common\Rust',
    [string]$EosDriver = 'C:\Program Files (x86)\EasyAntiCheat_EOS\EasyAntiCheat_EOS.sys',
    [string]$R6Root = 'D:\SteamLibrary\steamapps\common\Tom Clancy''s Rainbow Six Siege',
    [string]$ArkRoot = 'D:\SteamLibrary\steamapps\common\ARK Survival Ascended',
    [string]$DeltaRoot = 'D:\Delta Force\Delta Force',
    [string]$AbiRoot = 'D:\Arena Breakout Infinite',
    [string]$BeCommon = 'C:\Program Files (x86)\Common Files\BattlEye',
    [string]$AceProgram = 'C:\Program Files\AntiCheatExpert',
    [string]$SystemDrivers = 'C:\Windows\System32\drivers'
)

$script:LabScriptsRoot = if ($PSScriptRoot) { $PSScriptRoot } else { Split-Path -Parent $MyInvocation.MyCommand.Path }
$LabDumpsMirror = (Join-Path $script:LabScriptsRoot '..\dumps') | ForEach-Object { [IO.Path]::GetFullPath($_) }
if (-not $OutRoot) { $OutRoot = 'D:\driver-lab-ac-dumps' }

function Sync-DumpToLab {
    param([string]$SourceOut)
    if (-not (Test-Path $SourceOut)) { return }
    $leaf = Split-Path $SourceOut -Leaf
    $dest = Join-Path $LabDumpsMirror $leaf
    New-Item -ItemType Directory -Force -Path $LabDumpsMirror | Out-Null
    robocopy $SourceOut $dest /E /NFL /NDL /NJH /NJS /nc /ns /np | Out-Null
    Write-Host "    lab mirror: $dest"
}

$ErrorActionPreference = 'Stop'

function Get-HostOsBuild {
    $b = Get-ItemProperty 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion'
    return "$($b.CurrentBuildNumber).$($b.UBR)"
}

function Copy-FileRecord {
    param([string]$Source, [string]$DestDir, [string]$DestName = '')
    if (-not (Test-Path -LiteralPath $Source)) { return $null }
    New-Item -ItemType Directory -Force -Path $DestDir | Out-Null
    $leaf = if ($DestName) { $DestName } else { [IO.Path]::GetFileName($Source) }
    $dest = Join-Path $DestDir $leaf
    Copy-Item -LiteralPath $Source -Destination $dest -Force
    $item = Get-Item -LiteralPath $dest
    return @{
        source     = $Source
        path       = $dest
        bytes      = $item.Length
        sha256     = (Get-FileHash -LiteralPath $dest -Algorithm SHA256).Hash
        file_version = $item.VersionInfo.FileVersion
        product_version = $item.VersionInfo.ProductVersion
    }
}

function Copy-TreeRecord {
    param([string]$Source, [string]$Dest)
    if (-not (Test-Path -LiteralPath $Source)) { return $null }
    $parent = Split-Path $Dest -Parent
    New-Item -ItemType Directory -Force -Path $parent | Out-Null
    if (Test-Path -LiteralPath $Dest) { Remove-Item -LiteralPath $Dest -Recurse -Force }
    Copy-Item -LiteralPath $Source -Destination $Dest -Recurse -Force
    return @{ source = $Source; path = $Dest }
}

function Copy-AceBundleEssentials {
    param([string]$Source, [string]$DestRoot)
    New-Item -ItemType Directory -Force -Path $DestRoot | Out-Null
    $copied = [System.Collections.Generic.List[string]]::new()
    foreach ($pat in @('*.sys', '*.dat', 'product.rc', 'SGuard.rc', 'SGuard64.exe', 'SGuardSvc64.exe', 'ACE-Service64.exe', 'w*')) {
        Get-ChildItem -LiteralPath $Source -Filter $pat -File -ErrorAction SilentlyContinue | ForEach-Object {
            $rel = $_.Name
            $d = Join-Path $DestRoot $rel
            Copy-Item -LiteralPath $_.FullName -Destination $d -Force
            $copied.Add($rel)
        }
    }
    $sg = Join-Path $Source 'SGuard\x64'
    if (Test-Path $sg) {
        $sgDest = Join-Path $DestRoot 'SGuard\x64'
        New-Item -ItemType Directory -Force -Path $sgDest | Out-Null
        foreach ($pat in @('product.rc', 'SGuard.rc', 'SGuard64.exe', 'SGuardSvc64.exe')) {
            $f = Join-Path $sg $pat
            if (Test-Path $f) {
                Copy-Item -LiteralPath $f -Destination (Join-Path $sgDest $pat) -Force
                $copied.Add("SGuard\x64\$pat")
            }
        }
    }
    return @{ source = $Source; path = $DestRoot; files = $copied }
}

function Write-AcDump {
    param(
        [string]$Slug,
        [hashtable]$Manifest
    )
    $stamp = Get-Date -Format 'yyyy-MM-dd_HHmm'
    $out = Join-Path $OutRoot "${Slug}_$stamp"
    New-Item -ItemType Directory -Force -Path $out | Out-Null
    $Manifest.captured_utc = (Get-Date).ToUniversalTime().ToString('o')
    $Manifest.host_os_build = Get-HostOsBuild
    $manifestPath = Join-Path $out 'manifest.json'
    $Manifest | ConvertTo-Json -Depth 8 | Set-Content $manifestPath -Encoding UTF8
    return @{ Out = $out; ManifestPath = $manifestPath }
}

function Clone-KernelToInbox {
    param(
        [string]$SourcePath,
        [string]$InboxName
    )
    if (-not (Test-Path -LiteralPath $SourcePath)) { return $null }
    $inbox = Join-Path $script:LabScriptsRoot '..\audit\inbox'
    New-Item -ItemType Directory -Force -Path $inbox | Out-Null
    $inboxCopy = Join-Path $inbox $InboxName
    Copy-Item -LiteralPath $SourcePath -Destination $inboxCopy -Force
  # Unique IDA tree per inbox basename (without .sys)
    $cloneName = [IO.Path]::GetFileNameWithoutExtension($InboxName)
    $reverseRoot = Join-Path $script:LabScriptsRoot "..\reverse\$cloneName"
    $bin = Join-Path $reverseRoot 'bin'
    New-Item -ItemType Directory -Force -Path $bin, (Join-Path $reverseRoot 'notes') | Out-Null
    $cloneDest = Join-Path $bin ([IO.Path]::GetFileName($SourcePath))
    Copy-Item -LiteralPath $SourcePath -Destination $cloneDest -Force
    return @{
        inbox = $inboxCopy
        ida_clone = $reverseRoot
        sha256 = (Get-FileHash -LiteralPath $inboxCopy -Algorithm SHA256).Hash
    }
}

function Dump-Rust {
    if (-not (Test-Path $EosDriver)) { throw "Missing EOS driver: $EosDriver" }
    if (-not (Test-Path $RustRoot)) { throw "Missing Rust: $RustRoot" }
    $settings = Join-Path $RustRoot 'EasyAntiCheat\Settings.json'
    if (-not (Test-Path $settings)) { throw "Missing Rust Settings.json" }

    $stamp = Get-Date -Format 'yyyy-MM-dd_HHmm'
    $out = Join-Path $OutRoot "rust-eac-eos_$stamp"
    New-Item -ItemType Directory -Force -Path $out, (Join-Path $out 'kernel'), (Join-Path $out 'eac') | Out-Null
    $kDest = Join-Path $out 'kernel\EasyAntiCheat_EOS.sys'
    Copy-Item -LiteralPath $EosDriver -Destination $kDest -Force
    Copy-TreeRecord -Source (Join-Path $RustRoot 'EasyAntiCheat') -Dest (Join-Path $out 'eac\EasyAntiCheat') | Out-Null

    $clone = Clone-KernelToInbox -SourcePath $kDest -InboxName 'EasyAntiCheat_EOS.sys'
    $manifest = @{
        game = 'rust'
        ac_family = 'eac_eos'
        platform = 'steam'
        game_path = $RustRoot
        settings_json = (Get-Content $settings -Raw | ConvertFrom-Json)
        kernel = (Copy-FileRecord -Source $kDest -DestDir (Join-Path $out 'meta') -DestName 'driver.record.json')
    }
    $manifest.kernel = @{
        path = $kDest
        sha256 = (Get-FileHash $kDest -Algorithm SHA256).Hash
        bytes = (Get-Item $kDest).Length
    }
    $manifest.inbox = $clone.inbox
    $manifest.ida_clone = $clone.ida_clone
    $manifest.captured_utc = (Get-Date).ToUniversalTime().ToString('o')
    $manifest.host_os_build = Get-HostOsBuild
    $manifest | ConvertTo-Json -Depth 8 | Set-Content (Join-Path $out 'manifest.json') -Encoding UTF8
    Write-Host "[+] rust -> $out"
}

function Dump-R6Be {
    Write-Host '[.] r6: start'
    if (-not (Test-Path $R6Root)) { throw "Missing R6: $R6Root" }
    $beGame = Join-Path $R6Root 'BattlEye'
    if (-not (Test-Path $beGame)) { throw "Missing R6 BattlEye folder: $beGame" }

    $outInfo = Write-AcDump -Slug 'r6-battleye' -Manifest @{
        game = 'rainbow_six_siege'
        ac_family = 'battleye'
        platform = 'steam_ubisoft'
        game_path = $R6Root
        game_id_hint = (Get-Content (Join-Path $beGame 'BELauncher.ini') -Raw)
        ac_primary = 'BattlEye (BEDaisy when installed)'
        ac_secondary_note = 'EasyAntiCheat folder present — certificate/runtime channel only for this title'
    }

    $out = $outInfo.Out
    Write-Host '[.] r6: copy BattlEye game bundle'
    Copy-TreeRecord -Source $beGame -Dest (Join-Path $out 'battleye\game') | Out-Null
    $eacCert = Join-Path $R6Root 'EasyAntiCheat'
    if (Test-Path $eacCert) {
        Write-Host '[.] r6: copy EAC certificate channel'
        Copy-TreeRecord -Source $eacCert -Dest (Join-Path $out 'eac_certificates') | Out-Null
    }

    $kernelDir = Join-Path $out 'kernel'
    $records = @{}
    $bedaisyCandidates = @(
        Join-Path $SystemDrivers 'BEDaisy.sys',
        Join-Path $BeCommon 'BEDaisy.sys',
        Join-Path $beGame 'BEDaisy.sys'
    )
    foreach ($c in $bedaisyCandidates) {
        if (Test-Path -LiteralPath $c) {
            $records.BEDaisy = Copy-FileRecord -Source $c -DestDir $kernelDir
            break
        }
    }
    if (-not $records.BEDaisy) {
        $records.BEDaisy_missing = 'BEDaisy.sys not on disk — launch R6 once or run Install_BattlEye.bat to pull kernel service'
    }

    $svcR6 = Join-Path $BeCommon 'BEService_r6s.exe'
    $records.BEService_r6s = Copy-FileRecord -Source $svcR6 -DestDir (Join-Path $out 'usermode')

    $manifest = Get-Content $outInfo.ManifestPath -Raw | ConvertFrom-Json
    $manifest | Add-Member -NotePropertyName artifacts -NotePropertyValue $records -Force
    $manifest | ConvertTo-Json -Depth 8 | Set-Content $outInfo.ManifestPath -Encoding UTF8

    if ($records.BEDaisy) {
        $c = Clone-KernelToInbox -SourcePath $records.BEDaisy.path -InboxName 'BEDaisy_r6.sys'
        Write-Host "    BEDaisy inbox: $($c.inbox)"
    }
    Sync-DumpToLab -SourceOut $out
    Write-Host "[+] r6 -> $out"
}

function Dump-ArkBe {
    if (-not (Test-Path $ArkRoot)) { throw "Missing ARK SA: $ArkRoot" }
    $svcArk = Join-Path $BeCommon 'BEService_arksa.exe'
    if (-not (Test-Path $svcArk)) { throw "Missing BEService_arksa.exe — run ARK once to register BattlEye" }

    $gameExe = Get-ChildItem $ArkRoot -Recurse -Filter 'ShooterGame*.exe' -ErrorAction SilentlyContinue | Select-Object -First 1
    $outInfo = Write-AcDump -Slug 'arksa-battleye' -Manifest @{
        game = 'ark_survival_ascended'
        ac_family = 'battleye'
        platform = 'steam'
        game_path = $ArkRoot
        game_install_note = if ($gameExe) { "Main exe: $($gameExe.FullName)" } else { 'No ShooterGame*.exe in tree — install may be incomplete; BEService_arksa still identifies title' }
        battleye_game_token = 'arksa'
    }

    $out = $outInfo.Out
    $records = @{}
    $records.BEService_arksa = Copy-FileRecord -Source $svcArk -DestDir (Join-Path $out 'usermode')

    $beInGame = Get-ChildItem $ArkRoot -Recurse -Directory -Filter 'BattlEye' -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($beInGame) {
        Copy-TreeRecord -Source $beInGame.FullName -Dest (Join-Path $out 'battleye\game') | Out-Null
    }

    $bedaisy = Join-Path $SystemDrivers 'BEDaisy.sys'
    if (Test-Path $bedaisy) {
        $records.BEDaisy = Copy-FileRecord -Source $bedaisy -DestDir (Join-Path $out 'kernel')
    } else {
        $records.BEDaisy_missing = 'BEDaisy.sys not on disk'
    }

    $manifest = Get-Content $outInfo.ManifestPath -Raw | ConvertFrom-Json
    $manifest | Add-Member -NotePropertyName artifacts -NotePropertyValue $records -Force
    $manifest | ConvertTo-Json -Depth 8 | Set-Content $outInfo.ManifestPath -Encoding UTF8
    if ($records.BEDaisy) {
        Clone-KernelToInbox -SourcePath $records.BEDaisy.path -InboxName 'BEDaisy_arksa.sys' | Out-Null
    }
    Write-Host "[+] arksa -> $out"
}

function Dump-AceTitle {
    param(
        [string]$Slug,
        [string]$GameName,
        [string]$GameRoot,
        [string]$AceRel,
        [string]$InboxKernelName
    )
    $aceBundle = Join-Path $GameRoot $AceRel
    if (-not (Test-Path $aceBundle)) { throw "Missing ACE bundle: $aceBundle" }

    $productRc = Join-Path $aceBundle 'SGuard\x64\product.rc'
    $gameAceGame = Join-Path $aceBundle 'ACE-GAME.sys'
    $sysAceGame = Join-Path $SystemDrivers 'ACE-GAME.sys'

    $outInfo = Write-AcDump -Slug $Slug -Manifest @{
        game = $GameName
        ac_family = 'ace_tencent'
        game_path = $GameRoot
        ace_bundle = $aceBundle
        sguard_product_rc = if (Test-Path $productRc) { (Get-Content $productRc -Raw).Trim() } else { $null }
    }

    $out = $outInfo.Out
    Write-Host "[.] $GameName : copy ACE essentials (no large plugin DLLs)"
    $aceCopy = Copy-AceBundleEssentials -Source $aceBundle -DestRoot (Join-Path $out 'anticheatexpert')

    $records = @{}
    if (Test-Path $gameAceGame) {
        $records.ACE_GAME_game_bundle = Copy-FileRecord -Source $gameAceGame -DestDir (Join-Path $out 'kernel')
    }
    if (Test-Path $sysAceGame) {
        $records.ACE_GAME_system32 = Copy-FileRecord -Source $sysAceGame -DestDir (Join-Path $out 'kernel') -DestName 'ACE-GAME_system32.sys'
    }
    foreach ($extra in @('ACE-BASE.sys', 'ACE-ADVT.sys')) {
        $p = Join-Path $SystemDrivers $extra
        if (Test-Path $p) {
            $key = $extra.Replace('.', '_').Replace('-', '_')
            $records[$key] = Copy-FileRecord -Source $p -DestDir (Join-Path $out 'kernel') -DestName "${extra}"
        }
    }

    if ($records.ACE_GAME_game_bundle -and $records.ACE_GAME_system32) {
        $records.kernel_match = ($records.ACE_GAME_game_bundle.sha256 -eq $records.ACE_GAME_system32.sha256)
    }

    $manifest = Get-Content $outInfo.ManifestPath -Raw | ConvertFrom-Json
    $manifest | Add-Member -NotePropertyName ace_bundle_copy -NotePropertyValue $aceCopy -Force
    $manifest | Add-Member -NotePropertyName artifacts -NotePropertyValue $records -Force
    $manifest | ConvertTo-Json -Depth 8 | Set-Content $outInfo.ManifestPath -Encoding UTF8

    $cloneSrc = if ($records.ACE_GAME_game_bundle) { $records.ACE_GAME_game_bundle.path } elseif ($records.ACE_GAME_system32) { $records.ACE_GAME_system32.path } else { $null }
    if ($cloneSrc) {
        $c = Clone-KernelToInbox -SourcePath $cloneSrc -InboxName $InboxKernelName
        Write-Host "    kernel inbox: $($c.inbox) SHA256=$($c.sha256)"
    }
    Sync-DumpToLab -SourceOut $out
    Write-Host "[+] $GameName -> $out"
}

$expanded = foreach ($t in $Title) {
    if ($t -match ',') { $t -split ',' } else { $t }
}
$normalized = foreach ($t in $expanded) { "$t".Trim().ToLower() }
if ($normalized -contains 'all') {
    $normalized = @('rust', 'r6', 'arksa', 'deltaforce', 'arenabreakout')
}

foreach ($t in $normalized) {
    switch ($t) {
        'rust' { Dump-Rust }
        'r6' { Dump-R6Be }
        'arksa' { Dump-ArkBe }
        'ark' { Dump-ArkBe }
        'deltaforce' { Dump-AceTitle -Slug 'deltaforce-ace' -GameName 'delta_force' -GameRoot $DeltaRoot -AceRel 'DeltaForce\Binaries\Win64\AntiCheatExpert' -InboxKernelName 'ACE-GAME_deltaforce.sys' }
        'arenabreakout' { Dump-AceTitle -Slug 'arenabreakout-ace' -GameName 'arena_breakout_infinite' -GameRoot $AbiRoot -AceRel 'ABInfinite\Binaries\Win64\AntiCheatExpert' -InboxKernelName 'ACE-GAME_arenabreakout.sys' }
        default { throw "Unknown title: $t" }
    }
}
