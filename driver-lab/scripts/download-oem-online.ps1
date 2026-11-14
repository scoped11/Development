# Download official peripheral/OEM packages into audit/inbox/oem-raw (lab use only).
param(
    [switch]$SkipExisting
)

$ErrorActionPreference = 'Continue'
$raw = Join-Path $PSScriptRoot '..\audit\inbox\oem-raw'
New-Item -ItemType Directory -Force -Path $raw | Out-Null

# File = on-disk name when Url basename differs
$downloads = @(
  @{ Name = 'PolychromeRGB_v2.0.208.zip'; Url = 'https://download.asrock.com/Utility/RGB/PolychromeRGB(v2.0.208).zip' }
  @{ Name = 'PolychromeRGB_v2.0.190.zip'; Url = 'https://download.asrock.com/Utility/RGB/PolychromeRGB(v2.0.190).zip' }
  @{
    Name = 'MotherboardUtility_v4.1.11.zip'
    Url  = 'https://download.asrock.com/Utility/MotherboardUtility/MotherboardUtility(v4.1.11).zip'
  }
  @{
    Name = 'F-Stream_v2.0.188.zip'
    Url  = 'https://download.asrock.com/utility/F-Stream/F-Stream(v2.0.188).zip'
  }
  @{ Name = 'RGB_Fusion_B26.0923.1.zip'; Url = 'https://download.gigabyte.com/utility/RGBFusion_B26.0923.1.zip' }
  @{
    Name = 'TT_RGBPlus_v2.9.95.zip'
    Url  = 'https://ttrgbplus.thermaltake.com/download/RGBPlus_v2.9.95_installer.zip'
  }
  @{
    Name = 'FanControl.zip'
    Url  = 'https://github.com/Rem0o/FanControl.Releases/releases/latest/download/FanControl.zip'
  }
  @{
    Name = 'MSIAfterburnerSetup.zip'
    Url  = 'https://download-eu2.guru3d.com/afterburner/[Guru3D.com]-MSIAfterburnerSetup466.zip'
    File = 'MSIAfterburnerSetup466.zip'
  }
  @{ Name = 'OpenRGB_1.1_Windows_64.msi'; Url = 'https://codeberg.org/OpenRGB/OpenRGB/releases/download/release_1.1/OpenRGB_1.1_Windows_64_8e16de2.msi' }
  @{ Name = 'PawnIO_setup.exe'; Url = 'https://pawnio.eu/PawnIO_setup.exe' }
  @{ Name = 'ViGEmBus_1.22.0.exe'; Url = 'https://github.com/nefarius/ViGEmBus/releases/download/v1.22.0/ViGEmBus_1.22.0.exe' }
  @{ Name = 'ROCCAT-Swarm.zip'; Url = 'https://downloads.roccat.com/ROCCAT%20Swarm_v19478.zip'; File = 'ROCCAT-Swarm.zip' }
  @{ Name = 'NZXT_CAM_Setup_4.76.6.exe'; Url = 'https://nzxt.com/api/v1/cam/download/latest' }
  @{
    Name = 'ViperRGB_setup.exe'
    Url  = 'https://s3.amazonaws.com/dlcdn.patriotmemory.com/MEM/ViperRGB/viperrgbsetup.exe'
  }
  @{ Name = 'iCUESetup_3.38.88_release.msi'; Url = 'https://downloads.corsair.com/Files/CUE/iCUESetup_3.38.88_release.msi' }
  @{ Name = 'lghub_installer.exe'; Url = 'https://download01.logi.com/web/ftp/pub/techsupport/gaming/lghub_installer.exe' }
  @{ Name = 'RazerSynapse4Installer.exe'; Url = 'https://dl.razerzone.com/drivers/Synapse4/win/RazerSynapse4Installer.exe' }
  # Often 404 / stub — try anyway
  @{ Name = 'SignalRgbSetup.exe'; Url = 'https://releases.signalrgb.com/latest/SignalRgbSetup.exe' }
  @{ Name = 'L-Connect-3-Setup.exe'; Url = 'https://lian-li.com/wp-content/uploads/static/L-Connect-3-Setup.exe' }
  @{ Name = 'WootilitySetupLatest.exe'; Url = 'https://wooting.io/wootility/latest/WootilitySetupLatest.exe' }
  @{
    Name = 'HyperX_NGENUITY_installer.exe'
    Url  = 'https://hyperxgaming.com/attachment/NGENUITY/Kingston%20NGENUITY%20Installer.exe'
  }
)

function Save-Url {
    param($url, $destName)
    $dest = Join-Path $raw $destName
    if ($SkipExisting -and (Test-Path $dest) -and (Get-Item $dest).Length -gt 10000) {
        Write-Host "[=] skip $destName"
        return
    }
    Write-Host "[*] GET $destName"
    try {
        Invoke-WebRequest -Uri $url -OutFile $dest -UseBasicParsing -TimeoutSec 180
        $len = (Get-Item $dest).Length
        if ($len -lt 5000 -and $destName -notmatch 'cpu-z') {
            Write-Warning "[!] suspicious size ($len) for $destName"
        } else {
            Write-Host "[+] $destName ($len bytes)"
        }
    } catch {
        Write-Warning "FAIL $destName : $_"
    }
}

foreach ($d in $downloads) {
    $name = if ($d.File) { $d.File } else { $d.Name }
    Save-Url $d.Url $name
}

Write-Host "[+] raw dir: $raw"
