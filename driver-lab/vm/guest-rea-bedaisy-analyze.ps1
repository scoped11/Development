# Cold Ghidra analyze of offline BEDaisy.sys. Driver is NOT loaded.
$ErrorActionPreference = "Continue"
$env:GHIDRA_INSTALL_DIR = "C:\Tools\ghidra_12.1_PUBLIC"
$env:JAVA_HOME = "C:\Program Files\Microsoft\jdk-21.0.13.21-hotspot"
$env:REA_GHIDRA_STARTUP_TIMEOUT_MS = "1800000"
$env:_JAVA_OPTIONS = "-Xmx4g"
$out = "Z:\ac-captures-host\2026-11-18"
$art = "C:\re\artifacts\BEDaisy.sys"
New-Item -ItemType Directory -Force -Path $out | Out-Null
Write-Output "START_ANALYZE $(Get-Date -Format o)"
Write-Output "HASH=$((Get-FileHash $art -Algorithm SHA256).Hash)"
& npx.cmd -y rea-agents@latest analyze $art --provider ghidra --format json --filter-output "normalized_result,subject,limitations,confidence,evidence_id" 2>&1 |
    Tee-Object "$out\rea-analyze-bedaisy.json"
Write-Output "ANALYZE_EXIT $LASTEXITCODE $(Get-Date -Format o)"
if ($LASTEXITCODE -eq 0) {
    Write-Output "START_FUNCTION $(Get-Date -Format o)"
    & npx.cmd -y rea-agents@latest function $art --procedure 0x1425ccea8 --format json --filter-output "normalized_result.pseudocode,normalized_result.assembly,normalized_result.procedure,subject,evidence_id,confidence,limitations" 2>&1 |
        Tee-Object "$out\rea-function-bedaisy-entry-2.json"
    Write-Output "FUNCTION_EXIT $LASTEXITCODE $(Get-Date -Format o)"
    Write-Output "START_STRINGS $(Get-Date -Format o)"
    & npx.cmd -y rea-agents@latest search-strings $art --pattern BattlEye --format json --filter-output "normalized_result,subject,evidence_id" 2>&1 |
        Tee-Object "$out\rea-search-strings-bedaisy-battleye-2.json"
    Write-Output "STRINGS_EXIT $LASTEXITCODE $(Get-Date -Format o)"
}
Write-Output "GUEST_BEDAISY_ANALYZE_DONE $(Get-Date -Format o)"
