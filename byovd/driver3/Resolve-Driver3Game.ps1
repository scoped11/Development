# driver3 game + AC catalog (EAC / BattlEye / ACE + aliases).
$script:Driver3CatalogPath = Join-Path $PSScriptRoot 'games.json'
$script:Driver3Catalog = $null

function Get-Driver3Catalog {
    if (-not $script:Driver3Catalog) {
        if (-not (Test-Path $script:Driver3CatalogPath)) {
            throw "Missing $script:Driver3CatalogPath"
        }
        $script:Driver3Catalog = Get-Content $script:Driver3CatalogPath -Raw | ConvertFrom-Json
    }
    return $script:Driver3Catalog
}

. (Join-Path (Split-Path $PSScriptRoot -Parent) 'driver2\Resolve-Driver2Profile.ps1')

function Resolve-Driver3Profile {
    param([string]$Name)
    if (-not $Name) { return (Get-Driver3Catalog).default_profile }
    $cat = Get-Driver3Catalog
    $k = $Name.ToLower()
    if ($cat.profiles.PSObject.Properties.Name -contains $k) { return $k }
    if ($cat.aliases.PSObject.Properties.Name -contains $k) {
        return $cat.aliases.$k
    }
    return (Resolve-Driver2Profile $Name)
}

function Get-Driver3ProfileNames {
    $cat = Get-Driver3Catalog
    @($cat.profiles.PSObject.Properties.Name | Where-Object { $_ -ne 'universal' })
}

function Get-Driver3Meta {
    param([string]$Profile)
    $p = Resolve-Driver3Profile $Profile
    $cat = Get-Driver3Catalog
    if (-not ($cat.profiles.PSObject.Properties.Name -contains $p)) {
        return [pscustomobject]@{
            profile = $p
            title = $p
            ac_family = 'unknown'
            usermode_ac_patterns = @()
        }
    }
    $m = $cat.profiles.$p
    $docRel = if ($m.doc) { $m.doc } else { '' }
  $docPath = if ($docRel) { Join-Path $PSScriptRoot $docRel } else { '' }
    [pscustomobject]@{
        profile               = $p
        title                 = $m.title
        ac_family             = $m.ac_family
        doc                   = $docRel
        doc_path              = $docPath
        driver2_profile_file  = $m.driver2_profile_file
        capture_dump          = $m.capture_dump
        usermode_ac_patterns  = @($m.usermode_ac_patterns)
        kernel_modules_hint   = @($m.kernel_modules_hint)
        host_b                = $m.host_b
    }
}

function Show-Driver3RouteDoc {
    param([string]$Profile)
    $meta = Get-Driver3Meta $Profile
    if ($meta.doc_path -and (Test-Path $meta.doc_path)) {
        Write-Host "[doc] $($meta.doc) — method, route, R/W, tiers"
        return $meta.doc_path
    }
    Write-Host "[doc] See driver3/drivers/README.md"
    return $null
}

function Get-Driver3AcProcesses {
    param(
        [string]$Profile = '',
        [switch]$AnyKnownAc,
        [switch]$TitleScopedOnly
    )
    $cat = Get-Driver3Catalog
    $patterns = [System.Collections.Generic.List[string]]::new()

    if ($AnyKnownAc) {
        foreach ($fam in $cat.ac_family_patterns.PSObject.Properties) {
            foreach ($pat in $fam.Value) { $patterns.Add([string]$pat) }
        }
    } elseif ($Profile) {
        $meta = Get-Driver3Meta $Profile
        if ($TitleScopedOnly) {
            foreach ($pat in $meta.usermode_ac_patterns) { $patterns.Add($pat) }
        } else {
            foreach ($pat in $meta.usermode_ac_patterns) { $patterns.Add($pat) }
            if ($meta.ac_family -and $meta.ac_family -ne 'none') {
                $fam = $cat.ac_family_patterns.($meta.ac_family)
                if ($fam) { foreach ($pat in $fam) { if ($patterns -notcontains $pat) { $patterns.Add([string]$pat) } } }
            }
        }
    } else {
        return Get-Driver3AcProcesses -AnyKnownAc
    }

    if ($patterns.Count -eq 0) { return @() }
    $regex = ($patterns | ForEach-Object { [regex]::Escape($_) }) -join '|'
    @(Get-Process -ErrorAction SilentlyContinue | Where-Object { $_.ProcessName -match $regex })
}

function Test-Driver3LiveAcForTitle {
    param([string]$Profile)
    $procs = Get-Driver3AcProcesses -Profile $Profile
    return ($procs.Count -gt 0)
}

function Get-Driver3HostBDeveloperFlags {
    param([string]$Profile)
    $meta = Get-Driver3Meta $Profile
    $hb = $meta.host_b
    @{
        Profile    = if ($hb.runner_profile) { $hb.runner_profile } else { $meta.profile }
        IncludeAce = [bool]$hb.developer_include_ace
        IncludeBe  = [bool]$hb.developer_include_be
    }
}

function Get-Driver3LaneDMessage {
    param(
        [string]$Profile,
        [bool]$LaneD,
        [int]$PreflightExit
    )
    $meta = Get-Driver3Meta $Profile
    $fam = $meta.ac_family
    if ($LaneD) {
        return "Tier 0 ($fam on $($meta.title)) — Host A DMA only; driver2 on VM profile $($meta.profile)"
    }
    switch ($PreflightExit) {
        0 { return "Vehicle open — Lane D optional unless live $fam session on game PC" }
        1 { return "Vehicle closed — kernel proof on Host B; driver2 profile $($meta.profile)" }
        default { return "Heuristic gate for $($meta.title) ($fam)" }
    }
}
