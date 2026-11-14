function Set-LabProductionBcd {
    [CmdletBinding()]
    param(
        [switch]$BootstrapTestSign,
        [string]$Log = $null
    )

    function L([string]$m) {
        if ($Log) { Add-Content $Log "$(Get-Date -Format o) $m" }
        Write-Host $m
    }

    if ($BootstrapTestSign) {
        L 'BOOTSTRAP ONLY: testsigning on (turn off after trusted load verified)'
        bcdedit /set testsigning on 2>&1 | ForEach-Object { L $_ }
        bcdedit /set nointegritychecks on 2>&1 | ForEach-Object { L $_ }
        return
    }

    foreach ($pair in @(
            @('testsigning', 'off'),
            @('nointegritychecks', 'off')
        )) {
        $cmd = "bcdedit /set $($pair[0]) $($pair[1])"
        L $cmd
        cmd /c $cmd 2>&1 | ForEach-Object { L "  $_" }
    }

    bcdedit /enum '{current}' 2>$null | Select-String 'testsigning|nointegritychecks' | ForEach-Object { L $_.Line.Trim() }
}