# Complete MCP + rea doctor when install stopped early (e.g. no git).
$LabRoot = 'C:\lab'
$logDir = Join-Path $LabRoot 'logs'
$javaHome = 'C:\lab\tools\jdk-21'
$ghidraTarget = 'C:\lab\tools\ghidra\ghidra_12.1.4_PUBLIC'
$node = 'C:\lab\tools\node'
$env:JAVA_HOME = $javaHome
$env:GHIDRA_INSTALL_DIR = $ghidraTarget
[Environment]::SetEnvironmentVariable('JAVA_HOME', $javaHome, 'User')
[Environment]::SetEnvironmentVariable('GHIDRA_INSTALL_DIR', $ghidraTarget, 'User')
$env:Path = "$node;" + $env:Path

$mcpPath = Join-Path $env:USERPROFILE '.cursor\mcp.json'
New-Item -ItemType Directory -Force -Path (Split-Path $mcpPath) | Out-Null
$reaBlock = [ordered]@{
    command = Join-Path $node 'npx.cmd'
    args    = @('-y', 'rea-agents@latest', 'mcp')
    env     = [ordered]@{
        JAVA_HOME          = $javaHome
        GHIDRA_INSTALL_DIR = $ghidraTarget
    }
}
$cfg = [pscustomobject]@{ mcpServers = [pscustomobject]@{ rea = [pscustomobject]$reaBlock } }
($cfg | ConvertTo-Json -Depth 8) | Set-Content -LiteralPath $mcpPath -Encoding UTF8

$doctorOut = Join-Path $logDir 'rea-doctor-guest-snapshot.txt'
& (Join-Path $node 'npx.cmd') -y rea-agents@latest doctor 2>&1 | Tee-Object -FilePath $doctorOut
$share = '\\VBoxSvr\hostdev'
if (Test-Path (Join-Path $share 'docs')) {
    Copy-Item -Force $doctorOut (Join-Path $share 'docs\rea-doctor-guest-snapshot.txt')
}
Select-String -Path $doctorOut -Pattern 'ghidra|healthy' | Select-Object -First 15