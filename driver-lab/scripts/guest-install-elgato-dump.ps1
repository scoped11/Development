# VM elevated: install Elgato 4K Capture Utility payload from share, dump new kernel modules to audit.
$ErrorActionPreference = 'Stop'
$msi = 'Z:\driver-lab\audit\inbox\oem-raw\4KCaptureUtility_1.7.16.10419_x64.msi'
$drvInst = 'Z:\driver-lab\audit\inbox\oem-extract\elgato-4k\DRVInstaller.exe'
$out = 'C:\DriverLab\audit-dump'
New-Item -ItemType Directory -Force -Path $out | Out-Null
if (Test-Path $msi) {
    Start-Process msiexec.exe -ArgumentList @('/i', $msi, '/qn', '/norestart') -Wait
}
if (Test-Path $drvInst) {
    Start-Process $drvInst -ArgumentList @('/S') -Wait -ErrorAction SilentlyContinue
}
& 'Z:\driver-lab\scripts\enumerate-local-drivers.ps1' -CopyToInbox | Tee-Object -FilePath (Join-Path $out 'enumerate.log')
Copy-Item 'Z:\driver-lab\audit\inbox\oem-sys\x64\*.sys' $out -ErrorAction SilentlyContinue
Write-Output "dumped to $out — sync host from Z:\driver-lab\audit"
