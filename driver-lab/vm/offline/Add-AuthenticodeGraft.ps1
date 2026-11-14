#Requires -Version 5.1
<#
.SYNOPSIS
    Graft an Authenticode certificate table (WIN_CERTIFICATE / PKCS#7 SignedData)
    from one PE file onto another.
.DESCRIPTION
    Copies the source's security directory (all WIN_CERTIFICATE entries + padding)
    onto the target: strips the target's existing table (if any), appends the
    source table at an 8-byte aligned file offset, rewrites the security-directory
    data-directory entry, and recomputes the PE checksum.

    The embedded messageDigest still covers the SOURCE file's bytes. The digest
    skips exactly three regions — OptionalHeader.CheckSum, the security-directory
    data-directory entry, and everything from the certificate-table offset to EOF —
    which are precisely the regions this script touches. Unless the target's digest
    region is byte-identical to the source's, verification yields HashMismatch.
.EXAMPLE
    .\Add-AuthenticodeGraft.ps1 -SourceFile C:\Lab\EasyAntiCheat_EOS.sys -TargetFile C:\Lab\VmLabDrv.sys
.EXAMPLE
    .\Add-AuthenticodeGraft.ps1 -SourceFile C:\Windows\System32\drivers\wcifs.sys -TargetFile C:\Lab\wcifs.copy.sys
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory, Position = 0)] [string]$SourceFile,
    [Parameter(Mandatory, Position = 1)] [string]$TargetFile,
    [Parameter(Position = 2)] [string]$OutputFile,
    [switch]$InPlace,
    [switch]$NoVerify,
    [switch]$QuietLog
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if (-not ('AuthenticodeGraft.PeChecksum' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
namespace AuthenticodeGraft
{
    public static class PeChecksum
    {
        // PE/COFF header checksum: sum of all WORDs, skipping the 4-byte
        // CheckSum field, plus the file length, folded to 16 bits.
        public static ushort Compute(byte[] data, int checkSumFieldOffset)
        {
            ulong sum = 0;
            int i = 0;
            while (i + 1 < data.Length)
            {
                if (i == checkSumFieldOffset) { i += 4; continue; }
                sum += (ushort)(data[i] | (data[i + 1] * 256));
                i += 2;
            }
            if ((data.Length & 1) == 1) { sum += (ulong)data[data.Length - 1]; }
            while ((sum >> 16) != 0) { sum = (sum & 0xFFFF) + (sum >> 16); }
            sum += (ulong)data.Length;
            while ((sum >> 16) != 0) { sum = (sum & 0xFFFF) + (sum >> 16); }
            return (ushort)sum;
        }
    }
}
'@
}

function Get-PeLayout {
    param([byte[]]$Bytes, [string]$Path)

    if ($Bytes.Length -lt 0x100)                      { throw "File too small to be a PE: $Path" }
    if ($Bytes[0] -ne 0x4D -or $Bytes[1] -ne 0x5A)    { throw "Not an MZ image: $Path" }

    $e_lfanew = [BitConverter]::ToInt32($Bytes, 0x3C)
    if ($e_lfanew -le 0 -or ($e_lfanew + 264) -gt $Bytes.Length) { throw "Bad e_lfanew: $Path" }
    if ([BitConverter]::ToUInt32($Bytes, $e_lfanew) -ne 0x00004550) { throw "No PE signature: $Path" }

    $optOff = $e_lfanew + 24                          # PE sig (4) + COFF header (20)
    $magic  = [BitConverter]::ToUInt16($Bytes, $optOff)
    switch ($magic) {
        0x10B { $ddBase = 96;  $nDirsOff =  92 }      # PE32
        0x20B { $ddBase = 112; $nDirsOff = 108 }      # PE32+
        default { throw ("Unknown optional-header magic 0x{0:X4}: {1}" -f $magic, $Path) }
    }
    $nDirs = [BitConverter]::ToUInt32($Bytes, $optOff + $nDirsOff)
    if ($nDirs -lt 5) { throw "No security-directory slot (NumberOfRvaAndSizes = $nDirs): $Path" }

    $secDirOffset = $optOff + $ddBase + 32            # data directory index 4

    [pscustomobject]@{
        Path                  = $Path
        Magic                 = $magic
        CheckSumFieldOffset   = $optOff + 64          # same offset in PE32 and PE32+
        SecurityDirOffset     = $secDirOffset
        # NOTE: for the security directory, VirtualAddress is a FILE OFFSET, not an RVA.
        SecurityDirFileOffset = [BitConverter]::ToUInt32($Bytes, $secDirOffset)
        SecurityDirSize       = [BitConverter]::ToUInt32($Bytes, $secDirOffset + 4)
        StoredCheckSum        = [BitConverter]::ToUInt32($Bytes, $optOff + 64)
    }
}

function Get-WinCertificateEntries {
    param([byte[]]$Bytes, [int64]$TableOffset, [int64]$TableSize)

    $entries = New-Object System.Collections.Generic.List[object]
    $off = [int64]$TableOffset
    $end = [int64]$TableOffset + [int64]$TableSize
    while ($off + 8 -le $end) {
        $dwLen = [int64][BitConverter]::ToUInt32($Bytes, $off)
        if ($dwLen -lt 8 -or $off + $dwLen -gt $end) { throw "Bad WIN_CERTIFICATE dwLength at table offset $($off - $TableOffset)" }
        [void]$entries.Add([pscustomobject]@{
            Offset   = $off - $TableOffset
            Length   = $dwLen
            Revision = [BitConverter]::ToUInt16($Bytes, $off + 4)
            Type     = [BitConverter]::ToUInt16($Bytes, $off + 6)
        })
        $off += ($dwLen + 7) -band (-bnot 7)           # 8-byte aligned advance
    }
    if ($off -lt $end) { Write-Warning "Certificate table has a nonzero tail (<8 bytes) after last entry; ignored." }
    return ,$entries
}

function Get-AuthenticodeDigestRegion {
    # Byte region the Authenticode digest is computed over:
    # file minus [cert table offset .. EOF], with CheckSum field and the
    # security-directory data-directory entry zeroed.
    param([byte[]]$Bytes, [pscustomobject]$L)

    $len = if ($L.SecurityDirSize -gt 0 -and $L.SecurityDirFileOffset -gt 0) { [int64]$L.SecurityDirFileOffset } else { [int64]$Bytes.Length }
    $region = [byte[]]::new([int]$len)
    [Array]::Copy($Bytes, [long]0, $region, [long]0, $len)
    for ($i = 0; $i -lt 4; $i++) { $region[$L.CheckSumFieldOffset + $i] = 0 }
    for ($i = 0; $i -lt 8; $i++) { $region[$L.SecurityDirOffset + $i] = 0 }
    return [byte[]]$region
}

# ---------------------------------------------------------------- main ------
$sourcePath = (Resolve-Path -LiteralPath $SourceFile).Path
$targetPath = (Resolve-Path -LiteralPath $TargetFile).Path
if ($sourcePath -eq $targetPath) { throw "Source and target are the same file; copy the target first if this is an identity test." }
function GLog([string]$m) { if (-not $QuietLog) { Write-Host $m } }
GLog ("[graft] source: {0}" -f $sourcePath)
GLog ("[graft] target: {0}" -f $targetPath)

# --- source: extract the certificate table ------------------------------------
$srcBytes = [byte[]][IO.File]::ReadAllBytes($sourcePath)
$src = Get-PeLayout -Bytes $srcBytes -Path $sourcePath

if ($src.SecurityDirFileOffset -eq 0 -or $src.SecurityDirSize -eq 0) {
    throw "Source has no embedded Authenticode signature (no certificate table): $sourcePath"
}
if ([int64]$src.SecurityDirFileOffset + [int64]$src.SecurityDirSize -gt [int64]$srcBytes.Length) {
    throw "Source certificate table out of bounds: $sourcePath"
}

$srcEntries = Get-WinCertificateEntries -Bytes $srcBytes -TableOffset $src.SecurityDirFileOffset -TableSize $src.SecurityDirSize
for ($i = 0; $i -lt $srcEntries.Count; $i++) {
    if ($srcEntries[$i].Type -ne 0x0002) {
        Write-Warning ("Source entry #{0} is not WIN_CERT_TYPE_PKCS_SIGNED_DATA (Type=0x{1:X4}); copying anyway." -f $i, $srcEntries[$i].Type)
    }
}

# Checksummer self-test against the source's real, signed checksum.
$srcCheckComputed = [AuthenticodeGraft.PeChecksum]::Compute($srcBytes, $src.CheckSumFieldOffset)
$srcCheckSelfTestOk = ($srcCheckComputed -eq ([uint16]($src.StoredCheckSum -band 0xFFFF)))
if (-not $srcCheckSelfTestOk) {
    Write-Warning ("PE-checksum self-test mismatch on source (stored 0x{0:X8}, computed 0x{1:X4}); header checksummer may disagree with the file's signer toolchain." -f $src.StoredCheckSum, $srcCheckComputed)
}

# --- target: strip existing table, append source table -----------------------
$tgtBytes = [byte[]][IO.File]::ReadAllBytes($targetPath)
$tgt = Get-PeLayout -Bytes $tgtBytes -Path $targetPath

$baseLen = [int64]$tgtBytes.Length
if ($tgt.SecurityDirSize -gt 0 -and $tgt.SecurityDirFileOffset -gt 0) {
    if ([int64]$tgt.SecurityDirFileOffset + [int64]$tgt.SecurityDirSize -gt [int64]$tgtBytes.Length) {
        throw "Target certificate table out of bounds: $targetPath"
    }
    if (($tgt.SecurityDirFileOffset + $tgt.SecurityDirSize) -ne $tgtBytes.Length) {
        Write-Warning "Target had data after its certificate table; dropping it."
    }
    GLog ("[graft] stripping target's existing table (offset 0x{0:X}, 0x{1:X} bytes)" -f $tgt.SecurityDirFileOffset, $tgt.SecurityDirSize)
    $baseLen = [int64]$tgt.SecurityDirFileOffset
}

$pad    = [int]((8 - ($baseLen % 8)) % 8)
$newOff = $baseLen + $pad
$outLen = $newOff + [int64]$src.SecurityDirSize
$out    = [byte[]]::new([int]$outLen)

[Array]::Copy($tgtBytes, [long]0, $out, [long]0, $baseLen)                                      # pad bytes stay zero
[Array]::Copy($srcBytes, [long]$src.SecurityDirFileOffset, $out, [long]$newOff, [long]$src.SecurityDirSize)

# Will the graft actually verify? Compare digest regions of source vs target.
$digestIdentical = [bool][System.Linq.Enumerable]::SequenceEqual(
    [byte[]](Get-AuthenticodeDigestRegion -Bytes $srcBytes -L $src),
    [byte[]](Get-AuthenticodeDigestRegion -Bytes $tgtBytes -L $tgt))
$predictedStatus = if ($digestIdentical) { 'Valid' } else { 'HashMismatch' }

# Point the security directory at the grafted table, rewrite the checksum.
[BitConverter]::GetBytes([uint32]$newOff).CopyTo($out, $tgt.SecurityDirOffset)
[BitConverter]::GetBytes([uint32]$src.SecurityDirSize).CopyTo($out, $tgt.SecurityDirOffset + 4)
$newCheck = [AuthenticodeGraft.PeChecksum]::Compute($out, $tgt.CheckSumFieldOffset)
[BitConverter]::GetBytes([uint32]$newCheck).CopyTo($out, $tgt.CheckSumFieldOffset)

# --- write -------------------------------------------------------------------
if ($InPlace) {
    if ($OutputFile) { throw "Use either -InPlace or -OutputFile, not both." }
    $outPath = $targetPath
} elseif ($OutputFile) {
    $outPath = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputFile)
} else {
    $outPath = Join-Path (Split-Path -Parent $targetPath) (
        "{0}.grafted{1}" -f [IO.Path]::GetFileNameWithoutExtension($targetPath), [IO.Path]::GetExtension($targetPath))
}

