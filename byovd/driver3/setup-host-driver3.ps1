# Host A (this PC): print role, paths, quick gates.
$Driver3 = $PSScriptRoot
$Byovd = Split-Path $Driver3 -Parent

Write-Host ''
Write-Host '=== driver3 on THIS PC (Host A) ==='
Write-Host "    Computer: $env:COMPUTERNAME"
Write-Host '    Docs:     driver3\docs\HOST_PC_READ_WRITE.md'
Write-Host '    VM docs:  driver3\docs\VM_GUEST_READ_WRITE.md (run inside guest too)'
Write-Host ''

& (Join-Path $Driver3 'lane-gate.ps1') -Game rust
& (Join-Path $Driver3 'sync-build-id-from-dumps.ps1') -Game rust -AllTitles
& (Join-Path $Driver3 'build-dashboard.ps1') -AllTitles
& (Join-Path $Driver3 'build-s-tier.ps1') -AllTitles

Write-Host ''
Write-Host '[*] Next: VM map from host → .\vm-guest-hostb-from-host.ps1 -Profile rust -ForceMap'
Write-Host '    Full 100%:  .\complete-100-percent.ps1'
Write-Host ''
