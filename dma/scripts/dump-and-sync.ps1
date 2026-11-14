param(
  [string]$GameRoot = "D:\SteamLibrary\steamapps\common\Rob Run",
  [string]$DumpDate = (Get-Date -Format "yyyy-MM-dd"),
  [switch]$SkipBuild,
  [switch]$DummyDll
)

$ErrorActionPreference = "Stop"
$dma = Split-Path -Parent $PSScriptRoot
$dev = Split-Path -Parent $dma
$out = Join-Path $dev "dumps\$DumpDate"
$ga = Join-Path $GameRoot "GameAssembly.dll"
$meta = Join-Path $GameRoot "ROB RUN_Data\il2cpp_data\Metadata\global-metadata.dat"
$dumper = Join-Path $dev "tools\il2cpp-dumper\Il2CppDumper.exe"

foreach ($p in @($ga, $meta, $dumper)) {
  if (-not (Test-Path $p)) { throw "Missing: $p" }
}

$metaBytes = New-Object byte[] 8
$fs = [System.IO.File]::OpenRead($meta)
[void]$fs.Read($metaBytes, 0, 8)
$fs.Close()
$magic = [BitConverter]::ToUInt32($metaBytes, 0)
$metaVer = [BitConverter]::ToUInt32($metaBytes, 4)
Write-Host ("[*] global-metadata.dat magic=0x{0:X8} version={1} (need 0xFAB11BAF / 39)" -f $magic, $metaVer)
if ($magic -ne 0xFAB11BAF) {
  throw "global-metadata.dat magic 0x$($magic.ToString('X8')) is not 0xFAB11BAF"
}
if ($metaVer -ne 39) {
  throw "Unsupported IL2CPP metadata version $metaVer. Rob Run is Unity 6000 metadata v39. Use tools\il2cpp-dumper (c01ns v39.1)."
}

New-Item -ItemType Directory -Force -Path $out | Out-Null
$cfg = Join-Path (Split-Path $dumper) "config.json"
$dummy = "true"
if ($PSBoundParameters.ContainsKey('DummyDll') -and -not $DummyDll) { $dummy = "false" }
@"
{
  "DumpMethod": true,
  "DumpField": true,
  "DumpProperty": true,
  "DumpAttribute": true,
  "DumpFieldOffset": true,
  "DumpMethodOffset": true,
  "DumpTypeDefIndex": true,
  "GenerateDummyDll": $dummy,
  "GenerateStruct": true,
  "DummyDllAddToken": true,
  "RequireAnyKey": false,
  "ForceIl2CppVersion": false,
  "ForceVersion": 16,
  "ForceDump": false,
  "NoRedirectedPointer": false
}
"@ | Set-Content -Path $cfg -Encoding UTF8

Write-Host "[*] Dumping to $out with c01ns Il2CppDumper v39.1 ($dumper)"
Push-Location $out
try {
  & $dumper $ga $meta $out
  if ($LASTEXITCODE -ne 0) { throw "Il2CppDumper exited $LASTEXITCODE" }
}
finally {
  Pop-Location
}

if (-not (Test-Path (Join-Path $out "script.json"))) {
  throw "Dump did not produce script.json"
}

Write-Host "[*] Syncing TypeInfo RVAs into dma\src\game\offsets.h"
python (Join-Path $PSScriptRoot "sync_typeinfo_offsets.py") --dump-dir $out

if (-not $SkipBuild) {
  Write-Host "[*] Rebuilding debug/ only (build/ requires -IApprove)"
  & (Join-Path $PSScriptRoot "build-windows.ps1") -Section Debug
}

Write-Host "[+] Done. Review dumps\$DumpDate\OFFSET_REPORT.md"
