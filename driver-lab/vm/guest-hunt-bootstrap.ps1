# Run INSIDE the lab VM (after Windows install). Enables crash capture + Lane E hunt loop.
#Requires -RunAsAdministrator
$ErrorActionPreference = 'Stop'

$Lab = 'D:\hostdev\driver-lab'
if (-not (Test-Path $Lab)) {
    Write-Warning "Shared folder D:\hostdev not mounted. Install Guest Additions, reboot, retry."
    exit 1
}

# Kernel crash dumps
if (-not (Test-Path 'C:\crash')) { New-Item -ItemType Directory -Path 'C:\crash' | Out-Null }
cmd /c "wmic recoveros set DebugInfoType = 1 DebugFilePath = C:\crash\MEMORY.DMP AutoReboot = False" | Out-Null

# Disable sleep (fuzz sessions)
powercfg -change standby-timeout-ac 0
powercfg -change monitor-timeout-ac 0

# Record guest build next to host manifest
$build = (Get-ItemProperty 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion')
$info = @{
    guest_build = "$($build.CurrentBuildNumber).$($build.UBR)"
    lane_e      = @{ test_host_profile = 'E-VM'; primitive_id = ''; levels_passed = @() }
    started_utc = (Get-Date).ToUniversalTime().ToString('o')
}
$out = Join-Path $Lab 'exploit\notes\vm-guest-build.json'
$info | ConvertTo-Json -Depth 4 | Set-Content $out -Encoding UTF8

# Hunt phase 1: safe enumeration (no fuzz payloads in this script)
& (Join-Path $Lab 'scripts\enumerate-local-drivers.ps1') -ErrorAction SilentlyContinue
& (Join-Path $Lab 'scripts\probe-hijack-candidates.ps1') -OpenDeviceProbe

Write-Host "[+] Guest bootstrap done. Build logged to $out"
Write-Host "Next: VM-only CLFS / kernel hunt per driver-lab\docs\24_vm_exploit_hunt.md"
Write-Host "On bugcheck: copy C:\crash\MEMORY.DMP to D:\hostdev\driver-lab\exploit\notes\ then revert VM snapshot."
