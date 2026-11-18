$ErrorActionPreference = "Continue"
$env:GHIDRA_INSTALL_DIR = "C:\Tools\ghidra_12.1_PUBLIC"
$env:JAVA_HOME = "C:\Program Files\Microsoft\jdk-21.0.13.21-hotspot"
$out = "Z:\ac-captures-host\2026-11-18"
$art = "C:\re\artifacts"
New-Item -ItemType Directory -Force -Path $out, $art | Out-Null

function Invoke-Rea {
    param([string]$LogName, [string[]]$ReaArgs)
    $log = Join-Path $out $LogName
    Write-Output "=== START $LogName $(Get-Date -Format o) ==="
    & npx.cmd -y rea-agents@latest @ReaArgs 2>&1 | Tee-Object $log
    Write-Output "=== EXIT $LogName code=$LASTEXITCODE $(Get-Date -Format o) ==="
}

Write-Output "STAGE $(Get-Date -Format o)"
Copy-Item "Z:\ac-captures-host\2026-11-18\BEService.exe" "$art\BEService.exe" -Force
Copy-Item "Z:\ac-captures-host\2026-11-18\BEService_x64.exe" "$art\BEService_x64.exe" -Force
Copy-Item "Z:\ac-captures-host\2026-11-18\BEClient_x64.dll" "$art\BEClient_x64.dll" -Force
Copy-Item "Z:\ac-captures-host\2026-11-18\BEServer_x64.dll" "$art\BEServer_x64.dll" -Force
# kernel EAC already staged; re-hash
Get-ChildItem $art | Select-Object Name, Length | Format-Table -AutoSize | Out-String
Write-Output ("EOS " + (Get-FileHash "$art\EasyAntiCheat_EOS.sys" -Algorithm SHA256).Hash)
Write-Output ("LEG " + (Get-FileHash "$art\EasyAntiCheat.sys" -Algorithm SHA256).Hash)
Write-Output ("BESVC " + (Get-FileHash "$art\BEService.exe" -Algorithm SHA256).Hash)

Write-Output "=== guest BEDaisy hunt ==="
Get-ChildItem -Path "C:\Windows\System32\drivers","C:\Program Files","C:\Program Files (x86)","C:\re" -Recurse -Filter "BEDaisy.sys" -ErrorAction SilentlyContinue |
    Select-Object FullName, Length | Format-Table -AutoSize | Out-String

# CLI surface (short)
Invoke-Rea "rea-cli-help.txt" @("--help")

# PE IAT (no Ghidra)
Invoke-Rea "rea-inspect-imports-eos.json" @("inspect-pe-imports", "$art\EasyAntiCheat_EOS.sys", "--format", "json", "--filter-output", "normalized_result,subject,evidence_id,confidence")
Invoke-Rea "rea-inspect-imports-legacy.json" @("inspect-pe-imports", "$art\EasyAntiCheat.sys", "--format", "json", "--filter-output", "normalized_result,subject,evidence_id,confidence")
Invoke-Rea "rea-inspect-imports-beservice-x64.json" @("inspect-pe-imports", "$art\BEService_x64.exe", "--format", "json", "--filter-output", "normalized_result,subject,evidence_id,confidence")

# Ghidra: entries
Invoke-Rea "rea-function-legacy-entry.json" @("function", "$art\EasyAntiCheat.sys", "--procedure", "0x1400afde8", "--format", "json", "--filter-output", "normalized_result,subject,evidence_id,confidence,limitations")
Invoke-Rea "rea-function-eos-entry.json" @("function", "$art\EasyAntiCheat_EOS.sys", "--procedure", "0x140272978", "--format", "json", "--filter-output", "normalized_result,subject,evidence_id,confidence,limitations")

# Strings / procedures
Invoke-Rea "rea-search-strings-legacy-device.json" @("search-strings", "$art\EasyAntiCheat.sys", "--pattern", "EasyAntiCheat", "--format", "json", "--filter-output", "normalized_result,subject,evidence_id")
Invoke-Rea "rea-search-strings-eos-device.json" @("search-strings", "$art\EasyAntiCheat_EOS.sys", "--pattern", "EasyAntiCheat", "--format", "json", "--filter-output", "normalized_result,subject,evidence_id")
Invoke-Rea "rea-search-procedures-legacy.json" @("search-procedures", "$art\EasyAntiCheat.sys", "DriverEntry", "--format", "json", "--filter-output", "normalized_result,subject,evidence_id")
Invoke-Rea "rea-search-procedures-eos.json" @("search-procedures", "$art\EasyAntiCheat_EOS.sys", "DriverEntry", "--format", "json", "--filter-output", "normalized_result,subject,evidence_id")

Write-Output "GUEST_KERNEL_FACTS_DONE $(Get-Date -Format o)"
