# Bump DRIVERLAB_MCP_GEN in %USERPROFILE%\.cursor\mcp.json so Cursor respawns DriverLab MCP servers.
# Then Reload Window in Cursor (Ctrl+Shift+P -> "Developer: Reload Window").
$ErrorActionPreference = 'Stop'
$mcp = Join-Path $env:USERPROFILE '.cursor\mcp.json'
if (-not (Test-Path $mcp)) { throw "missing $mcp" }
$raw = Get-Content $mcp -Raw -Encoding UTF8
$n = 1
if ($raw -match 'DRIVERLAB_MCP_GEN"\s*:\s*"(\d+)"') {
    $n = [int]$Matches[1] + 1
}
$raw = [regex]::Replace($raw, '("DRIVERLAB_MCP_GEN"\s*:\s*")(\d+)(")', { param($m) $m.Groups[1].Value + $n + $m.Groups[3].Value })
Set-Content -LiteralPath $mcp -Value $raw -Encoding UTF8 -NoNewline
Write-Host "DRIVERLAB_MCP_GEN -> $n"
Write-Host 'Reload Cursor: Developer: Reload Window'