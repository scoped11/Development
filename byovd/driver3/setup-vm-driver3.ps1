# Guest (Z:\byovd\driver3): Host B role + smoke.
$Driver3 = if ($PSScriptRoot) { $PSScriptRoot } else { 'Z:\byovd\driver3' }
Write-Host '=== driver3 on VM (Host B) ==='
Write-Host "    Guest: $env:COMPUTERNAME"
Write-Host '    Read/write: Z:\byovd\driver3\docs\VM_GUEST_READ_WRITE.md'
Write-Host ''

& (Join-Path $Driver3 'vm-guest-smoke.ps1') -Profile rust
Write-Host ''
Write-Host '[*] Elevated once: register-driver3-elevated-task.ps1'
Write-Host '[*] Map proof:     host-b.ps1 -Profile rust -Execute -ForceMap'
