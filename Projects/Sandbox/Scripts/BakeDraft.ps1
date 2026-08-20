# Bake Sandbox with Draft quality
$PSScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
python "$PSScriptRoot\bake_draft.py" @args
