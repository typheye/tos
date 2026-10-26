param(
  [Parameter(Mandatory = $true)][string]$OutputHeader,
  [Parameter(Mandatory = $true)][string]$KeyPath,
  [Parameter(Mandatory = $true)][string]$KeyPathFile
)

$ErrorActionPreference = "Stop"

function Format-CBytes {
  param([byte[]]$Bytes)
  return (($Bytes | ForEach-Object { "0x{0:X2}U" -f $_ }) -join ", ")
}

$keyDirectory = Split-Path -Parent $KeyPath
$headerDirectory = Split-Path -Parent $OutputHeader
$keyPathFileDirectory = Split-Path -Parent $KeyPathFile
New-Item -ItemType Directory -Force -Path $keyDirectory | Out-Null
New-Item -ItemType Directory -Force -Path $headerDirectory | Out-Null
New-Item -ItemType Directory -Force -Path $keyPathFileDirectory | Out-Null

# Windows CNG (CngKey/ECDsaCng) is unavailable outside Windows. Other hosts
# use the cross-platform .NET ECDsa API and rebuild the same
# CNG ECC_PUBLIC_BLOB layout below, so key material and image signatures
# stay interchangeable between Windows and Linux.
$isWindowsHost = ($env:OS -eq 'Windows_NT')

if ($isWindowsHost) {
  if (Test-Path -LiteralPath $KeyPath) {
    $keyBytes = [IO.File]::ReadAllBytes($KeyPath)
    $cngKey = [Security.Cryptography.CngKey]::Import(
        $keyBytes, [Security.Cryptography.CngKeyBlobFormat]::Pkcs8PrivateBlob)
    $ecdsa = New-Object Security.Cryptography.ECDsaCng $cngKey
  } else {
    $ecdsa = New-Object Security.Cryptography.ECDsaCng 256
    $keyBytes = $ecdsa.Key.Export(
        [Security.Cryptography.CngKeyBlobFormat]::Pkcs8PrivateBlob)
    [IO.File]::WriteAllBytes($KeyPath, $keyBytes)
    Write-Warning "Generated DEVELOPMENT signing key: $KeyPath"
    Write-Warning "Set TOS_SIGNING_KEY to a protected production PKCS#8 key before release."
  }
  $publicBlob = $ecdsa.Key.Export(
      [Security.Cryptography.CngKeyBlobFormat]::EccPublicBlob)
} else {
  $ecdsa = [Security.Cryptography.ECDsa]::Create()
  if (Test-Path -LiteralPath $KeyPath) {
    $keyBytes = [IO.File]::ReadAllBytes($KeyPath)
    $consumed = 0
    $ecdsa.ImportPkcs8PrivateKey($keyBytes, [ref]$consumed)
    if ($consumed -ne $keyBytes.Length) {
      throw "Unexpected PKCS#8 key: imported $consumed of $($keyBytes.Length) bytes"
    }
  } else {
    $keyBytes = $ecdsa.ExportPkcs8PrivateKey()
    [IO.File]::WriteAllBytes($KeyPath, $keyBytes)
    Write-Warning "Generated DEVELOPMENT signing key: $KeyPath"
    Write-Warning "Set TOS_SIGNING_KEY to a protected production PKCS#8 key before release."
  }
  # SPKI for P-256 EC public keys is a fixed 26-byte prefix, then the
  # uncompressed point 0x04 || X || Y. Re-emit it as the CNG blob layout:
  # magic "ECS1", cbKey = 32, then X || Y (64 bytes) with no 0x04 marker.
  $spki = $ecdsa.ExportSubjectPublicKeyInfo()
  $rawKey = New-Object byte[] 64
  [Array]::Copy($spki, $spki.Length - 64, $rawKey, 0, 64)
  $publicBlob = New-Object byte[] 72
  $blobHeader = [byte[]](0x45, 0x43, 0x53, 0x31, 0x20, 0x00, 0x00, 0x00)
  [Array]::Copy($blobHeader, 0, $publicBlob, 0, 8)
  [Array]::Copy($rawKey, 0, $publicBlob, 8, 64)
}
if ($publicBlob.Length -ne 72) {
  throw "Unexpected P-256 public key blob length: $($publicBlob.Length)"
}
$publicKey = New-Object byte[] 64
[Array]::Copy($publicBlob, 8, $publicKey, 0, 64)
$sha = [Security.Cryptography.SHA256]::Create()
$publicHash = $sha.ComputeHash($publicKey)

$header = @"
#ifndef TOS_SECURE_BOOT_KEY_H
#define TOS_SECURE_BOOT_KEY_H

#define TOS_SECURE_BOOT_PUBLIC_KEY_BYTES { $(Format-CBytes $publicKey) }
#define TOS_SECURE_BOOT_PUBLIC_KEY_HASH_BYTES { $(Format-CBytes $publicHash) }

#endif /* TOS_SECURE_BOOT_KEY_H */
"@
[IO.File]::WriteAllText($OutputHeader, $header,
                        (New-Object Text.UTF8Encoding($false)))
[IO.File]::WriteAllText($KeyPathFile, [IO.Path]::GetFullPath($KeyPath),
                        (New-Object Text.UTF8Encoding($false)))
[IO.File]::WriteAllBytes($KeyPath + ".pub", $publicBlob)
$ecdsa.Dispose()
$sha.Dispose()
