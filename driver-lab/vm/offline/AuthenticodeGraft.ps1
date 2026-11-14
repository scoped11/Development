#Requires -Version 5.1
# Library: PE Authenticode cert-table graft. Dot-source; call Invoke-AuthenticodeGraft.
# No Add-Type / csc (guest worker hangs on JIT). ImageHlp-style 32-bit checksum.

Set-StrictMode -Version Latest

function Get-PeChecksum32 {
    # IMAGE_OPTIONAL_HEADER.CheckSum: 16-bit one's-complement word sum
    # (CheckSum field skipped), fold to 16 bits, then ADD file length.
    # Do not fold after the length add — the high word is the size contribution
    # (Microsoft-signed drivers store 0x0004xxxx, not a 16-bit remainder).
    param([byte[]]$Data, [int]$CheckSumFieldOffset)
    [uint64]$sum = 0
    $i = 0
    $len = $Data.Length
    while ($i + 1 -lt $len) {
        if ($i -eq $CheckSumFieldOffset) { $i += 4; continue }
        $sum += [uint64]$Data[$i] + ([uint64]$Data[$i + 1] * 256)
        $i += 2
    }
    if (($len -band 1) -eq 1) { $sum += [uint64]$Data[$len - 1] }
    while (($sum -shr 16) -ne 0) { $sum = ($sum -band 0xFFFF) + ($sum -shr 16) }
    $sum += [uint64]$len
    return [uint32]($sum -band 0xFFFFFFFF)
}

function Get-PeLayout {
    param([byte[]]$Bytes, [string]$Path)

    if ($Bytes.Length -lt 0x100) { throw "File too small to be a PE: $Path" }
    if ($Bytes[0] -ne 0x4D -or $Bytes[1] -ne 0x5A) { throw "Not an MZ image: $Path" }

    $e_lfanew = [BitConverter]::ToInt32($Bytes, 0x3C)
    if ($e_lfanew -le 0 -or ($e_lfanew + 264) -gt $Bytes.Length) { throw "Bad e_lfanew: $Path" }
    if ([BitConverter]::ToUInt32($Bytes, $e_lfanew) -ne 0x00004550) { throw "No PE signature: $Path" }

    $optOff = $e_lfanew + 24
    $magic = [BitConverter]::ToUInt16($Bytes, $optOff)
    switch ($magic) {
        0x10B { $ddBase = 96; $nDirsOff = 92 }
        0x20B { $ddBase = 112; $nDirsOff = 108 }
        default { throw ("Unknown optional-header magic 0x{0:X4}: {1}" -f $magic, $Path) }
    }
    if (($optOff + $nDirsOff + 4) -gt $Bytes.Length) { throw "Optional header truncated: $Path" }
    $nDirs = [BitConverter]::ToUInt32($Bytes, $optOff + $nDirsOff)
    if ($nDirs -lt 5) { throw "No security-directory slot (NumberOfRvaAndSizes = $nDirs): $Path" }

    $secDirOffset = $optOff + $ddBase + 32
    if (($secDirOffset + 8) -gt $Bytes.Length) { throw "Data directories truncated: $Path" }
    $checkSumFieldOffset = $optOff + 64
    if (($checkSumFieldOffset % 2) -ne 0) { throw "CheckSum field at odd offset: $Path" }

    [pscustomobject]@{
        Path                  = $Path
        Magic                 = $magic
        CheckSumFieldOffset   = $checkSumFieldOffset
        SecurityDirOffset     = $secDirOffset
        SecurityDirFileOffset = [BitConverter]::ToUInt32($Bytes, $secDirOffset)
        SecurityDirSize       = [BitConverter]::ToUInt32($Bytes, $secDirOffset + 4)
        StoredCheckSum        = [BitConverter]::ToUInt32($Bytes, $checkSumFieldOffset)
    }
}

