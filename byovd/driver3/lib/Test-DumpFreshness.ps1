function Test-Driver3DumpFreshness {
    param(
        [string]$Profile = 'rust',
        [int]$WarnDays = 14,
        [int]$FailDays = 45
    )
    . (Join-Path $PSScriptRoot 'ProfileDump.ps1')
    $p = Resolve-Driver3Profile $Profile
    $age = Get-ProfileDumpAgeDays $p
    $dir = Get-ProfileDumpDir $p
    $level = 'ok'
    $issues = @()
    if (-not $dir) {
        $issues += 'no dump dir — run driver2/sync-ac-dumps or title capture'
        return [pscustomobject]@{ ok = $false; profile = $p; age_days = $null; level = 'missing'; issues = $issues; dump_dir = $null }
    }
    if ($null -eq $age) { $age = 999 }
    if ($age -gt $FailDays) {
        $level = 'stale'
        $issues += "dump age ${age:N0}d > fail $FailDays — refresh before ranked"
    } elseif ($age -gt $WarnDays) {
        $level = 'warn'
        $issues += "dump age ${age:N0}d — patch may have shipped"
    }
    [pscustomobject]@{
        ok       = ($level -ne 'stale' -and $level -ne 'missing')
        profile  = $p
        age_days = [Math]::Round($age, 1)
        level    = $level
        issues   = $issues
        dump_dir = $dir
    }
}
