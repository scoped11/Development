function Invoke-Driver2RankedSafeGate {
    param(
        [ValidateSet('rust', 'r6', 'deltaforce', 'arenabreakout', 'arksa')]
        [string]$Profile = 'rust',
        [string]$Driver = 'rtcore',
        [switch]$Live,
        [switch]$AllowTier0Override
    )

    $Driver2 = Split-Path $PSScriptRoot -Parent
    $Byovd = Split-Path $Driver2 -Parent
    $Driver3 = Join-Path $Byovd 'driver3'
    $out = Join-Path $Driver2 'ranked_safe.json'

    . (Join-Path $Driver3 'Resolve-Driver3Game.ps1')
    $meta = Get-Driver3Meta $Profile

    $acTitle = @(Get-Driver3AcProcesses -Profile $Profile)
    $acAny = @(Get-Driver3AcProcesses -AnyKnownAc)

    $preflightExit = $null
    $vehicleOpen = $null
    $acPreflight = Join-Path $Byovd 'scripts\ac-preflight.ps1'
    if (Test-Path $acPreflight) {
        & $acPreflight -Driver $Driver 2>&1 | Out-Null
        $preflightExit = $LASTEXITCODE
        $vehicleOpen = ($preflightExit -eq 0)
    }

    $tier = switch ($preflightExit) {
        0 { 'tier1' }
        1 { 'tier1_prep' }
        2 { 'tier0' }
        default { 'unknown' }
    }

    $blocked = $false
    $reasons = [System.Collections.Generic.List[string]]::new()

    if ($tier -eq 'tier0') {
        $blocked = -not $AllowTier0Override
        $reasons.Add('Tier 0: AC up and vehicle not openable — ranked-safe blocks map')
    }
    if ($acTitle.Count -gt 0 -and -not $Live) {
        $blocked = $true
        $reasons.Add("Live $($meta.ac_family) for $($meta.title) — pass -Live to acknowledge kernel-on-session risk")
    }
    if ($acAny.Count -gt 0 -and $acTitle.Count -eq 0) {
        $reasons.Add('Other AC processes running (not title-scoped)')
    }

  $policy = [ordered]@{
        schema           = 'driver2_ranked_safe_v1'
        utc              = (Get-Date).ToUniversalTime().ToString('o')
        profile          = $Profile
        title            = $meta.title
        ac_family        = $meta.ac_family
        tier             = $tier
        vehicle_open     = $vehicleOpen
        ac_title_running = ($acTitle.Count -gt 0)
        live_ack         = [bool]$Live
        blocked          = $blocked
        map_policy       = 'session_ioctl_ac_safe'
        not_claimed      = @('undetected', 'ban_proof', 'ranked_undetected')
        reasons          = @($reasons)
        ok               = (-not $blocked)
    }

    ($policy | ConvertTo-Json -Depth 6) | Set-Content $out -Encoding utf8
    return [pscustomobject]$policy
}

function Test-Driver2RankedSafeFile {
    param([string]$Path = '')
    $Driver2 = Split-Path $PSScriptRoot -Parent
    if (-not $Path) { $Path = Join-Path $Driver2 'ranked_safe.json' }
    if (-not (Test-Path $Path)) { return $false }
    try {
        $p = Get-Content $Path -Raw | ConvertFrom-Json
        return ($p.ok -eq $true -and $p.map_policy -eq 'session_ioctl_ac_safe')
    } catch { return $false }
}
