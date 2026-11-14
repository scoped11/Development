. (Join-Path $PSScriptRoot 'lib\Test-SinglePcSTier.ps1')
$r = Test-SinglePcSTier
$out = Join-Path $PSScriptRoot 'single_pc_s.json'
($r | ConvertTo-Json -Depth 6) | Set-Content $out -Encoding utf8
Write-Host "[+] single_pc_s tier=$($r.tier) score=$($r.score_pct)% s_tier=$($r.s_tier) -> $out"
exit $(if ($r.s_tier) { 0 } else { 5 })
