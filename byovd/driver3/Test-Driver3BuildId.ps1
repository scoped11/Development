param(
    [string]$Profile = 'rust',
    [switch]$Strict
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'lib\Test-Driver3BuildIdGate.ps1')
$r = Test-Driver3BuildIdGate -Profile $Profile -Strict:$Strict
$r.issues | ForEach-Object { Write-Host "[!] $_" }
if ($r.ok) { Write-Host '[+] build_id gate OK' }
exit $(if ($r.ok) { 0 } else { 3 })
