# QLT-004: 整形は検査であって修復ではない。clang-format の --dry-run --Werror で全 C++ ファイルを照らし、
# 書き換えない。正本はこの 1 本で、eng/check.ps1 と CI の build-and-test job（check.yml）の両方が呼ぶ（#1057・ADR 0136 の決定 8）。
# 道具の版は toolchain.ps1 が照合する（clang-format は clang-cl と同じ LLVM 19.1.5）。
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'toolchain.ps1')
Push-Location $repoRoot
try {
    # 外部コード（third_party/）は書き換えないので整形の対象外。バイト列は CNF-013 が守る（ADR 0151 の決定 15）。
    $files = @(& git ls-files --cached --others --exclude-standard -- '*.cpp' '*.hpp' '*.h' '*.cc' '*.cxx' '*.hxx' ':(exclude)third_party/**') | Sort-Object -Unique
    if ($LASTEXITCODE -ne 0) { throw 'Could not enumerate C++ files.' }
    foreach ($file in $files) {
        if (-not (Test-Path -LiteralPath $file)) { continue }
        & clang-format --dry-run --Werror "--style=file:$repoRoot/.clang-format" $file
        if ($LASTEXITCODE -ne 0) { throw "QLT-004: formatting failed: $file" }
    }
    Write-Host "QLT-004: $($files.Count) C++ files are formatted."
}
finally {
    Pop-Location
}
