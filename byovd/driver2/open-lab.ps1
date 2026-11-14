# Driver2 lab launchers — run from driver2\ (canonical).
$Driver2 = $PSScriptRoot
$Bin = Join-Path $Driver2 'bin'
& (Join-Path $Driver2 'sync-driver2-bin.ps1') | Out-Null
if (-not (Test-Path (Join-Path $Bin 'DriverLoader.exe'))) {
    throw "Build first: driver2\build.ps1 -BuildLoaders"
}
Start-Process -FilePath (Join-Path $Bin 'DriverLoader.exe') -Verb RunAs -WorkingDirectory $Bin
Start-Sleep -Seconds 1
Start-Process -FilePath (Join-Path $Bin 'CheatLoader.exe') -WorkingDirectory $Bin
