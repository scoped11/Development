function Invoke-Driver2RankedVerify {
    param(
        [string]$Profile = 'rust',
        [string]$OutJson = ''
    )

    $Driver2 = Split-Path $PSScriptRoot -Parent
    $Byovd = Split-Path $Driver2 -Parent
    if (-not $OutJson) { $OutJson = Join-Path $Driver2 'driver2_ranked_verify.json' }

    $client = Join-Path $Byovd 'bin\Driver2Client.exe'
    $vehicle = Join-Path $Byovd 'bin\RTCore64.sys'
    if (-not (Test-Path $vehicle)) { $vehicle = Join-Path $Byovd 'vehicles\RTCore64.sys' }
    $mapper = Join-Path $Byovd 'bin\mapper_nal.exe'

    $probeExit = 99
    $probeOut = ''
    if (Test-Path $client) {
        $probeOut = & $client probe 2>&1 | Out-String
        $probeExit = $LASTEXITCODE
    }

    $result = [ordered]@{
        schema          = 'driver2_ranked_verify_v1'
        utc             = (Get-Date).ToUniversalTime().ToString('o')
        profile         = $Profile
        probe_exit      = $probeExit
        probe_ok        = ($probeExit -eq 0)
        probe_output    = $probeOut.Trim()
        vehicle_present = (Test-Path $vehicle)
        mapper_present  = (Test-Path $mapper)
        client_present  = (Test-Path $client)
        ok              = ($probeExit -eq 0 -and (Test-Path $client) -and (Test-Path $mapper))
        hostname        = $env:COMPUTERNAME
    }

    ($result | ConvertTo-Json -Depth 5) | Set-Content $OutJson -Encoding utf8
    return [pscustomobject]$result
}

function Test-Driver2RankedVerifyFile {
    param([string]$Path)
    if (-not (Test-Path $Path)) { return $false }
    try {
        $v = Get-Content $Path -Raw | ConvertFrom-Json
        return ($v.ok -eq $true -and $v.probe_ok -eq $true)
    } catch { return $false }
}
