$ErrorActionPreference = "Continue"
$env:GHIDRA_INSTALL_DIR = "C:\Tools\ghidra_12.1_PUBLIC"
$env:JAVA_HOME = "C:\Program Files\Microsoft\jdk-21.0.13.21-hotspot"
$out = "Z:\ac-captures-host\2026-11-18"
$art = "C:\re\artifacts"

function Invoke-Rea {
    param([string]$LogName, [string[]]$ReaArgs)
    $log = Join-Path $out $LogName
    Write-Output "=== START $LogName $(Get-Date -Format o) ==="
    & npx.cmd -y rea-agents@latest @ReaArgs 2>&1 | Tee-Object $log
    Write-Output "=== EXIT $LogName code=$LASTEXITCODE $(Get-Date -Format o) ==="
}

Invoke-Rea "rea-function-eos-123b1e.json" @(
    "function", "$art\EasyAntiCheat_EOS.sys", "--procedure", "0x140123b1e",
    "--format", "json", "--filter-output", "normalized_result.pseudocode,normalized_result.assembly,normalized_result.procedure,subject,evidence_id"
)
Invoke-Rea "rea-function-eos-381041.json" @(
    "function", "$art\EasyAntiCheat_EOS.sys", "--procedure", "0x140381041",
    "--format", "json", "--filter-output", "normalized_result.pseudocode,normalized_result.assembly,normalized_result.procedure,normalized_result.callees,subject,evidence_id"
)
Invoke-Rea "rea-decompile-legacy-section6.json" @(
    "decompile", "$art\EasyAntiCheat.sys", "--file-offset", "0x6b3a46",
    "--format", "json", "--filter-output", "normalized_result,subject,evidence_id,confidence"
)
Write-Output "GUEST_KERNEL_FACTS_3_DONE $(Get-Date -Format o)"
