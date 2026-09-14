[CmdletBinding()]
param([Parameter(Mandatory)][string]$MessageFile)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$env:PYTHONUTF8 = '1'
& python (Join-Path $PSScriptRoot 'git-conventions.py') $MessageFile
exit $LASTEXITCODE
