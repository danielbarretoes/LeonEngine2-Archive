<#
.SYNOPSIS
    Builds or recompiles the LeonEngine2 project and all its modules.
.DESCRIPTION
    Automates CMake configuration, building, and optional running of the Sandbox executable.
.PARAMETER Clean
    Performs a clean build by deleting the build directory and reconfiguring CMake.
.PARAMETER Rebuild
    Cleans previously compiled targets and compiles all targets from scratch.
.PARAMETER Run
    Automatically launches Sandbox.exe after a successful build.
.PARAMETER Config
    Specifies the build configuration (Debug, Release, RelWithDebInfo). Default: Debug.
.EXAMPLE
    .\scripts\BuildIncremental.ps1
    .\scripts\BuildIncremental.ps1 -Run
    .\scripts\BuildIncremental.ps1 -Clean -Run
    .\scripts\BuildIncremental.ps1 -Rebuild
#>

[CmdletBinding()]
param(
    [switch]$Clean,
    [switch]$Rebuild,
    [switch]$Run,
    [ValidateSet("Debug", "Release", "RelWithDebInfo")]
    [string]$Config = "Debug"
)

$ErrorActionPreference = "Stop"

$ProjectRoot = Resolve-Path "$PSScriptRoot\.."
Set-Location $ProjectRoot

$BuildDir = Join-Path $ProjectRoot "build"
$ExecutablePath = Join-Path $BuildDir "Projects\Sandbox\Sandbox.exe"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "   LeonEngine2 - Build Incremental      " -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  Configuration: $Config" -ForegroundColor Gray
Write-Host "  Project Root:  $ProjectRoot" -ForegroundColor Gray
Write-Host "----------------------------------------" -ForegroundColor Cyan

# 1. Clean build directory if requested
if ($Clean) {
    if (Test-Path $BuildDir) {
        Write-Host "[INFO] Cleaning build directory ($BuildDir)..." -ForegroundColor Yellow
        Remove-Item -Path $BuildDir -Recurse -Force
    }
}

# 2. Configure CMake if build directory does not exist or CMakeCache.txt is missing
$NeedsConfigure = (-not (Test-Path $BuildDir)) -or (-not (Test-Path (Join-Path $BuildDir "CMakeCache.txt")))

if ($NeedsConfigure) {
    Write-Host "[INFO] Configuring CMake ($Config)..." -ForegroundColor Cyan
    cmake -B build -G "Ninja" -DCMAKE_BUILD_TYPE=$Config -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[ERROR] CMake configuration failed!" -ForegroundColor Red
        exit $LASTEXITCODE
    }
}

# 3. Build the project
Write-Host "[INFO] Building LeonEngine2..." -ForegroundColor Cyan
$Stopwatch = [System.Diagnostics.Stopwatch]::StartNew()

$BuildArgs = @("--build", "build", "--config", $Config)
if ($Rebuild -and -not $Clean) {
    $BuildArgs += "--clean-first"
}

& cmake @BuildArgs
$BuildExitCode = $LASTEXITCODE

$Stopwatch.Stop()

if ($BuildExitCode -ne 0) {
    Write-Host "----------------------------------------" -ForegroundColor Red
    Write-Host "[ERROR] Build failed with exit code $BuildExitCode!" -ForegroundColor Red
    Write-Host "========================================" -ForegroundColor Red
    exit $BuildExitCode
}

Write-Host "----------------------------------------" -ForegroundColor Green
Write-Host "[SUCCESS] Build completed in $($Stopwatch.ElapsedMilliseconds) ms." -ForegroundColor Green
Write-Host "  Executable: $ExecutablePath" -ForegroundColor Gray
Write-Host "========================================" -ForegroundColor Green

# 4. Optional: Run the executable if requested
if ($Run) {
    if (Test-Path $ExecutablePath) {
        Write-Host "`n[LAUNCH] Starting Sandbox.exe..." -ForegroundColor Cyan
        & "$ExecutablePath"
    } else {
        Write-Host "[ERROR] Executable not found at $ExecutablePath" -ForegroundColor Red
    }
}
