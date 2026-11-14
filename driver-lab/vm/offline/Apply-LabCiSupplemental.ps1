# Elevated guest: deploy supplemental policy so lab-signed kernel code loads with testsigning OFF.
#Requires -RunAsAdministrator
$ErrorActionPreference = 'Stop'
$Share = if (Test-Path 'Z:\driver-lab\vm') { 'Z:\driver-lab\vm' } else { Split-Path -Parent $PSScriptRoot }
$cip = Join-Path $Share 'vault\staged\driverlab-supplemental.cip'
if (-not (Test-Path $cip)) { throw "Missing $cip — host runs Build-LabCiSupplemental.ps1" }

$log = Join-Path $Share 'bg\selfsign-live.log'
function L([string]$m) { Add-Content $log "$(Get-Date -Format o) $m"; Write-Host $m }

$metaPath = Join-Path $Share 'vault\staged\driverlab-ci.json'
$policyId = if (Test-Path $metaPath) { (Get-Content $metaPath -Raw | ConvertFrom-Json).policy_id } else { $null }
if (-not $policyId) { $policyId = ([guid]::NewGuid()).ToString('D').ToUpper() }
$policyId = ($policyId -replace '[{}]', '').ToUpper()
$active = Join-Path $env:windir 'System32\CodeIntegrity\CiPolicies\Active'
New-Item -ItemType Directory -Force -Path $active | Out-Null
$dest = Join-Path $active "{$policyId}.cip"
Copy-Item $cip $dest -Force
L "copied supplemental cip -> $dest"

$ciTool = Join-Path $env:windir 'System32\CiTool.exe'
if (Test-Path $ciTool) {
    # CiTool may prompt "Press Enter"; feed newline so elevated worker does not stall.
    $ciOut = cmd /c "echo.|`"$ciTool`" --update-policy `"$dest`"" 2>&1
    $ciOut | ForEach-Object { L $_ }
    L 'CiTool --update-policy complete (reboot if load still fails)'
} else {
    L 'CiTool missing — policy staged; reboot required'
}

@{
    schema    = 'apply_lab_ci_supplemental_v1'
    policy_id = $policyId
    utc       = (Get-Date).ToUniversalTime().ToString('o')
} | ConvertTo-Json | Set-Content (Join-Path $Share 'bg\ci-supplemental.json') -Encoding utf8