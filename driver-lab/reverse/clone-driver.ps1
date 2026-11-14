<#
.SYNOPSIS
  Clone one .sys into driver-lab/reverse/<name>/ for isolated IDA work.

.EXAMPLE
  .\clone-driver.ps1 -SourcePath C:\Windows\System32\drivers\foo.sys
#>
param(
    [Parameter(Mandatory)]
    [string]$SourcePath
)

if (-not (Test-Path -LiteralPath $SourcePath)) {
    Write-Error "Not found: $SourcePath"
}
$name = [System.IO.Path]::GetFileNameWithoutExtension($SourcePath)
$root = Join-Path $PSScriptRoot $name
$bin = Join-Path $root "bin"
New-Item -ItemType Directory -Force -Path $bin, (Join-Path $root "notes") | Out-Null
$dest = Join-Path $bin ([System.IO.Path]::GetFileName($SourcePath))
Copy-Item -LiteralPath $SourcePath -Destination $dest -Force
$hash = (Get-FileHash -LiteralPath $dest -Algorithm SHA256).Hash
Write-Host "[+] Clone: $root"
Write-Host "    SHA256: $hash"
Write-Host "    IDA: idb_open input_path=$dest"
