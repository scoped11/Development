# Quick post-map sanity: Driver2Client probe (no remapping).
param([ValidateSet('rust', 'r6')][string]$Profile = 'rust')

. (Join-Path $PSScriptRoot 'lib\Invoke-Driver2RankedVerify.ps1')
$v = Invoke-Driver2RankedVerify -Profile $Profile
Write-Host "[*] probe_exit=$($v.probe_exit) ok=$($v.ok) -> driver2_ranked_verify.json"
exit $(if ($v.ok) { 0 } else { 5 })
