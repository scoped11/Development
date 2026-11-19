$ver = 'v26.13.1'
$tools = 'C:\lab\tools'
$nodeRoot = Join-Path $tools 'node'
$zip = Join-Path $tools "node-$ver-win-x64.zip"
$url = "https://nodejs.org/dist/$ver/node-$ver-win-x64.zip"
Invoke-WebRequest -Uri $url -OutFile $zip -UseBasicParsing
Get-ChildItem $nodeRoot -ErrorAction SilentlyContinue | Remove-Item -Recurse -Force -ErrorAction SilentlyContinue
Expand-Archive -LiteralPath $zip -DestinationPath $nodeRoot -Force
$inner = Get-ChildItem $nodeRoot -Directory | Select-Object -First 1
if ($inner) {
    Get-ChildItem $inner.FullName | Move-Item -Destination $nodeRoot -Force
    Remove-Item $inner.FullName -Force
}
& (Join-Path $nodeRoot 'node.exe') -v