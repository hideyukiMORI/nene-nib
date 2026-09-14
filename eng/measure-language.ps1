[CmdletBinding()]
param([switch]$Survey)

# Phase 0 の実測（ADR 0001 / ADR 0003）。eng/probes/language.json の各ケースを MSVC cl と clang-cl で実際にコンパイルし、
# 期待どおりの結果（通る／落ちる）であることを確かめて out/phase0/results.json に残す。
# 後半は Nib 固有の実測: リンカ段のシンボル（時刻・スレッド・STL）、DirectX の静的リンク、md4c、SIMD、
# clang-tidy の検査、サニタイザ、カバレッジ、静的 CRT、/showIncludes、CMake の C++23、Vim の headless 実行。
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'toolchain.ps1')
$probeRoot = Join-Path $repoRoot 'out/phase0'
New-Item -ItemType Directory -Force -Path $probeRoot | Out-Null
$spec = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'probes/language.json') -Raw | ConvertFrom-Json
$results = @()
$mismatches = @()
# -Survey: 期待と違っても止めずに全ケースを回し、最後に一覧を出して非 0 で終わる（期待値を実測に合わせるための調査用。ゲートでは使わない）。
function Add-Result([hashtable]$entry) { $script:results += [pscustomobject]$entry }
function Relative([string]$text) {
    $trimmed = $text.Trim() -replace [regex]::Escape($probeRoot), '<probe>'
    return ($trimmed -replace [regex]::Escape($repoRoot), '<repo>')
}
function Head([string]$text, [int]$lines) { return (($text -split "`n" | Select-Object -First $lines) -join "`n").Trim() }
function Expand-Flags($flags) {
    $expanded = @()
    foreach ($flag in @($flags)) {
        if ($flag -like '@*') { $expanded += @($spec.flagSets.($flag.Substring(1))) } else { $expanded += $flag }
    }
    return $expanded
}
function Invoke-Compile([string]$tool, [string]$source, [string[]]$flags, [string]$objectPath) {
    if ($tool -eq 'msvc') {
        $arguments = @('/nologo', '/std:c++latest', '/W4', '/WX', '/permissive-', '/EHsc', '/utf-8', '/c', $source, "/Fo$objectPath") + $flags
        $output = (& cl @arguments 2>&1 | Out-String)
    }
    else {
        $arguments = @('/nologo', '/clang:-std=c++23', '/W4', '/WX', '/EHsc', '/utf-8', '/c', $source, "/Fo$objectPath") + $flags
        $output = (& clang-cl @arguments 2>&1 | Out-String)
    }
    return @{ code = $LASTEXITCODE; output = $output }
}
function Undefined-Symbols([string]$objectPath) {
    $nm = (& llvm-nm --undefined-only $objectPath 2>&1 | Out-String)
    if ($LASTEXITCODE -ne 0) { throw "llvm-nm failed for $objectPath`n$nm" }
    return @(($nm -split "`n") | ForEach-Object { $_.Trim() } | Where-Object { $_ -match '^U\s+' } | ForEach-Object { ($_ -split '\s+', 2)[1] })
}
$clangStrict = @($spec.flagSets.clangStrict)
$msvcVersion = (Get-Item (Get-Command cl).Source).VersionInfo.FileVersion
$clangVersion = (& clang-cl --version 2>&1 | Select-Object -First 1)
Write-Host "cl: $msvcVersion"
Write-Host "clang-cl: $clangVersion"
Add-Result @{ id = 'toolchain'; tool = 'versions'; exitCode = 0; output = "cl $msvcVersion / $clangVersion / SDK $env:WindowsSDKVersion" }

# 1. 言語のケース（M / K1 / S / C）
foreach ($case in $spec.cases) {
    $source = Join-Path $probeRoot ($case.id + '.cpp')
    Set-Content -LiteralPath $source -Value $case.source -Encoding utf8NoBOM -NoNewline
    $flags = Expand-Flags $case.flags
    $result = Invoke-Compile $case.tool $source $flags (Join-Path $probeRoot ($case.id + '.obj'))
    Add-Result @{ id = $case.id; tool = $case.tool; flags = @($case.flags); expectedCompiles = $case.compiles; exitCode = $result.code; output = (Relative (Head $result.output 6)) }
    Write-Host ("{0,-42} {1,-7} exit={2} expected={3}" -f $case.id, $case.tool, $result.code, $case.compiles)
    if (($result.code -eq 0) -ne $case.compiles) {
        if (-not $Survey) { throw "Unexpected compiler result: $($case.id)`n$($result.output)" }
        $script:mismatches += "$($case.id): exit=$($result.code) expected=$($case.compiles)`n$($result.output)"
    }
}

