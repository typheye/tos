$ErrorActionPreference = "Stop"
Push-Location (Split-Path $PSScriptRoot -Parent)
$header = git show HEAD:partitions/SAH/include/sah_logo.h 2>$null
if (-not $header) { throw "git failed" }
$data = [regex]::Matches($header, '0x[0-9A-Fa-f]+') | ForEach-Object { [uint16]$_.Value }
$total = $data.Count
$bitmap = $data[($total-115200)..($total-1)]
$w = 240; $h = 240
$pixels = [Collections.Generic.List[object]]::new()
for ($y = 0; $y -lt $h; $y++) {
  for ($x = 0; $x -lt $w; $x++) {
    $c = $bitmap[$y * $w + $x]
    if ($c -ne 0) { $pixels.Add((@{x=$x; y=$y; color=$c})) }
  }
}
Write-Host "Non-zero: $($pixels.Count) / $($w*$h)"
$out  = '/* Extracted logo pixels */' + "`n"
$out += '#include "manifest.h"' + "`n"
$out += "#define LOGO_PIXEL_COUNT $($pixels.Count)`n`n"
$out += 'typedef struct { uint16_t x; uint16_t y; uint16_t color; } LogoPixel;' + "`n`n"
$out += '#if LCD_ENABLED' + "`n"
$out += 'static const LogoPixel logo_pixels[LOGO_PIXEL_COUNT] SBL_CONST = {' + "`n"
foreach ($p in $pixels) {
  $out += "  {$($p.x), $($p.y), 0x$($p.color.ToString('X4'))}," + "`n"
}
$out += '};' + "`n"
$out += '#endif' + "`n"
[IO.File]::WriteAllText("C:/Code/tos/slave-board/partitions/SBL/include/logo_data.h", $out)
Write-Host "Written"
