<#
.SYNOPSIS
  Inventory kernel drivers on this machine and flag BYOVD-relevant imports (dumpbin).

.EXAMPLE
  .\enumerate-local-drivers.ps1
  .\enumerate-local-drivers.ps1 -CopyToInbox -OpenHighRiskInIda
#>
param(
    [string]$OutDir = (Join-Path $PSScriptRoot "..\audit"),
    [switch]$CopyToInbox,
    [switch]$IncludeMicrosoftWhcp,
    [string[]]$DangerImports = @(
        "MmMapIoSpace", "MmUnmapIoSpace", "MmCopyMemory", "MmGetPhysicalAddress",
        "MmAllocateContiguousMemory", "ZwMapViewOfSection", "ZwOpenSection",
        "HalGetBusDataByOffset", "HalSetBusDataByOffset", "ObReferenceObjectByHandle",
        "MmMapLockedPages", "MmProbeAndLockPages"
    )
)

$ErrorActionPreference = "Stop"
$OutDir = [System.IO.Path]::GetFullPath($OutDir)
$inbox = Join-Path $OutDir "inbox"
New-Item -ItemType Directory -Force -Path $OutDir, $inbox | Out-Null

$dumpbin = Get-ChildItem "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\*\VC\Tools\MSVC\*\bin\Hostx64\x64\dumpbin.exe" -ErrorAction SilentlyContinue |
    Select-Object -First 1
if (-not $dumpbin) {
    $dumpbin = Get-ChildItem "$env:ProgramFiles\Microsoft Visual Studio\2022\*\VC\Tools\MSVC\*\bin\Hostx64\x64\dumpbin.exe" -ErrorAction SilentlyContinue |
        Select-Object -First 1
}

$stamp = Get-Date -Format "yyyy-MM-dd"
$inventoryPath = Join-Path $OutDir "driver_inventory_$stamp.csv"
$heuristicPath = Join-Path $OutDir "driver_byovd_heuristic_$stamp.csv"

$csvTmp = Join-Path $env:TEMP "driverquery_enumerate.csv"
& driverquery.exe /v /fo csv | Out-File -FilePath $csvTmp -Encoding utf8
$rows = Import-Csv $csvTmp

function Get-PathLiteral([string]$raw) {
    if ([string]::IsNullOrWhiteSpace($raw)) { return $null }
    $p = $raw -replace '^\\?\?\?\\', ''
    if (Test-Path -LiteralPath $p) { return $p }
    return $null
}

$inventory = foreach ($d in $rows) {
    $path = Get-PathLiteral $d.Path
    if (-not $path) { continue }
    $fi = Get-Item -LiteralPath $path
    try {
        $sig = Get-AuthenticodeSignature -LiteralPath $path
        $signer = if ($sig.SignerCertificate) { $sig.SignerCertificate.Subject } else { "unsigned" }
    } catch {
        $signer = "error"
    }
    $hash = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
    [pscustomobject]@{
        Service     = $d.'Display Name'
        State       = $d.State
        StartMode   = $d.'Start Mode'
        Path        = $path
        Signer      = $signer
        Sha256      = $hash
        SizeKB      = [math]::Round($fi.Length / 1kb, 1)
        FileName    = $fi.Name
    }
}

$inventory | Export-Csv $inventoryPath -NoTypeInformation

function Test-ThirdPartySigner([string]$signer) {
    if ($signer -eq "unsigned") { return $true }
    if ($signer -match "Microsoft Corporation" -and $signer -notmatch "Hardware Compatibility Publisher") {
        return $false
    }
    if ($IncludeMicrosoftWhcp) { return $true }
    if ($signer -match "Hardware Compatibility Publisher") {
        return $false
    }
    return $true
}

$heuristic = foreach ($item in $inventory) {
    $hits = @()
    if ($dumpbin) {
        $imp = & $dumpbin.FullName /imports $item.Path 2>$null | Out-String
        foreach ($name in $DangerImports) {
            if ($imp -match [regex]::Escape($name)) { $hits += $name }
        }
    }
    $tier = 0
    if ($hits.Count -ge 3) { $tier = 3 }
    elseif ($hits.Count -ge 1) { $tier = 2 }
    [pscustomobject]@{
        FileName    = $item.FileName
        State       = $item.State
        Tier        = $tier
        DangerHits  = ($hits -join "; ")
        ThirdParty  = (Test-ThirdPartySigner $item.Signer)
        Path        = $item.Path
        Signer      = $item.Signer
        Sha256      = $item.Sha256
    }
}

$heuristic |
    Sort-Object -Property @{ Expression = { -$_.Tier } }, FileName |
    Export-Csv $heuristicPath -NoTypeInformation

if ($CopyToInbox) {
    $copy = $heuristic | Where-Object { $_.Tier -ge 2 -or $_.ThirdParty }
    foreach ($c in $copy) {
        Copy-Item -LiteralPath $c.Path -Destination (Join-Path $inbox $c.FileName) -Force -ErrorAction SilentlyContinue
    }
}

Write-Host "Wrote $inventoryPath ($($inventory.Count) rows)"
Write-Host "Wrote $heuristicPath"
$top = $heuristic | Where-Object Tier -ge 2 | Select-Object -First 15 FileName, Tier, DangerHits, State
if ($top) {
    Write-Host "`nHigh-signal drivers (tier >= 2):"
    $top | Format-Table -AutoSize
} else {
    Write-Host "No tier-2+ drivers by import heuristics (install VS dumpbin for full scan)."
}

if (-not $dumpbin) {
    Write-Host "WARNING: dumpbin not found — import heuristics skipped." -ForegroundColor Yellow
}

$blockScript = Join-Path $PSScriptRoot "check-driver-blocklists.ps1"
$greenScript = Join-Path $PSScriptRoot "greenfield-driver-candidates.ps1"
if (Test-Path -LiteralPath $blockScript) {
    Write-Host "[*] Blocklist cross-check (LOLDrivers = skip list, not hunt list)..."
    & $blockScript -InventoryCsv $inventoryPath
}
if (Test-Path -LiteralPath $greenScript) {
    & $greenScript -MinTier 2
}
