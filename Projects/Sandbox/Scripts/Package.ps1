# Package Sandbox (Shipping)
$PSScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
python "$PSScriptRoot\package.py" @args
