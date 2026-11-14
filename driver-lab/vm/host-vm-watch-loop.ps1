# Run on host during long VM hunts: stuck check every ~4 minutes + optional recover.
param(
    [int]$IntervalSec = 240,
    [int]$StallThresholdSec = 270,
    [switch]$Recover
)
$watch = Join-Path $PSScriptRoot 'host-vm-stuck-watchdog.ps1'
while ($true) {
    Write-Host "[watch-loop] Next stuck-watchdog in 0s (interval ${IntervalSec}s, stall ${StallThresholdSec}s)."
    & $watch -CheckSec $IntervalSec -StallThresholdSec $StallThresholdSec -Recover:$Recover
    $code = $LASTEXITCODE
    if ($code -eq 3) { Write-Host "[!] stuck detected at $(Get-Date -Format o)" }
    Start-Sleep -Seconds $IntervalSec
}
