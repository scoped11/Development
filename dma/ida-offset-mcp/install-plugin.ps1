# Copy the RobRun Offset MCP plugin into IDA's user plugins folder.
$ErrorActionPreference = "Stop"
$src = Join-Path $PSScriptRoot "plugin\robrun_offset_mcp.py"
$dstDir = Join-Path $env:APPDATA "Hex-Rays\IDA Pro\plugins"
if (-not (Test-Path $src)) { throw "missing $src" }
New-Item -ItemType Directory -Force -Path $dstDir | Out-Null
Copy-Item $src (Join-Path $dstDir "robrun_offset_mcp.py") -Force
Write-Host "Installed: $(Join-Path $dstDir 'robrun_offset_mcp.py')"
Write-Host "Restart IDA (or Edit → Plugins → RobRun Offset MCP) so it binds 127.0.0.1:13338"
Write-Host "Then reload Cursor MCP for server 'robrun-ida'."
