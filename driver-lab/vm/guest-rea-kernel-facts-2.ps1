$ErrorActionPreference = "Continue"
$env:GHIDRA_INSTALL_DIR = "C:\Tools\ghidra_12.1_PUBLIC"
$env:JAVA_HOME = "C:\Program Files\Microsoft\jdk-21.0.13.21-hotspot"
$out = "Z:\ac-captures-host\2026-11-18"
$art = "C:\re\artifacts"
New-Item -ItemType Directory -Force -Path $out | Out-Null

function Invoke-Rea {
    param([string]$LogName, [string[]]$ReaArgs)
    $log = Join-Path $out $LogName
    Write-Output "=== START $LogName $(Get-Date -Format o) ==="
    & npx.cmd -y rea-agents@latest @ReaArgs 2>&1 | Tee-Object $log
    Write-Output "=== EXIT $LogName code=$LASTEXITCODE $(Get-Date -Format o) ==="
}

# Real DriverEntry bodies (legacy SECTION.6 trampoline target; EOS SECTION.7 thunk target)
Invoke-Rea "rea-function-legacy-real-entry.json" @(
    "function", "$art\EasyAntiCheat.sys", "--procedure", "0x14071c246",
    "--format", "json", "--filter-output", "normalized_result.pseudocode,normalized_result.assembly,normalized_result.callees,normalized_result.referenced_strings,normalized_result.procedure,subject,evidence_id,confidence"
)
Invoke-Rea "rea-function-eos-real-entry.json" @(
    "function", "$art\EasyAntiCheat_EOS.sys", "--procedure", "0x1403a572e",
    "--format", "json", "--filter-output", "normalized_result.pseudocode,normalized_result.assembly,normalized_result.callees,normalized_result.referenced_strings,normalized_result.procedure,subject,evidence_id,confidence"
)

# FltRegisterFilter call site (legacy .text)
Invoke-Rea "rea-function-legacy-fltreg.json" @(
    "function", "$art\EasyAntiCheat.sys", "--procedure", "0x1400658d4",
    "--format", "json", "--filter-output", "normalized_result.pseudocode,normalized_result.assembly,normalized_result.procedure,normalized_result.callers,subject,evidence_id"
)

# Byte search for device path UTF-16
Invoke-Rea "rea-search-bytes-legacy-device.json" @(
    "search-bytes", "$art\EasyAntiCheat.sys", "--pattern", "5c004400650076006900630065005c0045006100730079",
    "--format", "json", "--filter-output", "normalized_result,subject,evidence_id"
)
Invoke-Rea "rea-search-bytes-eos-device.json" @(
    "search-bytes", "$art\EasyAntiCheat_EOS.sys", "--pattern", "450061007300790041006e0074006900430068006500610074005f0045004f005300",
    "--format", "json", "--filter-output", "normalized_result,subject,evidence_id"
)

Write-Output "GUEST_KERNEL_FACTS_2_DONE $(Get-Date -Format o)"
