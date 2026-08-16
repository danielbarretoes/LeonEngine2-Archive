<#
.SYNOPSIS
    Formats C/C++ sources via Scripts/format_code.py.
#>

$ErrorActionPreference = "Stop"
$Py = Join-Path $PSScriptRoot "format_code.py"
& python $Py @args
exit $LASTEXITCODE
