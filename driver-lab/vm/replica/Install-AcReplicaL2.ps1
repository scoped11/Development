#Requires -RunAsAdministrator
# Deploy captured paid-AC dump to guest 1:1 paths (L2 lab replica). VM only.
param(
    [ValidateSet('rust', 'r6', 'deltaforce', 'arenabreakout', 'all')]
    [string]$Profile = 'rust',
    [switch]$LoadKernel,
    [switch]$StartUsermode,
    [switch]$StopExisting,
  [switch]$StrictKernel
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
    Where-Object { $_.ProcessName -match 'BEService|EasyAntiCheat|SGuard|ACE-Service' } |
    ForEach-Object { Stop-Process -Id $_.Id -Force -ErrorAction SilentlyContinue }
  foreach ($svc in @('EasyAntiCheat_EOS', 'ACE-GAME', 'ACE-BASE', 'BEDaisy', 'EasyAntiCheat')) {
    sc.exe stop $svc 2>$null | Out-Null
    sc.exe delete $svc 2>$null | Out-Null
  }
}

function Remove-ServiceIfPresent([string]$Name) {
  sc.exe query $Name 2>$null | Out-Null
  if ($LASTEXITCODE -ne 0) { return }
  sc.exe stop $Name 2>$null | Out-Null
  Start-Sleep -Milliseconds 500
  sc.exe delete $Name 2>$null | Out-Null
  Start-Sleep -Milliseconds 500
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
  $sigStatus = $null
  $sigSubject = $null
  if (-not $t.recurse -and (Test-Path $dest -PathType Leaf)) {
    $h = Get-Sha256 $dest
    if ($t.sha256 -and $h -ne $t.sha256.ToUpper()) {
      throw "Hash mismatch $dest expected $($t.sha256) got $h"
    }
    $sig = Get-AuthenticodeSignature -FilePath $dest
    $sigStatus = $sig.Status.ToString()
    if ($sig.SignerCertificate) { $sigSubject = $sig.SignerCertificate.Subject }
  }
  $kernelDrv = $null
  if ($t.kernel_service) {
    $kernelDrv = Join-Path $env:SystemRoot "System32\drivers\$([IO.Path]::GetFileName($dest))"
    $srcLeaf = if (Test-Path $dest -PathType Leaf) { $dest } else { $null }
    if (-not $srcLeaf) {
      $srcLeaf = Join-Path $dumpRoot $t.from
    }
    if ((Test-Path $srcLeaf -PathType Leaf) -and $kernelDrv -ne $srcLeaf) {
      Copy-Item $srcLeaf $kernelDrv -Force
    } elseif ($dest -ne $kernelDrv -and (Test-Path $dest -PathType Leaf)) {
      Copy-Item $dest $kernelDrv -Force
    }
  }
  @{
    ok               = $true
    dest             = $dest
    sha256           = $h
    authenticode     = $sigStatus
    signer           = $sigSubject
    kernel_service   = $t.kernel_service
    kernel_load_path = $kernelDrv
  }
}

function Start-KernelService([string]$Name, [string]$ImagePath, [string]$KernelLoadPath) {
  if (-not $ImagePath) { return @{ service = $Name; skipped = $true } }
  $leaf = [IO.Path]::GetFileName($ImagePath)
  $onDisk = if ($KernelLoadPath) { $KernelLoadPath } else {
    Join-Path $env:SystemRoot "System32\drivers\$leaf"
  }
  if (-not (Test-Path $onDisk) -and (Test-Path $ImagePath)) {
    Copy-Item $ImagePath $onDisk -Force
  }
  if (-not (Test-Path $onDisk)) {
    return @{ service = $Name; missing = $onDisk; sc_start_ec = 2 }
  }
  # SCM expects SystemRoot-relative path — not Program Files or \??\ full paths.
  $bin = "\SystemRoot\System32\drivers\$leaf"
  Write-Host "[*] sc create $Name type=kernel binPath=$bin onDisk=$onDisk"
  Remove-ServiceIfPresent $Name
  Write-Host "[*] sc.exe create $Name ... binPath=$bin"
  $null = sc.exe create $Name type= kernel start= demand "binPath=$bin" 2>&1 | ForEach-Object { Write-Host $_ }
  if ($LASTEXITCODE -ne 0) {
    return @{ service = $Name; sc_create_ec = $LASTEXITCODE; bin_path = $bin; on_disk = $onDisk }
  }
  $startOut = sc.exe start $Name 2>&1 | Out-String
  Write-Host $startOut
  $startEc = $LASTEXITCODE
  $running = $false
  sc.exe query $Name 2>$null | Out-String | ForEach-Object {
    if ($_ -match 'STATE\s*:\s*\d+\s+RUNNING') { $running = $true }
  }
  @{
    service      = $Name
    sc_start_ec  = $startEc
    bin_path     = $bin
    on_disk      = $onDisk
    kernel_running = $running
  }
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
    foreach ($i in $items | Where-Object { $_.kernel_service -and -not $_.skipped }) {
      $kernels += Start-KernelService $i.kernel_service $i.dest $i.kernel_load_path
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
  if ($StartUsermode -and $p -eq 'rust') {
    $setup = 'C:\Program Files (x86)\EasyAntiCheat_EOS\EasyAntiCheat\EasyAntiCheat_EOS_Setup.exe'
    $meta = Join-Path $dump 'manifest.json'
    $deploy = '429c2212-ad284866-aee071454c2125b5'
    if (Test-Path $meta) {
      try { $deploy = (Get-Content $meta -Raw | ConvertFrom-Json).settings_json.productid } catch { }
    }
    if (Test-Path $setup) {
      Write-Host "[*] EAC EOS setup deploy=$deploy (after kernel sc — setup must not replace kernel service)"
      Start-Process -FilePath $setup -ArgumentList 'install', $deploy -Wait -NoNewWindow -ErrorAction SilentlyContinue
      if ($LoadKernel) {
        foreach ($row in $kernels | Where-Object { $_.kernel_running -eq $true }) {
          sc.exe query $row.service 2>$null | Out-String | ForEach-Object {
            if ($_ -match 'WIN32_OWN_PROCESS') {
              Write-Warning "$($row.service) replaced by EOS setup — re-starting kernel service"
              Remove-ServiceIfPresent $row.service
              $leaf = [IO.Path]::GetFileName($row.on_disk)
              $kernels = @($kernels | Where-Object { $_.service -ne $row.service })
              $kernels += Start-KernelService $row.service (Join-Path $env:SystemRoot "System32\drivers\$leaf") $row.on_disk
            }
          }
        }
      }
    }
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

if ($LoadKernel) {
  $bad = @()
  foreach ($p in $profiles) {
    $k = $report.profiles[$p].kernel
    if (-not $k) { continue }
    foreach ($row in $k) {
      if ($row.sc_start_ec -and $row.sc_start_ec -ne 0) { $bad += $row }
      elseif ($row.missing) { $bad += $row }
      elseif ($row.kernel_running -eq $false -and $row.sc_start_ec -eq 0) { $bad += $row }
    }
  }
  if ($bad.Count -gt 0) {
    Write-Warning "Kernel replica start incomplete: $($bad.Count) service(s)"
    if ($StrictKernel) { exit 10 }
  }
}
exit 0