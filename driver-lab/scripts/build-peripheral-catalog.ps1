# Classify retail peripheral list for kernel .sys hunt (not AC approval).
param(
    [string]$List = (Join-Path $PSScriptRoot '..\data\peripheral_list_105.txt'),
    [string]$Out = (Join-Path $PSScriptRoot '..\data\peripheral_retail_catalog_105.csv')
)

function Get-HuntMeta([string]$n) {
    if ($n -match '^Kingston') {
        return @{ HuntTier = 'skip'; SoftwareStack = 'USB mass storage (USBSTOR)'; LabAction = 'No vendor .sys' }
    }
    if ($n -match '^Elgato') {
        return @{
            HuntTier      = 'vm_install'
            SoftwareStack = 'Elgato 4K Capture / Cam Link app'
            LabAction     = 'guest-install-elgato-dump.ps1 then IDA any new .sys'
        }
    }
    if ($n -match 'POP Mouse|Lift mouse|Mobi Fold|ERGO M575|MX Ergo|Pebble Mouse|Pebble Keys|Signature M650|POP Keys|POP Icon|Brio 500|Brio 4K|MX Brio|StreamCam|MX Master|MX Keys|MX Mechanical|Craft|Alto Keys|K950|K980|Keys-To-Go|ERGO K860|Wave Keys|MX Vertical|Anywhere 3S|Master 4|Signature Slim|Signature Comfort|M850|M840') {
        return @{
            HuntTier      = 'ghub'
            SoftwareStack = 'Logitech G HUB or Options+'
            LabAction     = 'VM install offline G HUB; enumerate-local-drivers -CopyToInbox'
        }
    }
    if ($n -match 'H340|H390|H540|H570e|H650e|H820e|960 USB|Zone') {
        return @{ HuntTier = 'usb_audio'; SoftwareStack = 'USB audio class'; LabAction = 'Skip unless G HUB filter present' }
    }
    if ($n -match 'C270|C310|C505|C920|C922|C925|C930|Brio 100|Brio 300') {
        return @{ HuntTier = 'uvc'; SoftwareStack = 'usbvideo.sys'; LabAction = 'No vendor kernel expected' }
    }
    if ($n -match '^Logitech [BKM]') {
        return @{ HuntTier = 'hid'; SoftwareStack = 'USB HID'; LabAction = 'Skip — no custom signed .sys for mapper' }
    }
    return @{ HuntTier = 'review'; SoftwareStack = 'unknown'; LabAction = 'Manual' }
}

$items = Get-Content -LiteralPath $List
$rows = foreach ($name in $items) {
    if ([string]::IsNullOrWhiteSpace($name)) { continue }
    $m = Get-HuntMeta $name.Trim()
    [pscustomobject]@{
        Product         = $name.Trim()
        HuntTier        = $m.HuntTier
        SoftwareStack   = $m.SoftwareStack
        LabAction       = $m.LabAction
        MapperRelevant  = $(if ($m.HuntTier -in @('ghub', 'vm_install')) { 'maybe' } else { 'no' })
    }
}
$rows | Export-Csv -LiteralPath $Out -NoTypeInformation
Write-Host "[+] $($rows.Count) rows -> $Out"
$rows | Group-Object HuntTier | Format-Table Name, Count -AutoSize
