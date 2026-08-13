<#
.SYNOPSIS
    Formats all C/C++ source and header files in the LeonEngine2 project using clang-format.
.DESCRIPTION
    Scans Engine/, Plugins/, and Projects/ directories, excluding ThirdParty/ and build/.
#>

$ErrorActionPreference = "Stop"

$ProjectRoot = Resolve-Path "$PSScriptRoot\.."
Set-Location $ProjectRoot

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "   LeonEngine2 - Code Formatter         " -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan

# Check for clang-format
$ClangFormatCmd = Get-Command "clang-format" -ErrorAction SilentlyContinue

if (-not $ClangFormatCmd) {
    Write-Host "[ERROR] clang-format was not found in PATH." -ForegroundColor Red
    Write-Host "Installing clang-format via pip..." -ForegroundColor Yellow
    python -m pip install clang-format
    $ClangFormatCmd = Get-Command "clang-format" -ErrorAction SilentlyContinue
    if (-not $ClangFormatCmd) {
        Write-Host "[FATAL] Failed to locate clang-format. Please ensure Python/clang-format is installed." -ForegroundColor Red
        exit 1
    }
}

Write-Host "[INFO] Using: $($ClangFormatCmd.Source)" -ForegroundColor Gray

# Target directories to format
$TargetDirs = @("Engine", "Plugins", "Projects")
$Extensions = @("*.hpp", "*.h", "*.cpp", "*.c", "*.inl")

$FileCount = 0
$Stopwatch = [System.Diagnostics.Stopwatch]::StartNew()

foreach ($Dir in $TargetDirs) {
    $DirPath = Join-Path $ProjectRoot $Dir
    if (Test-Path $DirPath) {
        $Files = Get-ChildItem -Path $DirPath -Recurse -File -Include $Extensions | 
                 Where-Object { $_.FullName -notmatch '\\(ThirdParty|build|\.cache|\.git)\\' }

        foreach ($File in $Files) {
            $RelativePath = Resolve-Path -Relative $File.FullName
            Write-Host "  -> Formatting: $RelativePath" -ForegroundColor DarkGray
            & clang-format -i -style=file "$($File.FullName)"
            $FileCount++
        }
    }
}

$Stopwatch.Stop()

Write-Host "----------------------------------------" -ForegroundColor Cyan
Write-Host "[SUCCESS] Formatted $FileCount files in $($Stopwatch.ElapsedMilliseconds) ms." -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Cyan
