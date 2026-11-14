param([switch]$AllTitles, [string]$Game = 'rust')

$Driver3 = $PSScriptRoot
& (Join-Path $Driver3 'set-live-read-method.ps1') -Method kernel_lab_s -Game $Game
$env:DRIVER3_S_TIER = '1'
& (Join-Path $Driver3 'reach-scale-s.ps1') -AllTitles:$AllTitles -SkipVm
& (Join-Path $Driver3 'build-s-tier.ps1') -AllTitles:$AllTitles
$s = Get-Content (Join-Path $Driver3 's_tier.json') -Raw | ConvertFrom-Json
Write-Host "[*] Kernel Lab S: $($s.tier) $($s.score_pct)% method=kernel_lab_s"
exit $(if ($s.s_tier) { 0 } else { 7 })
