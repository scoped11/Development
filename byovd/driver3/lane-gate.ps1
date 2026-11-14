# Lane D gate — all driver3 titles (EAC / BattlEye / ACE). Full vehicle test needs Admin.
param(
    [string]$Game = 'rust',
    [switch]$AllTitles,
    [switch]$Execute
)

$ErrorActionPreference = 'Continue'
$Driver3 = $PSScriptRoot
$Byovd = Split-Path $Driver3 -Parent
$DevRoot = Split-Path $Byovd -Parent
$DmaRoot = Join-Path $DevRoot 'dma'
$OutJson = Join-Path $Driver3 'driver3_gate.json'

. (Join-Path $Driver3 'Resolve-Driver3Game.ps1')

if ($AllTitles) {
    & (Join-Path $Driver3 'gate-all.ps1') -Execute:$Execute
    exit $LASTEXITCODE
}

$profile = Resolve-Driver3Profile $Game
$meta = Get-Driver3Meta $profile

$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
    [Security.Principal.WindowsBuiltInRole]::Administrator)

$acAny = Get-Driver3AcProcesses -AnyKnownAc
$acTitle = Get-Driver3AcProcesses -Profile $profile
$dmaOk = Test-Path $DmaRoot
$driver2Ok = Test-Path (Join-Path $Byovd 'driver2\bin\Driver2Runner.exe')
$d2ProfilePath = Join-Path $Byovd ("driver2\" + $meta.driver2_profile_file)
$d2ProfileOk = Test-Path $d2ProfilePath

$preflightExit = $null
$recommendHost = 'unknown'
$laneD = $false

Write-Host '=== driver3 lane gate ==='
Write-Host "Title: $($meta.title)"
Write-Host "Profile=$profile AC_family=$($meta.ac_family) Admin=$isAdmin DMA=$dmaOk driver2_profile=$d2ProfileOk"
Show-Driver3RouteDoc $profile | Out-Null

if ($isAdmin) {
    $pre = Join-Path $Byovd 'scripts\lane-d-preflight.ps1'
    & $pre -Game $profile
    $preflightExit = $LASTEXITCODE
    switch ($preflightExit) {
        0 { $recommendHost = 'A_or_B' }
        1 { $recommendHost = 'B' }
        2 { $recommendHost = 'A'; $laneD = $true }
        default { $recommendHost = 'A'; $laneD = $true }
    }
} else {
    Write-Host '[!] Not elevated — vehicle open test skipped (run Admin for exact Tier 0/1).'
    $liveForTitle = Test-Driver3LiveAcForTitle $profile
    $anyAc = ($acAny.Count -gt 0)
    if ($anyAc) {
        $recommendHost = 'A'
        $laneD = $true
        if ($liveForTitle) {
            Write-Host "[!] Live AC for $profile : $($acTitle.ProcessName -join ', ')"
        } else {
            Write-Host "[!] Other AC running (not title-scoped): $($acAny.ProcessName -join ', ')"
        }
    } elseif (-not $driver2Ok) {
        $recommendHost = 'B'
    } else {
        $recommendHost = 'B'
    }
}

$message = Get-Driver3LaneDMessage -Profile $profile -LaneD $laneD -PreflightExit $(if ($null -ne $preflightExit) { $preflightExit } else { -1 })
$hb = Get-Driver3HostBDeveloperFlags $profile

$gate = [ordered]@{
    utc                 = (Get-Date).ToUniversalTime().ToString('o')
    product             = 'driver3'
    profile             = $profile
    title               = $meta.title
    ac_family           = $meta.ac_family
    elevated            = $isAdmin
    ac_processes_any    = @($acAny.ProcessName | Select-Object -Unique)
    ac_processes_title  = @($acTitle.ProcessName | Select-Object -Unique)
    dma_tree            = $dmaOk
    driver2_profile_ok  = $d2ProfileOk
    capture_dump        = $meta.capture_dump
    route_doc           = $meta.doc
    lane_d              = $laneD
    recommend_host      = $recommendHost
    preflight_exit      = $preflightExit
    host_b_runner       = $hb.Profile
    message             = $message
    next                = @(
        ".\driver3\ranked.ps1 -Profile $profile -Execute -SyncBuildId"
        ".\driver3\host-b.ps1 -Profile $profile -Execute -ForceMap"
        ".\driver3\host-b.ps1 -Profile $profile -Execute -ForceMap -Inspector"
    )
}

($gate | ConvertTo-Json -Depth 6) | Set-Content $OutJson -Encoding utf8
Write-Host "[+] Wrote $OutJson"
Write-Host "[*] $message"

if ($Execute -and $laneD) {
    & (Join-Path $Driver3 'host-a.ps1') -Profile $profile -Execute
    exit $LASTEXITCODE
}

if ($laneD) { exit 2 }
if ($preflightExit -eq 1) { exit 1 }
exit 0
