$ErrorActionPreference = 'Continue'
$out = 'Z:\ac-captures-host\2026-11-18\dyn-lab-ci-events.txt'
$t0 = Get-Date '2026-11-18 14:50:00'
@(
    "UTC=$((Get-Date).ToUniversalTime().ToString('o'))"
    '--- leftover services ---'
) | Set-Content $out -Encoding UTF8
foreach ($n in @('LabDynEACEOS','LabDynEAC','LabDynBEDaisy','EasyAntiCheat_EOS','EasyAntiCheat','BEDaisy')) {
    $q = (sc.exe query $n 2>&1 | Out-String).Trim() -replace '\s+', ' '
    Add-Content $out "SVC $n $q"
}
Add-Content $out '--- CodeIntegrity Operational since 14:50 ---'
try {
    Get-WinEvent -FilterHashtable @{ LogName = 'Microsoft-Windows-CodeIntegrity/Operational'; StartTime = $t0 } -ErrorAction SilentlyContinue |
        Select-Object -First 40 TimeCreated, Id, Message |
        ForEach-Object {
            $msg = ($_.Message -replace '\s+', ' ')
            if ($msg.Length -gt 400) { $msg = $msg.Substring(0, 400) }
            Add-Content $out ("CIEVT t=$($_.TimeCreated) id=$($_.Id) $msg")
        }
} catch { Add-Content $out $_.Exception.Message }
Add-Content $out '--- System FilterManager/SCM/CI since 14:50 ---'
try {
    Get-WinEvent -FilterHashtable @{ LogName = 'System'; StartTime = $t0 } -ErrorAction SilentlyContinue |
        Where-Object { $_.ProviderName -match 'FilterManager|Service Control Manager|CodeIntegrity|CI' -or $_.Id -in 7045,577,219 } |
        Select-Object -First 40 TimeCreated, Id, ProviderName, Message |
        ForEach-Object {
            $msg = ($_.Message -replace '\s+', ' ')
            if ($msg.Length -gt 400) { $msg = $msg.Substring(0, 400) }
            Add-Content $out ("SYSEVT t=$($_.TimeCreated) id=$($_.Id) src=$($_.ProviderName) $msg")
        }
} catch { Add-Content $out $_.Exception.Message }
Get-Content $out
