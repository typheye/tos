param(
  [Parameter(Mandatory = $true)][string]$ImagePath,
  [Parameter(Mandatory = $true)][string]$HeaderPath,
  [Parameter(Mandatory = $true)][string]$KeyPath,
  [Parameter(Mandatory = $true)][UInt32]$ImageType,
  [Parameter(Mandatory = $true)][UInt32]$LoadAddress,
  [Parameter(Mandatory = $true)][UInt32]$SignedSize,
  [Parameter(Mandatory = $true)][UInt32]$ImageVersion
)

$ErrorActionPreference = "Stop"
$HeaderOffset = 0x200
$HeaderSize = 0x80
$Magic = 0x31474953
$HeaderVersion = 1
$Flags = 0

function Write-U32LE {
  param([byte[]]$Bytes, [int]$Offset, [UInt32]$Value)
  $Bytes[$Offset] = [byte]($Value -band 0xFF)
  $Bytes[$Offset + 1] = [byte](($Value -shr 8) -band 0xFF)
  $Bytes[$Offset + 2] = [byte](($Value -shr 16) -band 0xFF)
  $Bytes[$Offset + 3] = [byte](($Value -shr 24) -band 0xFF)
}

function Get-Crc32 {
  param([byte[]]$Bytes, [int]$Count)
  [UInt32]$crc = [UInt32]::MaxValue
  for ($i = 0; $i -lt $Count; $i++) {
    $crc = $crc -bxor [UInt32]$Bytes[$i]
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
if ($SignedSize -gt $image.Length -or
    $SignedSize -lt ($HeaderOffset + $HeaderSize)) {
  throw "Signed span $SignedSize is outside image length $($image.Length)"
}
for ($i = 0; $i -lt $HeaderSize; $i++) {
  $image[$HeaderOffset + $i] = 0xFF
}
# Linker leaves 0x00 in the gap between .isr_vector end and TosImageHeader;
# on-device erased flash reads 0xFF.  NUL those bytes so the hashes match.
for ($i = 0x180; $i -lt $HeaderOffset; $i++) {
  if ($image[$i] -eq 0) { $image[$i] = 0xFF }
}

$sha = [Security.Cryptography.SHA256]::Create()
$imageDigest = $sha.ComputeHash($image, 0, $SignedSize)
$header = New-Object byte[] $HeaderSize
Write-U32LE $header 0 ([UInt32]$Magic)
Write-U32LE $header 4 ([UInt32]$HeaderVersion)
Write-U32LE $header 8 $ImageType
Write-U32LE $header 12 $LoadAddress
Write-U32LE $header 16 $SignedSize
Write-U32LE $header 20 $ImageVersion
Write-U32LE $header 24 ([UInt32]$Flags)
[Array]::Copy($imageDigest, 0, $header, 28, 32)

$domain = [byte[]](
    [char]'T', [char]'O', [char]'S', [char]'-',
    [char]'S', [char]'B', [char]'-', [char]'P',
    [char]'2', [char]'5', [char]'6', [char]'-',
    [char]'V', [char]'1', 0, 0)
$signatureInput = New-Object byte[] 76
[Array]::Copy($domain, 0, $signatureInput, 0, 16)
[Array]::Copy($header, 0, $signatureInput, 16, 28)
[Array]::Copy($imageDigest, 0, $signatureInput, 44, 32)
$signatureDigest = $sha.ComputeHash($signatureInput)

$keyBytes = [IO.File]::ReadAllBytes($KeyPath)
$cngKey = [Security.Cryptography.CngKey]::Import(
    $keyBytes, [Security.Cryptography.CngKeyBlobFormat]::Pkcs8PrivateBlob)
$ecdsa = New-Object Security.Cryptography.ECDsaCng $cngKey
$ecdsa.HashAlgorithm = [Security.Cryptography.CngAlgorithm]::Sha256
$signature = $ecdsa.SignHash($signatureDigest)
if ($signature.Length -ne 64 -or
    -not $ecdsa.VerifyHash($signatureDigest, $signature)) {
  throw "P-256 signing self-test failed"
}
[Array]::Copy($signature, 0, $header, 60, 64)
Write-U32LE $header 124 (Get-Crc32 $header 124)
[Array]::Copy($header, 0, $image, $HeaderOffset, $HeaderSize)

[IO.File]::WriteAllBytes($ImagePath, $image)
[IO.File]::WriteAllBytes($HeaderPath, $header)
$ecdsa.Dispose()
$sha.Dispose()
Write-Host ("Signed {0}: type={1} address=0x{2:X8} size={3} version={4}" -f
           $ImagePath, $ImageType, $LoadAddress, $SignedSize, $ImageVersion)
