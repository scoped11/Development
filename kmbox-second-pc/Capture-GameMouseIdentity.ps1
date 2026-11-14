# Captures HID mouse hardware IDs from this PC and writes firmware\kmbox_identity.txt
# Run on the GAME PC (or any PC with the target mouse plugged in).
param(
    [string]$OutDir = (Join-Path $PSScriptRoot "firmware"),
    [string]$CaptureDir = (Join-Path $PSScriptRoot "captures"),
    [switch]$Stock,
    [switch]$UseLogitechPreset
)

$ErrorActionPreference = "Stop"
New-Item -ItemType Directory -Force -Path $OutDir, $CaptureDir | Out-Null

$identityPath = Join-Path $OutDir "kmbox_identity.txt"
$stamp = Get-Date -Format "yyyy-MM-dd_HHmmss"
$jsonPath = Join-Path $CaptureDir "golden_mouse_$stamp.json"

if ($Stock) {
    @"
# USB identity toward the GAME PC (HID port).
preset=stock
vid=
pid=
"@ | Set-Content -Path $identityPath -Encoding UTF8
    Write-Host "[+] Stock identity -> $identityPath"
    exit 0
}

if ($UseLogitechPreset) {
    @"
# USB identity toward the GAME PC (HID port).
# ROBRUN_IDENTITY_V3
preset=logitech_lightspeed_c547
vid=046d
pid=c547
manufacturer=Logitech
product=USB Receiver
bcd_device=0110
"@ | Set-Content -Path $identityPath -Encoding UTF8
    Write-Host "[+] Logitech LIGHTSPEED C547 preset -> $identityPath"
    Write-Host "[*] Re-run Flash-KMBox.bat on the second PC, then power-cycle the box."
    exit 0
}

$mice = @(Get-PnpDevice -Class Mouse -Status OK -ErrorAction SilentlyContinue)
if (-not $mice) {
    Write-Error "No OK Mouse class devices found."
}

$pick = $null
foreach ($m in $mice) {
    if ($m.InstanceId -match 'HID\\VID_([0-9A-F]{4})&PID_([0-9A-F]{4})') {
        $pick = $m
        break
    }
}
if (-not $pick) {
    $pick = $mice[0]
}

if ($pick.InstanceId -notmatch 'VID_([0-9A-F]{4})&PID_([0-9A-F]{4})') {
    Write-Error "Could not parse VID/PID from: $($pick.InstanceId)"
}
$vid = $Matches[1].ToLower()
$pid = $Matches[2].ToLower()

$related = @(Get-PnpDevice -PresentOnly -ErrorAction SilentlyContinue |
    Where-Object { $_.InstanceId -match "VID_$($vid)&PID_$($pid)" })

$capture = [ordered]@{
    captured_at = (Get-Date).ToString("o")
    friendly_name = $pick.FriendlyName
    mouse_instance_id = $pick.InstanceId
    vid = $vid
    pid = $pid
    note = "MI_00 mouse collection preferred; full composite may include extra MI_xx paths."
    related_instance_ids = @($related | ForEach-Object { $_.InstanceId })
}

$capture | ConvertTo-Json -Depth 5 | Set-Content -Path $jsonPath -Encoding UTF8

@"
# USB identity toward the GAME PC (HID port).
# ROBRUN_IDENTITY_V3 — from Capture-GameMouseIdentity.ps1
# Source: $($pick.FriendlyName)
preset=custom
vid=$vid
pid=$pid
"@ | Set-Content -Path $identityPath -Encoding UTF8

Write-Host "[+] Wrote $identityPath  (vid=$vid pid=$pid)"
Write-Host "[+] Golden capture: $jsonPath"
Write-Host "[*] Copy firmware to second PC and Flash-KMBox.bat, or use overlay Write KMBox sidecar files."
