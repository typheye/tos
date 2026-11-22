param([Parameter(Mandatory = $true)][string]$ConfigPath)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$config = Get-Content -LiteralPath $ConfigPath -Raw -Encoding UTF8 | ConvertFrom-Json
$shell = New-Object -ComObject WScript.Shell
$menu = [Environment]::GetFolderPath('Programs')
$desktopDir = [Environment]::GetFolderPath('DesktopDirectory')
$paths = @( (Join-Path $menu 'TOS Helper.lnk') )
$desktopLink = Join-Path $desktopDir 'TOS Helper.lnk'
if ($config.desktop -or (Test-Path -LiteralPath $desktopLink)) { $paths += $desktopLink }
foreach ($path in $paths) {
    $link = $shell.CreateShortcut($path)
    if ((Test-Path -LiteralPath $path) -and $link.TargetPath -and
        ([IO.Path]::GetFileName($link.TargetPath) -ne 'TOS Helper.exe')) {
        throw "Refusing to replace another application's shortcut: $path"
    }
    $link.TargetPath = $config.target
    $link.Arguments = '--gui'
    $link.WorkingDirectory = $config.directory
    $link.IconLocation = $config.icon
    $link.Description = 'TOS HID diagnostics'
    $link.WindowStyle = 7
    $link.Save()
    Write-Host "Shortcut: $path"
}
