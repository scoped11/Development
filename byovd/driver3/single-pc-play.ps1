# One PC: map Driver2 here, launch cheat EXE, you play. No VM. No FPGA.
#Requires -RunAsAdministrator
param(
    [ValidateSet('rust', 'r6', 'deltaforce', 'arenabreakout', 'arksa', 'abi')]
    [string]$Game = 'rust',
    [switch]$Execute,
    [switch]$Live,
    [string]$CheatExe = '',
    [switch]$SkipMap,
    [switch]$Build,
    [switch]$UnsafeFullBypass,
    [switch]$RankedSafe
)

$ErrorActionPreference = 'Stop'
$Driver3 = $PSScriptRoot
$Byovd = Split-Path $Driver3 -Parent

. (Join-Path $Driver3 'Resolve-Driver3Game.ps1')
$profile = Resolve-Driver3Profile $Game
$meta = Get-Driver3Meta $profile
$useRankedSafe = $RankedSafe -or (-not $UnsafeFullBypass)

& (Join-Path $Driver3 'set-live-read-method.ps1') -Method single_pc_kernel -Game $profile | Out-Null

$log = Join-Path $Driver3 'host_local_map.log'
function L($m) { $line = "$(Get-Date -Format o) $m"; Add-Content $log $line; Write-Host $line }

Write-Host ''
Write-Host '=== SINGLE PC PLAY (driver + EXE) ==='
Write-Host "    Title: $($meta.title)"
Write-Host '    No VM. No FPGA. Driver2 maps on THIS machine.'
if ($useRankedSafe) {
    Write-Host '    Ranked-safe: --ac-safe session map (ioctl only), Tier 0 fail-closed.'
}
if ($Live) {
    Write-Host '    -Live: kernel on session with AC possible — NOT undetected.'
} else {
    Write-Host '    Use -Live if EAC/BE is running (required when AC is up).'
}
Write-Host ''

& (Join-Path $Driver3 'sync-build-id-from-dumps.ps1') -Game $profile | Out-Null
& (Join-Path $Driver3 'lane-gate.ps1') -Game $profile | Out-Null
$gate = $LASTEXITCODE

if (-not $Execute) {
    Write-Host '[*] PLAN — re-run with -Execute [-Live] [-CheatExe path\to\your.exe]'
    Write-Host '    Map: driver2\ranked-play.ps1 -Profile' $profile '-ForceMap -Execute'
    exit 0
}

if ($Build) {
    & (Join-Path $Byovd 'build.ps1')
    & (Join-Path $Byovd 'driver2\build.ps1') -BuildExamples
}

if (-not $CheatExe) {
    $candidates = @(
        (Join-Path $Byovd 'bin\CheatLoader.exe'),
        (Join-Path $Byovd 'driver2\bin\CheatLoader.exe'),
        (Join-Path $Byovd 'driver2\bin\cheat_dev_minimal.exe'),
        (Join-Path $Byovd 'bin\cheat_dev_minimal.exe')
    )
    $CheatExe = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
}

L "=== single-pc-play profile=$profile gate=$gate live=$Live admin=True"

$proofFile = Join-Path $Driver3 "kernel_proof_$profile.json"
$mapExit = 0
if (-not $SkipMap) {
    if ($Live -or $gate -ne 2) {
        $env:DRIVER2_RANKED_OVERRIDE = '1'
    }
    if ($useRankedSafe) {
        if ($gate -eq 2 -and -not $Live) {
            L 'gate=2 Tier0 — blocked without -Live (ranked-safe)'
            throw 'AC Tier 0 — add -Live when title AC is running, or close AC before map'
        }
        L 'ranked-safe map on local host...'
        & (Join-Path $Byovd 'driver2\ranked-safe-map.ps1') -Profile $profile -Live:$Live
    } elseif ($gate -eq 2 -and -not $Live) {
        L 'gate=2 Tier0 — blocked without -Live'
        throw 'AC Tier 0 — add -Live to map on this PC anyway (your risk) or use dma mode'
    } else {
        L 'unsafe full bypass map (not ranked-safe)...'
        & (Join-Path $Byovd 'driver2\ranked-play.ps1') -Profile $profile -Execute -ForceMap -SkipProve
    }
    $mapExit = $LASTEXITCODE
    L "map exit=$mapExit"
    if ($mapExit -eq 0) {
        . (Join-Path $Byovd 'driver2\lib\Invoke-Driver2RankedVerify.ps1')
        $verify = Invoke-Driver2RankedVerify -Profile $profile
        L "probe exit=$($verify.probe_exit) ok=$($verify.probe_ok)"
        if (-not $verify.probe_ok) {
            L 'FAILED Driver2Client probe'
            throw 'Driver2Client probe failed — driver not usable; see driver2_ranked_verify.json'
        }
        $ver = Get-ItemProperty 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion'
        $proof = [ordered]@{
            marker_ok = $true
            os_build  = ('{0}.{1}' -f $ver.CurrentBuild, $ver.UBR)
            profile   = $profile
            title     = $meta.title
            ac_family = $meta.ac_family
            host      = 'single_pc_kernel'
            utc       = (Get-Date).ToUniversalTime().ToString('o')
        } | ConvertTo-Json -Depth 4
        $proof | Set-Content $proofFile -Encoding utf8
        & (Join-Path $Driver3 'sync-lab-manifest.ps1') -Game $profile -KernelProofJson $proofFile
    }
} else {
    L 'SkipMap — assume driver already mapped'
    . (Join-Path $Byovd 'driver2\lib\Invoke-Driver2RankedVerify.ps1')
    $verify = Invoke-Driver2RankedVerify -Profile $profile
    L "probe exit=$($verify.probe_exit) ok=$($verify.probe_ok)"
    if (-not $verify.probe_ok) {
        throw 'Driver2Client probe failed — map driver first or run confirm-ranked-100.ps1'
    }
}

if ($mapExit -ne 0) {
    L "FAILED map exit=$mapExit"
    exit $mapExit
}

if ($CheatExe -and (Test-Path $CheatExe)) {
    L "launch cheat=$CheatExe"
    Start-Process -FilePath $CheatExe -WorkingDirectory (Split-Path $CheatExe -Parent)
} else {
    Write-Host '[!] No cheat EXE — build: driver2\build.ps1 -BuildExamples'
    Write-Host '    Or: -CheatExe C:\path\YourCheat.exe'
    L 'cheat_exe_missing'
}

Write-Host ''
Write-Host '[+] Driver mapped on this PC. Start the game when ready.'
Write-Host '    Read: Driver2Client pread <pid> <va> <n>'
Write-Host '    Or your EXE using Driver2Session (already launched if found)'
Write-Host ''

& (Join-Path $Driver3 'sync-lab-manifest.ps1') -Game $profile -DmaStatus not_used -LiveHost $env:COMPUTERNAME
& (Join-Path $Driver3 'single-pc-s-tier.ps1') | Out-Null

exit $(if ($mapExit -eq 0) { 0 } else { $mapExit })
