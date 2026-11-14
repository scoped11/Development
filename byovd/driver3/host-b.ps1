# driver3 Host B — kernel proof via driver2 (per-title EAC/BE/ACE profile).
param(
    [string]$Profile = 'rust',
    [switch]$Execute,
    [switch]$ForceMap,
    [switch]$Inspector,
    [switch]$SkipProve,
    [switch]$FullAcMatrix
)

$ErrorActionPreference = 'Stop'
$Driver3 = $PSScriptRoot
$Byovd = Split-Path $Driver3 -Parent
$Driver2 = Join-Path $Byovd 'driver2'

. (Join-Path $Driver3 'Resolve-Driver3Game.ps1')
$profile = Resolve-Driver3Profile $Profile
$meta = Get-Driver3Meta $profile
$hb = Get-Driver3HostBDeveloperFlags $profile
$runnerProfile = $hb.Profile

Write-Host '=== driver3 Host B (lab VM / kernel proof) ==='
Write-Host "Title: $($meta.title) ($($meta.ac_family))"
Write-Host "driver2 profile=$runnerProfile Execute=$Execute ForceMap=$ForceMap Inspector=$Inspector"

if (-not (Test-Path (Join-Path $Driver2 $meta.driver2_profile_file))) {
    Write-Warning "Missing driver2 profile file: $($meta.driver2_profile_file)"
}

$plan = @(
    "replica: ac_replica_profiles.json / $($meta.ac_family)"
    "map: Driver2Runner --profile $runnerProfile"
    'sync: kernel_proof_last.json → lab_manifest lane_d'
)
$plan | ForEach-Object { Write-Host "  - $_" }

if (-not $Execute) {
    Write-Host '[*] PLAN ONLY — re-run with -Execute -ForceMap on DriverLab guest'
    exit 2
}

$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
    [Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    throw 'Host B -Execute requires Administrator (VM: vm-test-from-host.ps1 -Execute)'
}

if ($Inspector -or $FullAcMatrix) {
    $dg = @{
        Profile = $runnerProfile
        Execute = $true
        ForceMap = $true
        Inspector = $Inspector
    }
    if ($FullAcMatrix -or $hb.IncludeAce) { $dg.IncludeAce = $true }
    if ($FullAcMatrix -or $hb.IncludeBe) { $dg.IncludeBe = $true }
    & (Join-Path $Driver2 'developer-approval-gate.ps1') @dg
    $ec = $LASTEXITCODE
} else {
    $a = @('-Profile', $runnerProfile, '-Target', 'vm', '-Execute')
    if ($ForceMap) { $a += '-ForceMap' }
    if ($SkipProve) { $a += '-SkipProve' }
    & (Join-Path $Driver2 'everything.ps1') @a
    $ec = $LASTEXITCODE
}

$ver = Get-ItemProperty 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion'
$proof = [ordered]@{
    marker_ok  = ($ec -eq 0)
    os_build   = ('{0}.{1}' -f $ver.CurrentBuild, $ver.UBR)
    profile    = $profile
    runner     = $runnerProfile
    ac_family  = $meta.ac_family
    title      = $meta.title
    utc        = (Get-Date).ToUniversalTime().ToString('o')
} | ConvertTo-Json -Depth 4

$proofFile = Join-Path $Driver3 "kernel_proof_$profile.json"
$proof | Set-Content $proofFile -Encoding utf8

& (Join-Path $Driver3 'sync-lab-manifest.ps1') -Game $profile -LabHost $env:COMPUTERNAME -KernelProofJson $proofFile

Write-Host "[+] Host B done exit=$ec proof=$proofFile"
exit $ec
