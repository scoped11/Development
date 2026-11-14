# Developer approval gate — target >= 90% (VM lab; -Execute to map).
param(
    [string]$Driver = 'rtcore',
    [switch]$ForceMap,
    [switch]$Execute,
    [switch]$IncludeAce,
    [switch]$Inspector,
    [switch]$IncludeBe,
    [string]$OutJson = ''
)

$ErrorActionPreference = 'Stop'
$Driver2 = $PSScriptRoot
$Byovd = Split-Path $Driver2 -Parent
if (-not $OutJson) { $OutJson = Join-Path $Driver2 'developer_approval.json' }

$profiles = @('rust', 'r6', 'universal')
if ($IncludeAce) {
    $profiles = @('rust', 'r6', 'deltaforce', 'arenabreakout', 'universal')
}
if ($IncludeBe -and ($profiles -notcontains 'arksa')) {
    $profiles += 'arksa'
}
$rubric = @{
    schema = 'driver2_developer_approval_v1'
    target_score_pct = 90
    profiles = $profiles
    checks_per_profile = @(
        'bypass_map_exit_0'
        'pf_diag_bypass'
        'strip_869'
        'title_ioctl_in_log'
        'process_rw'
        'driver2_client_probe'
        'hygiene_in_pf_diag'
        'symlink_kept_log'
        'dkom_unlink_skipped_log'
        'inspector_tier1_log'
    )
    execute = $Execute
    inspector_tier1 = $Inspector
}

if (-not $Execute) {
    $rubric.note = 'PLAN ONLY — pass -Execute on DriverLab VM when ready'
    $rubric | ConvertTo-Json -Depth 5 | Set-Content $OutJson -Encoding UTF8
    Write-Host '[*] Developer approval rubric written (no map). See DEVELOPER_APPROVAL.md'
    Write-Host "    .\developer-approval-gate.ps1 -Execute -ForceMap"
    exit 0
}

$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
    [Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) { throw 'developer-approval-gate -Execute requires Administrator (use DriverLab VM)' }

& (Join-Path $Byovd 'build.ps1')
& (Join-Path $Driver2 'build.ps1')
$veh = Join-Path $Byovd 'vehicles\RTCore64.sys'
if (Test-Path $veh) { Copy-Item $veh (Join-Path $Byovd 'bin\RTCore64.sys') -Force }

$runner = Join-Path $Byovd 'bin\Driver2Runner.exe'
$client = Join-Path $Byovd 'bin\Driver2Client.exe'
$rwExe = Join-Path $Byovd 'bin\process_rw.exe'
$verify = Join-Path $Byovd 'scripts\verify-pf-diag.ps1'

$allResults = @()
foreach ($p in $profiles) {
    $args = @('--profile', $p, '--mode', 'bypass', '--driver', $Driver)
    if ($ForceMap) { $args += '--force-map' }
    if ($Inspector) { $args += '--inspector' }
    & $runner @args
    $mapExit = $LASTEXITCODE

    $diag = & $verify -Profile $p
    $rwExit = $null
    if (Test-Path $rwExe) {
        & $rwExe
        $rwExit = $LASTEXITCODE
    }
    $clientExit = $null
    if (Test-Path $client) {
        & $client probe
        $clientExit = $LASTEXITCODE
    }

    $pfPath = Join-Path $env:TEMP 'pf_diag.tmp'
    $pfText = ''
    if (Test-Path $pfPath) { $pfText = Get-Content $pfPath -Raw }
    $hygiene = $pfText -match 'Hygiene:|PiDDB|WdFilter|hash bucket'
    $symlinkKept = $pfText -match 'symlink kept'
    $dkomSkipped = $pfText -match 'DKOM vehicle unlink skipped'
    $inspectorTier = $pfText -match 'Cheat inspector tier: 1'
    $titleIoctl = $false
    switch ($p) {
        'rust' { $titleIoctl = $diag.eac844 -and $diag.eac851 -and $diag.strip -and $diag.max869 }
        'r6' { $titleIoctl = $diag.be852 -and $diag.max869 -and $diag.strip }
        'arksa' { $titleIoctl = $diag.be852 -and $diag.max869 -and $diag.strip }
        'universal' { $titleIoctl = (($diag.eac844 -and $diag.eac851) -or ($diag.be852) -or ($diag.ace846 -and $diag.ace86a)) -and $diag.strip -and $diag.max869 }
        'deltaforce' { $titleIoctl = $diag.ace846 -and $diag.ace84e -and $diag.ace86a -and $diag.strip -and $diag.max869 }
        'arenabreakout' { $titleIoctl = $diag.ace846 -and $diag.ace86a -and $diag.strip -and $diag.max869 }
        default { $titleIoctl = $diag.ok }
    }

    $checks = @(
        @{ name = 'bypass_map_exit_0'; pass = ($mapExit -eq 0) }
        @{ name = 'pf_diag_bypass'; pass = $diag.ok }
        @{ name = 'strip_869'; pass = ($diag.strip -and $diag.max869) }
        @{ name = 'title_ioctl_in_log'; pass = $titleIoctl }
        @{ name = 'process_rw'; pass = ($rwExit -eq 0) }
        @{ name = 'driver2_client_probe'; pass = ($clientExit -eq 0) }
        @{ name = 'hygiene_in_pf_diag'; pass = $hygiene }
        @{ name = 'symlink_kept_log'; pass = $symlinkKept }
        @{ name = 'dkom_unlink_skipped_log'; pass = $dkomSkipped }
        @{ name = 'inspector_tier1_log'; pass = ($inspectorTier -or -not $Inspector) }
    )
    $n = $checks.Count
    $ok = ($checks | Where-Object { $_.pass }).Count
    $score = [math]::Round(100.0 * $ok / $n, 1)

    $allResults += [pscustomobject]@{
        profile = $p
        score_pct = $score
        approved = ($score -ge 90) -and ($mapExit -eq 0) -and ($rwExit -eq 0) -and ($clientExit -eq 0)
        checks = $checks
        map_exit = $mapExit
        rw_exit = $rwExit
        client_exit = $clientExit
    }
}

$approved = ($allResults | Where-Object { -not $_.approved }).Count -eq 0
$avg = [math]::Round(($allResults | Measure-Object score_pct -Average).Average, 1)

@{
    schema = 'driver2_developer_approval_v1'
    utc = (Get-Date).ToUniversalTime().ToString('o')
    hostname = $env:COMPUTERNAME
    avg_score_pct = $avg
    target_score_pct = 90
    approved = $approved
    profiles = $allResults
    disclaimer = 'VM lab approval — not live anti-cheat bypass certification'
    cheat_inspector = 'driver2/CHEAT_INSPECTOR.md'
    inspector_tier1 = [bool]$Inspector
} | ConvertTo-Json -Depth 8 | Set-Content $OutJson -Encoding UTF8

Write-Host "[*] developer approval=$approved avg=$avg% -> $OutJson"
if (-not $approved) { exit 1 }
exit 0
