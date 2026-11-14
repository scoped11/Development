function Get-LastHostBExit {
    param([string]$LogPath)
    if (-not (Test-Path $LogPath)) { return $null }
    $last = Get-Content $LogPath -ErrorAction SilentlyContinue | Select-String 'host-b exit=(\d+)' | Select-Object -Last 1
    if (-not $last) { return $null }
    if ([string]$last.Line -match 'host-b exit=(\d+)') { return [int]$Matches[1] }
    return $null
}

function Test-LastHostBProof {
    param([string]$LogPath)
    return (Get-LastHostBExit -LogPath $LogPath) -eq 0
}