# 2. リンカ段のシンボル（L）。時刻・乱数・環境・ファイル・ロケールは C++ でも llvm-nm の未定義シンボルに現れるか。
$symbolProbes = @(
    @{ id = 'L1-system-clock'; source = "#include <chrono>`nlong long read();`nlong long read() { return std::chrono::system_clock::now().time_since_epoch().count(); }`n"; expect = '_Xtime_get_ticks' },
    @{ id = 'L1-steady-clock'; source = "#include <chrono>`nlong long read();`nlong long read() { return std::chrono::steady_clock::now().time_since_epoch().count(); }`n"; expect = '_Query_perf_counter' },
    @{ id = 'L1-random-device'; source = "#include <random>`nunsigned read();`nunsigned read() { std::random_device device; return device(); }`n"; expect = 'Random_device' },
    @{ id = 'L1-getenv'; source = "#define _CRT_SECURE_NO_WARNINGS`n#include <cstdlib>`nconst char* read();`nconst char* read() { return std::getenv(""PATH""); }`n"; expect = '^getenv$' },
    @{ id = 'L1-filesystem'; source = "#include <filesystem>`nbool read();`nbool read() { return std::filesystem::exists(""x""); }`n"; expect = '__std_fs_' },
    @{ id = 'L1-locale'; source = "#include <locale>`nbool read();`nbool read() { std::locale current; return current.name().empty(); }`n"; expect = 'locale@std@@' },
    @{ id = 'L2-win32-tick'; source = "#include <windows.h>`nunsigned long read();`nunsigned long read() { return GetTickCount(); }`n"; expect = '__imp_GetTickCount' },
    @{ id = 'L2-win32-create-file'; source = "#include <windows.h>`nvoid* open();`nvoid* open() { return CreateFileW(L""x"", 0, 0, nullptr, 3, 0, nullptr); }`n"; expect = '__imp_CreateFileW' }
)
foreach ($probe in $symbolProbes) {
    $source = Join-Path $probeRoot ($probe.id + '.cpp')
    Set-Content -LiteralPath $source -Value $probe.source -Encoding utf8NoBOM -NoNewline
    $objectPath = Join-Path $probeRoot ($probe.id + '.obj')
    $result = Invoke-Compile 'clangcl' $source @('/MT') $objectPath
    if ($result.code -ne 0) { throw "$($probe.id): probe did not compile`n$($result.output)" }
    $undefined = Undefined-Symbols $objectPath
    $matched = @($undefined | Where-Object { $_ -match $probe.expect })
    if (-not $matched) { throw "$($probe.id): llvm-nm did not expose $($probe.expect); undefined: $($undefined -join ', ')" }
    Add-Result @{ id = $probe.id; tool = 'clang-cl+llvm-nm'; exitCode = 0; expectedSymbol = $probe.expect; matched = $matched; undefined = $undefined }
    Write-Host ("{0,-42} llvm-nm: {1}" -f $probe.id, ($matched -join ', '))
}
# L3: 中核が普通に使う STL だけを使ったときの未定義シンボル。symbol-allowlist.json の出発点になる。
$stlSource = @"
#include <algorithm>
#include <array>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
enum class Failure { empty };
[[nodiscard]] std::expected<std::string, Failure> join(std::span<const std::string_view> parts);
std::expected<std::string, Failure> join(std::span<const std::string_view> parts) {
    if (parts.empty()) { return std::unexpected(Failure::empty); }
    std::string text;
    for (auto part : parts) { text.append(part); }
    return text;
}
int probe();
int probe() {
    std::vector<int> values{3, 1, 2};
    std::ranges::sort(values);
    std::variant<int, std::string> either = values.front();
    std::optional<int> maybe = values.back();
    auto owner = std::make_unique<std::array<int, 2>>();
    std::array<std::string_view, 2> parts{"a", "b"};
    auto joined = join(parts);
    return static_cast<int>(either.index()) + maybe.value_or(0) + (*owner)[0] + (joined ? static_cast<int>(joined->size()) : 0);
}
"@
$source = Join-Path $probeRoot 'L3-stl-baseline.cpp'
Set-Content -LiteralPath $source -Value $stlSource -Encoding utf8NoBOM
$objectPath = Join-Path $probeRoot 'L3-stl-baseline.obj'
$result = Invoke-Compile 'clangcl' $source ($clangStrict + @('/MT')) $objectPath
if ($result.code -ne 0) { throw "L3: STL baseline did not compile under the strict set`n$($result.output)" }
$undefined = Undefined-Symbols $objectPath
Add-Result @{ id = 'L3-stl-baseline-undefined'; tool = 'clang-cl+llvm-nm'; exitCode = 0; undefined = $undefined }
Write-Host ("L3: {0} undefined symbols from the STL baseline" -f $undefined.Count)
$sanitized = Join-Path $probeRoot 'L3-stl-baseline-asan.obj'
$result = Invoke-Compile 'clangcl' $source ($clangStrict + @('/MT', '-fsanitize=address', '-fsanitize=undefined')) $sanitized
if ($result.code -ne 0) { throw "L3: STL baseline did not compile with sanitizers`n$($result.output)" }
$sanitizedUndefined = @(Undefined-Symbols $sanitized | Where-Object { $_ -notin $undefined })
Add-Result @{ id = 'L3-stl-baseline-sanitizer-undefined'; tool = 'clang-cl+llvm-nm'; exitCode = 0; undefined = $sanitizedUndefined }

