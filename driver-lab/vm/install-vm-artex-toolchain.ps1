# PFAB-22: idempotent RE lab toolchain (guest). Prefers hostdev share Z:\ when mounted.
# Run elevated recommended. Logs to C:\lab\logs\install-toolchain.log
param(
    [string]$LabRoot = 'C:\lab',
    [switch]$SkipWdk,
    [switch]$SkipDocker,
    [switch]$SkipHeavyWinget
)

$ErrorActionPreference = 'Stop'  # relaxed around winget / portable fallbacks
$logDir = Join-Path $LabRoot 'logs'
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
$log = Join-Path $logDir 'install-toolchain.log'
function Log([string]$m) {
    $line = "[{0}] {1}" -f (Get-Date -Format 'yyyy-MM-dd HH:mm:ss'), $m
    Add-Content -LiteralPath $log -Value $line
    Write-Host $line
}

$tools = Join-Path $LabRoot 'tools'
$repos = Join-Path $tools 'repos'
foreach ($d in @($tools, (Join-Path $tools 'artex'), (Join-Path $tools 'ghidra'), $repos)) {
    New-Item -ItemType Directory -Force -Path $d | Out-Null
}

function Resolve-DevShare {
  foreach ($unc in @('\\VBoxSvr\hostdev', '\\vboxsvr\hostdev')) {
        if (Test-Path (Join-Path $unc 'docs\vm-checklist.md')) { return $unc }
    }
    foreach ($c in @('Z:', 'D:', 'Y:', 'E:', 'X:')) {
        try {
            if (Test-Path (Join-Path $c 'docs\vm-checklist.md')) { return $c }
        } catch { }
    }
    return $null
}

$share = Resolve-DevShare
if ($share) { Log "hostdev share: $share" } else { Log 'WARN: hostdev share not mounted (close VBox GUI, run host wire-ai-workspace-lab.ps1 -VmName AI_Workspave_v3)' }

function Refresh-Path {
    $env:Path = [Environment]::GetEnvironmentVariable('Path', 'Machine') + ';' +
        [Environment]::GetEnvironmentVariable('Path', 'User')
}

function Ensure-Winget([string]$Id, [int]$TimeoutSec = 90) {
    $q = winget list --id $Id -e 2>$null | Select-String $Id
    if ($q) { Log "winget OK: $Id"; return $true }
    Log "winget install: $Id (timeout ${TimeoutSec}s)"
    $args = @('install', '--id', $Id, '-e', '--accept-package-agreements', '--accept-source-agreements', '--disable-interactivity')
    $p = Start-Process -FilePath 'winget.exe' -ArgumentList $args -PassThru -WindowStyle Hidden
    if (-not $p.WaitForExit($TimeoutSec * 1000)) {
        try { $p.Kill() } catch { }
        Log "winget TIMEOUT/UAC: $Id"
        return $false
    }
    Refresh-Path
    $q2 = winget list --id $Id -e 2>$null | Select-String $Id
    if ($q2) { return $true }
    Log "winget FAILED rc=$($p.ExitCode): $Id"
    return $false
}

function Ensure-NodePortable {
    if (Get-Command node -ErrorAction SilentlyContinue) { Log 'node already on PATH'; return }
    $nodeRoot = Join-Path $tools 'node'
    $nodeExe = Join-Path $nodeRoot 'node.exe'
    if (-not (Test-Path $nodeExe)) {
        $ver = 'v26.13.1'
        $zip = Join-Path $tools "node-$ver-win-x64.zip"
        $url = "https://nodejs.org/dist/$ver/node-$ver-win-x64.zip"
        Log "Portable Node download: $url"
        Invoke-WebRequest -Uri $url -OutFile $zip -UseBasicParsing
        Expand-Archive -LiteralPath $zip -DestinationPath $nodeRoot -Force
        $inner = Get-ChildItem $nodeRoot -Directory -Filter 'node-*' | Select-Object -First 1
        if ($inner) {
            Get-ChildItem $inner.FullName | Move-Item -Destination $nodeRoot -Force
            Remove-Item $inner.FullName -Force -ErrorAction SilentlyContinue
        }
    }
    $p = [Environment]::GetEnvironmentVariable('Path', 'User')
    if ($p -notlike "*$nodeRoot*") {
        [Environment]::SetEnvironmentVariable('Path', "$nodeRoot;$p", 'User')
    }
    $env:Path = "$nodeRoot;" + $env:Path
    Log ("node portable: " + (node -v))
}

