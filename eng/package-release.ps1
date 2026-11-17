# 配布用の正規入口。ビルドは既存の build-release.ps1 に委ねる（#413 / ADR 0115）。
# 版は CMake の project() の素の x.y.z だけ。Beta や pre-release の札は名前に入れない（#1596・1.1.0 から GitHub の Release も Beta を付けない・#1815）。
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
