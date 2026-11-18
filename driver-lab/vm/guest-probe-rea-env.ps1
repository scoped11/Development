# Read-only probe: share mount, Node, Ghidra, REA doctor (guest).
# Host: .\host-guest-exec.ps1 -VmName AI-Workspace -Command powershell,-NoProfile,-ExecutionPolicy,Bypass,-File,D:\hostdev\driver-lab\vm\guest-probe-rea-env.ps1
$ErrorActionPreference = 'Continue'
$out = @{
    hostname   = $env:COMPUTERNAME
    utc        = (Get-Date).ToUniversalTime().ToString('o')
    sharePaths = @()
    node       = $null
    ghidra     = @{
        GHIDRA_INSTALL_DIR = $env:GHIDRA_INSTALL_DIR
        candidates         = @()
    }
    cursorMcp  = $null
    doctorPath = $null
}

foreach ($letter in @('Z:', 'D:', 'Y:', 'E:', 'X:')) {
    $p = Join-Path $letter 'docs\vm-checklist.md'
    if (Test-Path $p) {
        $out.sharePaths += @{
            drive  = $letter
            vboxsvr = Test-Path "\\vboxsvr\hostdev\docs\vm-checklist.md"
            hostMatch = (Get-Item $p).FullName
        }
    }
}

try {
    $nv = (& node -v 2>&1) -join ''
    $out.node = @{ version = $nv.Trim(); ok = $true }
} catch {
    $out.node = @{ ok = $false; error = $_.Exception.Message }
}

foreach ($c in @(
    'C:\Tools\ghidra_*',
    'C:\Program Files\ghidra_*',
    'C:\ghidra_*'
)) {
    Get-Item $c -ErrorAction SilentlyContinue | ForEach-Object {
        $out.ghidra.candidates += $_.FullName
    }
}

$mcp = "$env:USERPROFILE\.cursor\mcp.json"
if (Test-Path $mcp) { $out.cursorMcp = $mcp }

$shareRoot = $null
foreach ($letter in @('Z:', 'D:', 'Y:', 'E:', 'X:')) {
    if (Test-Path (Join-Path $letter 'docs\vm-checklist.md')) {
        $shareRoot = $letter
        break
    }
}
if ($shareRoot -and $out.node.ok) {
    $doctorJson = Join-Path $shareRoot 'docs\rea-doctor-guest-snapshot.json'
    Push-Location $shareRoot
    try {
        & npx.cmd --yes rea-agents@latest doctor 2>&1 | Set-Content -LiteralPath $doctorJson -Encoding utf8
        $out.doctorPath = $doctorJson
    } catch {
        $out.doctorPath = "failed: $($_.Exception.Message)"
    }
    Pop-Location
}

$out | ConvertTo-Json -Depth 6