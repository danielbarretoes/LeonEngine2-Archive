# Bake LeonTournament with Production quality
$PSScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
python "$PSScriptRoot\bake_production.py" @args
