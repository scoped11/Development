# Bootstrap JDK 21, Ghidra 12.x, and REA MCP on AI-Workspace guest.
# Thin entry point — delegates to install-ghidra-rea-guest.ps1.
#
#   powershell -NoProfile -ExecutionPolicy Bypass -File D:\hostdev\driver-lab\vm\bootstrap-vm-tools.ps1
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
    throw 'hostdev share not found — install Guest Additions and mount hostdev'
}

$share = Resolve-DevShare
$installScript = Join-Path $share 'driver-lab\vm\install-ghidra-rea-guest.ps1'
if (-not (Test-Path $installScript)) {
    throw "Missing $installScript"
}

Write-Host '[bootstrap-vm-tools] hostdev:' $share
Write-Host '[bootstrap-vm-tools] invoking install-ghidra-rea-guest.ps1'

$args = @(
    '-NoProfile', '-ExecutionPolicy', 'Bypass',
    '-File', $installScript,
    '-InstallRoot', $InstallRoot,
    '-GhidraVersion', $GhidraVersion
)
if ($GhidraZipPath) { $args += @('-GhidraZipPath', $GhidraZipPath) }
if ($SkipReaSetup) { $args += '-SkipReaSetup' }

& powershell.exe @args
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host ''
Write-Host '[bootstrap-vm-tools] Next steps (guest):'
Write-Host '  1. npx.cmd -y rea-agents@latest setup    # select Cursor'
Write-Host '  2. npx.cmd -y rea-agents@latest doctor   | Tee-Object' (Join-Path $share 'docs\rea-doctor-guest-snapshot.txt')
Write-Host '  3. Restart Cursor'
Write-Host '  4. Compare doctor output to docs\rea-doctor-guest-expected.md'
Write-Host '  5. Smoke test: docs\rea-vm-smoke-test.md (Betest.sys on share)'