# 3. 並行性のシンボル（TH）。UI スレッド＋固定ワーカー 1 系統をシンボル許可リストで強制できるか。
$threadSource = @"
#include <windows.h>
#include <process.h>
#include <atomic>
#include <condition_variable>
#include <future>
#include <mutex>
#include <thread>
unsigned long __stdcall body(void*);
unsigned long __stdcall body(void*) { return 0; }
void probe_std_thread();
void probe_std_thread() { std::thread worker([] {}); worker.join(); }
void probe_jthread();
void probe_jthread() { std::jthread worker([] {}); }
void probe_mutex();
void probe_mutex() { std::mutex lock; std::lock_guard<std::mutex> guard(lock); }
void probe_condition();
void probe_condition() { std::condition_variable signal; signal.notify_one(); }
void probe_async();
void probe_async() { auto task = std::async(std::launch::async, [] { return 1; }); task.get(); }
void probe_win32();
void probe_win32() { HANDLE handle = CreateThread(nullptr, 0, body, nullptr, 0, nullptr); if (handle) { CloseHandle(handle); } }
void probe_crt();
void probe_crt() { uintptr_t handle = _beginthreadex(nullptr, 0, [](void*) -> unsigned { return 0u; }, nullptr, 0, nullptr); (void)handle; }
"@
$source = Join-Path $probeRoot 'TH1-threads.cpp'
Set-Content -LiteralPath $source -Value $threadSource -Encoding utf8NoBOM
$objectPath = Join-Path $probeRoot 'TH1-threads.obj'
$result = Invoke-Compile 'clangcl' $source @('/MT') $objectPath
if ($result.code -ne 0) { throw "TH1: thread probe did not compile`n$($result.output)" }
$undefined = Undefined-Symbols $objectPath
foreach ($expected in @('_beginthreadex', '__imp_CreateThread', '_Mtx_', '_Cnd_')) {
    if (-not ($undefined | Where-Object { $_ -match [regex]::Escape($expected) })) { throw "TH1: llvm-nm did not expose $expected; undefined: $($undefined -join ', ')" }
}
Add-Result @{ id = 'TH1-thread-symbols'; tool = 'clang-cl+llvm-nm'; exitCode = 0; undefined = $undefined }
Write-Host ("TH1: {0} undefined symbols; thread creation and mutex are visible to llvm-nm" -f $undefined.Count)
$atomicSource = "#include <atomic>`nint probe();`nint probe() { std::atomic<int> counter{0}; counter.fetch_add(1); return counter.load(); }`n"
$source = Join-Path $probeRoot 'TH2-atomic.cpp'
Set-Content -LiteralPath $source -Value $atomicSource -Encoding utf8NoBOM -NoNewline
$objectPath = Join-Path $probeRoot 'TH2-atomic.obj'
$result = Invoke-Compile 'clangcl' $source @('/MT') $objectPath
if ($result.code -ne 0) { throw "TH2: atomic probe did not compile`n$($result.output)" }
$atomicUndefined = Undefined-Symbols $objectPath
Add-Result @{ id = 'TH2-atomic-symbols-hole'; tool = 'clang-cl+llvm-nm'; exitCode = 0; undefined = $atomicUndefined; note = 'inline atomics leave no symbol; shared mutable state cannot be seen by the linker' }
Write-Host ("TH2: atomic fetch_add leaves {0} undefined symbols (inline)" -f $atomicUndefined.Count)

