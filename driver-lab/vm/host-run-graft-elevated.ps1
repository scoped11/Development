# Host: wcifs graft lab on guest at HIGHEST (no UAC / no filtered token).
param(
    [string]$VmName = 'DriverLab-E-26200',
    [switch]$NoVerify
)
$bg = Join-Path $PSScriptRoot 'bg'
$admin = Join-Path $bg 'host-bg-admin.ps1'
$graft = '-NoVerify'
if (-not $NoVerify) { $graft = '' }
$ps = "& 'Z:\driver-lab\vm\bg\guest-authenticode-graft-lab.ps1' $graft".Trim()
& $admin -Action exec -PowerShell $ps -TimeoutSec 120 -PollSec 2 -StallSec 25