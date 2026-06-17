param(
  [string]$BuildDir = "build\Release"
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$buildPath = Join-Path $root $BuildDir
$elfPath = Join-Path $buildPath "tos.elf"
$cachePath = Join-Path $buildPath "CMakeCache.txt"
$distDir = Join-Path $root "dist"
$firmwareDir = Join-Path $distDir "firmware"
$tmpHex = Join-Path $firmwareDir "flash.hex"

$flashBase = 0x08000000
$flashSize = 0x00100000
$sblOffset = 0x00000000
$sblSize = 0x00010000
$recOffset = 0x00010000
$recSize = 0x00010000
$sahOffset = 0x00020000
$sahSize = 0x00020000
$systemOffset = 0x00040000
$systemSize = 0x00080000

if (!(Test-Path -LiteralPath $elfPath)) {
  throw "ELF not found: $elfPath"
}
if (!(Test-Path -LiteralPath $cachePath)) {
  throw "CMake cache not found: $cachePath"
}

$cache = Get-Content -LiteralPath $cachePath
$objcopyPath = $null

try {
  $cmd = Get-Command arm-none-eabi-objcopy.exe -ErrorAction Stop
  $objcopyPath = $cmd.Source
} catch {
}

if (-not $objcopyPath) {
  $objdumpLine = $cache | Where-Object { $_ -like "CMAKE_OBJDUMP:FILEPATH=*" } | Select-Object -First 1
  if ($objdumpLine) {
    $objdumpPath = $objdumpLine.Substring($objdumpLine.IndexOf("=") + 1)
    $toolDir = Split-Path -Parent $objdumpPath
    $candidate = Join-Path $toolDir "arm-none-eabi-objcopy.exe"
    if (Test-Path -LiteralPath $candidate) {
      $objcopyPath = $candidate
    }
  }
}

if (-not $objcopyPath) {
  throw "Cannot find arm-none-eabi-objcopy.exe from PATH or $cachePath"
}

New-Item -ItemType Directory -Force -Path $firmwareDir | Out-Null
Get-ChildItem -LiteralPath $firmwareDir -Filter "*.bin" -ErrorAction SilentlyContinue |
  Remove-Item -Force

& $objcopyPath -O ihex $elfPath $tmpHex
if ($LASTEXITCODE -ne 0) {
  throw "objcopy failed"
}

[byte[]]$full = New-Object byte[] $flashSize
for ($i = 0; $i -lt $full.Length; $i++) {
  $full[$i] = 0xFF
}

$hexLines = Get-Content -LiteralPath $tmpHex
$upperAddr = 0
foreach ($line in $hexLines) {
  if (-not $line.StartsWith(":")) {
    continue
  }
  $count = [Convert]::ToInt32($line.Substring(1, 2), 16)
  $addr = [Convert]::ToInt32($line.Substring(3, 4), 16)
  $rtype = [Convert]::ToInt32($line.Substring(7, 2), 16)
  $data = $line.Substring(9, $count * 2)

  if ($rtype -eq 0) {
    $abs = $upperAddr + $addr
    if ($abs -lt $flashBase -or ($abs + $count) -gt ($flashBase + $flashSize)) {
      throw ("HEX data out of flash range: 0x{0:X8}" -f $abs)
    }
    $off = $abs - $flashBase
    for ($i = 0; $i -lt $count; $i++) {
      $full[$off + $i] = [Convert]::ToByte($data.Substring($i * 2, 2), 16)
    }
  } elseif ($rtype -eq 1) {
    break
  } elseif ($rtype -eq 4) {
    $upperAddr = [Convert]::ToInt32($data, 16) -shl 16
  }
}

function Write-Slice {
  param(
    [byte[]]$Source,
    [int]$Offset,
    [int]$Length,
    [string]$Path
  )
  [byte[]]$slice = New-Object byte[] $Length
  [Array]::Copy($Source, $Offset, $slice, 0, $Length)
  [System.IO.File]::WriteAllBytes($Path, $slice)
}

$sblPath = Join-Path $firmwareDir "sbl.bin"
$recPath = Join-Path $firmwareDir "rec.bin"
$sahPath = Join-Path $firmwareDir "sah.bin"
$systemPath = Join-Path $firmwareDir "system.bin"
$csvPath = Join-Path $distDir "partitions.csv"

Write-Slice -Source $full -Offset $sblOffset -Length $sblSize -Path $sblPath
Write-Slice -Source $full -Offset $recOffset -Length $recSize -Path $recPath
Write-Slice -Source $full -Offset $sahOffset -Length $sahSize -Path $sahPath
Write-Slice -Source $full -Offset $systemOffset -Length $systemSize -Path $systemPath

Remove-Item -LiteralPath $tmpHex -Force

$csv = @(
  "Name,Offset,Size"
  ('sbl,0x{0:X8},0x{1:X8}' -f $sblOffset, $sblSize)
  ('rec,0x{0:X8},0x{1:X8}' -f $recOffset, $recSize)
  ('sah,0x{0:X8},0x{1:X8}' -f $sahOffset, $sahSize)
  ('system,0x{0:X8},0x{1:X8}' -f $systemOffset, $systemSize)
)
Set-Content -LiteralPath $csvPath -Value $csv -Encoding ASCII

Write-Host "Export complete:"
Write-Host "  sbl.bin    $(('{0} bytes @ 0x{1:X8}' -f $sblSize, ($flashBase + $sblOffset)))"
Write-Host "  rec.bin    $(('{0} bytes @ 0x{1:X8}' -f $recSize, ($flashBase + $recOffset)))"
Write-Host "  sah.bin    $(('{0} bytes @ 0x{1:X8}' -f $sahSize, ($flashBase + $sahOffset)))"
Write-Host "  system.bin $(('{0} bytes @ 0x{1:X8}' -f $systemSize, ($flashBase + $systemOffset)))"
Write-Host "  partitions.csv"
Write-Host "Output: $distDir"