# 4. clang-tidy の名指し検査（T）。
function Invoke-Tidy([string]$id, [string]$file, [string]$checks, [string]$mustMatch, [bool]$expectReject, [string]$config = '') {
    $arguments = @($file, "--checks=-*,$checks", '--warnings-as-errors=*')
    if ($config) { $arguments += "--config=$config" }
    $arguments += @('--', '-std=c++23', '-fms-compatibility-version=19.44', '-fms-extensions')
    $output = (& clang-tidy @arguments 2>&1 | Out-String)
    $code = $LASTEXITCODE
    $rejected = ($code -ne 0) -and ($output -match $mustMatch)
    if ($expectReject -and -not $rejected) { throw "${id}: clang-tidy did not reject with $checks`n$output" }
    if (-not $expectReject -and $code -ne 0) { throw "${id}: clang-tidy unexpectedly rejected`n$output" }
    Add-Result @{ id = $id; tool = 'clang-tidy'; checks = $checks; exitCode = $code; expectedReject = $expectReject; output = (Relative (Head $output 4)) }
    Write-Host ("{0,-42} clang-tidy exit={1} expectedReject={2}" -f $id, $code, $expectReject)
}
Invoke-Tidy 'T1-tidy-aggregate-hole' (Join-Path $probeRoot 'M2-public.cpp') 'misc-non-private-member-variables-in-classes' 'non-private' $false '{CheckOptions: {misc-non-private-member-variables-in-classes.IgnoreClassesWithAllMemberVariablesBeingPublic: false, misc-non-private-member-variables-in-classes.IgnorePublicMemberVariables: false}}'
Invoke-Tidy 'T1-tidy-public-with-method' (Join-Path $probeRoot 'M2-public-method.cpp') 'misc-non-private-member-variables-in-classes' 'non-private' $true '{CheckOptions: {misc-non-private-member-variables-in-classes.IgnoreClassesWithAllMemberVariablesBeingPublic: false, misc-non-private-member-variables-in-classes.IgnorePublicMemberVariables: false}}'
Invoke-Tidy 'T2-tidy-null-dereference' (Join-Path $probeRoot 'M3-null-clang-hole.cpp') 'clang-analyzer-core.NullDereference' 'NullDereference' $true
# T3: 既定構築した空の optional の逆参照は検査が見ない（穴）。状態が分からない引数の optional は拒否する。
Invoke-Tidy 'T3-tidy-unchecked-optional-local-hole' (Join-Path $probeRoot 'M3-optional-hole.cpp') 'bugprone-unchecked-optional-access' 'unchecked-optional' $false
# MSVC STL の optional は value() だけが検査に掛かり、operator* は通る（穴）。
Set-Content -LiteralPath (Join-Path $probeRoot 'T3-optional-parameter.cpp') -Value "#include <optional>`nint read(std::optional<int> value);`nint read(std::optional<int> value) { return value.value(); }`n" -Encoding utf8NoBOM -NoNewline
Invoke-Tidy 'T3-tidy-unchecked-optional-value' (Join-Path $probeRoot 'T3-optional-parameter.cpp') 'bugprone-unchecked-optional-access' 'unchecked-optional' $true
Set-Content -LiteralPath (Join-Path $probeRoot 'T3-optional-star.cpp') -Value "#include <optional>`nint read(std::optional<int> value);`nint read(std::optional<int> value) { return *value; }`n" -Encoding utf8NoBOM -NoNewline
Invoke-Tidy 'T3-tidy-unchecked-optional-star-hole' (Join-Path $probeRoot 'T3-optional-star.cpp') 'bugprone-unchecked-optional-access' 'unchecked-optional' $false
Invoke-Tidy 'T4-tidy-const-cast' (Join-Path $probeRoot 'M2-const-cast-clang-hole.cpp') 'cppcoreguidelines-pro-type-const-cast' 'const-cast' $true
$reinterpretSource = "int forge(long long address);`nint forge(long long address) { return *reinterpret_cast<int*>(address); }`n"
Set-Content -LiteralPath (Join-Path $probeRoot 'T5-reinterpret.cpp') -Value $reinterpretSource -Encoding utf8NoBOM -NoNewline
Invoke-Tidy 'T5-tidy-reinterpret-cast' (Join-Path $probeRoot 'T5-reinterpret.cpp') 'cppcoreguidelines-pro-type-reinterpret-cast' 'reinterpret-cast' $true
$globalSource = "int counter = 0;`nint bump();`nint bump() { counter += 1; return counter; }`n"
Set-Content -LiteralPath (Join-Path $probeRoot 'T6-global.cpp') -Value $globalSource -Encoding utf8NoBOM -NoNewline
Invoke-Tidy 'T6-tidy-mutable-global' (Join-Path $probeRoot 'T6-global.cpp') 'cppcoreguidelines-avoid-non-const-global-variables' 'non-const-global' $true
$complexBody = "int deep(int a, int b);`nint deep(int a, int b) {`n    int r = 0;`n"
foreach ($i in 1..12) { $complexBody += "    if (a > $i) { if (b > $i) { r += $i; } else { r -= $i; } }`n" }
$complexBody += "    return r;`n}`n"
Set-Content -LiteralPath (Join-Path $probeRoot 'T7-complexity.cpp') -Value $complexBody -Encoding utf8NoBOM -NoNewline
Invoke-Tidy 'T7-tidy-cognitive-complexity' (Join-Path $probeRoot 'T7-complexity.cpp') 'readability-function-cognitive-complexity' 'cognitive' $true '{CheckOptions: {readability-function-cognitive-complexity.Threshold: 10}}'
# T8: Vim のコマンド分岐。60 分岐の switch は関数長で落ち、表駆動の同じ分岐は通る（複雑度 10・引数 4・60 行）。
$switchBody = "int dispatch(int key);`nint dispatch(int key) {`n    switch (key) {`n"
foreach ($i in 1..60) { $switchBody += "    case ${i}:`n        return $($i * 2);`n" }
$switchBody += "    }`n    return 0;`n}`n"
Set-Content -LiteralPath (Join-Path $probeRoot 'T8-switch.cpp') -Value $switchBody -Encoding utf8NoBOM -NoNewline
$sizeConfig = '{CheckOptions: {readability-function-cognitive-complexity.Threshold: 10, readability-function-size.LineThreshold: 60, readability-function-size.NestingThreshold: 3, readability-function-size.ParameterThreshold: 4}}'
Invoke-Tidy 'T8-tidy-switch-60-cases' (Join-Path $probeRoot 'T8-switch.cpp') 'readability-function-cognitive-complexity,readability-function-size' 'function-size' $true $sizeConfig
$tableBody = "#include <array>`nstruct Binding { int key; int action; };`nconstexpr std::array<Binding, 60> table{{`n"
foreach ($i in 1..60) { $tableBody += "    Binding{${i}, $($i * 2)},`n" }
$tableBody += "}};`nint dispatch(int key);`nint dispatch(int key) {`n    for (const auto& binding : table) {`n        if (binding.key == key) { return binding.action; }`n    }`n    return 0;`n}`n"
Set-Content -LiteralPath (Join-Path $probeRoot 'T8-table.cpp') -Value $tableBody -Encoding utf8NoBOM -NoNewline
Invoke-Tidy 'T8-tidy-table-60-entries' (Join-Path $probeRoot 'T8-table.cpp') 'readability-function-cognitive-complexity,readability-function-size' 'function-size' $false $sizeConfig

