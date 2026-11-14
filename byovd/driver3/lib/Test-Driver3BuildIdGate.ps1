function Test-Driver3BuildIdGate {
    param(
        [string]$Profile = 'rust',
        [switch]$Strict
    )

    $Driver3Root = Split-Path $PSScriptRoot -Parent
    . (Join-Path $Driver3Root 'Resolve-Driver3Game.ps1')
    $profile = Resolve-Driver3Profile $Profile
    $manifestPath = Join-Path (Split-Path $Driver3Root -Parent) 'docs\lab_manifest.json'

    $issues = [System.Collections.Generic.List[string]]::new()
    if (-not (Test-Path $manifestPath)) {
        $issues.Add("missing manifest: $manifestPath")
    } else {
        $m = Get-Content $manifestPath -Raw | ConvertFrom-Json
        $bid = [string]$m.lane_d.build_id
        $profileBid = $null
        if ($m.lane_d.by_profile) {
            $bp = $m.lane_d.by_profile
            if ($bp.PSObject.Properties.Name -contains $profile) {
                $profileBid = [string]$bp.$profile.build_id
            }
        }
        $effective = if ($profileBid) { $profileBid } else { $bid }
        if (-not $effective -or $effective -match '^\s*$|replace|changenumber|TODO') {
            $issues.Add('build_id empty — run sync-build-id-from-dumps.ps1 -AllTitles')
        }
        if (-not $profileBid -and $m.lane_d.game -and $m.lane_d.game -ne $profile) {
            $issues.Add("manifest game=$($m.lane_d.game) but profile=$profile — sync per-title by_profile")
        }
        if ($m.lane_d.dma_status -eq 'missing') {
            $issues.Add('lane_d.dma_status=missing')
        }
    }

    $devRoot = Split-Path (Split-Path $Driver3Root -Parent) -Parent
    $dumps = Join-Path $devRoot 'dumps'
    if ($Strict -and (Test-Path $dumps)) {
        $latest = Get-ChildItem $dumps -Directory -ErrorAction SilentlyContinue |
            Sort-Object Name -Descending | Select-Object -First 1
        if (-not $latest) { $issues.Add('no dumps/<date>/ folder for offset truth') }
    }

    [pscustomobject]@{
        ok      = ($issues.Count -eq 0)
        profile = $profile
        issues  = @($issues)
    }
}
