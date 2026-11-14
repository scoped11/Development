param(
    [string]$Profile = 'rust',
    [switch]$AllTitles,
    [string]$OutJson = ''
)

$ErrorActionPreference = 'Stop'
$Driver3 = $PSScriptRoot
if (-not $OutJson) { $OutJson = Join-Path $Driver3 's_tier.json' }

. (Join-Path $Driver3 'lib\Test-STier.ps1')
& (Join-Path $Driver3 'build-dashboard.ps1') -Profile $Profile -AllTitles:$AllTitles | Out-Null

$result = Test-Driver3STier -Profile $Profile -AllTitles:$AllTitles

$dashPath = Join-Path $Driver3 'dashboard.json'
if (Test-Path $dashPath) {
    $dash = Get-Content $dashPath -Raw | ConvertFrom-Json
    if ($dash -is [pscustomobject]) {
        $dash | Add-Member -NotePropertyName s_tier -NotePropertyValue $result.tier -Force
        $dash | Add-Member -NotePropertyName s_tier_pct -NotePropertyValue $result.score_pct -Force
        $dash | Add-Member -NotePropertyName s_tier_issues -NotePropertyValue @($result.issues) -Force
        ($dash | ConvertTo-Json -Depth 14) | Set-Content $dashPath -Encoding utf8
    }
}

($result | ConvertTo-Json -Depth 10) | Set-Content $OutJson -Encoding utf8

Write-Host "[+] S-tier grade=$($result.tier) score=$($result.score_pct)% -> $OutJson"
$result.issues | ForEach-Object { Write-Host "    [!] $_" }

exit $(if ($result.s_tier) { 0 } elseif ($result.score_pct -ge 70) { 5 } else { 6 })
