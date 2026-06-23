param(
    [switch]$Install
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$SourceRoot = Join-Path $ProjectRoot "src"
$BuildRoot = Join-Path $ProjectRoot "build"
$DistRoot = Join-Path $ProjectRoot "dist"
$PlatformTools = Join-Path $DistRoot "platform-tools"

function Invoke-PyInstaller {
    param([string[]]$Arguments)
    Write-Host "python -m PyInstaller $($Arguments -join ' ')" -ForegroundColor Cyan
    & python -m PyInstaller @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "PyInstaller failed with exit code $LASTEXITCODE"
    }
}

try {
    & python -c "import PyInstaller, serial, hid, tkinter"
    if ($LASTEXITCODE -ne 0) {
        throw "Missing build dependency. Run scripts\setup-conda.bat first."
    }

    if (Test-Path $BuildRoot) {
        Remove-Item $BuildRoot -Recurse -Force
    }
    if (Test-Path $DistRoot) {
        Remove-Item $DistRoot -Recurse -Force
    }
    New-Item -ItemType Directory -Path $PlatformTools -Force | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $BuildRoot "spec") -Force | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $BuildRoot "pyinstaller") -Force | Out-Null

    $Common = @(
        "--noconfirm",
        "--clean",
        "--onefile",
        "--noupx",
        "--paths", $SourceRoot,
        "--specpath", (Join-Path $BuildRoot "spec"),
        "--workpath", (Join-Path $BuildRoot "pyinstaller")
    )

    Invoke-PyInstaller ($Common + @(
        "--windowed",
        "--name", "TOS Helper",
        "--distpath", $DistRoot,
        "--hidden-import", "hid",
        (Join-Path $SourceRoot "tos_helper\__main__.py")
    ))

    Invoke-PyInstaller ($Common + @(
        "--console",
        "--name", "tsblboot",
        "--distpath", $PlatformTools,
        "--hidden-import", "serial.tools.list_ports",
        (Join-Path $SourceRoot "tsblboot\__main__.py")
    ))

    Invoke-PyInstaller ($Common + @(
        "--console",
        "--name", "tdb",
        "--distpath", $PlatformTools,
        "--hidden-import", "serial.tools.list_ports",
        (Join-Path $SourceRoot "tdb\__main__.py")
    ))

    $Expected = @(
        (Join-Path $DistRoot "TOS Helper.exe"),
        (Join-Path $PlatformTools "tsblboot.exe"),
        (Join-Path $PlatformTools "tdb.exe")
    )
    foreach ($File in $Expected) {
        if (-not (Test-Path $File -PathType Leaf)) {
            throw "Expected output was not generated: $File"
        }
    }

    & (Join-Path $PlatformTools "tsblboot.exe") partitions | Out-Host
    if ($LASTEXITCODE -ne 0) {
        throw "tsblboot smoke test failed."
    }
    & (Join-Path $PlatformTools "tdb.exe") --version | Out-Host
    if ($LASTEXITCODE -ne 0) {
        throw "tdb smoke test failed."
    }

    if ($Install) {
        if (-not $env:CONDA_PREFIX) {
            throw "No active Conda environment. Activate 'tos' before using -Install."
        }
        $ScriptsDir = Join-Path $env:CONDA_PREFIX "Scripts"
        New-Item -ItemType Directory -Path $ScriptsDir -Force | Out-Null
        Copy-Item (Join-Path $PlatformTools "tsblboot.exe") $ScriptsDir -Force
        Copy-Item (Join-Path $PlatformTools "tdb.exe") $ScriptsDir -Force
        Write-Host "Installed tsblboot.exe and tdb.exe to $ScriptsDir" -ForegroundColor Green
    }

    Write-Host "" 
    Write-Host "Build complete:" -ForegroundColor Green
    Write-Host "  dist\TOS Helper.exe"
    Write-Host "  dist\platform-tools\tsblboot.exe"
    Write-Host "  dist\platform-tools\tdb.exe"
}
catch {
    Write-Error $_
    exit 1
}
