# Fast guest probe. Drivers are NOT loaded. Share is Z:\.
$ErrorActionPreference = "Continue"
Write-Output "HOSTNAME=$(hostname)"
Write-Output "Z=$(Test-Path 'Z:\')"
Write-Output "GHIDRA=$(Test-Path 'C:\Tools\ghidra_12.1_PUBLIC\support\analyzeHeadless.bat')"
Write-Output "JAVA_HOME_DIR=$(Test-Path 'C:\Program Files\Microsoft\jdk-21.0.13.21-hotspot')"
Write-Output "NPX=$(Get-Command npx.cmd -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source)"
Write-Output "OBJDUMP=$(Get-Command objdump.exe -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source)"
$art = "C:\re\artifacts"
Get-ChildItem $art -ErrorAction SilentlyContinue | Select-Object Name, Length | Format-Table -AutoSize | Out-String
foreach ($n in @("EasyAntiCheat_EOS.sys","EasyAntiCheat.sys","BEDaisy.sys")) {
    $p = Join-Path $art $n
    if (Test-Path $p) {
        Write-Output ("HASH $n " + (Get-FileHash $p -Algorithm SHA256).Hash)
    } else {
        Write-Output "MISSING $p"
    }
}
Write-Output "HEADLESS=$(Test-Path 'C:\Tools\ghidra_12.1_PUBLIC\support\analyzeHeadless.bat')"
Write-Output "PROJECTS=$(Test-Path 'C:\re\ghidra-projects')"
Get-ChildItem "C:\re\ghidra-projects" -ErrorAction SilentlyContinue | Select-Object Name, Mode | Format-Table | Out-String
Write-Output "PROBE_DONE"
