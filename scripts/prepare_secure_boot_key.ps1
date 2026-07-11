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
New-Item -ItemType Directory -Force -Path $keyDirectory | Out-Null
New-Item -ItemType Directory -Force -Path $headerDirectory | Out-Null

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
