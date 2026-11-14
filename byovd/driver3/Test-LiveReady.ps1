param(
    [string]$Game = 'rust',
    [switch]$StrictDump
)

. (Join-Path $PSScriptRoot 'lib\Test-Driver3LiveReady.ps1')
$r = Test-Driver3LiveReady -Profile $Game -StrictDump:$StrictDump
$r.checks | ForEach-Object { Write-Host ("  [{0}] {1} {2}" -f $(if ($_.pass) { '+' } else { '!' }), $_.name, $_.detail) }
if ($r.ok) { Write-Host '[+] live ready (Lane D Host A — not undetected)'; exit 0 }
$r.issues | ForEach-Object { Write-Host "[!] $_" }
exit 4
