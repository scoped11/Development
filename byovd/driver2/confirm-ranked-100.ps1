# 100% ranked verification: map + Driver2Client probe + rust/r6 gates (admin).
#Requires -RunAsAdministrator
param(
    [ValidateSet('rust', 'r6', 'both')]
    [string]$Title = 'both'
)

$ErrorActionPreference = 'Stop'
$Driver2 = $PSScriptRoot
$Driver3 = Join-Path (Split-Path $Driver2 -Parent) 'driver3'

Write-Host '=== CONFIRM RANKED 100% (map + probe + gates) ==='

& (Join-Path $Driver2 'enable-ranked-default.ps1')
$Byovd = Split-Path $Driver2 -Parent
if (Test-Path (Join-Path $Byovd 'build.ps1')) { & (Join-Path $Byovd 'build.ps1') }
& (Join-Path $Driver2 'build.ps1') -BuildExamples

$games = if ($Title -eq 'both') { @('rust', 'r6') } else { ,$Title }
$allOk = $true

foreach ($g in $games) {
    Write-Host "`n--- verify title: $g ---"
    & (Join-Path $Driver3 'single-pc-play.ps1') -Game $g -Execute -Live -RankedSafe -Build:$(if ($g -eq $games[0]) { $true } else { $false })
    if ($LASTEXITCODE -ne 0) { $allOk = $false; continue }

    . (Join-Path $Driver2 'lib\Invoke-Driver2RankedVerify.ps1')
    $v = Invoke-Driver2RankedVerify -Profile $g
    if (-not $v.ok) {
        Write-Host "[!] probe failed profile=$g exit=$($v.probe_exit)"
        $allOk = $false
    } else {
        Write-Host "[+] probe OK profile=$g"
    }
}

& (Join-Path $Driver2 'ranked-ready.ps1') -PreflightOnly -Title both
. (Join-Path $Driver2 'lib\Test-Driver2RankedReady.ps1')
$final = Test-Driver2RankedReady -RequireMapProof

$report = Join-Path $Driver2 'ranked_100_confirm.json'
[ordered]@{
    schema     = 'ranked_100_confirm_v1'
    utc        = (Get-Date).ToUniversalTime().ToString('o')
    confirmed  = ($allOk -and $final.s_tier -and $final.score_pct -eq 100)
    driver2    = $final
    titles     = $games
} | ConvertTo-Json -Depth 10 | Set-Content $report -Encoding utf8

Write-Host "`n[+] report -> $report"
Write-Host "    tier=$($final.tier) s_tier=$($final.s_tier) score=$($final.score_pct)%"

if (-not $allOk -or -not $final.s_tier) {
    $final.issues | ForEach-Object { Write-Host "    [!] $_" }
    exit 6
}

Write-Host '[+] RANKED 100% CONFIRMED — Driver2 probe passed; use your EXE + game.'
exit 0
