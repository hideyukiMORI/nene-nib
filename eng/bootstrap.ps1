[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'toolchain.ps1')
& git -C $repoRoot config --local core.hooksPath .githooks
if ($LASTEXITCODE -ne 0) { throw 'Could not configure repository-owned hooks.' }
Write-Host 'Pinned tools verified; repository-local hooks enabled. Full gate not run.'
