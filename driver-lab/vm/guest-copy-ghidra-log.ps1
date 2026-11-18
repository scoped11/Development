$ErrorActionPreference = "Continue"
$dst = "Z:\ac-captures-host\2026-11-18"
$roots = Get-ChildItem "C:\Users\vboxuser\AppData\Local\Temp" -Directory -Filter "rea-ghidra-*" -ErrorAction SilentlyContinue |
    Sort-Object LastWriteTime -Descending
Write-Output "ROOTS=$($roots.Count)"
foreach ($r in $roots | Select-Object -First 2) {
    Write-Output "DIR=$($r.FullName) $($r.LastWriteTime.ToString('o'))"
    foreach ($name in @("ghidra.log", "script.log", "bridge-endpoint.json")) {
        $p = Join-Path $r.FullName $name
        if (Test-Path $p) {
            Copy-Item $p (Join-Path $dst ("ghidra-" + $r.Name + "-" + $name)) -Force
            Write-Output "COPIED=$name LEN=$((Get-Item $p).Length)"
        } else {
            Write-Output "MISSING=$name"
        }
    }
}
Get-Content (Join-Path $roots[0].FullName "ghidra.log") -Tail 80 -ErrorAction SilentlyContinue
Write-Output "LOG_COPY_DONE"
