# 配布用の正規入口。ビルドは既存の build-release.ps1 に委ねる（#413 / ADR 0115）。
# 版は CMake の project() の素の x.y.z だけ。Beta は GitHub の Release の題と pre-release の印で、名前に入れない（#1596）。
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$OutputDirectory,
    [Parameter(Mandatory)][string]$Verification,
    [switch]$Preview
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'toolchain.ps1')
$arguments = @((Join-Path $PSScriptRoot 'package-release.py'),
    '--output', $OutputDirectory, '--verification', $Verification)
if ($Preview) { $arguments += '--preview' }
& python @arguments
if ($LASTEXITCODE -ne 0) { throw 'Release packaging failed; preserve its partial output for inspection.' }
