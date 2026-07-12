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

# Auto-activate 'tos' conda environment if not already active
if ($env:CONDA_DEFAULT_ENV -ne "tos") {
    $conda = Get-Command conda.exe -ErrorAction SilentlyContinue
    if (-not $conda) {
        Write-Error "Conda not found. Open Anaconda Prompt or run scripts\setup-conda.bat first."
        exit 1
    }
    Write-Host "Activating 'tos' conda environment..." -ForegroundColor Yellow
    $installArg = if ($Install) { "-Install" } else { "" }
    & conda run --no-capture-output -n tos powershell -NoProfile -ExecutionPolicy Bypass -File "$PSScriptRoot\build.ps1" $installArg
    exit $LASTEXITCODE
}

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

    # Build all three tools
    $Jobs = @(
        @{ Spec = "tos_helper\tos_helper.spec"; Dist = $DistRoot; Name = "tos_helper" },
        @{ Spec = "sbltool\sbltool.spec";      Dist = $PlatformTools; Name = "sbltool" },
        @{ Spec = "tdb\tdb.spec";              Dist = $PlatformTools; Name = "tdb" }
    )

    foreach ($Job in $Jobs) {
        Invoke-PyInstallerSpec `
            -SpecPath (Join-Path $SourceRoot $Job.Spec) `
            -DistPath $Job.Dist `
            -WorkName $Job.Name
    }

    # Smoke test
    $Expected = @(
        (Join-Path $DistRoot "TOS Helper.exe"),
        (Join-Path $PlatformTools "sbltool.exe"),
        (Join-Path $PlatformTools "tdb.exe")
    )
    foreach ($File in $Expected) {
        if (-not (Test-Path $File -PathType Leaf)) {
            throw "Expected output was not generated: $File"
        }
    }
    foreach ($File in $Expected) {
        & $File --version | Out-Host
        if ($LASTEXITCODE -ne 0) {
            throw "$File version test failed."
        }
    }

    if ($Install) {
        if (-not $env:CONDA_PREFIX) {
            throw "No active Conda environment. Activate 'tos' before using -Install."
        }
        $ScriptsDir = Join-Path $env:CONDA_PREFIX "Scripts"
        New-Item -ItemType Directory -Path $ScriptsDir -Force | Out-Null
        Copy-Item (Join-Path $PlatformTools "sbltool.exe") $ScriptsDir -Force
        Copy-Item (Join-Path $PlatformTools "tdb.exe") $ScriptsDir -Force
        Write-Host "Installed sbltool.exe and tdb.exe to $ScriptsDir" -ForegroundColor Green
    }

    Write-Host ""
    Write-Host "Build complete:" -ForegroundColor Green
    Write-Host "  dist\TOS Helper.exe"
    Write-Host "  dist\platform-tools\sbltool.exe"
    Write-Host "  dist\platform-tools\tdb.exe"
}
catch {
    Write-Error $_
    exit 1
}
