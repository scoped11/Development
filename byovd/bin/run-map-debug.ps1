$bin = $PSScriptRoot
$log = Join-Path $bin 'map-debug.log'
$runner = Join-Path $bin 'Driver2Runner.exe'
$cmd = "& `"$runner`" --profile universal --mode session --driver auto --skip-rw *>&1 | Out-File -FilePath `"$log`" -Encoding utf8"
Start-Process powershell -Verb RunAs -ArgumentList '-NoProfile','-ExecutionPolicy','Bypass','-Command',$cmd -Wait
Get-Content $log -ErrorAction SilentlyContinue
