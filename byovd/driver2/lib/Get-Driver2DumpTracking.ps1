function Get-Driver2ProfileJsonPath {
    param([string]$Profile)
    $Driver2 = Split-Path $PSScriptRoot -Parent
    $map = @{
        rust = 'profiles\eac-rust.json'
        r6   = 'profiles\be-r6.json'
    }
    if (-not $map.ContainsKey($Profile)) { return $null }
    Join-Path $Driver2 $map[$Profile]
}

function Get-Driver2DumpManifest {
    param([string]$Profile)
    $Driver2 = Split-Path $PSScriptRoot -Parent
    $Byovd = Split-Path $Driver2 -Parent
    $Driver3 = Join-Path $Byovd 'driver3'
    . (Join-Path $Driver3 'lib\ProfileDump.ps1')
    $dir = Get-ProfileDumpDir $Profile
    if (-not $dir) { return $null }
    $man = Join-Path $dir 'manifest.json'
    if (-not (Test-Path $man)) { return $null }
    $slim = [ordered]@{}
    $j = $null
    if ((Get-Item $man).Length -gt 200000) {
        $raw = Get-Content $man -Raw
        foreach ($k in @('game', 'driver_sha256', 'host_os_build', 'captured_utc', 'note', 'bedaisy_sha256', 'beservice_r6s_sha256')) {
            if ($raw -match ('"' + $k + '"\s*:\s*"([^"]*)"')) { $slim[$k] = $Matches[1] }
        }
        $slim['dump_dir'] = $dir
        return [pscustomobject]$slim
    }
    $j = Get-Content $man -Raw | ConvertFrom-Json
    foreach ($k in @(
            'game', 'driver_sha256', 'host_os_build', 'captured_utc', 'note',
            'eos_driver_path', 'platform', 'bedaisy_sha256', 'beservice_r6s_sha256'
        )) {
        if ($j.PSObject.Properties.Name -contains $k -and $j.$k) { $slim[$k] = $j.$k }
    }
    if ($j.settings_json) { $slim['eos_settings'] = $j.settings_json }
    $slim['dump_dir'] = $dir
    return [pscustomobject]$slim
}

function Get-Driver2DumpTrackingModel {
    param([string]$Profile)

    $Driver2 = Split-Path $PSScriptRoot -Parent
    $pfPath = Get-Driver2ProfileJsonPath $Profile
    if (-not (Test-Path $pfPath)) { throw "No profile json for $Profile" }
    $pf = Get-Content $pfPath -Raw | ConvertFrom-Json
    $man = Get-Driver2DumpManifest $Profile

    $policyTier = switch ($pf.ac_family) {
        'eac_eos' { 'paid_eac' }
        'battleye' { 'paid_battleye' }
        default { 'unknown' }
    }

    $vehicles = @('RTCore64.sys', '\\.\RTCore64')
    if (Test-Path (Join-Path (Split-Path $Driver2 -Parent) 'driver-lab\data\ac_eac_be_vehicle_policy.json')) {
        $pol = Get-Content (Join-Path (Split-Path $Driver2 -Parent) 'driver-lab\data\ac_eac_be_vehicle_policy.json') -Raw |
            ConvertFrom-Json
        if ($pol.tiers.$policyTier) { $vehicles = @($pol.tiers.$policyTier.block_device_patterns) + $vehicles }
    }

    $cmds = @()
    if ($pf.dump_tracking -and $pf.dump_tracking.session_client_cmds) {
        $cmds = @($pf.dump_tracking.session_client_cmds)
    } else {
        switch ($Profile) {
            'rust' { $cmds = @(
                    [pscustomobject]@{ cmd = 'eac-improve'; args = '7f' }
                    [pscustomobject]@{ cmd = 'eac-encptr'; args = 'f' }
                ) }
            'r6' { $cmds = @([pscustomobject]@{ cmd = 'be-improve'; args = '1f' }) }
        }
    }

    [pscustomobject]@{
        profile             = $Profile
        ac_family           = $pf.ac_family
        policy_tier         = $policyTier
        manifest            = $man
        expected_pf_diag    = @($pf.expected_pf_diag)
        session_pf_diag     = @($(if ($pf.dump_tracking.session_pf_diag) { $pf.dump_tracking.session_pf_diag } else { $pf.expected_pf_diag | Where-Object { $_ -notmatch '869|81B' } }))
        forbid_bypass_ioctl = @($(if ($pf.dump_tracking.forbid_bypass_ioctl) { $pf.dump_tracking.forbid_bypass_ioctl } else { '0x869', '0x81B' }))
        kernel_hunt_high    = @($(if ($pf.dump_tracking.kernel_hunt_high) { $pf.dump_tracking.kernel_hunt_high } else {
                'known_byovd', 'manual_map_pool', 'cheat_device_ioctl', 'vehicle_majorfunction'
            }))
        account_signals     = @($(if ($pf.dump_tracking.account_signals) { $pf.dump_tracking.account_signals } else { @() }))
        session_client_cmds = $cmds
        vehicle_signals     = $vehicles
        session_ioctl       = @($pf.session_ioctl)
    }
}

function Test-LiveAcDriverMatchesDump {
    param($Model)
    $warnings = [System.Collections.Generic.List[string]]::new()
    $m = $Model.manifest
    if (-not $m) {
        $warnings.Add('no dump manifest')
        return @{ ok = $false; warnings = @($warnings) }
    }

    if ($m.eos_driver_path -and $m.driver_sha256) {
        if (Test-Path -LiteralPath $m.eos_driver_path) {
            $hash = (Get-FileHash -LiteralPath $m.eos_driver_path -Algorithm SHA256).Hash
            if ($hash -ne $m.driver_sha256) {
                $warnings.Add("EAC sys hash drift: live $hash vs dump $($m.driver_sha256)")
            }
        } else {
            $warnings.Add("EAC sys not on disk at $($m.eos_driver_path) (normal until game install)")
        }
    }

    if ($m.beservice_r6s_sha256) {
        $candidates = @(
            'C:\Program Files (x86)\Common Files\BattlEye\BEService.exe',
            'C:\Program Files (x86)\Steam\steamapps\common\Tom Clancy''s Rainbow Six Siege\BattlEye\BEService.exe'
        )
        $found = $false
        foreach ($p in $candidates) {
            if (Test-Path $p) {
                $found = $true
                $hash = (Get-FileHash $p -Algorithm SHA256).Hash
                if ($hash -ne $m.beservice_r6s_sha256) {
                    $warnings.Add("BEService hash drift vs dump for $p")
                }
                break
            }
        }
        if (-not $found) { $warnings.Add('BEService not found on disk yet') }
    }

    $ok = -not @($warnings | Where-Object { $_ -match 'drift' }).Count
    return @{ ok = $ok; warnings = @($warnings) }
}