function Get-WinCertificateEntries {
    param([byte[]]$Bytes, [int64]$TableOffset, [int64]$TableSize)

    $entries = New-Object System.Collections.Generic.List[object]
    $off = [int64]$TableOffset
    $end = $off + [int64]$TableSize
    while ($off + 8 -le $end) {
        $dwLen = [int64][BitConverter]::ToUInt32($Bytes, [int]$off)
        if ($dwLen -lt 8 -or $off + $dwLen -gt $end) {
            throw "Bad WIN_CERTIFICATE dwLength at table offset $($off - $TableOffset)"
        }
        [void]$entries.Add([pscustomobject]@{
            Offset   = $off - $TableOffset
            Length   = $dwLen
            Revision = [BitConverter]::ToUInt16($Bytes, [int]($off + 4))
            Type     = [BitConverter]::ToUInt16($Bytes, [int]($off + 6))
        })
        $off += ($dwLen + 7) -band (-bnot 7)
    }
    if ($entries.Count -eq 0) { throw "Certificate table walk produced zero entries (size $TableSize)" }
    return ,$entries
}

function Get-AuthenticodeDigestRegion {
    param([byte[]]$Bytes, [int64]$HashedLength, [int]$CheckSumFieldOffset, [int]$SecurityDirOffset)
    $region = [byte[]]::new([int]$HashedLength)
    [Array]::Copy($Bytes, [long]0, $region, [long]0, $HashedLength)
    for ($i = 0; $i -lt 4; $i++) { $region[$CheckSumFieldOffset + $i] = 0 }
    for ($i = 0; $i -lt 8; $i++) { $region[$SecurityDirOffset + $i] = 0 }
    return [byte[]]$region
}

function Test-ByteArraysEqual {
    param([byte[]]$A, [byte[]]$B)
    if ($null -eq $A -or $null -eq $B) { return $false }
    if ($A.Length -ne $B.Length) { return $false }
    for ($i = 0; $i -lt $A.Length; $i++) {
        if ($A[$i] -ne $B[$i]) { return $false }
    }
    return $true
}

