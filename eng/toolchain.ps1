Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$versions = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'tool-versions.json') -Raw | ConvertFrom-Json
if ($PSVersionTable.PSVersion -lt [version]$versions.powershellMinimum) { throw 'QLT-011: PowerShell is too old.' }
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$installation = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if ($LASTEXITCODE -ne 0 -or -not $installation) { throw 'QLT-011: Visual Studio Build Tools were not found.' }
# DevShell は vswhere.exe を PATH から探すので、先に Installer ディレクトリを足す（無いと文字化けした警告だけが出る）。
$env:PATH = (Split-Path -Parent $vswhere) + [IO.Path]::PathSeparator + $env:PATH
Import-Module (Join-Path $installation 'Common7/Tools/Microsoft.VisualStudio.DevShell.dll')
Enter-VsDevShell -VsInstallPath $installation -SkipAutomaticLocation -DevCmdArguments "-arch=amd64 -host_arch=amd64 -vcvars_ver=$($versions.msvcToolset)" | Out-Null
$utf8Encoding = [Text.UTF8Encoding]::new($false)
[Console]::InputEncoding = $utf8Encoding
[Console]::OutputEncoding = $utf8Encoding
$OutputEncoding = $utf8Encoding
$toolDirectories = @(
    (Join-Path $installation 'VC/Tools/Llvm/x64/bin'),
    (Join-Path $installation 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin'),
    (Join-Path $installation 'Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja')
)
$env:PATH = ($toolDirectories -join [IO.Path]::PathSeparator) + [IO.Path]::PathSeparator + $env:PATH
$env:PYTHONDONTWRITEBYTECODE = '1'
$env:PYTHONUTF8 = '1'
$env:VSLANG = '1033'
# C++ のコンパイラは clang-cl ただ 1 つ（ADR 0003）。MSVC の toolset はリンカ・SDK・dumpbin・Phase 0 の比較対象のためだけに入る。
$env:CC = 'clang-cl'
$env:CXX = 'clang-cl'
foreach ($variable in @('CL', '_CL_', 'CFLAGS', 'CXXFLAGS', 'LDFLAGS')) {
    if ([Environment]::GetEnvironmentVariable($variable)) { throw "QLT-011: clear external build flags in $variable." }
}
$checks = @(
    @{ Name = 'cmake'; Expected = $versions.cmake },
    @{ Name = 'ninja'; Expected = $versions.ninja },
    @{ Name = 'clang-cl'; Expected = $versions.llvm },
    @{ Name = 'clang-tidy'; Expected = $versions.llvm },
    @{ Name = 'clang-format'; Expected = $versions.llvm },
    @{ Name = 'llvm-nm'; Expected = $versions.llvm },
    @{ Name = 'llvm-readobj'; Expected = $versions.llvm },
    @{ Name = 'llvm-profdata'; Expected = $versions.llvm },
    @{ Name = 'llvm-cov'; Expected = $versions.llvm },
    @{ Name = 'python'; Expected = $versions.python }
)
foreach ($check in $checks) {
    $output = & $check.Name --version 2>&1 | Out-String
    if ($LASTEXITCODE -ne 0 -or $output -notmatch ('(?<![\d.])' + [regex]::Escape($check.Expected) + '(?![\d.\w-])')) {
        throw "QLT-011: $($check.Name) must be $($check.Expected); actual: $output"
    }
}
