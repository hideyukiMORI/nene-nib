[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'toolchain.ps1')
Push-Location $repoRoot
try {
    & python eng/conformance.py
    if ($LASTEXITCODE -ne 0) { throw 'Conformance failed.' }
    & python eng/test-conformance.py
    if ($LASTEXITCODE -ne 0) { throw 'QLT-007: conformance self-tests failed.' }
    & pwsh -NoProfile -File eng/validate-git.ps1
    if ($LASTEXITCODE -ne 0) { throw 'Git conventions failed.' }
    $files = @(& git ls-files --cached --others --exclude-standard -- '*.cpp' '*.hpp' '*.h' '*.cc' '*.cxx' '*.hxx') | Sort-Object -Unique
    if ($LASTEXITCODE -ne 0) { throw 'Could not enumerate C++ files.' }
    foreach ($file in $files) {
        if (-not (Test-Path -LiteralPath $file)) { continue }
        & clang-format --dry-run --Werror "--style=file:$repoRoot/.clang-format" $file
        if ($LASTEXITCODE -ne 0) { throw "QLT-004: formatting failed: $file" }
    }
    New-Item -ItemType Directory -Force -Path build/.cmake/api/v1/query | Out-Null
    New-Item -ItemType File -Force -Path build/.cmake/api/v1/query/codemodel-v2 | Out-Null
    & cmake --fresh -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
    if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed.' }
    & cmake --build build --clean-first
    if ($LASTEXITCODE -ne 0) { throw 'QLT-002: compilation or clang-tidy failed.' }
    & python eng/conformance.py --build-dir build
    if ($LASTEXITCODE -ne 0) { throw 'ARC-002: actual build graph failed.' }
    # --require は Phase 3 の縦切りで core / application を結線するときに足す。今は守る対象が無い
    # （0 libraries checked）。「在ること」ではなく「落ちること」が検査なので、ここは planned のまま。
    & python eng/symbols.py --build-dir build
    if ($LASTEXITCODE -ne 0) { throw 'ARC-003 / ARC-007 / CPP-013: undefined symbols outside the allowlist.' }
    & ctest --test-dir build --output-on-failure --no-tests=error
    if ($LASTEXITCODE -ne 0) { throw 'C++ verification failed.' }
    & python eng/prove-gates.py
    if ($LASTEXITCODE -ne 0) { throw 'QLT-007: gate proofs failed.' }
    & git diff --check
    if ($LASTEXITCODE -ne 0) { throw 'Whitespace verification failed.' }
    Write-Host 'NeNe Nib full gate passed (UI hardware checks are recorded separately).'
}
finally { Pop-Location }
