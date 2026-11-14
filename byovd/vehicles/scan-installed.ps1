# Lists signed BYOVD vehicles from official install paths only (no downloads).
$names = 'RTCore64.sys','CorsairLLAccess64.sys','ThrottleStop.sys','TRIXX.sys','SIVX64.sys'
$roots = @($env:ProgramFiles, ${env:ProgramFiles(x86)}, $env:LOCALAPPDATA, $env:ProgramData) | Where-Object { $_ }
foreach ($n in $names) {
    $hit = $null
    foreach ($root in $roots) {
        $hit = Get-ChildItem -Path $root -Filter $n -Recurse -Depth 10 -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($hit) { break }
    }
    if ($hit) { Write-Host ("FOUND  {0,-26} {1}" -f $n, $hit.FullName) }
    else { Write-Host ("MISSING {0}" -f $n) }
}
