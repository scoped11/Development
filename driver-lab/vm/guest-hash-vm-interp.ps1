# Static hash check on AI_workspace. Drivers are NOT loaded.
$ErrorActionPreference = 'Continue'
Write-Output "Z=$([bool](Test-Path 'Z:\ac-captures-host\2026-11-18\BEDaisy.sys'))"
Write-Output "hostname=$env:COMPUTERNAME"
foreach ($p in @(
    'C:\re\artifacts\EasyAntiCheat_EOS.sys',
    'C:\re\artifacts\EasyAntiCheat.sys',
    'C:\re\artifacts\BEDaisy.sys'
)) {
    if (Test-Path $p) {
        Write-Output ("HASH " + [IO.Path]::GetFileName($p) + " " + (Get-FileHash $p -Algorithm SHA256).Hash)
    } else {
        Write-Output ("MISSING " + $p)
    }
}
Get-Service BEService -ErrorAction SilentlyContinue | ForEach-Object {
    Write-Output ("SERVICE " + $_.Name + " status=" + $_.Status + " start=" + $_.StartType)
}
Write-Output "DONE"