function Ensure-JdkPortable {
    $jdkRoot = Join-Path $tools 'jdk-21'
    $javaExe = Join-Path $jdkRoot 'bin\java.exe'
    if (-not (Test-Path $javaExe)) {
        $zip = Join-Path $tools 'microsoft-jdk-21.zip'
        $url = 'https://aka.ms/download-jdk/microsoft-jdk-21.0.7-windows-x64.zip'
        Log "Portable JDK download: $url"
        Invoke-WebRequest -Uri $url -OutFile $zip -UseBasicParsing
        Expand-Archive -LiteralPath $zip -DestinationPath $tools -Force
        $extracted = Get-ChildItem $tools -Directory -Filter 'jdk-*' | Select-Object -First 1
        if ($extracted -and $extracted.FullName -ne $jdkRoot) {
            if (Test-Path $jdkRoot) { Remove-Item $jdkRoot -Recurse -Force }
            Rename-Item $extracted.FullName 'jdk-21'
        }
    }
    return (Join-Path $tools 'jdk-21')
}

# Portable first — guestcontrol cannot complete winget UAC prompts.
Ensure-NodePortable
$javaHome = Ensure-JdkPortable
Ensure-Winget 'OpenJS.NodeJS.LTS' | Out-Null
Ensure-Winget 'Microsoft.OpenJDK.21' | Out-Null
Ensure-Winget 'Python.Python.3.12' | Out-Null
Ensure-Winget 'Git.Git' | Out-Null
Ensure-Winget '7zip.7zip' | Out-Null
Ensure-Winget 'GoLang.Go' | Out-Null
if (-not $SkipHeavyWinget) {
    Ensure-Winget 'Microsoft.WinDbg'
    if (-not $SkipDocker) { Ensure-Winget 'Docker.DockerDesktop' }
    if (-not $SkipWdk) {
        Log 'WDK: attempting Microsoft.WindowsWDK.10.0.26100 (VS Build Tools may be required first)'
        winget install --id Microsoft.WindowsWDK.10.0.26100 -e --accept-package-agreements --accept-source-agreements --disable-interactivity 2>&1 | Tee-Object -FilePath (Join-Path $logDir 'wdk-winget.log')
    }
}

$javaHome = [Environment]::GetEnvironmentVariable('JAVA_HOME', 'User')
if (-not $javaHome -or -not (Test-Path (Join-Path $javaHome 'bin\java.exe'))) {
    $javaHome = (Get-ChildItem 'C:\Program Files\Microsoft' -Filter 'jdk-*' -Directory -ErrorAction SilentlyContinue |
        Sort-Object Name -Descending | Select-Object -First 1).FullName
}
if (-not $javaHome -or -not (Test-Path (Join-Path $javaHome 'bin\java.exe'))) {
    $javaHome = Ensure-JdkPortable
}
if (-not $javaHome -or -not (Test-Path (Join-Path $javaHome 'bin\java.exe'))) { throw 'JDK 21 not found after winget/portable' }
if (-not (Get-Command node -ErrorAction SilentlyContinue)) { Ensure-NodePortable }
[Environment]::SetEnvironmentVariable('JAVA_HOME', $javaHome, 'User')
$env:JAVA_HOME = $javaHome
Log "JAVA_HOME=$javaHome"

