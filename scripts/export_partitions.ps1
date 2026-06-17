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
$factoryDir = Join-Path $distDir "factory"

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
New-Item -ItemType Directory -Force -Path $factoryDir | Out-Null
Get-ChildItem -LiteralPath $firmwareDir -Filter "*.bin" -ErrorAction SilentlyContinue |
  Remove-Item -Force
Get-ChildItem -LiteralPath $factoryDir -Filter "*.elf" -ErrorAction SilentlyContinue |
  Remove-Item -Force

function Export-Partition {
  param(
    [string]$Target,
    [UInt32]$Address,
    [UInt32]$Size,
    [string]$Output
  )
  $elf = Join-Path $buildPath ($Target + ".elf")
  if (!(Test-Path -LiteralPath $elf)) { throw "ELF not found: $elf" }
  $path = Join-Path $firmwareDir $Output
  $padTo = $Address + $Size
  & $objcopyPath -O binary --gap-fill 0xFF --pad-to ("0x{0:X8}" -f $padTo) $elf $path
  if ($LASTEXITCODE -ne 0) { throw "objcopy failed for $Target" }
  $actual = (Get-Item -LiteralPath $path).Length
  if ($actual -ne $Size) {
    throw "$Output size mismatch: expected $Size, got $actual"
  }
}

# ELF (sector 0) is intentionally not exported as a BIN. It is factory-only
# and must be programmed from elf_stage.elf so normal update packages cannot
# accidentally contain or overwrite the immutable root loader.
$elfStage = Join-Path $buildPath "elf_stage.elf"
if (!(Test-Path -LiteralPath $elfStage)) { throw "ELF stage not found: $elfStage" }
Copy-Item -LiteralPath $elfStage -Destination (Join-Path $factoryDir "elf_stage.elf") -Force

Export-Partition -Target "sbl"    -Address 0x08004000 -Size 0x00008000 -Output "sbl.bin"
Export-Partition -Target "tee"    -Address 0x0800C000 -Size 0x00004000 -Output "tee.bin"
Export-Partition -Target "rec"    -Address 0x08010000 -Size 0x00010000 -Output "rec.bin"
Export-Partition -Target "sah"    -Address 0x08020000 -Size 0x00020000 -Output "sah.bin"
Export-Partition -Target "system" -Address 0x08040000 -Size 0x00080000 -Output "system.bin"

$csvPath = Join-Path $distDir "partitions.csv"
$csv = @(
  "Name,Offset,Size,Image,Policy"
  "elf,0x00000000,0x00004000,,factory-elf-only"
  "sbl,0x00004000,0x00008000,sbl.bin,staged-via-tmp"
  "tee,0x0000C000,0x00004000,tee.bin,factory-security-state"
  "rec,0x00010000,0x00010000,rec.bin,staged-via-tmp"
  "sah,0x00020000,0x00020000,sah.bin,direct"
  "system,0x00040000,0x00080000,system.bin,direct"
  "tmp,0x000C0000,0x00020000,,runtime-cache"
  "userdata,0x000E0000,0x00020000,,persistent-data"
)
Set-Content -LiteralPath $csvPath -Value $csv -Encoding ASCII

Write-Host "Export complete (ELF stage intentionally omitted from BIN output):"
Write-Host ("  {0,-12} {1,8} bytes" -f "elf_stage.elf", (Get-Item -LiteralPath (Join-Path $factoryDir "elf_stage.elf")).Length)
Get-ChildItem -LiteralPath $firmwareDir -Filter "*.bin" | ForEach-Object {
  Write-Host ("  {0,-12} {1,8} bytes" -f $_.Name, $_.Length)
}
Write-Host "  partitions.csv"
