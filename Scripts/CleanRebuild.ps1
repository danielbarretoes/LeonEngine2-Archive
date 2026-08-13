<#
.SYNOPSIS
    Cleans previously compiled binaries and rebuilds LeonEngine2 from zero.
.EXAMPLE
    .\scripts\CleanRebuild.ps1
    .\scripts\CleanRebuild.ps1 -Run
#>

[CmdletBinding()]
param(
    [switch]$Run,
    [string]$Config = "Debug"
)

$BuildScript = Join-Path $PSScriptRoot "BuildIncremental.ps1"
& $BuildScript -Rebuild -Config $Config -Run:$Run
