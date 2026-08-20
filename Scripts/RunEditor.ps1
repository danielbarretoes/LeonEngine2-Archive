[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release", "RelWithDebInfo")]
    [string]$Config = "Debug",
    [switch]$Clean,
    [switch]$Rebuild
)

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

$ArgsList = @("$ScriptDir/run_editor.py", "--config", $Config)
if ($Clean) { $ArgsList += "--clean" }
if ($Rebuild) { $ArgsList += "--rebuild" }

python @ArgsList
exit $LASTEXITCODE