function Invoke-AuthenticodeGraft {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string]$SourceFile,
        [Parameter(Mandatory)][string]$TargetFile,
        [string]$OutputFile,
        [switch]$InPlace,
        [switch]$NoVerify,
        [int]$VerifyTimeoutSec = 15,
        [string]$ResultJson
    )

    $sourcePath = (Resolve-Path -LiteralPath $SourceFile).Path
    $targetPath = (Resolve-Path -LiteralPath $TargetFile).Path
    if ($sourcePath -eq $targetPath) { throw "Source and target are the same file." }

    $srcBytes = [byte[]][IO.File]::ReadAllBytes($sourcePath)
    $src = Get-PeLayout -Bytes $srcBytes -Path $sourcePath
    if ($src.SecurityDirFileOffset -eq 0 -or $src.SecurityDirSize -eq 0) {
        throw "Source has no Authenticode certificate table: $sourcePath"
    }
    if ([int64]$src.SecurityDirFileOffset + [int64]$src.SecurityDirSize -gt [int64]$srcBytes.Length) {
        throw "Source certificate table out of bounds: $sourcePath"
    }

    $srcEntries = Get-WinCertificateEntries -Bytes $srcBytes -TableOffset $src.SecurityDirFileOffset -TableSize $src.SecurityDirSize
    $srcCheckComputed = Get-PeChecksum32 -Data $srcBytes -CheckSumFieldOffset $src.CheckSumFieldOffset
    $srcCheckSelfTestOk = ($srcCheckComputed -eq $src.StoredCheckSum)

    $tgtBytes = [byte[]][IO.File]::ReadAllBytes($targetPath)
    $tgt = Get-PeLayout -Bytes $tgtBytes -Path $targetPath
    $baseLen = [int64]$tgtBytes.Length
    if ($tgt.SecurityDirSize -gt 0 -and $tgt.SecurityDirFileOffset -gt 0) {
        if ([int64]$tgt.SecurityDirFileOffset + [int64]$tgt.SecurityDirSize -gt [int64]$tgtBytes.Length) {
            throw "Target certificate table out of bounds: $targetPath"
        }
        $baseLen = [int64]$tgt.SecurityDirFileOffset
    }

    $pad = [int]((8 - ($baseLen % 8)) % 8)
    $newOff = $baseLen + $pad
    $outLen = $newOff + [int64]$src.SecurityDirSize
    if ($outLen -gt 0x70000000) { throw "Resulting image would exceed 2GB safe limit." }
    $out = [byte[]]::new([int]$outLen)
    [Array]::Copy($tgtBytes, [long]0, $out, [long]0, $baseLen)
    [Array]::Copy($srcBytes, [long]$src.SecurityDirFileOffset, $out, [long]$newOff, [long]$src.SecurityDirSize)
    [BitConverter]::GetBytes([uint32]$newOff).CopyTo($out, $tgt.SecurityDirOffset)
    [BitConverter]::GetBytes([uint32]$src.SecurityDirSize).CopyTo($out, $tgt.SecurityDirOffset + 4)
    $newCheck = Get-PeChecksum32 -Data $out -CheckSumFieldOffset $tgt.CheckSumFieldOffset
    [BitConverter]::GetBytes([uint32]$newCheck).CopyTo($out, $tgt.CheckSumFieldOffset)

    $digestIdentical = ([int64]$newOff -eq [int64]$src.SecurityDirFileOffset) -and (Test-ByteArraysEqual `
            (Get-AuthenticodeDigestRegion -Bytes $out -HashedLength $newOff -CheckSumFieldOffset $tgt.CheckSumFieldOffset -SecurityDirOffset $tgt.SecurityDirOffset) `
            (Get-AuthenticodeDigestRegion -Bytes $srcBytes -HashedLength $src.SecurityDirFileOffset -CheckSumFieldOffset $src.CheckSumFieldOffset -SecurityDirOffset $src.SecurityDirOffset))
    $predictedStatus = if ($digestIdentical) { 'Valid' } else { 'HashMismatch' }

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

    function New-GraftResult([string]$status, [string]$signer) {
        [pscustomobject]@{
            SourceFile               = $sourcePath
            TargetFile               = $targetPath
            OutputFile               = $outPath
            GraftedEntryCount        = $srcEntries.Count
            GraftedEntries           = [object[]]$srcEntries
            NewTableFileOffset       = $newOff
            NewTableSizeBytes        = [int64]$src.SecurityDirSize
            PaddingBytes             = $pad
            NewHeaderChecksum        = '0x{0:X8}' -f $newCheck
            ChecksumSelfTestOnSource = $srcCheckSelfTestOk
            StoredCheckSum           = '0x{0:X8}' -f $src.StoredCheckSum
            ComputedCheckSum         = '0x{0:X8}' -f $srcCheckComputed
            DigestRegionIdentical    = $digestIdentical
            PredictedVerifyStatus    = $predictedStatus
            ActualVerifyStatus       = $status
            Signer                   = $signer
        }
    }
    if ($ResultJson) { New-GraftResult 'Pending' $null | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $ResultJson -Encoding UTF8 }

    $signatureStatus = 'NotRun'
    $signer = $null
    if (-not $NoVerify) {
        $vjob = Start-Job -ScriptBlock { param($p) Get-AuthenticodeSignature -LiteralPath $p } -ArgumentList $outPath
        if (Wait-Job $vjob -Timeout $VerifyTimeoutSec) {
            $sig = Receive-Job $vjob
            $signatureStatus = [string]$sig.Status
            if ($sig.SignerCertificate) { $signer = $sig.SignerCertificate.Subject }
        } else {
            Stop-Job $vjob
            $signatureStatus = "NotChecked (verify exceeded ${VerifyTimeoutSec}s)"
        }
        Remove-Job $vjob -Force
    }
    $obj = New-GraftResult $signatureStatus $signer
    if ($ResultJson) { $obj | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $ResultJson -Encoding UTF8 }
    return $obj
}
