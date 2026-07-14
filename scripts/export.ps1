param(
  [string]$BuildDir = "build\Release"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
if ([IO.Path]::IsPathRooted($BuildDir)) {
  $buildPath = [IO.Path]::GetFullPath($BuildDir)
} else {
  $buildPath = Join-Path $root $BuildDir
}
$cachePath = Join-Path $buildPath "CMakeCache.txt"
$distDir = Join-Path $root "dist"
$firmwareDir = Join-Path $distDir "firmware"
$flashDir = Join-Path $distDir "flash"

if (!(Test-Path -LiteralPath $cachePath)) {
  throw "CMake cache not found: $cachePath"
}

$cache = Get-Content -LiteralPath $cachePath
$objcopyPath = $null
try {
  $objcopyPath = (Get-Command arm-none-eabi-objcopy.exe -ErrorAction Stop).Source
} catch {}
if (-not $objcopyPath) {
  $line = $cache | Where-Object { $_ -like "CMAKE_OBJCOPY:FILEPATH=*" } | Select-Object -First 1
  if ($line) {
    $candidate = $line.Substring($line.IndexOf("=") + 1)
    if (Test-Path -LiteralPath $candidate) { $objcopyPath = $candidate }
  }
}
if (-not $objcopyPath) {
  $line = $cache | Where-Object { $_ -like "CMAKE_OBJDUMP:FILEPATH=*" } | Select-Object -First 1
  if ($line) {
    $toolDir = Split-Path -Parent $line.Substring($line.IndexOf("=") + 1)
    $candidate = Join-Path $toolDir "arm-none-eabi-objcopy.exe"
    if (Test-Path -LiteralPath $candidate) { $objcopyPath = $candidate }
  }
}
if (-not $objcopyPath) {
  throw "Cannot find arm-none-eabi-objcopy.exe"
}

New-Item -ItemType Directory -Force -Path $firmwareDir | Out-Null
New-Item -ItemType Directory -Force -Path $flashDir | Out-Null
Remove-Item -LiteralPath (Join-Path $distDir "factory") -Recurse -Force -ErrorAction SilentlyContinue
Get-ChildItem -LiteralPath $firmwareDir -Filter "*.bin" -ErrorAction SilentlyContinue |
  Remove-Item -Force
Get-ChildItem -LiteralPath $flashDir -Filter "*.bin" -ErrorAction SilentlyContinue |
  Remove-Item -Force
Get-ChildItem -LiteralPath $flashDir -Filter "*.elf" -ErrorAction SilentlyContinue |
  Remove-Item -Force

function Pad-File4 {
  param([string]$Path)
  $item = Get-Item -LiteralPath $Path
  $pad = [int](($item.Length % 4))
  if ($pad -eq 0) { return }
  $bytes = New-Object byte[] (4 - $pad)
  for ($i = 0; $i -lt $bytes.Length; $i++) { $bytes[$i] = 0xFF }
  $stream = [IO.File]::Open($Path, [IO.FileMode]::Append, [IO.FileAccess]::Write)
  try {
    $stream.Write($bytes, 0, $bytes.Length)
  } finally {
    $stream.Dispose()
  }
}

$signScript = Join-Path $root "scripts\sign.ps1"
$verifyScript = Join-Path $root "scripts\verify.ps1"
$keyPathFile = Join-Path $buildPath "secure-boot\signing-key.path"
if (!(Test-Path -LiteralPath $keyPathFile)) {
  throw "Secure boot signing key path not found: $keyPathFile"
}
$signingKeyPath = (Get-Content -LiteralPath $keyPathFile -Raw).Trim()
if (!(Test-Path -LiteralPath $signingKeyPath)) {
  throw "Secure boot signing key not found: $signingKeyPath"
}
$imageVersion = 1
$versionLine = $cache | Where-Object { $_ -like "TOS_IMAGE_VERSION:STRING=*" } | Select-Object -First 1
if ($versionLine) {
  $imageVersion = [UInt32]$versionLine.Substring($versionLine.IndexOf("=") + 1)
}
function Export-Partition {
  param(
    [string]$Target,
    [UInt32]$Address,
    [UInt32]$Size,
    [string]$Output,
    [UInt32]$ImageType = 0,
    [UInt32]$SignedSize = 0
  )
  $elf = Join-Path $buildPath ($Target + ".elf")
  if (!(Test-Path -LiteralPath $elf)) { throw "ELF not found: $elf" }
  $path = Join-Path $firmwareDir $Output
  $flashPath = Join-Path $flashDir ($Target + ".elf")
  Copy-Item -LiteralPath $elf -Destination $flashPath -Force

  $padTo = $Address + $Size
  & $objcopyPath -O binary --gap-fill 0xFF --pad-to ("0x{0:X8}" -f $padTo) $elf $path
  if ($LASTEXITCODE -ne 0) { throw "objcopy failed for $Target" }
  $actual = (Get-Item -LiteralPath $path).Length
  if ($actual -ne $Size) {
    throw "$Output size mismatch: expected $Size, got $actual"
  }

  Copy-Item -LiteralPath $elf -Destination $flashPath -Force
  if ($ImageType -ne 0) {
    if ($SignedSize -eq 0) { $SignedSize = $Size }
    $headerPath = Join-Path $buildPath ("secure-boot\" + $Target + ".header.bin")
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $signScript -ImagePath $path -HeaderPath $headerPath -KeyPath $signingKeyPath -ImageType $ImageType -LoadAddress $Address -SignedSize $SignedSize -ImageVersion $imageVersion
    if ($LASTEXITCODE -ne 0) { throw "Signing failed for $Target" }
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $verifyScript -ImagePath $path -PublicKeyPath ($signingKeyPath + ".pub") -ExpectedType $ImageType -ExpectedAddress $Address -ExpectedSize $SignedSize -MinimumVersion $imageVersion
    if ($LASTEXITCODE -ne 0) { throw "Signature verification failed for $Target" }
    # Signing also normalizes the unused vector-table tail (0x180..0x1FF) to
    # erased 0xFF. Patch both loadable sections so sparse ELF flashing is
    # byte-identical to the verified full-partition BIN.
    $vectorPath = Join-Path $buildPath ("secure-boot\" + $Target + ".vector.bin")
    $signedImage = [IO.File]::ReadAllBytes($path)
    $signedVector = New-Object byte[] 0x200
    [Array]::Copy($signedImage, 0, $signedVector, 0, $signedVector.Length)
    [IO.File]::WriteAllBytes($vectorPath, $signedVector)
    & $objcopyPath `
        --update-section (".isr_vector=" + $vectorPath) `
        --update-section (".tos_image_header=" + $headerPath) `
        $flashPath
    if ($LASTEXITCODE -ne 0) { throw "ELF signed-section update failed for $Target" }

    $flashVerifyPath = Join-Path $buildPath ("secure-boot\" + $Target + ".flash-verify.bin")
    & $objcopyPath -O binary --gap-fill 0xFF `
        --pad-to ("0x{0:X8}" -f $padTo) $flashPath $flashVerifyPath
    if ($LASTEXITCODE -ne 0) { throw "ELF expansion failed for $Target" }
    $signedHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $path).Hash
    $flashHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $flashVerifyPath).Hash
    $flashLength = (Get-Item -LiteralPath $flashVerifyPath).Length
    Remove-Item -LiteralPath $flashVerifyPath -Force
    if ($flashLength -ne $Size -or $flashHash -ne $signedHash) {
      throw "Host-flash ELF is not byte-identical to signed BIN for $Target"
    }
  }
}

# ELF (sector 0) is intentionally not exported as a BIN. It is factory-only
# and must be programmed from flash/factory.elf so normal update packages cannot
# accidentally contain or overwrite the immutable root loader.
$elfStage = Join-Path $buildPath "elf_stage.elf"
if (!(Test-Path -LiteralPath $elfStage)) { throw "ELF stage not found: $elfStage" }
Copy-Item -LiteralPath $elfStage -Destination (Join-Path $flashDir "factory.elf") -Force

Export-Partition -Target "sbl"    -Address 0x08004000 -Size 0x0000C000 -Output "sbl.bin" -ImageType 1 -SignedSize 0x0000C000
Export-Partition -Target "rec"    -Address 0x08010000 -Size 0x00010000 -Output "rec.bin" -ImageType 2 -SignedSize 0x0000FC00
Export-Partition -Target "system" -Address 0x08040000 -Size 0x000C0000 -Output "system.bin" -ImageType 3 -SignedSize 0x000C0000

$csvPath = Join-Path $distDir "partitions.csv"
$csv = @(
  "Name,Offset,Size,Image,Policy"
  "elf,0x00000000,0x00004000,,factory-elf-only"
  "sbl,0x00004000,0x0000C000,sbl.bin,staged-via-tmp"
  "rec,0x00010000,0x00010000,rec.bin,staged-via-tmp"
  "tmp,0x00020000,0x00020000,,runtime-cache"
  "system,0x00040000,0x000C0000,system.bin,direct"
)
Set-Content -LiteralPath $csvPath -Value $csv -Encoding ASCII

Write-Host "Export complete (ELF stage intentionally omitted from BIN output):"
Write-Host "  firmware/ full partition images:"
Get-ChildItem -LiteralPath $firmwareDir -Filter "*.bin" | ForEach-Object {
  Write-Host ("  {0,-12} {1,8} bytes" -f $_.Name, $_.Length)
}
Write-Host "  flash/ host-flash ELF images:"
Get-ChildItem -LiteralPath $flashDir -Filter "*.elf" | ForEach-Object {
  Write-Host ("  {0,-12} {1,8} bytes" -f $_.Name, $_.Length)
}
Write-Host "  partitions.csv"
