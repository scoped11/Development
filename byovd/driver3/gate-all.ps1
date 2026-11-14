# Run lane gate for every driver3 title; writes driver3_gates_all.json
param([switch]$Execute)

$ErrorActionPreference = 'Continue'
$Driver3 = $PSScriptRoot
. (Join-Path $Driver3 'Resolve-Driver3Game.ps1')

$names = Get-Driver3ProfileNames
$results = [ordered]@{}
$worst = 0

foreach ($p in $names) {
    Write-Host ''
    Write-Host "----- gate: $p -----"
    & (Join-Path $Driver3 'lane-gate.ps1') -Game $p
    $ec = $LASTEXITCODE
    if ($ec -gt $worst) { $worst = $ec }
    $gj = Join-Path $Driver3 'driver3_gate.json'
    if (Test-Path $gj) {
        $results[$p] = Get-Content $gj -Raw | ConvertFrom-Json
    }
}

$out = [ordered]@{
    utc      = (Get-Date).ToUniversalTime().ToString('o')
    profiles = $results
    summary  = [ordered]@{
        lane_d_recommended = @($results.Keys | Where-Object { $results.$_.lane_d })
        host_a_only        = @($results.Keys | Where-Object { $results.$_.recommend_host -eq 'A' })
        worst_exit         = $worst
    }
}

$allPath = Join-Path $Driver3 'driver3_gates_all.json'
($out | ConvertTo-Json -Depth 8) | Set-Content $allPath -Encoding utf8
Write-Host ''
Write-Host "[+] Wrote $allPath (worst_exit=$worst)"
if ($Execute -and $worst -eq 2) {
    Write-Host '[*] At least one title needs Host A on this session — see summary.lane_d_recommended'
}
exit $worst