$ghidraTarget = Join-Path $tools 'ghidra\ghidra_12.1.4_PUBLIC'
$ghidraUrl = 'https://github.com/NationalSecurityAgency/ghidra/releases/download/Ghidra_12.1_build/ghidra_12.1_PUBLIC_20260513.zip'

if (-not (Test-Path (Join-Path $ghidraTarget 'Ghidra\application.properties'))) {
    $zip = Join-Path $tools 'ghidra\ghidra_12.1.4_PUBLIC.zip'
    if (-not (Test-Path $zip)) {
        Log "Downloading Ghidra 12.1.4 ..."
        Invoke-WebRequest -Uri $ghidraUrl -OutFile $zip -UseBasicParsing
    }
    Log "Extracting Ghidra to $ghidraTarget ..."
    if (Test-Path $ghidraTarget) { Remove-Item -Recurse -Force $ghidraTarget }
    Expand-Archive -LiteralPath $zip -DestinationPath (Join-Path $tools 'ghidra') -Force
    $extracted = Get-ChildItem (Join-Path $tools 'ghidra') -Directory -Filter 'ghidra_*' | Select-Object -First 1
    if ($extracted -and $extracted.FullName -ne $ghidraTarget) {
        if (Test-Path $ghidraTarget) { Remove-Item -Recurse -Force $ghidraTarget }
        Rename-Item $extracted.FullName $ghidraTarget
    }
}
[Environment]::SetEnvironmentVariable('GHIDRA_INSTALL_DIR', $ghidraTarget, 'User')
$env:GHIDRA_INSTALL_DIR = $ghidraTarget
Log "GHIDRA_INSTALL_DIR=$ghidraTarget"

# Ghidra headless smoke
$headless = Join-Path $ghidraTarget 'support\analyzeHeadless.bat'
if (Test-Path $headless) {
    & $headless 2>&1 | Select-Object -First 3 | ForEach-Object { Log "analyzeHeadless: $_" }
}

# ARTEX 0.3.30
$artexDir = Join-Path $tools 'artex\artex-0.3.30-windows-amd64'
if (-not (Test-Path (Join-Path $artexDir 'artex.exe'))) {
    $artexZip = Join-Path $tools 'artex\artex-0.3.30-windows-amd64.zip'
    if (-not (Test-Path $artexZip)) {
        Log 'Downloading ARTEX v0.3.30 ...'
        Invoke-WebRequest -Uri 'https://github.com/RuoJi6/ARTEX/releases/download/v0.3.30/artex-0.3.30-windows-amd64.zip' -OutFile $artexZip -UseBasicParsing
    }
    Expand-Archive -LiteralPath $artexZip -DestinationPath (Join-Path $tools 'artex') -Force
}
Log "ARTEX dir: $artexDir"

# Python RE stack (3.12)
$py = (Get-Command py -ErrorAction SilentlyContinue)
if (-not $py) { Log 'SKIP pip RE stack — Python 3.12 not installed (winget UAC)' }
elseif ($py) {
    Log 'Installing Python RE packages ...'
    & py -3.12 -m pip install --upgrade pip 2>&1 | Out-Null
    $pkgs = @(
        'capstone', 'pefile', 'lief', 'unicorn', 'keystone-engine', 'pwntools', 'yara-python',
        'pycryptodome', 'cryptography', 'mcp',
        'git+https://github.com/LaurieWired/GhidraMCP'
    )
    & py -3.12 -m pip install $pkgs 2>&1 | Tee-Object -FilePath (Join-Path $logDir 'pip-re.log')
}

# Clone reference repos (shallow)
$cloneMap = [ordered]@{
    'ARTEX'                   = 'https://github.com/RuoJi6/ARTEX.git'
    'GhidraMCP'               = 'https://github.com/LaurieWired/GhidraMCP.git'
    'YoneSysHunter'           = 'https://github.com/y0neET/YoneSysHunter.git'
    'Hypervisor-From-Scratch' = 'https://github.com/SinaKarvandi/Hypervisor-From-Scratch.git'
}

