# Plan-only: show what rust/r6 dumps say AC tracks + expected ranked-safe IOCTLs.
param(
    [ValidateSet('rust', 'r6', 'both')]
    [string]$Title = 'both'
)

$Driver2 = $PSScriptRoot
. (Join-Path $Driver2 'lib\Get-Driver2DumpTracking.ps1')

$games = if ($Title -eq 'both') { @('rust', 'r6') } else { ,$Title }
foreach ($g in $games) {
    Write-Host "`n=== dump tracking: $g ==="
    $m = Get-Driver2DumpTrackingModel $g
    Write-Host "  policy: $($m.policy_tier)"
    if ($m.manifest.driver_sha256) {
        Write-Host "  capture driver_sha256: $($m.manifest.driver_sha256.Substring(0, 16))..."
    }
    Write-Host "  kernel hunt (high): $($m.kernel_hunt_high -join ', ')"
    Write-Host "  session pf_diag: $($m.session_pf_diag -join ', ')"
    Write-Host "  forbid bypass: $($m.forbid_bypass_ioctl -join ', ')"
    $cl = ($m.session_client_cmds | ForEach-Object { "$($_.cmd) $($_.args)" }) -join ', '
    Write-Host "  post-map client: $cl"
}

Write-Host "`n[*] Full model: AC_FINGERPRINTS.md · index: dumps\ac_fingerprints_index.json"
