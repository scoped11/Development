$ErrorActionPreference = "Continue"
$env:GHIDRA_INSTALL_DIR = "C:\Tools\ghidra_12.1_PUBLIC"
$env:JAVA_HOME = "C:\Program Files\Microsoft\jdk-21.0.13.21-hotspot"
npx.cmd -y rea-agents@latest function --help 2>&1 | Select-Object -First 30
Write-Output "=== function legacy entry ==="
npx.cmd -y rea-agents@latest function "C:\re\artifacts\EasyAntiCheat.sys" --procedure 0x1400afde8 --format json --filter-output "normalized_result,subject,evidence_id,confidence" 2>&1 | Select-Object -First 80
Write-Output "FUNC_DONE"
