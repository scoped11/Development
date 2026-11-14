#Requires -RunAsAdministrator
# Deploy captured paid-AC dump to guest 1:1 paths (L2 lab replica). VM only.
param(
    [ValidateSet('rust', 'r6', 'deltaforce', 'arenabreakout', 'all')]
    [string]$Profile = 'rust',
    [switch]$LoadKernel,
    [switch]$StartUsermode,
    [switch]$StopExisting
)

$ErrorActionPreference = 'Stop'
$LabRoot = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$RepoRoot = Resolve-Path (Join-Path $LabRoot '..')
$Layouts = Get-Content (Join-Path $PSScriptRoot 'replica-layouts.json') -Raw | ConvertFrom-Json
$Captures = Join-Path $RepoRoot 'ac-captures-host\config\ac_host_captures.json'
if ($Captures) { $hostMeta = Get-Content $Captures -Raw | ConvertFrom-Json }

function Find-DumpDir([string]$Glob) {
    $roots = @(
        'Z:\driver-lab\dumps',
        'D:\hostdev\driver-lab\dumps',
        (Join-Path $LabRoot 'dumps')
    ) | Where-Object { Test-Path $_ }
    foreach ($r in $roots) {
        $d = Get-ChildItem $r -Directory -Filter $Glob -ErrorAction SilentlyContinue | Sort-Object Name -Descending | Select-Object -First 1
        if ($d) { return $d.FullName }
    }
    throw "No dump dir matching $Glob under driver-lab/dumps"
}

function Get-Sha256([string]$Path) {
  if (-not (Test-Path $Path)) { return $null }
  return (Get-FileHash -Algorithm SHA256 -Path $Path).Hash.ToUpper()
}

function Stop-LabReplica([string]$P) {
  Get-Process -ErrorAction SilentlyContinue |
    Where-Object { $_.ProcessName -match 'BEService|EasyAntiCheat|SGuard|ACE-Service|BattlEye' } |
    ForEach-Object { Stop-Process -Id $_.Id -Force -ErrorAction SilentlyContinue }
  foreach ($svc in @('EasyAntiCheat_EOS', 'EasyAntiCheat_EOSLab', 'ACE-GAME', 'ACE-BASE', 'ACE-ADVT', 'ACE-CORE', 'BEDaisy', 'EasyAntiCheat')) {
    sc.exe stop $svc 2>$null | Out-Null
  }
}

function Wait-ServiceGone([string]$Name, [int]$Tries = 20) {
  sc.exe stop $Name 2>$null | Out-Null
  sc.exe delete $Name 2>$null | Out-Null
  for ($i = 0; $i -lt $Tries; $i++) {
    sc.exe query $Name 2>$null | Out-Null
    if ($LASTEXITCODE -ne 0) { return $true }
    Start-Sleep -Milliseconds 300
    sc.exe delete $Name 2>$null | Out-Null
  }
  return $false
}

