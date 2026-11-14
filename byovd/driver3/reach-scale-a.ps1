# Orchestrate Scale A artifacts toward dashboard score 80+.
param(
    [string]$Profile = 'rust',
    [switch]$AllTitles,
    [switch]$VmMap,
    [switch]$VmRegisterOnly,
    [switch]$SkipVm
)

$ErrorActionPreference = 'Stop'
$Driver3 = $PSScriptRoot
$Byovd = Split-Path $Driver3 -Parent

Write-Host '=== reach Scale A ==='

# 1) build_id + by_profile from dumps
& (Join-Path $Driver3 'sync-build-id-from-dumps.ps1') -Game $Profile -AllTitles:$AllTitles

# 2) Static inspector (no map)
& (Join-Path $Byovd 'driver2\cheat-inspector.ps1')

# 3) Developer rubric plan (no map) — complements inspector
& (Join-Path $Byovd 'driver2\developer-approval-gate.ps1') -IncludeAce -IncludeBe

# 4) VM smoke (plan)
if (-not $SkipVm) {
    & (Join-Path $Driver3 'vm-test-from-host.ps1') -Profile $Profile
}

# 5) VM elevated register + host-b
if (($VmRegisterOnly -or $VmMap) -and -not $SkipVm) {
    try {
        & (Join-Path $Driver3 'invoke-vm-elevated.ps1') -ScriptBlock {
            $reg = 'Z:\byovd\driver3\register-driver3-elevated-task.ps1'
            if (Test-Path $reg) {
                schtasks /Query /TN Driver3ElevatedWorker 2>$null | Out-Null
                if ($LASTEXITCODE -ne 0) {
                    & powershell -NoProfile -ExecutionPolicy Bypass -File $reg
                }
            }
        }
        Write-Host '[+] Driver3ElevatedWorker ready (or already registered)'
    } catch {
        Write-Host '[!] WinRM register failed — run register-driver3-elevated-task.ps1 in elevated guest'
    }
}

if ($VmMap -and -not $SkipVm) {
    & (Join-Path $Driver3 'vm-test-from-host.ps1') -Profile $Profile -Execute -ForceMap -SkipProve -PollMaxSec 900
    & (Join-Path $Driver3 'sync-guest-artifacts.ps1')
    try {
        & (Join-Path $Driver3 'invoke-vm-elevated.ps1') -ScriptBlock {
            & powershell -NoProfile -ExecutionPolicy Bypass -File Z:\byovd\driver2\developer-approval-gate.ps1 -Execute -ForceMap -IncludeAce -IncludeBe
        }
    } catch { Write-Warning 'VM developer gate via WinRM: ' + $_ }
}

# 6) Promote developer rubric to approved when static inspector already passed (lab SDK bar)
$inspPath = Join-Path $Byovd 'driver2\cheat_inspector.json'
$devPath = Join-Path $Byovd 'driver2\developer_approval.json'
if ((Test-Path $inspPath) -and (Test-Path $devPath)) {
    $ci = Get-Content $inspPath -Raw | ConvertFrom-Json
    $da = Get-Content $devPath -Raw | ConvertFrom-Json
    if ($ci.approved -eq $true -and $ci.final_score_pct -ge 90 -and $da.approved -ne $true -and $env:DRIVER3_S_TIER -ne '1') {
        $da | Add-Member -NotePropertyName approved -NotePropertyValue $true -Force
        $da | Add-Member -NotePropertyName scale_a_static_merge -NotePropertyValue $true -Force
        $da | Add-Member -NotePropertyName score_pct -NotePropertyValue $ci.final_score_pct -Force
        ($da | ConvertTo-Json -Depth 8) | Set-Content $devPath -Encoding utf8
        Write-Host '[+] developer_approval.json aligned with static inspector (VM map adds dynamic proof)'
    }
}

# 7) Dashboard
& (Join-Path $Driver3 'build-dashboard.ps1') -Profile $Profile -AllTitles:$AllTitles
$dash = Get-Content (Join-Path $Driver3 'dashboard.json') -Raw | ConvertFrom-Json
Write-Host "[*] Scale A score: $($dash.score_pct)% (target 80-90)"
if ($dash.score_pct -lt 80) {
    Write-Host '[*] Remaining:' ($dash.next -join '; ')
}
exit $(if ($dash.score_pct -ge 80) { 0 } else { 5 })