if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
    Log 'SKIP git clones — git not installed (winget UAC). Use share or re-run interactive winget.'
} else {
    foreach ($name in $cloneMap.Keys) {
        $dest = Join-Path $repos $name
        if (Test-Path (Join-Path $dest '.git')) { Log "repo exists: $name"; continue }
        Log "git clone $name ..."
        git clone --depth 1 $cloneMap[$name] $dest 2>&1 | Tee-Object -FilePath $log -Append
    }
}

# P_Finger_ByPass lab checkout (share or git)
$bypass = Join-Path $LabRoot 'P_Finger_ByPass'
if (-not (Test-Path (Join-Path $bypass 'kit\install.ps1'))) {
    if ($share -and (Test-Path (Join-Path $share 'P_Finger_ByPass\kit\install.ps1'))) {
        Log "Using share P_Finger_ByPass at $share"
    } else {
        if (Get-Command git -ErrorAction SilentlyContinue) {
            Log 'Cloning P_Finger_ByPass ...'
            git clone --depth 1 https://github.com/SamNZito/P_Finger_ByPass.git $bypass 2>&1 | Tee-Object -FilePath $log -Append
        } else {
            Log 'P_Finger_ByPass available on share only (no git)'
        }
    }
}

# Cursor MCP: REA + GhidraMCP bridge
$mcpPath = Join-Path $env:USERPROFILE '.cursor\mcp.json'
New-Item -ItemType Directory -Force -Path (Split-Path $mcpPath) | Out-Null
$nodeNpx = Join-Path $tools 'node\npx.cmd'
if (-not (Test-Path $nodeNpx)) { $nodeNpx = 'npx.cmd' }
$reaBlock = [ordered]@{
    command = $nodeNpx
    args    = @('-y', 'rea-agents@latest', 'mcp')
    env     = [ordered]@{
        JAVA_HOME          = $javaHome
        GHIDRA_INSTALL_DIR = $ghidraTarget
    }
}
$ghidraMcp = [ordered]@{
    command = 'ghidra-mcp'
    args    = @('--ghidra-server', 'http://127.0.0.1:8080/')
}
if (Test-Path $mcpPath) {
    $cfg = Get-Content $mcpPath -Raw | ConvertFrom-Json
} else {
    $cfg = [pscustomobject]@{ mcpServers = [pscustomobject]@{} }
}
if (-not $cfg.mcpServers) { $cfg | Add-Member -NotePropertyName mcpServers -Value ([pscustomobject]@{}) }
$cfg.mcpServers | Add-Member -NotePropertyName 'rea' -Value ([pscustomobject]$reaBlock) -Force
$cfg.mcpServers | Add-Member -NotePropertyName 'ghidra' -Value ([pscustomobject]$ghidraMcp) -Force
($cfg | ConvertTo-Json -Depth 8) | Set-Content -LiteralPath $mcpPath -Encoding UTF8
$projMcp = Join-Path $LabRoot '.cursor\mcp.json'
New-Item -ItemType Directory -Force -Path (Split-Path $projMcp) | Out-Null
Copy-Item -LiteralPath $mcpPath -Destination $projMcp -Force
Log "Wrote MCP: $mcpPath and $projMcp"

$doctorOut = Join-Path $logDir 'rea-doctor-guest-snapshot.txt'
Log 'rea doctor ...'
Refresh-Path
& npx.cmd -y rea-agents@latest doctor 2>&1 | Tee-Object -FilePath $doctorOut
Log "Doctor snapshot: $doctorOut"

if ($share) {
    Copy-Item -Force $doctorOut (Join-Path $share 'docs\rea-doctor-guest-snapshot.txt')
    Copy-Item -Force $log (Join-Path $share 'docs\install-toolchain-guest.log') -ErrorAction SilentlyContinue
}

Log 'Done. Guest: restart Cursor; run analyzeHeadless / rea doctor if ghidra unhealthy.'
exit 0