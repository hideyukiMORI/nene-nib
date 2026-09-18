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
    # Issue #3 で core / application の静的ライブラリが生まれたので --require を結線する（ADR 0007）。
    # 中核のライブラリが消えたらここが落ちる＝検査が対象を失ったことに気づける。
    & python eng/symbols.py --build-dir build --require core application
    if ($LASTEXITCODE -ne 0) { throw 'ARC-003 / ARC-007 / CPP-013: undefined symbols outside the allowlist.' }
    & ctest --test-dir build --output-on-failure --no-tests=error
    if ($LASTEXITCODE -ne 0) { throw 'C++ verification failed.' }
    # 速さは製品の速さで測る。Debug の exe は ASan / UBSan が掛かっているので、サニタイザを
    # 当てていない Release 構成の NeNeNib だけをここで作る（ADR 0006 / 0011）。警告集合と
    # clang-tidy は同じものが掛かる（迂回路は作らない）。差分ビルドなので --fresh は当てない。
    & cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release
    if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed for the Release measurement build.' }
    & cmake --build build-release --target NeNeNib
    if ($LASTEXITCODE -ne 0) { throw 'QLT-002: the Release measurement build failed.' }
    # ADR 0011 / 0013: 4 本のベンチ（基準値の鍵は 5 つ）を測って eng/perf-reference.json の
    # 指紋ごとの基準値と比べる。
    # 基準値の無い機械と窓を作れない機械は、値を記録して通る（CI は指紋ごとに持つので、
    # 基準値のある host では落ちる。ADR 0016）。
    # 終了 2 は「退行」ではなく「刺激が窓に届かず測れなかった」（Issue #30）。どちらでもゲートは落ちるが、
    # 直す先が違う（コードの速さ / 計測中の机の状態）ので別の言葉で言う。
    & python eng/measure-speed.py --check | Tee-Object -Variable speedLines
    $speed = $LASTEXITCODE
    if ($speed -eq 2) { throw 'QLT-014: speed could not be measured (the stimulus did not reach the window; see the lines above). Not a regression.' }
    if ($speed -ne 0) { throw 'QLT-014: speed regression.' }
    # ADR 0016 の決定 3: 基準値の無い host は記録だけで通る。通ったことと「速さを判定していないこと」は
    # 別なので、その 1 行をまとめにも出して黙らない（終了コードは変えない）。
    $unjudgedSpeed = @($speedLines | Where-Object { $_ -like 'Speed: no reference for *' })
    & python eng/coverage.py
    if ($LASTEXITCODE -ne 0) { throw 'QLT-009: branch coverage or its negative proof failed.' }
    & python eng/prove-gates.py
    if ($LASTEXITCODE -ne 0) { throw 'QLT-007: gate proofs failed.' }
    & git diff --check
    if ($LASTEXITCODE -ne 0) { throw 'Whitespace verification failed.' }
    Write-Host 'NeNe Nib full gate passed (UI hardware checks are recorded separately).'
    foreach ($line in $unjudgedSpeed) { Write-Host $line }
}
finally { Pop-Location }