function Install-Target($dumpRoot, $t) {
  $src = Join-Path $dumpRoot $t.from
  if (-not (Test-Path $src) -and $t.optional) { return @{ ok = $true; skipped = $true; from = $t.from } }
  if (-not (Test-Path $src)) { throw "Missing capture: $src" }
  $dest = $t.to
  if ($t.recurse) {
    New-Item -ItemType Directory -Force -Path $dest | Out-Null
    Copy-Item -Path (Join-Path $src '*') -Destination $dest -Recurse -Force
  } else {
    New-Item -ItemType Directory -Force -Path (Split-Path $dest -Parent) | Out-Null
    if ((Test-Path $dest) -and $t.sha256) {
      $have = Get-Sha256 $dest
      if ($have -eq $t.sha256.ToUpper()) { } else {
        try { Copy-Item -Path $src -Destination $dest -Force }
        catch {
          $alt = Join-Path 'C:\DriverLab\replica' ([IO.Path]::GetFileName($dest))
          New-Item -ItemType Directory -Force -Path (Split-Path $alt) | Out-Null
          Copy-Item -Path $src -Destination $alt -Force
          $dest = $alt
        }
      }
    } else {
      try { Copy-Item -Path $src -Destination $dest -Force }
      catch {
        $alt = Join-Path 'C:\DriverLab\replica' ([IO.Path]::GetFileName($dest))
        New-Item -ItemType Directory -Force -Path (Split-Path $alt) | Out-Null
        Copy-Item -Path $src -Destination $alt -Force
        $dest = $alt
      }
    }
  }
  $h = $null
  if (-not $t.recurse -and (Test-Path $dest -PathType Leaf)) {
    $h = Get-Sha256 $dest
    if ($t.sha256 -and $h -ne $t.sha256.ToUpper()) {
      throw "Hash mismatch $dest expected $($t.sha256) got $h"
    }
  }
  if ($t.kernel_service) {
    $drv = Join-Path $env:SystemRoot ("System32\drivers\" + [IO.Path]::GetFileName($dest))
    $destFull = [IO.Path]::GetFullPath($dest)
    $drvFull = [IO.Path]::GetFullPath($drv)
    if ($destFull -ne $drvFull) {
      try { Copy-Item $dest $drv -Force -ErrorAction Stop }
      catch {
        Write-Host "[!] sys32 copy skipped ($($_.Exception.Message))"
      }
    }
  }
  @{ ok = $true; dest = $dest; sha256 = $h; kernel_service = $t.kernel_service }
}

function Start-KernelService([string]$Name, [string]$ImagePath) {
  if (-not $ImagePath) { return @{ service = $Name; skipped = $true } }
  $onDisk = $ImagePath
  if (-not (Test-Path -LiteralPath $onDisk)) {
    $onDisk = Join-Path $env:SystemRoot ("System32\drivers\" + [IO.Path]::GetFileName($ImagePath))
  }
  if (-not (Test-Path -LiteralPath $onDisk)) { return @{ service = $Name; missing = $onDisk } }
  $sys32 = Join-Path $env:SystemRoot ("System32\drivers\" + [IO.Path]::GetFileName($onDisk))
  if (([IO.Path]::GetFullPath($onDisk) -ne [IO.Path]::GetFullPath($sys32)) -and (Test-Path -LiteralPath $onDisk)) {
    try { Copy-Item $onDisk $sys32 -Force -ErrorAction Stop } catch { }
    if (Test-Path -LiteralPath $sys32) { $onDisk = $sys32 }
  }
  $bin = '\??\' + $onDisk
  Write-Host "[*] sc create $Name type= kernel binPath= $bin"
  $gone = Wait-ServiceGone $Name
  if (-not $gone) {
    Write-Host "[!] $Name still present after delete (WIN32 clobber?)"
  }
  $created = sc.exe create $Name type= kernel start= demand binPath= $bin 2>&1 | Out-String
  Write-Host $created.Trim()
  if ($LASTEXITCODE -ne 0) { return @{ service = $Name; sc_create_ec = $LASTEXITCODE; detail = $created.Trim() } }
  $qc = sc.exe qc $Name 2>&1 | Out-String
  Write-Host $qc.Trim()
  if ($qc -notmatch 'KERNEL_DRIVER') {
    return @{ service = $Name; sc_create_ec = -1; detail = 'not KERNEL_DRIVER after create'; qc = $qc.Trim() }
  }
  $startOut = sc.exe start $Name 2>&1 | Out-String
  Write-Host $startOut.Trim()
  @{ service = $Name; sc_start_ec = $LASTEXITCODE; start_out = $startOut.Trim() }
}

$profiles = if ($Profile -eq 'all') { @('rust', 'r6', 'deltaforce', 'arenabreakout') } else { @($Profile) }
if ($StopExisting) { Stop-LabReplica $Profile }

$report = [ordered]@{
  schema   = 'ac_replica_l2_install_v1'
  utc      = (Get-Date).ToUniversalTime().ToString('o')
  level    = 'L2'
  hostname = $env:COMPUTERNAME
  profiles = @{}
}

foreach ($p in $profiles) {
  $lay = $Layouts.profiles.$p
  if (-not $lay) { throw "No layout for $p" }
  $dump = Find-DumpDir $lay.dump_glob
  Write-Host "[+] $p dump=$dump"
  $items = @()
  foreach ($t in $lay.targets) {
    $items += Install-Target $dump $t
  }
  $kernels = @()
  if ($LoadKernel) {
    foreach ($i in $items | Where-Object { $_.kernel_service }) {
      $kernels += Start-KernelService $i.kernel_service $i.dest
    }
  }
  $um = $null
  if ($StartUsermode -and $lay.usermode_start -and (Test-Path $lay.usermode_start)) {
    Start-Process -FilePath $lay.usermode_start -WindowStyle Hidden -ErrorAction SilentlyContinue
    Start-Sleep -Seconds 2
    $um = Get-Process -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $lay.usermode_start } | Select-Object -First 1
  }
  if ($StartUsermode -and $lay.usermode_patterns) {
    Get-ChildItem 'C:\Program Files\AntiCheatExpert' -Recurse -Filter 'SGuardSvc64.exe' -EA SilentlyContinue |
      Select-Object -First 1 | ForEach-Object { Start-Process $_.FullName -ErrorAction SilentlyContinue }
  }
  if ($StartUsermode -and $p -eq 'rust' -and -not $LoadKernel) {
    $setup = 'C:\Program Files (x86)\EasyAntiCheat_EOS\EasyAntiCheat\EasyAntiCheat_EOS_Setup.exe'
    $meta = Join-Path $dump 'manifest.json'
    $deploy = '429c2212-ad284866-aee071454c2125b5'
    if (Test-Path $meta) {
      try { $deploy = (Get-Content $meta -Raw | ConvertFrom-Json).settings_json.productid } catch { }
    }
    if (Test-Path $setup) {
      Write-Host "[*] EAC EOS setup deploy=$deploy"
      Start-Process -FilePath $setup -ArgumentList 'install', $deploy -Wait -NoNewWindow -ErrorAction SilentlyContinue
    }
  } elseif ($LoadKernel -and $p -eq 'rust') {
    Write-Host "[*] skip EAC EOS setup (would replace kernel EasyAntiCheat_EOS with WIN32 .exe)"
  }
  if ($StartUsermode -and $p -eq 'r6') {
    $beIni = 'C:\Program Files (x86)\Common Files\BattlEye\BELauncher.ini'
    if (-not (Test-Path $beIni)) {
      $srcIni = Join-Path $dump 'battleye\game\BELauncher.ini'
      if (Test-Path $srcIni) { Copy-Item $srcIni $beIni -Force }
    }
  }
  $report.profiles[$p] = [ordered]@{
    dump_dir = $dump
    files    = $items
    kernel   = $kernels
    usermode = $um.ProcessName
    mapper   = "--ac-replica $p"
  }
}

$outVm = 'Z:\driver-lab\vm\bg\ac-replica-l2.json'
if (-not (Test-Path (Split-Path $outVm))) {
  $outVm = Join-Path (Join-Path $LabRoot 'vm\bg') 'ac-replica-l2.json'
}
($report | ConvertTo-Json -Depth 8) | Set-Content $outVm -Encoding utf8
Write-Host "[+] Wrote $outVm"