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
$Args = @()
if ($Project) { $Args += @("--project", $Project) }
if ($Run) { $Args += "--run" }
$Args += @("--config", $Config)
& python $Py @Args
exit $LASTEXITCODE
