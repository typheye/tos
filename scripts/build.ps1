param(
    [switch]$Install
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$BuildRoot = Join-Path $ProjectRoot "build"
$DistRoot = Join-Path $ProjectRoot "dist"
$PlatformTools = Join-Path $DistRoot "platform-tools"
$SourceRoot = Join-Path $ProjectRoot "src"

function Invoke-PyInstallerSpec {
    param(
        [Parameter(Mandatory = $true)][string]$SpecPath,
        [Parameter(Mandatory = $true)][string]$DistPath,
        [Parameter(Mandatory = $true)][string]$WorkName
    )

    $WorkPath = Join-Path (Join-Path $BuildRoot "pyinstaller") $WorkName
    New-Item -ItemType Directory -Path $WorkPath -Force | Out-Null

    $Arguments = @(
        "-m", "PyInstaller",
        "--noconfirm",
        "--clean",
        "--distpath", $DistPath,
        "--workpath", $WorkPath,
        $SpecPath
    )
    Write-Host "python $($Arguments -join ' ')" -ForegroundColor Cyan
    & python @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "PyInstaller failed for $SpecPath with exit code $LASTEXITCODE"
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

    Invoke-PyInstallerSpec `
        -SpecPath (Join-Path $SourceRoot "tos_helper\tos_helper.spec") `
        -DistPath $DistRoot `
        -WorkName "tos_helper"

    Invoke-PyInstallerSpec `
        -SpecPath (Join-Path $SourceRoot "tsblboot\tsblboot.spec") `
        -DistPath $PlatformTools `
        -WorkName "tsblboot"

    Invoke-PyInstallerSpec `
        -SpecPath (Join-Path $SourceRoot "tdb\tdb.spec") `
        -DistPath $PlatformTools `
        -WorkName "tdb"

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

    & (Join-Path $PlatformTools "tsblboot.exe") --version | Out-Host
    if ($LASTEXITCODE -ne 0) {
        throw "tsblboot version test failed."
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
