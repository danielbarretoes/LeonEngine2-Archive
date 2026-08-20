<#
.SYNOPSIS
    Clean rebuild via Scripts/clean_rebuild.py (requires --project or LEON_PROJECT).
.EXAMPLE
    $env:LEON_PROJECT = "Projects/Sandbox/Sandbox.lproject"
    .\Scripts\CleanRebuild.ps1
    .\Scripts\CleanRebuild.ps1 -Project Projects/Sandbox/Sandbox.lproject -Run
#>

[CmdletBinding()]
param(
    [string]$Project = $env:LEON_PROJECT,
    [switch]$Run,
    [string]$Config = "Debug"
)

$ScriptDir = $PSScriptRoot
$Py = Join-Path $ScriptDir "clean_rebuild.py"
$PyArgs = @()
if ($Project) { $PyArgs += @("--project", $Project) }
if ($Run) { $PyArgs += "--run" }
$PyArgs += @("--config", $Config)
& python $Py @PyArgs
exit $LASTEXITCODE
