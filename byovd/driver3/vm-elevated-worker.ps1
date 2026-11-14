# Thin wrapper — real work is driver-lab\vm\bg\worker.ps1 (looping copy on C:\DriverLab\bg).
$ErrorActionPreference = 'Continue'
$bg = 'Z:\driver-lab\vm\bg\worker.ps1'
if (-not (Test-Path $bg)) { $bg = 'C:\DriverLab\bg\worker.ps1' }
if (-not (Test-Path $bg)) { exit 0 }
Copy-Item -LiteralPath $bg -Destination 'C:\DriverLab\bg\worker.ps1' -Force -ErrorAction SilentlyContinue
& $bg -Loop
exit $LASTEXITCODE