# 5. 検出層（A）。ASan は両コンパイラ、UBSan は clang-cl。
$asanSource = "#include <cstdlib>`nint main() { int* p = static_cast<int*>(std::malloc(4 * sizeof(int))); p[4] = 1; int r = p[4]; std::free(p); return r == 1 ? 0 : 3; }`n"
$asanPath = Join-Path $probeRoot 'A1-asan.cpp'
Set-Content -LiteralPath $asanPath -Value $asanSource -Encoding utf8NoBOM -NoNewline
foreach ($compiler in @('cl', 'clang-cl')) {
    $standard = if ($compiler -eq 'cl') { '/std:c++latest' } else { '/clang:-std=c++23' }
    $build = (& $compiler /nologo $standard /EHsc /W4 /WX /Zi /MT /fsanitize=address $asanPath "/Fo$probeRoot/A1-$compiler.obj" "/Fe$probeRoot/A1-$compiler.exe" 2>&1 | Out-String)
    if ($LASTEXITCODE -ne 0) { throw "A1: $compiler could not build the ASan probe`n$build" }
    $run = (& "$probeRoot/A1-$compiler.exe" 2>&1 | Out-String)
    $runCode = $LASTEXITCODE
    if ($runCode -eq 0 -or $run -notmatch 'AddressSanitizer') { throw "A1: ASan did not fire under $compiler" }
    Add-Result @{ id = "A1-asan-heap-overflow-$compiler"; tool = "$compiler+asan"; exitCode = $runCode; output = (Relative (Head ($build + $run) 4)) }
    Write-Host "A1-asan-heap-overflow-${compiler}: exit=$runCode (AddressSanitizer fired)"
}
$ubsanSource = "int main(int argc, char**) { int x = 2147483647; x += argc; return x < 0 ? 0 : 3; }`n"
$ubsanPath = Join-Path $probeRoot 'A2-ubsan.cpp'
Set-Content -LiteralPath $ubsanPath -Value $ubsanSource -Encoding utf8NoBOM -NoNewline
$build = (& clang-cl /nologo /clang:-std=c++23 /EHsc /W4 /WX /MT -fsanitize=undefined -fno-sanitize-recover=all $ubsanPath "/Fo$probeRoot/A2-ubsan.obj" "/Fe$probeRoot/A2-ubsan.exe" 2>&1 | Out-String)
if ($LASTEXITCODE -ne 0) { throw "A2: clang-cl could not build the UBSan probe`n$build" }
$run = (& "$probeRoot/A2-ubsan.exe" 2>&1 | Out-String)
if ($LASTEXITCODE -eq 0 -or $run -notmatch 'UndefinedBehaviorSanitizer|runtime error') { throw 'A2: UBSan did not fire' }
Add-Result @{ id = 'A2-ubsan-signed-overflow'; tool = 'clang-cl+ubsan'; exitCode = $LASTEXITCODE; output = (Relative (Head $run 3)) }
Write-Host "A2-ubsan-signed-overflow: exit=$LASTEXITCODE (UBSan fired)"

# 6. 整形（F1）とカバレッジ（V1）と静的 CRT（R1）。
$format = (& clang-format --dry-run --Werror "--style=file:$repoRoot/.clang-format" (Join-Path $probeRoot 'M1-w4.cpp') 2>&1 | Out-String)
if ($LASTEXITCODE -eq 0) { throw 'F1: clang-format accepted an unformatted probe' }
Add-Result @{ id = 'F1-clang-format-rejects'; tool = 'clang-format'; exitCode = $LASTEXITCODE; output = (Relative (Head $format 2)) }
Write-Host "F1-clang-format-rejects: exit=$LASTEXITCODE"
$coverageSource = "int pick(int a);`nint pick(int a) { if (a > 0) { return 1; } return 0; }`nint main() { return pick(1) == 1 ? 0 : 1; }`n"
$coveragePath = Join-Path $probeRoot 'V1-cov.cpp'
Set-Content -LiteralPath $coveragePath -Value $coverageSource -Encoding utf8NoBOM -NoNewline
$build = (& clang-cl /nologo /clang:-std=c++23 /EHsc /MT -fprofile-instr-generate -fcoverage-mapping $coveragePath "/Fo$probeRoot/V1-cov.obj" "/Fe$probeRoot/V1-cov.exe" 2>&1 | Out-String)
if ($LASTEXITCODE -ne 0) { throw "V1: coverage build failed`n$build" }
$env:LLVM_PROFILE_FILE = "$probeRoot/V1.profraw"
& "$probeRoot/V1-cov.exe" | Out-Null
& llvm-profdata merge -sparse "$probeRoot/V1.profraw" -o "$probeRoot/V1.profdata" | Out-Null
$report = (& llvm-cov report "$probeRoot/V1-cov.exe" "-instr-profile=$probeRoot/V1.profdata" 2>&1 | Out-String)
if ($report -notmatch 'TOTAL') { throw 'V1: llvm-cov produced no report' }
Add-Result @{ id = 'V1-llvm-cov-branches'; tool = 'clang-cl+llvm-cov'; exitCode = 0; output = (Relative (($report -split "`n" | Select-Object -Last 3 | Out-String).Trim())) }
Write-Host 'V1-llvm-cov-branches: report produced'
$plainSource = "#include <string>`n#include <vector>`nint main() { std::vector<std::string> names{""nib""}; return names.size() == 1 ? 0 : 1; }`n"
$plainPath = Join-Path $probeRoot 'R1-plain.cpp'
Set-Content -LiteralPath $plainPath -Value $plainSource -Encoding utf8NoBOM -NoNewline
& clang-cl /nologo /clang:-std=c++23 /EHsc /WX /MT $plainPath "/Fo$probeRoot/R1.obj" "/Fe$probeRoot/R1.exe" | Out-Null
$imports = (& llvm-readobj --coff-imports "$probeRoot/R1.exe" 2>&1 | Out-String)
$names = @((($imports -split "`n") | Where-Object { $_ -match 'Name:' } | ForEach-Object { ($_ -split 'Name:\s*')[1].Trim() }))
if (($names -join ',') -match 'VCRUNTIME|UCRTBASE|MSVCP') { throw "R1: static CRT still imports $names" }
Add-Result @{ id = 'R1-static-crt-imports'; tool = 'llvm-readobj'; exitCode = 0; imports = $names }
Write-Host "R1-static-crt-imports: $($names -join ', ')"

