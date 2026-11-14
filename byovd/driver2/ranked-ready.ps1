# driver2 S-tier ranked (Rust + R6) — one PC, map + cheat, no VM/DMA.
param(
    [ValidateSet('rust', 'r6', 'both')]
    [string]$Title = 'both',
    [switch]$Execute,
    [switch]$Live,
    [switch]$PreflightOnly,
    [string]$CheatExe = ''
)

$ErrorActionPreference = 'Stop'
$Driver2 = $PSScriptRoot
$Byovd = Split-Path $Driver2 -Parent
$Driver3 = Join-Path $Byovd 'driver3'

Write-Host '=== driver2 RANKED READY (Rust / R6) ==='
Write-Host '    One PC · Driver2 map · your EXE · play'
Write-Host ''

& (Join-Path $Driver3 'set-live-read-method.ps1') -Method single_pc_kernel -Game rust | Out-Null
& (Join-Path $Driver3 'sync-build-id-from-dumps.ps1') -Game rust -AllTitles | Out-Null
& (Join-Path $Driver2 'cheat-inspector.ps1') | Out-Null

$games = if ($Title -eq 'both') { @('rust', 'r6') } else { ,$Title }
foreach ($g in $games) {
    & (Join-Path $Driver3 'lane-gate.ps1') -Game $g | Out-Null
}

. (Join-Path $Driver2 'lib\Test-Driver2RankedReady.ps1')
$ready = Test-Driver2RankedReady -RequireMapProof:$Execute
$out = Join-Path $Driver2 'driver2_ranked_ready.json'
($ready | ConvertTo-Json -Depth 8) | Set-Content $out -Encoding utf8

Write-Host "[*] tier=$($ready.tier) ranked_ready=$($ready.ranked_ready) score=$($ready.score_pct)%"
$ready.checks | ForEach-Object { Write-Host ("  [{0}] {1}" -f $(if ($_.pass) { '+' } else { '!' }), $_.id) }

if ($Execute) {
    $adm = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
        [Security.Principal.WindowsBuiltInRole]::Administrator)
    if (-not $adm) { throw 'ranked-ready -Execute requires Administrator' }
}

if ($PreflightOnly -or -not $Execute) {
    Write-Host ''
    Write-Host '[*] RANKED READY preflight →' $out
    Write-Host '    Go live (admin): .\driver2\ranked-ready.ps1 -Execute -Live'
    Write-Host "    Or: .\driver3\single-pc-play.ps1 -Game rust|r6 -Execute -Live -Build"
    exit $(if ($ready.ranked_ready) { 0 } else { 5 })
}

if (-not $Live) {
    Write-Host '[!] -Live required for ranked execute (EAC/BE on session)'
    exit 4
}

foreach ($g in $games) {
    $a = @{
        Game    = $g
        Execute = $true
        Live    = $true
        Build   = ($g -eq $games[0])
    }
    if ($CheatExe) { $a.CheatExe = $CheatExe }
    & (Join-Path $Driver3 'single-pc-play.ps1') @a
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

. (Join-Path $Driver2 'lib\Invoke-Driver2RankedVerify.ps1')
Invoke-Driver2RankedVerify -Profile $games[-1] | Out-Null

$ready = Test-Driver2RankedReady -RequireMapProof
($ready | ConvertTo-Json -Depth 8) | Set-Content $out -Encoding utf8
& (Join-Path $Driver3 'single-pc-s-tier.ps1') | Out-Null
& (Join-Path $Driver3 'build-s-tier.ps1') -AllTitles | Out-Null

Write-Host "[+] $($ready.tier) verified_100=$($ready.verified_100) ranked_ready=$($ready.ranked_ready) → $out"
if (-not $ready.s_tier) {
    $ready.issues | ForEach-Object { Write-Host "    [!] $_" }
    Write-Host '    Run: .\driver2\confirm-ranked-100.ps1 -Title both'
}
exit $(if ($ready.s_tier) { 0 } elseif ($ready.ranked_ready) { 5 } else { 6 })
