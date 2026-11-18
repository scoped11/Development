# One-shot: JDK + Ghidra 12.x + REA MCP env for AI-Workspace guest.
# Run **inside the VM** (elevated PowerShell recommended for winget).
#
#   powershell -NoProfile -ExecutionPolicy Bypass -File D:\hostdev\driver-lab\vm\install-ghidra-rea-guest.ps1
#   # or: Z:\driver-lab\vm\install-ghidra-rea-guest.ps1
#
# Optional: -GhidraZipPath <path> if you copied ghidra_*_PUBLIC_*.zip to the share offline.
param(
    [string]$InstallRoot = 'C:\Tools',
    [string]$GhidraZipPath = '',
    [string]$GhidraVersion = '12.1_PUBLIC',
    [switch]$SkipReaSetup
)

$ErrorActionPreference = 'Stop'

function Resolve-DevShare {
    foreach ($c in @('D:', 'Z:', 'Y:', 'E:', 'X:')) {
        if (Test-Path (Join-Path $c 'docs\vm-checklist.md')) { return $c }
    }
    if (Test-Path '\\vboxsvr\hostdev\docs\vm-checklist.md') { return '\\vboxsvr\hostdev' }
    throw 'hostdev share not found (mount Guest Additions shared folder hostdev)'
}

$share = Resolve-DevShare
Write-Host "[+] hostdev share: $share"
Write-Host "[*] host path (from share): C:\Users\bj284\Downloads\TraeTesting\cursor\Development"

New-Item -ItemType Directory -Force -Path $InstallRoot | Out-Null

# Node (REA)
$nodeOk = $false
try {
    $v = (node -v 2>&1).ToString().Trim()
    if ($v -match '^v(\d+)\.') {
        $maj = [int]$Matches[1]
        if ($maj -ge 22) { $nodeOk = $true; Write-Host "[+] Node $v" }
    }
} catch { }

if (-not $nodeOk) {
    Write-Host '[*] Installing Node.js LTS via winget ...'
    winget install --id OpenJS.NodeJS.LTS -e --accept-package-agreements --accept-source-agreements
    $env:Path = [System.Environment]::GetEnvironmentVariable('Path', 'Machine') + ';' +
        [System.Environment]::GetEnvironmentVariable('Path', 'User')
}

# JDK 21+ for Ghidra 12
$javaHome = $env:JAVA_HOME
if (-not $javaHome -or -not (Test-Path (Join-Path $javaHome 'bin\java.exe'))) {
    Write-Host '[*] Installing Microsoft OpenJDK 21 via winget ...'
    winget install --id Microsoft.OpenJDK.21 -e --accept-package-agreements --accept-source-agreements
    $javaHome = 'C:\Program Files\Microsoft\jdk-21.0.13.21-hotspot'
    if (-not (Test-Path $javaHome)) {
        $javaHome = (Get-ChildItem 'C:\Program Files\Microsoft' -Filter 'jdk-*' -Directory | Sort-Object Name -Descending | Select-Object -First 1).FullName
    }
}
if (-not $javaHome) { throw 'JDK 21 not found after winget' }
[Environment]::SetEnvironmentVariable('JAVA_HOME', $javaHome, 'User')
$env:JAVA_HOME = $javaHome
Write-Host "[+] JAVA_HOME=$javaHome"

# Ghidra zip
$ghidraDir = Get-ChildItem $InstallRoot -Directory -Filter 'ghidra_*' -ErrorAction SilentlyContinue |
    Sort-Object Name -Descending | Select-Object -First 1
if (-not $ghidraDir) {
    if (-not $GhidraZipPath) {
        $GhidraZipPath = Join-Path $InstallRoot "ghidra_$GhidraVersion.zip"
    }
    if (-not (Test-Path $GhidraZipPath)) {
        $url = "https://github.com/NationalSecurityAgency/ghidra/releases/download/Ghidra_$($GhidraVersion -replace '_PUBLIC','')_build/ghidra_$GhidraVersion.zip"
        Write-Host "[*] Downloading Ghidra: $url"
        Write-Host '    (Large ~400MB; or place zip at' $GhidraZipPath 'and re-run with -GhidraZipPath)'
        Invoke-WebRequest -Uri $url -OutFile $GhidraZipPath -UseBasicParsing
    }
    Write-Host "[*] Extracting Ghidra to $InstallRoot ..."
    Expand-Archive -LiteralPath $GhidraZipPath -DestinationPath $InstallRoot -Force
    $ghidraDir = Get-ChildItem $InstallRoot -Directory -Filter 'ghidra_*' | Sort-Object Name -Descending | Select-Object -First 1
}
if (-not $ghidraDir) { throw 'Ghidra directory missing after extract' }
$ghidraPath = $ghidraDir.FullName
[Environment]::SetEnvironmentVariable('GHIDRA_INSTALL_DIR', $ghidraPath, 'User')
$env:GHIDRA_INSTALL_DIR = $ghidraPath
Write-Host "[+] GHIDRA_INSTALL_DIR=$ghidraPath"

# Cursor MCP — merge GHIDRA into existing rea registration
$mcpPath = Join-Path $env:USERPROFILE '.cursor\mcp.json'
$mcpDir = Split-Path $mcpPath -Parent
New-Item -ItemType Directory -Force -Path $mcpDir | Out-Null

$reaBlock = [ordered]@{
    command = 'npx.cmd'
    args    = @('-y', 'rea-agents@latest', 'mcp')
    env     = [ordered]@{
        JAVA_HOME          = $javaHome
        GHIDRA_INSTALL_DIR = $ghidraPath
    }
}

if (Test-Path $mcpPath) {
    $cfg = Get-Content $mcpPath -Raw | ConvertFrom-Json
} else {
    $cfg = [pscustomobject]@{ mcpServers = [pscustomobject]@{} }
}
if (-not $cfg.mcpServers) { $cfg | Add-Member -NotePropertyName mcpServers -Value ([pscustomobject]@{}) }

$cfg.mcpServers | Add-Member -NotePropertyName 'rea' -Value ([pscustomobject]$reaBlock) -Force
($cfg | ConvertTo-Json -Depth 6) | Set-Content -LiteralPath $mcpPath -Encoding UTF8
Write-Host "[+] Updated $mcpPath (rea server env includes GHIDRA_INSTALL_DIR)"

if (-not $SkipReaSetup) {
    Write-Host '[*] REA doctor (guest) ...'
    Push-Location $share
  try {
        & npx.cmd -y rea-agents@latest doctor 2>&1 | Tee-Object -FilePath (Join-Path $share 'docs\rea-doctor-guest-snapshot.txt')
    } finally { Pop-Location }
    Write-Host '[*] Run interactively once: npx.cmd rea-agents@latest setup  (select Cursor)'
}

$manifest = @{
    wired_utc            = (Get-Date).ToUniversalTime().ToString('o')
    guest_hostname       = $env:COMPUTERNAME
    hostdev_share        = $share
    host_development     = 'C:\Users\bj284\Downloads\TraeTesting\cursor\Development'
    GHIDRA_INSTALL_DIR   = $ghidraPath
    JAVA_HOME            = $javaHome
    cursor_mcp           = $mcpPath
} | ConvertTo-Json -Depth 4
$manifest | Set-Content -LiteralPath (Join-Path $share 'docs\ghidra-guest-manifest.json') -Encoding UTF8
Write-Host '[+] Wrote docs\ghidra-guest-manifest.json on share'
Write-Host '[!] Restart Cursor on the guest after this script.'