$ErrorActionPreference = "Stop"
$src = "Z:\ac-captures-host\2026-11-18\BEDaisy.sys"
$dstDir = "C:\re\artifacts"
$dst = Join-Path $dstDir "BEDaisy.sys"
$expect = "E7BCF344EEFA7A4EBEA9B048E376958A5AD711489249033FA480A7EC8B6C018E"
New-Item -ItemType Directory -Force -Path $dstDir | Out-Null
Copy-Item -LiteralPath $src -Destination $dst -Force
$item = Get-Item -LiteralPath $dst
$hash = (Get-FileHash -LiteralPath $dst -Algorithm SHA256).Hash
Write-Output "DST=$($item.FullName)"
Write-Output "LEN=$($item.Length)"
Write-Output "HASH=$hash"
Write-Output "MATCH=$($hash -eq $expect)"
Write-Output "GHIDRA=$(Test-Path 'C:\Tools\ghidra_12.1_PUBLIC')"
Write-Output "NPX=$((Get-Command npx.cmd -ErrorAction SilentlyContinue).Source)"
Write-Output "JAVA=$env:JAVA_HOME"
Write-Output "STAGE_OK"
