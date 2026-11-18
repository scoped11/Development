# Static REA of offline BEDaisy.sys on guest AI_workspace. Driver is NOT loaded.
$ErrorActionPreference = "Continue"
$env:GHIDRA_INSTALL_DIR = "C:\Tools\ghidra_12.1_PUBLIC"
$env:JAVA_HOME = "C:\Program Files\Microsoft\jdk-21.0.13.21-hotspot"
$out = "Z:\ac-captures-host\2026-11-18"
$art = "C:\re\artifacts"
$src = "Z:\ac-captures-host\2026-11-18\BEDaisy.sys"
$expect = "E7BCF344EEFA7A4EBEA9B048E376958A5AD711489249033FA480A7EC8B6C018E"
New-Item -ItemType Directory -Force -Path $out, $art | Out-Null

function Invoke-Rea {
    param([string]$LogName, [string[]]$ReaArgs)
    $log = Join-Path $out $LogName
    Write-Output "=== START $LogName $(Get-Date -Format o) ==="
    & npx.cmd -y rea-agents@latest @ReaArgs 2>&1 | Tee-Object $log
    Write-Output "=== EXIT $LogName code=$LASTEXITCODE $(Get-Date -Format o) ==="
}

Write-Output "STAGE $(Get-Date -Format o)"
if (-not (Test-Path $src)) { throw "missing $src" }
Copy-Item $src "$art\BEDaisy.sys" -Force
$h = (Get-FileHash "$art\BEDaisy.sys" -Algorithm SHA256).Hash
Write-Output "BEDAISY $h"
if ($h -ne $expect) { throw "hash mismatch guest=$h expect=$expect" }
Get-Item "$art\BEDaisy.sys" | Select-Object FullName, Length | Format-List | Out-String

Invoke-Rea "rea-inspect-imports-bedaisy.json" @(
    "inspect-pe-imports", "$art\BEDaisy.sys", "--format", "json",
    "--filter-output", "normalized_result,subject,evidence_id,confidence"
)
Invoke-Rea "rea-inspect-exports-bedaisy.json" @(
    "inspect-native-exports", "$art\BEDaisy.sys", "--format", "json",
    "--filter-output", "normalized_result,subject,evidence_id,confidence"
)
Invoke-Rea "rea-function-bedaisy-entry.json" @(
    "function", "$art\BEDaisy.sys", "--procedure", "0x1425ccea8", "--format", "json",
    "--filter-output", "normalized_result,subject,evidence_id,confidence,limitations"
)
Invoke-Rea "rea-search-strings-bedaisy-device.json" @(
    "search-strings", "$art\BEDaisy.sys", "--pattern", "BattlEye", "--format", "json",
    "--filter-output", "normalized_result,subject,evidence_id"
)
Invoke-Rea "rea-search-strings-bedaisy-name.json" @(
    "search-strings", "$art\BEDaisy.sys", "--pattern", "BEDaisy", "--format", "json",
    "--filter-output", "normalized_result,subject,evidence_id"
)
Invoke-Rea "rea-search-strings-bedaisy-devicepath.json" @(
    "search-strings", "$art\BEDaisy.sys", "--pattern", "\\Device\\", "--format", "json",
    "--filter-output", "normalized_result,subject,evidence_id"
)
Invoke-Rea "rea-search-procedures-bedaisy.json" @(
    "search-procedures", "$art\BEDaisy.sys", "DriverEntry", "--format", "json",
    "--filter-output", "normalized_result,subject,evidence_id"
)

Write-Output "GUEST_BEDAISY_REA_DONE $(Get-Date -Format o)"
