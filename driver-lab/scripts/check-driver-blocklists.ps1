<#
.SYNOPSIS
  Cross-check local driver SHA256 / names against LOLDrivers + OS CI blocklist gate + EAC/BE paid-tier heuristics.
  Does NOT load or start any driver.

.EXAMPLE
  .\check-driver-blocklists.ps1
  .\check-driver-blocklists.ps1 -InventoryCsv ..\audit\driver_inventory_2026-11-13.csv
  .\check-driver-blocklists.ps1 -RefreshLoldrivers -Path C:\Users\...\RTCore64.sys
#>
param(
    [string]$InventoryCsv = "",
    [string[]]$Path = @(),
    [string]$LoldriversCsv = (Join-Path $PSScriptRoot "..\data\loldrivers-drivers.csv"),
    [string]$AcPolicyJson = (Join-Path $PSScriptRoot "..\data\ac_eac_be_vehicle_policy.json"),
    [string]$OutDir = (Join-Path $PSScriptRoot "..\audit"),
    [switch]$RefreshLoldrivers
)

$ErrorActionPreference = "Stop"
$dataDir = Split-Path -Parent $LoldriversCsv
New-Item -ItemType Directory -Force -Path $dataDir, $OutDir | Out-Null

function Write-BlockEnv {
    $hvci = $null
    try {
        $hvci = (Get-ItemProperty "HKLM:\SYSTEM\CurrentControlSet\Control\DeviceGuard\Scenarios\HypervisorEnforcedCodeIntegrity").Enabled
    } catch {}
  $bl = $null
  try {
    $bl = (Get-ItemProperty "HKLM:\SYSTEM\CurrentControlSet\Control\CI\Config").VulnerableDriverBlocklistEnable
  } catch {}
    [pscustomobject]@{
        HVCI_Enabled                    = $hvci
        VulnerableDriverBlocklistEnable = $bl
        Note                            = if ($bl -eq 0) {
            "Blocklist OFF locally — LOLDrivers match still means load fails when policy enabled or on other hosts"
        } else {
            "Blocklist ON — matching LOLDrivers hashes should fail at load (CI)"
        }
    }
}

if ($RefreshLoldrivers -or -not (Test-Path -LiteralPath $LoldriversCsv)) {
    Write-Host "[*] Fetching LOLDrivers CSV (may take up to ~2 minutes)..."
    $uri = "https://www.loldrivers.io/api/drivers.csv"
    $tmp = "$LoldriversCsv.download"
    Invoke-WebRequest -Uri $uri -OutFile $tmp -UseBasicParsing -TimeoutSec 120
    Move-Item -Force $tmp $LoldriversCsv
    Write-Host "[+] Cached $LoldriversCsv"
}

$policy = Get-Content -LiteralPath $AcPolicyJson -Raw | ConvertFrom-Json

# Build hash -> LOLDrivers row index (CSV may list comma-separated hashes per row)
Write-Host "[*] Indexing LOLDrivers..."
$lolRows = Import-Csv -LiteralPath $LoldriversCsv
$hashIndex = @{}
$tagIndex = @{}
foreach ($row in $lolRows) {
    $sha = $row.KnownVulnerableSamples_SHA256
    if ($sha) {
        foreach ($h in ($sha -split ',' | ForEach-Object { $_.Trim() } | Where-Object { $_ })) {
            $hashIndex[$h.ToLowerInvariant()] = $row
        }
    }
    $tags = $row.Tags
    if ($tags) {
        foreach ($t in ($tags -split ',' | ForEach-Object { $_.Trim() } | Where-Object { $_ })) {
            $tagIndex[$t.ToLowerInvariant()] = $row
        }
    }
}

