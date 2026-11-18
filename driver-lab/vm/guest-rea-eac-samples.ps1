$ErrorActionPreference = "Continue"
$env:GHIDRA_INSTALL_DIR = "C:\Tools\ghidra_12.1_PUBLIC"
$env:JAVA_HOME = "C:\Program Files\Microsoft\jdk-21.0.13.21-hotspot"
$outDir = "C:\re\artifacts"
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
Copy-Item "Z:\ac-captures-host\2026-11-17\EasyAntiCheat_EOS.sys" "$outDir\EasyAntiCheat_EOS.sys" -Force
Copy-Item "Z:\ac-captures-host\2026-11-17\EasyAntiCheat.sys" "$outDir\EasyAntiCheat.sys" -Force
Get-ChildItem $outDir | Select-Object Name, Length | Format-Table | Out-String
Write-Output ("EOS " + (Get-FileHash "$outDir\EasyAntiCheat_EOS.sys" -Algorithm SHA256).Hash)
Write-Output ("LEG " + (Get-FileHash "$outDir\EasyAntiCheat.sys" -Algorithm SHA256).Hash)
Write-Output "=== BEDaisy guest search (bounded) ==="
$roots = @("C:\Windows\System32\drivers","C:\Program Files (x86)","C:\Program Files","C:\re")
foreach ($r in $roots) {
  if (Test-Path $r) {
    Get-ChildItem $r -Filter "BEDaisy.sys" -Recurse -ErrorAction SilentlyContinue | Select-Object FullName, Length
  }
}
Write-Output "=== npx help (first lines) ==="
npx.cmd -y rea-agents@latest --help 2>&1 | Select-Object -First 60
Write-Output "=== inspect-pe-imports ==="
npx.cmd -y rea-agents@latest inspect-pe-imports --path "C:\re\artifacts\EasyAntiCheat_EOS.sys" --json 2>&1 | Select-Object -First 80
Write-Output "GUEST_STAGE_DONE"
