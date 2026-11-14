param(
    [string]$Vid,
    [string]$Pid,
    [ValidateSet("stock", "logitech_lightspeed_c547", "logitech_c547")]
    [string]$Preset,
    [switch]$Stock,
    [switch]$Capture
)

$ErrorActionPreference = "Stop"
$fw = Join-Path $PSScriptRoot "firmware"
$path = Join-Path $fw "kmbox_identity.txt"

if ($Capture) {
    & (Join-Path $PSScriptRoot "Capture-GameMouseIdentity.ps1") -OutDir $fw
    exit $LASTEXITCODE
}

if ($Stock -or $Preset -eq "stock") {
    & (Join-Path $PSScriptRoot "Capture-GameMouseIdentity.ps1") -OutDir $fw -Stock
    exit 0
}

if ($Preset -eq "logitech_lightspeed_c547" -or $Preset -eq "logitech_c547") {
    & (Join-Path $PSScriptRoot "Capture-GameMouseIdentity.ps1") -OutDir $fw -UseLogitechPreset
    exit 0
}

if (-not $Vid -or -not $Pid) {
    Write-Host "Usage:"
    Write-Host "  .\Set-KmboxIdentity.ps1 -Vid 046d -Pid c547"
    Write-Host "  .\Set-KmboxIdentity.ps1 -Preset logitech_lightspeed_c547"
    Write-Host "  .\Set-KmboxIdentity.ps1 -Stock"
    Write-Host "  .\Set-KmboxIdentity.ps1 -Capture"
    exit 1
}

$Vid = ($Vid.ToLower() -replace '^0x', '')
$Pid = ($Pid.ToLower() -replace '^0x', '')
@"
# USB identity toward the GAME PC (HID port).
# ROBRUN_IDENTITY_V3
preset=custom
vid=$Vid
pid=$Pid
"@ | Set-Content -Path $path -Encoding UTF8
Write-Host "[+] Wrote firmware\kmbox_identity.txt vid=$Vid pid=$Pid"
Write-Host "[*] Run Flash-KMBox.bat, then power-cycle both USB cables on the box."
