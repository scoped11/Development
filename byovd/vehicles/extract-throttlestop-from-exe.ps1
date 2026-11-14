# Vendor ThrottleStop.exe -> ThrottleStop.sys (embedded signed driver). Does not load driver.
param(
    [string]$ExePath = '',
    [string]$OutDir = ''
)
$ErrorActionPreference = 'Stop'
if (-not $ExePath) {
    $hit = Get-ChildItem "$env:LOCALAPPDATA\Microsoft\WinGet\Packages" -Filter 'ThrottleStop.exe' -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $hit) { throw 'Install ThrottleStop via winget or pass -ExePath' }
    $ExePath = $hit.FullName
}
if (-not $OutDir) {
    $OutDir = Split-Path $MyInvocation.MyCommand.Path -Parent
}
$bytes = [IO.File]::ReadAllBytes($ExePath)
$best = $null
for ($i = 0; $i -lt $bytes.Length - 2; $i++) {
    if ($bytes[$i] -ne 0x4D -or $bytes[$i + 1] -ne 0x5A) { continue }
    try {
        $e = [BitConverter]::ToInt32($bytes, $i + 0x3C)
        if ($e -le 0 -or ($i + $e + 6) -ge $bytes.Length) { continue }
        if ([Text.Encoding]::ASCII.GetString($bytes, $i + $e, 4) -ne 'PE' + [char]0) { continue }
        $nsect = [BitConverter]::ToUInt16($bytes, $i + $e + 6)
        $opt = $i + $e + 24
        $magic = [BitConverter]::ToUInt16($bytes, $opt)
        $sectBase = $opt + $(if ($magic -eq 0x20b) { 108 } else { 96 })
        $sub = if ($magic -eq 0x20b) { [BitConverter]::ToUInt16($bytes, $opt + 68) } else { [BitConverter]::ToUInt16($bytes, $opt + 56) }
        if ($sub -ne 1) { continue }
        $maxEnd = 0
        for ($s = 0; $s -lt $nsect; $s++) {
            $base = $sectBase + $s * 40
            $rsize = [BitConverter]::ToUInt32($bytes, $base + 16)
            $rptr = [BitConverter]::ToUInt32($bytes, $base + 20)
            $maxEnd = [Math]::Max($maxEnd, $rptr + $rsize)
        }
        if ($magic -eq 0x20b) {
            $dd = $opt + 112
            $certOff = [BitConverter]::ToUInt32($bytes, $dd + 32)
            $certSz = [BitConverter]::ToUInt32($bytes, $dd + 36)
            if ($certOff -gt 0) { $maxEnd = [Math]::Max($maxEnd, $certOff + $certSz) }
        }
        if ($maxEnd -lt 4096) { continue }
        $best = @{ Off = $i; Size = $maxEnd; Machine = [BitConverter]::ToUInt16($bytes, $i + $e + 4) }
        break
    } catch { continue }
}
if (-not $best) { throw 'No embedded kernel PE in ThrottleStop.exe' }
$slice = New-Object byte[] $best.Size
[Array]::Copy($bytes, $best.Off, $slice, 0, $best.Size)
$out = Join-Path $OutDir 'ThrottleStop.sys'
[IO.File]::WriteAllBytes($out, $slice)
Copy-Item $out (Join-Path (Split-Path $OutDir -Parent) 'bin\ThrottleStop.sys') -Force -ErrorAction SilentlyContinue
Write-Host "[+] $($best.Size) bytes from offset $($best.Off) -> $out"