# 7. DirectX の静的リンク（D1）。Direct2D / DirectWrite / DXGI（flip model・waitable swap chain・composition）/ DirectComposition / DWM を
#    窓なしで呼び、依存が OS の DLL だけに収まるか。両コンパイラで build して実行する。
$directxSource = @'
#include <windows.h>
#include <d2d1_3.h>
#include <d3d11.h>
#include <dcomp.h>
#include <dwmapi.h>
#include <dwrite_3.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;
constexpr int backdrop_mica = static_cast<int>(DWMSBT_MAINWINDOW);
constexpr int backdrop_attribute = static_cast<int>(DWMWA_SYSTEMBACKDROP_TYPE);
int main()
{
    ComPtr<ID2D1Factory7> d2d;
    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, d2d.GetAddressOf()))) { return 1; }
    ComPtr<IDWriteFactory7> dwrite;
    if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory7), reinterpret_cast<IUnknown**>(dwrite.GetAddressOf())))) { return 2; }
    ComPtr<IDXGIFactory6> dxgi;
    if (FAILED(CreateDXGIFactory2(0, IID_PPV_ARGS(&dxgi)))) { return 3; }
    ComPtr<ID3D11Device> device;
    if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION, &device, nullptr, nullptr))) { return 4; }
    ComPtr<IDXGIDevice> dxgiDevice;
    if (FAILED(device.As(&dxgiDevice))) { return 5; }
    ComPtr<IDCompositionDevice> composition;
    if (FAILED(DCompositionCreateDevice(dxgiDevice.Get(), IID_PPV_ARGS(&composition)))) { return 6; }
    DXGI_SWAP_CHAIN_DESC1 description{};
    description.Width = 64;
    description.Height = 64;
    description.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    description.SampleDesc.Count = 1;
    description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    description.BufferCount = 2;
    description.Scaling = DXGI_SCALING_STRETCH;
    description.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    description.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
    description.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
    ComPtr<IDXGISwapChain1> swapChain;
    if (FAILED(dxgi->CreateSwapChainForComposition(device.Get(), &description, nullptr, &swapChain))) { return 7; }
    ComPtr<IDXGISwapChain2> waitable;
    if (FAILED(swapChain.As(&waitable))) { return 8; }
    HANDLE latency = waitable->GetFrameLatencyWaitableObject();
    if (latency == nullptr) { return 9; }
    ComPtr<ID2D1Device6> d2dDevice;
    if (FAILED(d2d->CreateDevice(dxgiDevice.Get(), &d2dDevice))) { return 10; }
    ComPtr<ID2D1DeviceContext6> context;
    if (FAILED(d2dDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &context))) { return 11; }
    ComPtr<IDXGISurface> surface;
    if (FAILED(swapChain->GetBuffer(0, IID_PPV_ARGS(&surface)))) { return 12; }
    D2D1_BITMAP_PROPERTIES1 properties = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
    ComPtr<ID2D1Bitmap1> target;
    if (FAILED(context->CreateBitmapFromDxgiSurface(surface.Get(), &properties, &target))) { return 13; }
    context->SetTarget(target.Get());
    context->BeginDraw();
    context->Clear(D2D1::ColorF(D2D1::ColorF::White));
    if (FAILED(context->EndDraw())) { return 14; }
    if (WaitForSingleObjectEx(latency, 1000, TRUE) != WAIT_OBJECT_0) { return 15; }
    if (FAILED(swapChain->Present(1, 0))) { return 16; }
    DWORD colorization = 0;
    BOOL opaque = FALSE;
    if (FAILED(DwmGetColorizationColor(&colorization, &opaque))) { return 17; }
    return backdrop_mica == 2 && backdrop_attribute == 38 ? 0 : 18;
}
'@
$directxPath = Join-Path $probeRoot 'D1-directx.cpp'
Set-Content -LiteralPath $directxPath -Value $directxSource -Encoding utf8NoBOM
$directxLibraries = @('d2d1.lib', 'dwrite.lib', 'dxgi.lib', 'd3d11.lib', 'dcomp.lib', 'dwmapi.lib')
foreach ($compiler in @('clang-cl', 'cl')) {
    if ($compiler -eq 'cl') {
        $arguments = @('/nologo', '/std:c++latest', '/W4', '/WX', '/permissive-', '/EHsc', '/utf-8', '/MT', '/DUNICODE', '/D_UNICODE', '/DWIN32_LEAN_AND_MEAN', '/DNOMINMAX', $directxPath, "/Fo$probeRoot/D1-cl.obj", "/Fe$probeRoot/D1-cl.exe", '/link') + $directxLibraries
        $build = (& cl @arguments 2>&1 | Out-String)
    }
    else {
        $arguments = @('/nologo', '/clang:-std=c++23', '/W4', '/WX', '/EHsc', '/utf-8', '/MT', '/DUNICODE', '/D_UNICODE', '/DWIN32_LEAN_AND_MEAN', '/DNOMINMAX') + $clangStrict + @($directxPath, "/Fo$probeRoot/D1-clang-cl.obj", "/Fe$probeRoot/D1-clang-cl.exe", '/link') + $directxLibraries
        $build = (& clang-cl @arguments 2>&1 | Out-String)
    }
    if ($LASTEXITCODE -ne 0) { throw "D1: $compiler could not build the DirectX probe`n$build" }
    $run = (& "$probeRoot/D1-$compiler.exe" 2>&1 | Out-String)
    $runCode = $LASTEXITCODE
    if ($runCode -ne 0) { throw "D1: DirectX probe built with $compiler exited $runCode`n$run" }
    $imports = (& llvm-readobj --coff-imports "$probeRoot/D1-$compiler.exe" 2>&1 | Out-String)
    $dlls = @((($imports -split "`n") | Where-Object { $_ -match 'Name:' } | ForEach-Object { ($_ -split 'Name:\s*')[1].Trim() }))
    if (($dlls -join ',') -match 'VCRUNTIME|UCRTBASE|MSVCP') { throw "D1: $compiler exe imports a CRT DLL: $dlls" }
    Add-Result @{ id = "D1-directx-static-$compiler"; tool = "$compiler+llvm-readobj"; exitCode = $runCode; imports = $dlls; output = (Relative (Head $build 3)) }
    Write-Host "D1-directx-static-${compiler}: exit=$runCode imports=$($dlls -join ', ')"
}

