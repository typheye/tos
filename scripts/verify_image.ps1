param(
  [Parameter(Mandatory = $true)][string]$ImagePath,
  [Parameter(Mandatory = $true)][string]$PublicKeyPath,
  [Parameter(Mandatory = $true)][UInt32]$ExpectedType,
  [Parameter(Mandatory = $true)][UInt32]$ExpectedAddress,
  [Parameter(Mandatory = $true)][UInt32]$ExpectedSize,
  [UInt32]$MinimumVersion = 1
)

$ErrorActionPreference = "Stop"
$HeaderOffset = 0x200
$HeaderSize = 0x80

function Read-U32LE {
  param([byte[]]$Bytes, [int]$Offset)
  return [UInt32](([UInt32]$Bytes[$Offset]) -bor
      ([UInt32]$Bytes[$Offset + 1] -shl 8) -bor
      ([UInt32]$Bytes[$Offset + 2] -shl 16) -bor
      ([UInt32]$Bytes[$Offset + 3] -shl 24))
}

function Get-Crc32 {
  param([byte[]]$Bytes, [int]$Offset, [int]$Count)
  [UInt32]$crc = [UInt32]::MaxValue
  for ($i = 0; $i -lt $Count; $i++) {
    $crc = $crc -bxor [UInt32]$Bytes[$Offset + $i]
    for ($bit = 0; $bit -lt 8; $bit++) {
      if (($crc -band 1) -ne 0) {
        $crc = [UInt32](($crc -shr 1) -bxor [UInt32]3988292384)
      } else {
        $crc = [UInt32]($crc -shr 1)
      }
    }
  }
  return [UInt32](([UInt64]$crc) -bxor [UInt64]4294967295)
}

$image = [IO.File]::ReadAllBytes($ImagePath)
if ($ExpectedSize -gt $image.Length -or
    $ExpectedSize -lt ($HeaderOffset + $HeaderSize)) {
  throw "Expected signed span is outside image"
}
$header = New-Object byte[] $HeaderSize
[Array]::Copy($image, $HeaderOffset, $header, 0, $HeaderSize)

$magic = Read-U32LE $header 0
$headerVersion = Read-U32LE $header 4
$imageType = Read-U32LE $header 8
$loadAddress = Read-U32LE $header 12
$imageSize = Read-U32LE $header 16
$imageVersion = Read-U32LE $header 20
$flags = Read-U32LE $header 24
$storedCrc = Read-U32LE $header 124
$calculatedCrc = Get-Crc32 $header 0 124
if ($magic -ne 0x31474953 -or $headerVersion -ne 1 -or
    $imageType -ne $ExpectedType -or $loadAddress -ne $ExpectedAddress -or
    $imageSize -ne $ExpectedSize -or $imageVersion -lt $MinimumVersion -or
    $flags -ne 0 -or $storedCrc -ne $calculatedCrc) {
  throw "Secure image header validation failed"
}

$storedDigest = New-Object byte[] 32
$signature = New-Object byte[] 64
[Array]::Copy($header, 28, $storedDigest, 0, 32)
[Array]::Copy($header, 60, $signature, 0, 64)
for ($i = 0; $i -lt $HeaderSize; $i++) {
  $image[$HeaderOffset + $i] = 0xFF
}
$sha = [Security.Cryptography.SHA256]::Create()
$imageDigest = $sha.ComputeHash($image, 0, $ExpectedSize)
[UInt32]$digestDifference = 0
for ($i = 0; $i -lt 32; $i++) {
  $digestDifference = $digestDifference -bor [UInt32]($storedDigest[$i] -bxor $imageDigest[$i])
}
if ($digestDifference -ne 0) {
  throw "Secure image digest mismatch"
}

$domain = [byte[]](
    [char]'T', [char]'O', [char]'S', [char]'-',
    [char]'S', [char]'B', [char]'-', [char]'P',
    [char]'2', [char]'5', [char]'6', [char]'-',
    [char]'V', [char]'1', 0, 0)
$signatureInput = New-Object byte[] 76
[Array]::Copy($domain, 0, $signatureInput, 0, 16)
[Array]::Copy($header, 0, $signatureInput, 16, 28)
[Array]::Copy($storedDigest, 0, $signatureInput, 44, 32)
$signatureDigest = $sha.ComputeHash($signatureInput)

$publicBlob = [IO.File]::ReadAllBytes($PublicKeyPath)
$cngKey = [Security.Cryptography.CngKey]::Import(
    $publicBlob, [Security.Cryptography.CngKeyBlobFormat]::EccPublicBlob)
$ecdsa = New-Object Security.Cryptography.ECDsaCng $cngKey
$ecdsa.HashAlgorithm = [Security.Cryptography.CngAlgorithm]::Sha256
if (-not $ecdsa.VerifyHash($signatureDigest, $signature)) {
  throw "Secure image signature mismatch"
}
$ecdsa.Dispose()
$sha.Dispose()
Write-Host ("Verified {0}: type={1} address=0x{2:X8} size={3} version={4}" -f
           $ImagePath, $imageType, $loadAddress, $imageSize, $imageVersion)
