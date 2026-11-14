# Parses %TEMP%\pf_diag.tmp after map-ac-replica / bypass run.
param(
    [ValidateSet('rust', 'r6', 'deltaforce', 'arenabreakout', 'arksa', 'universal', 'any')]
    [string]$Profile = 'any',
    [ValidateSet('prove', 'bypass', 'session', 'any')]
    [string]$MapMode = 'any',
    [string]$LogPath = ''
)

if (-not $LogPath) { $LogPath = Join-Path $env:TEMP 'pf_diag.tmp' }
if (-not (Test-Path $LogPath)) {
    return [pscustomobject]@{
        ok = $false
        path = $LogPath
        error = 'pf_diag missing — map may have failed before logging'
    }
}

$text = Get-Content -LiteralPath $LogPath -Raw -ErrorAction SilentlyContinue
if (-not $text) {
    return [pscustomobject]@{ ok = $false; path = $LogPath; error = 'empty log' }
}

$strip = $text -match 'callback strip|Pre-emptive callback strip|STRIP_CALLBACKS'
$max869 = $text -match '0x869|EAC/BE max-tier'
$eac844 = $text -match '0x844|EAC improve IOCTL 0x844'
$eac851 = $text -match '0x851|EOS enc ptr|Enc pointer'
$eac_legacy = $text -match 'EAC improve: active'
$be852 = $text -match '0x852|BattlEye|BE improve'
$ace846 = $text -match '0x846|ACE strengthen IOCTL 0x846'
$ace84e = $text -match '0x84E|ACE Delta IOCTL 0x84E'
$ace86a = $text -match '0x86A|ACE/VGK max-tier IOCTL 0x86A'
$rwOnly = $text -match 'RW-only'

$result = @{
    path       = $LogPath
    strip      = [bool]$strip
    max869     = [bool]$max869
    eac844     = [bool]$eac844
    eac851     = [bool]$eac851
    eac_legacy = [bool]$eac_legacy
    be852      = [bool]$be852
    ace846     = [bool]$ace846
    ace84e     = [bool]$ace84e
    ace86a     = [bool]$ace86a
    rw_only    = [bool]$rwOnly
}

$mode = 'unknown'
if ($rwOnly -and -not $max869) { $mode = 'prove' }
elseif ($max869 -and $strip) { $mode = 'bypass' }
elseif ($eac844 -or $eac851 -or $be852) { $mode = 'ioctl_or_bypass' }

$profileOk = $true
if ($MapMode -eq 'session') {
    if ($Profile -eq 'rust') {
        $profileOk = ($eac844 -or $eac_legacy) -and ($eac851 -or $true)
    } elseif ($Profile -in @('r6', 'arksa')) {
        $profileOk = $be852
    } else { $profileOk = ($eac844 -or $be852 -or $ace846) }
} elseif ($Profile -eq 'rust') {
    $profileOk = ($eac844 -or $eac_legacy) -and $eac851 -and $strip -and $max869
} elseif ($Profile -in @('r6', 'arksa')) {
    $profileOk = $be852 -and $strip -and $max869
}
if ($MapMode -ne 'session') {
    if ($Profile -eq 'deltaforce') { $profileOk = $ace846 -and $ace84e -and $ace86a -and $strip -and $max869 }
    if ($Profile -eq 'arenabreakout') { $profileOk = $ace846 -and $ace86a -and $strip -and $max869 }
    if ($Profile -eq 'universal') {
        $profileOk = ((($eac844 -or $eac_legacy) -and $eac851) -or $be852 -or $ace846) -and $strip -and $max869
    }
}

$ok = $false
if ($MapMode -eq 'session') {
    $ok = $profileOk -and (-not $max869)
} elseif ($mode -eq 'prove') {
    $ok = $true
} elseif ($mode -eq 'bypass') {
    $ok = $profileOk -and $strip -and $max869
} else {
    $ok = $profileOk
}

[pscustomobject]@{
    ok         = $ok
    mode       = $mode
    map_mode   = $MapMode
    profile    = $Profile
    strip      = $result.strip
    max869     = $result.max869
    eac844     = $result.eac844
    eac851     = $result.eac851
    be852      = $result.be852
    ace846     = $result.ace846
    ace84e     = $result.ace84e
    ace86a     = $result.ace86a
    path       = $LogPath
}