function Test-AcTier([string]$fileName, [string]$tier) {
    $patterns = @($policy.tiers.$tier.block_filename_patterns)
    foreach ($p in $patterns) {
        if ($fileName -match $p) { return "match" }
    }
    $mv = $policy.mapper_vehicles.PSObject.Properties | Where-Object { $_.Name -ieq $fileName }
    if ($mv) {
        $prop = "${tier}_paid"
        if ($mv.Value.PSObject.Properties.Name -contains $prop) {
            return $mv.Value.$prop
        }
    }
    return "no_match"
}

$items = @()
if ($Path.Count -gt 0) {
    foreach ($p in $Path) {
        if (-not (Test-Path -LiteralPath $p)) { Write-Warning "Missing $p"; continue }
        $fi = Get-Item -LiteralPath $p
        $items += [pscustomobject]@{ Path = $fi.FullName; FileName = $fi.Name; Sha256 = (Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash }
    }
} else {
    if (-not $InventoryCsv) {
        $latest = Get-ChildItem (Join-Path $OutDir "driver_inventory_*.csv") -ErrorAction SilentlyContinue |
            Sort-Object LastWriteTime -Descending | Select-Object -First 1
        if ($latest) { $InventoryCsv = $latest.FullName }
    }
    if (-not $InventoryCsv -or -not (Test-Path -LiteralPath $InventoryCsv)) {
        Write-Error "No -Path and no inventory CSV. Run enumerate-local-drivers.ps1 first."
    }
    $items = Import-Csv -LiteralPath $InventoryCsv | ForEach-Object {
        [pscustomobject]@{ Path = $_.Path; FileName = $_.FileName; Sha256 = $_.Sha256 }
    }
}

$stamp = Get-Date -Format "yyyy-MM-dd"
$reportPath = Join-Path $OutDir "driver_blocklist_report_$stamp.csv"

$report = foreach ($item in $items) {
    $sha = $item.Sha256.ToLowerInvariant()
    $lol = $hashIndex[$sha]
    $lolByTag = $tagIndex[$item.FileName.ToLowerInvariant()]
    if (-not $lol -and $lolByTag) { $lol = $lolByTag }

    $eac = Test-AcTier $item.FileName "paid_eac"
    $be = Test-AcTier $item.FileName "paid_battleye"

    $osBlock = if ($lol) { "likely_when_blocklist_on" } else { "no_loldrivers_hash" }
    $mapper = if ($lol) {
        "skip_already_public_lol"
    } elseif ($eac -match "block|match" -or $be -match "block|match") {
        "skip_ac_name_policy"
    } else {
        "ida_if_high_import_tier"
    }

    [pscustomobject]@{
        FileName                = $item.FileName
        Sha256                  = $item.Sha256
        LOLDrivers              = if ($lol) { "yes" } else { "no" }
        MapperVehicle           = $mapper
        LOLDrivers_Tags         = if ($lol) { $lol.Tags } else { "" }
        LOLDrivers_Category     = if ($lol) { $lol.Category } else { "" }
        Win11_VulnBlocklist     = $osBlock
        EAC_PaidTier_Heuristic  = $eac
        BE_PaidTier_Heuristic   = $be
        Path                    = $item.Path
    }
}

$report | Export-Csv $reportPath -NoTypeInformation

Write-Host ""
Write-Host "=== Host policy ==="
Write-BlockEnv | Format-List

$lolCount = @($report | Where-Object LOLDrivers -eq "yes").Count
Write-Host "=== LOLDrivers hits ($lolCount) — expected dead ends for paid AC / CI blocklist; not mapper targets ==="
$green = $report | Where-Object { $_.MapperVehicle -eq "ida_if_high_import_tier" }
Write-Host "=== Non-LOL files to cross-check with greenfield-driver-candidates.ps1: $($green.Count) ==="
if ($green.Count -gt 0 -and $green.Count -le 30) {
    $green | Format-Table FileName, MapperVehicle, EAC_PaidTier_Heuristic, BE_PaidTier_Heuristic -AutoSize
}

Write-Host ""
Write-Host "Wrote $reportPath"