if ($outPath -eq $sourcePath) { throw "Refusing to overwrite the source file." }
if ($outPath -eq $targetPath) {
    $tmp = Join-Path (Split-Path -Parent $outPath) ("." + [IO.Path]::GetFileName($outPath) + ".tmp")
    [IO.File]::WriteAllBytes($tmp, $out)
    Move-Item -LiteralPath $tmp -Destination $outPath -Force
} else {
    [IO.File]::WriteAllBytes($outPath, $out)
}

# --- verify -------------------------------------------------------------------
$signatureStatus = 'Unknown'; $signer = $null
if (-not $NoVerify) {
    $sig = Get-AuthenticodeSignature -LiteralPath $outPath
    $signatureStatus = [string]$sig.Status
    if ($sig.SignerCertificate) { $signer = $sig.SignerCertificate.Subject }
}

GLog ("[graft] wrote {0}" -f $outPath)
GLog ("[graft] entries grafted: {0}  table: offset 0x{1:X} size 0x{2:X} (pad {3} bytes)" -f $srcEntries.Count, $newOff, $src.SecurityDirSize, $pad)
GLog ("[graft] header checksum: 0x{0:X8} -> 0x{1:X8} (self-test on source: {2})" -f $tgt.StoredCheckSum, $newCheck, $(if ($srcCheckSelfTestOk) {'OK'} else {'MISMATCH'}))
GLog ("[graft] digest region identical to source: {0} -> expected verify: {1}" -f $digestIdentical, $predictedStatus)
GLog ("[graft] Get-AuthenticodeSignature: {0}  Signer: {1}" -f $signatureStatus, $signer)

[pscustomobject]@{
    SourceFile             = $sourcePath
    TargetFile             = $targetPath
    OutputFile             = $outPath
    GraftedEntryCount      = $srcEntries.Count
    GraftedEntries         = [object[]]$srcEntries
    NewTableFileOffset     = $newOff
    NewTableSizeBytes      = [int64]$src.SecurityDirSize
    PaddingBytes           = $pad
    NewHeaderChecksum      = '0x{0:X8}' -f $newCheck
    ChecksumSelfTestOnSource = $srcCheckSelfTestOk
    DigestRegionIdentical  = $digestIdentical
    PredictedVerifyStatus  = $predictedStatus
    ActualVerifyStatus     = $signatureStatus
    Signer                 = $signer
}