# 8. md4c（MD1）。固定 tag を clone し、Nib の厳格な警告集合と /W4 /WX だけの 2 通りで clang-cl と cl がどう扱うか。ネットワークが無ければ未実測と記録する。
$md4cTag = 'release-0.5.2'
$md4cRoot = Join-Path $probeRoot 'md4c'
if (-not (Test-Path (Join-Path $md4cRoot 'src/md4c.c'))) {
    if (Test-Path $md4cRoot) { Remove-Item -Recurse -Force $md4cRoot }
    & git clone --quiet --depth 1 --branch $md4cTag https://github.com/mity/md4c.git $md4cRoot 2>&1 | Out-Null
}
if (Test-Path (Join-Path $md4cRoot 'src/md4c.c')) {
    $md4cSource = Join-Path $md4cRoot 'src/md4c.c'
    $md4cHashes = @{}
    foreach ($file in @('src/md4c.c', 'src/md4c.h', 'LICENSE.md')) { $md4cHashes[$file] = (Get-FileHash -Algorithm SHA256 (Join-Path $md4cRoot $file)).Hash }
    $md4cCommit = (& git -C $md4cRoot rev-parse HEAD 2>&1 | Out-String).Trim()
    $cStrict = @('-Wextra', '-Wpedantic', '-Wno-switch-default', '-Wswitch-enum', '-Wcovered-switch-default', '-Wcast-qual', '-Wconversion', '-Wsign-conversion', '-Wshadow', '-Wimplicit-fallthrough', '-Wdouble-promotion', '-Wformat=2', '-Wundef', '-Wunused-result', '-Werror=vla', '-Wstrict-prototypes', '-Wmissing-prototypes')
    $md4cBuilds = @(
        @{ id = 'MD1-md4c-clang-cl-w4'; command = 'clang-cl'; arguments = @('/nologo', '/clang:-std=c17', '/W4', '/WX', '/utf-8', '/c', $md4cSource, "/Fo$probeRoot/MD1-clang-w4.obj") },
        @{ id = 'MD1-md4c-clang-cl-strict'; command = 'clang-cl'; arguments = @('/nologo', '/clang:-std=c17', '/W4', '/WX', '/utf-8', '/c', $md4cSource, "/Fo$probeRoot/MD1-clang-strict.obj") + $cStrict },
        @{ id = 'MD1-md4c-cl-w4'; command = 'cl'; arguments = @('/nologo', '/std:c17', '/W4', '/WX', '/permissive-', '/utf-8', '/c', $md4cSource, "/Fo$probeRoot/MD1-cl-w4.obj") }
    )
    foreach ($build in $md4cBuilds) {
        $output = (& $build.command @($build.arguments) 2>&1 | Out-String)
        $code = $LASTEXITCODE
        $diagnostics = @((($output -split "`n") | Where-Object { $_ -match 'warning|error' } | ForEach-Object { if ($_ -match '\[(-W[^\]]+)\]') { $Matches[1] } elseif ($_ -match '(C\d{4})') { $Matches[1] } else { $_.Trim() } }) | Sort-Object -Unique)
        Add-Result @{ id = $build.id; tool = $build.command; exitCode = $code; tag = $md4cTag; commit = $md4cCommit; sha256 = $md4cHashes; diagnostics = $diagnostics; output = (Relative (Head $output 4)) }
        Write-Host ("{0,-42} exit={1} diagnostics={2}" -f $build.id, $code, ($diagnostics -join ' '))
    }
}
else {
    Add-Result @{ id = 'MD1-md4c'; tool = 'git'; exitCode = -1; output = 'unmeasured: md4c could not be cloned (no network?)' }
    Write-Host 'MD1-md4c: unmeasured (clone failed)'
}

# 9. /showIncludes の接頭辞（H1）。Loupe Issue #5 の CP932 問題が clang-cl でも起きるか。
Set-Content -LiteralPath (Join-Path $probeRoot 'H1-include.hpp') -Value "#pragma once`nusing Shared = int;`n" -Encoding utf8NoBOM -NoNewline
Set-Content -LiteralPath (Join-Path $probeRoot 'H1-consumer.cpp') -Value "#include ""H1-include.hpp""`nShared read();`nShared read() { return 1; }`n" -Encoding utf8NoBOM -NoNewline
foreach ($compiler in @('clang-cl', 'cl')) {
    $standard = if ($compiler -eq 'cl') { '/std:c++latest' } else { '/clang:-std=c++23' }
    $output = (& $compiler /nologo $standard /showIncludes /c (Join-Path $probeRoot 'H1-consumer.cpp') "/Fo$probeRoot/H1-$compiler.obj" 2>&1 | Out-String)
    $line = (($output -split "`n") | Where-Object { $_ -match 'H1-include\.hpp' } | Select-Object -First 1)
    if (-not $line) { throw "H1: $compiler /showIncludes did not report the header`n$output" }
    $prefix = $line.Substring(0, $line.IndexOf((Join-Path $probeRoot 'H1-include.hpp').Substring(0, 3)))
    $ascii = ($prefix -match '^[\x20-\x7E]+$')
    Add-Result @{ id = "H1-showincludes-prefix-$compiler"; tool = $compiler; exitCode = 0; prefix = $prefix; asciiPrefix = $ascii; vslang = $env:VSLANG }
    Write-Host "H1-showincludes-prefix-${compiler}: '$prefix' ascii=$ascii"
}

