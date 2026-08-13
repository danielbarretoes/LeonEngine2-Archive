<#
.SYNOPSIS
    Builds pending changes and runs Sandbox.exe.
.EXAMPLE
    .\scripts\RunSandbox.ps1
#>

[CmdletBinding()]
param(
    [string]$Config = "Debug"
)

$BuildScript = Join-Path $PSScriptRoot "BuildIncremental.ps1"
& $BuildScript -Run -Config $Config
