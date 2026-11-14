# Resolve latest co-located AC dump dir per profile (driver2/dumps).
. (Join-Path (Split-Path $PSScriptRoot -Parent) 'Resolve-Driver3Game.ps1')

$script:DumpGlobByProfile = @{
    rust          = 'rust-eac-eos_*'
    r6            = 'r6-battleye_*'
    arksa         = 'r6-battleye_*'
    deltaforce    = 'deltaforce-ace_*'
    arenabreakout = 'arenabreakout-ace_*'
}

function Get-ProfileDumpDir {
    param([string]$Profile)
    $p = Resolve-Driver3Profile $Profile
    $Driver3 = Split-Path $PSScriptRoot -Parent
    $Byovd = Split-Path $Driver3 -Parent
    $Driver2Dumps = Join-Path $Byovd 'driver2\dumps'
    $glob = $script:DumpGlobByProfile[$p]
    if ($glob) {
        $hit = Get-ChildItem -Path $Driver2Dumps -Directory -Filter $glob -ErrorAction SilentlyContinue |
            Sort-Object Name -Descending | Select-Object -First 1
        if ($hit) { return $hit.FullName }
    }
    $meta = Get-Driver3Meta $p
    if ($meta.capture_dump) {
        $d = Join-Path $Byovd ($meta.capture_dump -replace '/', '\')
        if (Test-Path $d) { return $d }
    }
    return $null
}

function Get-ProfileBuildIdFromDump {
    param([string]$Profile)
    $dir = Get-ProfileDumpDir $Profile
    if (-not $dir) { return $null }
    $man = Join-Path $dir 'manifest.json'
    if (Test-Path $man) {
        $j = Get-Content $man -Raw | ConvertFrom-Json
        if ($j.deploymentid) { return $j.deploymentid }
        if ($j.driver_sha256) { return ('sha256:' + $j.driver_sha256.Substring(0, 16)) }
        if ($j.captured_utc) { return ('capture:' + ($j.captured_utc -replace '[^0-9T\-:Z]', '').Substring(0, [Math]::Min(19, $j.captured_utc.Length))) }
    }
    return (Split-Path $dir -Leaf)
}

function Get-ProfileDumpAgeDays {
    param([string]$Profile)
    $dir = Get-ProfileDumpDir $Profile
    if (-not $dir) { return $null }
    $man = Join-Path $dir 'manifest.json'
    if (Test-Path $man) {
        $j = Get-Content $man -Raw | ConvertFrom-Json
        if ($j.captured_utc) {
            return ((Get-Date) - [datetime]$j.captured_utc).TotalDays
        }
    }
    return ((Get-Date) - (Get-Item $dir).LastWriteTime).TotalDays
}