# 10. CMake 3.31 が clang-cl の CMAKE_CXX_STANDARD 23 をどう渡すか（CM1）。
$cmakeRoot = Join-Path $probeRoot 'CM1'
if (Test-Path $cmakeRoot) { Remove-Item -Recurse -Force $cmakeRoot }
New-Item -ItemType Directory -Force -Path $cmakeRoot | Out-Null
Set-Content -LiteralPath (Join-Path $cmakeRoot 'CMakeLists.txt') -Value "cmake_minimum_required(VERSION 3.31)`nproject(CM1 LANGUAGES CXX)`nset(CMAKE_CXX_STANDARD 23)`nset(CMAKE_CXX_STANDARD_REQUIRED ON)`nset(CMAKE_CXX_EXTENSIONS OFF)`nset(CMAKE_EXPORT_COMPILE_COMMANDS ON)`nadd_executable(cm1 main.cpp)`n" -Encoding utf8NoBOM
Set-Content -LiteralPath (Join-Path $cmakeRoot 'main.cpp') -Value "int main() { auto n = 3uz; return n == 3 ? 0 : 1; }`n" -Encoding utf8NoBOM -NoNewline
$configure = (& cmake -S $cmakeRoot -B (Join-Path $cmakeRoot 'build') -G Ninja -DCMAKE_BUILD_TYPE=Debug 2>&1 | Out-String)
$configureCode = $LASTEXITCODE
$standardFlag = ''
if ($configureCode -eq 0) {
    $commands = Get-Content -LiteralPath (Join-Path $cmakeRoot 'build/compile_commands.json') -Raw | ConvertFrom-Json
    $standardFlag = (($commands[0].command -split '\s+') | Where-Object { $_ -match 'std' }) -join ' '
    $buildOutput = (& cmake --build (Join-Path $cmakeRoot 'build') 2>&1 | Out-String)
    $buildCode = $LASTEXITCODE
}
else { $buildCode = -1; $buildOutput = '' }
Add-Result @{ id = 'CM1-cmake-cxx23-clang-cl'; tool = 'cmake'; exitCode = $configureCode; buildExit = $buildCode; standardFlag = $standardFlag; output = (Relative (Head ($configure + $buildOutput) 6)) }
Write-Host "CM1-cmake-cxx23-clang-cl: configure=$configureCode build=$buildCode flag='$standardFlag'"

# 11. Vim を headless の oracle として使えるか（V2・SPEC D4）。同じ入力とキー列で決定的な出力が得られ、終了コードが 0 か。
$vimPath = Join-Path ${env:ProgramFiles} 'Vim/vim91/vim.exe'
if (Test-Path $vimPath) {
    $vimVersion = (& $vimPath --version 2>&1 | Select-Object -First 1)
    $vimRoot = Join-Path $probeRoot 'V2'
    New-Item -ItemType Directory -Force -Path $vimRoot | Out-Null
    Set-Content -LiteralPath (Join-Path $vimRoot 'input.txt') -Value "alpha beta`ngamma`n" -Encoding utf8NoBOM -NoNewline
    Set-Content -LiteralPath (Join-Path $vimRoot 'probe.vim') -Value "set nocompatible`nnormal! ggdwjp`ncall writefile(getline(1, '`$') + ['cursor=' . line('.') . ',' . col('.')] + ['reg=' . getreg('""')], 'vim-out.txt')`nqa!`n" -Encoding utf8NoBOM
    $runs = @()
    foreach ($attempt in 1..2) {
        Remove-Item -LiteralPath (Join-Path $vimRoot 'vim-out.txt') -ErrorAction SilentlyContinue
        Push-Location $vimRoot
        try { & $vimPath -u NONE -i NONE -N -n -es -S probe.vim input.txt 2>&1 | Out-Null; $vimCode = $LASTEXITCODE } finally { Pop-Location }
        $runs += @{ exitCode = $vimCode; output = @(Get-Content -LiteralPath (Join-Path $vimRoot 'vim-out.txt')) }
    }
    $expectedLines = @('beta', 'galpha amma', 'cursor=2,7', 'reg=alpha ')
    $identical = (($runs[0].output -join "`n") -eq ($runs[1].output -join "`n"))
    $asExpected = (($runs[0].output -join "`n") -eq ($expectedLines -join "`n"))
    if ($runs[0].exitCode -ne 0 -or -not $identical -or -not $asExpected) { throw "V2: Vim oracle was not deterministic or not as expected: $($runs[0].output -join ' | ')" }
    Add-Result @{ id = 'V2-vim-headless-oracle'; tool = 'vim'; exitCode = $runs[0].exitCode; version = $vimVersion; output = ($runs[0].output -join "`n"); identicalRuns = $identical }
    Write-Host "V2-vim-headless-oracle: exit=$($runs[0].exitCode) '$vimVersion' identical=$identical"
}
else {
    Add-Result @{ id = 'V2-vim-headless-oracle'; tool = 'vim'; exitCode = -1; output = 'unmeasured: vim.exe not installed' }
    Write-Host 'V2-vim-headless-oracle: unmeasured (vim.exe not found)'
}

$results | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $probeRoot 'results.json') -Encoding utf8NoBOM
if ($mismatches.Count -gt 0) {
    Write-Host "Survey: $($mismatches.Count) case(s) differ from the expectation:"
    $mismatches | ForEach-Object { Write-Host "---`n$_" }
    exit 1
}
Write-Host "Phase 0 probes matched every expected result ($($results.Count) records)."
