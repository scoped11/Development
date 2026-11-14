# Recursively 7z-extract installers under oem-raw -> oem-extract/deep-* ; report .sys
param(
    [string[]]$Only = @()
)

$7z = @('C:\Program Files\7-Zip\7z.exe', 'C:\Program Files (x86)\7-Zip\7z.exe') | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $7z) { throw '7-Zip not found' }

$raw = Join-Path $PSScriptRoot '..\audit\inbox\oem-raw'
$extract = Join-Path $PSScriptRoot '..\audit\inbox\oem-extract'
$maxDepth = 6
$seen = [System.Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)

function Extract-Tree {
    param([string]$Path, [string]$OutRoot, [int]$Depth)
    if ($Depth -gt $maxDepth) { return }
  $fi = Get-Item -LiteralPath $Path -ErrorAction SilentlyContinue
    if (-not $fi) { return }
    $key = $fi.FullName.ToLower()
    if (-not $seen.Add($key)) { return }

    $leaf = [IO.Path]::GetFileNameWithoutExtension($Path)
    $out = Join-Path $OutRoot $leaf
    New-Item -ItemType Directory -Force -Path $out | Out-Null
    & $7z x -y -o$out $Path 2>&1 | Out-Null

    Get-ChildItem $out -Recurse -Filter *.sys -ErrorAction SilentlyContinue | ForEach-Object {
        Write-Host "[sys] $($_.FullName)"
    }

    $nested = Get-ChildItem $out -Recurse -File -ErrorAction SilentlyContinue |
        Where-Object { $_.Extension -match '^\.(exe|msi|7z|zip|cab)$' -and $_.Length -gt 200000 }
    foreach ($n in $nested) {
        Extract-Tree $n.FullName (Join-Path $out '_nested') ($Depth + 1)
    }
}

$items = Get-ChildItem $raw -File | Where-Object {
    $_.Extension -match '^\.(zip|exe|msi)$' -and $_.Length -gt 500000
}
if ($Only.Count -gt 0) {
    $items = $items | Where-Object { $Only -contains $_.Name }
}

foreach ($f in $items) {
    Write-Host "=== deep extract $($f.Name) ==="
    $root = Join-Path $extract ('deep-' + $f.BaseName)
    New-Item -ItemType Directory -Force -Path $root | Out-Null
    Extract-Tree $f.FullName $root 0
}
