# Elevated body: driver3 Host B map (VM only). Profile via env (Guest Additions safe).
$ErrorActionPreference = 'Stop'

$Profile = $env:DRIVER3_HOSTB_PROFILE
if (-not $Profile) { $Profile = 'rust' }
$ForceMap = ($env:DRIVER3_HOSTB_FORCEMAP -eq '1')
$SkipProve = ($env:DRIVER3_HOSTB_SKIPPROVE -eq '1')

$Driver3 = 'Z:\byovd\driver3'
$log = Join-Path $Driver3 'vm_hostb_execute.log'
function Log($m) { $line = "$(Get-Date -Format o) $m"; Add-Content $log $line; Write-Host $line }

Log "=== host-b execute guest=$env:COMPUTERNAME profile=$Profile admin=$((([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)))"

$params = @{ Profile = $Profile; Execute = $true }
if ($ForceMap) { $params.ForceMap = $true }
if ($SkipProve) { $params.SkipProve = $true }
& (Join-Path $Driver3 'host-b.ps1') @params
$ec = $LASTEXITCODE
Log "host-b exit=$ec"
exit $ec
