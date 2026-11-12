# MSIX の正規入口。exe は build-release.ps1 の出力を受け、作り直さない（#1581 / ADR 0186）。
# llvm-readobj を使うので toolchain を通してから Python の道具に渡す。署名・導入・起動はしない。
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$OutputDirectory,
    [string]$Zip,
    [switch]$Store
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'toolchain.ps1')
$arguments = @((Join-Path $PSScriptRoot 'package-msix.py'), '--output', $OutputDirectory)
if ($Zip) { $arguments += @('--zip', $Zip) }
if ($Store) { $arguments += '--store' }
& python @arguments
if ($LASTEXITCODE -ne 0) { throw 'MSIX packaging failed; preserve its partial output for inspection.' }
